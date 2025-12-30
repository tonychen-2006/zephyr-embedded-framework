#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/gap.h>

#include <app/app_bus.h>
#include <app/app_msg.h>

LOG_MODULE_REGISTER(comms_ble, LOG_LEVEL_INF);

#define BT_UUID_ZBRAIN_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x1a2b3c4d, 0x1111, 0x2222, 0x3333, 0x1234567890ab)

#define BT_UUID_ZBRAIN_EVENT_VAL \
    BT_UUID_128_ENCODE(0x1a2b3c4d, 0x1111, 0x2222, 0x3333, 0x1234567890ac)

#define BT_UUID_ZBRAIN_CMD_VAL \
    BT_UUID_128_ENCODE(0x1a2b3c4d, 0x1111, 0x2222, 0x3333, 0x1234567890ad)

static struct bt_uuid_128 zb_service_uuid = BT_UUID_INIT_128(BT_UUID_ZBRAIN_SERVICE_VAL);
static struct bt_uuid_128 zb_event_uuid   = BT_UUID_INIT_128(BT_UUID_ZBRAIN_EVENT_VAL);
static struct bt_uuid_128 zb_cmd_uuid     = BT_UUID_INIT_128(BT_UUID_ZBRAIN_CMD_VAL);

static struct bt_conn *g_conn;
static bool g_notify_enabled;

/**
 * @brief BLE GATT write callback for command characteristic
 * 
 * Called when a BLE client writes to the command characteristic. Parses the 5-byte
 * command format and publishes it to the application message bus.
 * 
 * @param conn BLE connection handle
 * @param attr GATT attribute being written
 * @param buf Buffer containing the written data
 * @param len Length of the written data
 * @param offset Write offset (must be 0)
 * @param flags Write flags
 * @return Number of bytes written on success, BT_GATT_ERR on error
 */
static ssize_t cmd_write_cb(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len,
                            uint16_t offset, uint8_t flags);

/**
 * @brief GATT CCC (Client Characteristic Configuration) change callback
 * 
 * Called when a client enables or disables notifications on the event characteristic.
 * Updates the global notification state.
 * 
 * @param attr GATT attribute that changed
 * @param value New CCC value (BT_GATT_CCC_NOTIFY = notifications enabled)
 */
static void event_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    g_notify_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("notify %s", g_notify_enabled ? "enabled" : "disabled");
}

/**
 * @brief BLE connection established callback
 * 
 * Called when a client successfully connects. Stores the connection handle.
 * 
 * @param conn BLE connection handle
 * @param err Connection error code (0 = success)
 */
static void connected_cb(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_WRN("connect failed (err %u)", err);
        return;
    }
    g_conn = bt_conn_ref(conn);
    LOG_INF("connected");
}

/**
 * @brief BLE disconnection callback
 * 
 * Called when a client disconnects. Cleans up connection resources and resets notification state.
 * 
 * @param conn BLE connection handle
 * @param reason Disconnection reason code
 */
static void disconnected_cb(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("disconnected (reason %u)", reason);
    g_notify_enabled = false;

    if (g_conn) {
        bt_conn_unref(g_conn);
        g_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected_cb,
    .disconnected = disconnected_cb,
};

BT_GATT_SERVICE_DEFINE(zb_svc,
    BT_GATT_PRIMARY_SERVICE(&zb_service_uuid),

    BT_GATT_CHARACTERISTIC(&zb_event_uuid.uuid,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ,
                           NULL, NULL, NULL),
    BT_GATT_CCC(event_ccc_changed,
                BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),

    BT_GATT_CHARACTERISTIC(&zb_cmd_uuid.uuid,
                           BT_GATT_CHRC_WRITE,
                           BT_GATT_PERM_WRITE,
                           NULL, cmd_write_cb, NULL)
);

/**
 * @brief Send a BLE notification with event data
 * 
 * Internal helper that sends notification data to the connected client if notifications
 * are enabled. Non-blocking operation.
 * 
 * @param data Pointer to data buffer to send
 * @param len Length of data in bytes
 */
static void notify_event(const uint8_t *data, uint16_t len)
{
    if (!g_conn || !g_notify_enabled) {
        return;
    }

    struct bt_gatt_notify_params params = {
        .attr = &zb_svc.attrs[1],
        .data = data,
        .len = len,
    };

    int rc = bt_gatt_notify_cb(g_conn, &params);
    if (rc) {
        LOG_WRN("notify failed (%d)", rc);
    }
}

/**
 * @brief BLE GATT write callback implementation
 * 
 * Validates the command format (5 bytes), extracts command ID and value,
 * then publishes the command to the application message bus.
 * 
 * @param conn BLE connection handle
 * @param attr GATT attribute being written
 * @param buf Buffer containing command data
 * @param len Length of data (must be 5 bytes)
 * @param offset Write offset (must be 0)
 * @param flags Write flags
 * @return len on success, BT_GATT_ERR code on error
 */
static ssize_t cmd_write_cb(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            const void *buf, uint16_t len,
                            uint16_t offset, uint8_t flags)
{
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len != 5) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    const uint8_t *b = (const uint8_t *)buf;
    uint8_t command_id = b[0];
    uint32_t value = sys_get_le32(&b[1]);

    struct app_msg msg = {0};
    msg.type = APP_MSG_COMMAND;
    msg.source = APP_SRC_COMMS;
    msg.timestamp_ms = (uint32_t)k_uptime_get();
    msg.data.command.command_id = command_id;
    msg.data.command.value = value;

    int rc = app_bus_publish(&msg);
    LOG_INF("cmd write id=%u val=%u publish_rc=%d", command_id, value, rc);

    return len;
}

/**
 * @brief BLE transmission thread (currently disabled)
 * 
 * Originally intended to consume messages from the bus and send as notifications.
 * Currently disabled to prevent message queue conflicts - notifications are sent
 * directly via comms_ble_notify_button() instead.
 */
static void ble_tx_thread(void *, void *, void *) {
    /* Thread disabled to prevent message queue conflicts */
    /* Button notifications via BLE are not currently supported */
    while (1) {
        k_sleep(K_FOREVER);
    }
}

/**
 * @brief Send button event notification via BLE
 * 
 * Public interface for sending button press/release notifications to connected BLE clients.
 * Called directly from controller thread. Non-blocking - returns immediately if no client
 * is connected or notifications are disabled.
 * 
 * @param button_id Button identifier (0-3)
 * @param pressed Press state (0 = released, 1 = pressed)
 * @param timestamp_ms Timestamp in milliseconds since boot
 */
void comms_ble_notify_button(uint8_t button_id, uint8_t pressed, uint32_t timestamp_ms)
{
    LOG_DBG("notify_button called: id=%u pressed=%u", button_id, pressed);
    
    if (!g_conn || !g_notify_enabled) {
        LOG_DBG("notify skipped: conn=%p enabled=%d", g_conn, g_notify_enabled);
        return;
    }

    uint8_t out[1 + 1 + 1 + 4];
    out[0] = (uint8_t)APP_MSG_BUTTON_EVENT;
    out[1] = button_id;
    out[2] = pressed;
    sys_put_le32(timestamp_ms, &out[3]);
    
    LOG_DBG("calling notify_event");
    notify_event(out, sizeof(out));
    LOG_DBG("notify_event returned");
}

K_THREAD_STACK_DEFINE(ble_tx_stack, 1024);
static struct k_thread ble_tx_thread_data;

/**
 * @brief Initialize and start BLE subsystem
 * 
 * Enables the Bluetooth controller, configures advertising with device name and custom service UUID,
 * and starts advertising as a connectable peripheral. Creates the BLE TX thread (currently dormant).
 * 
 * @return 0 on success, negative error code on failure
 */
int comms_ble_start(void) {
    int rc = bt_enable(NULL);
    if (rc) {
        LOG_ERR("bt_enable failed (%d)", rc);
        return rc;
    }
    LOG_INF("BLE enabled");

    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_ZBRAIN_SERVICE_VAL),
    };

    const struct bt_data sd[] = {
        BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
    };

    struct bt_le_adv_param adv_param = {
        .id = BT_ID_DEFAULT,
        .sid = 0,
        .secondary_max_skip = 0,
        .options = 0x01,  /* Connectable */
        .interval_min = 0x0020,
        .interval_max = 0x4000,
        .peer = NULL,
    };

    rc = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (rc) {
        LOG_ERR("adv start failed (%d)", rc);
        return rc;
    }
    LOG_INF("Advertising started");

    k_thread_create(&ble_tx_thread_data,
                    ble_tx_stack,
                    K_THREAD_STACK_SIZEOF(ble_tx_stack),
                    ble_tx_thread,
                    NULL, NULL, NULL,
                    9, 0, K_NO_WAIT);

    return 0;
}