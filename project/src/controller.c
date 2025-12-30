#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <app/app_bus.h>
#include <app/app_msg.h>
#include <app/comms_ble.h>
#include <app/actuator.h>

LOG_MODULE_REGISTER(controller, LOG_LEVEL_INF);

static enum app_mode g_mode = APP_MODE_IDLE;
static uint32_t g_button_press_count[16];

static void publish_cmd(uint8_t cmd_id, uint32_t value) {

    LOG_INF("publish_cmd: id=%u val=%u", cmd_id, value);
    
    struct app_msg out = {0};

    out.type = APP_MSG_COMMAND;
    out.source = APP_SRC_CONTROLLER;
    out.timestamp_ms = (uint32_t)k_uptime_get();
    out.data.command.command_id = cmd_id;
    out.data.command.value = value;

    int rc = app_bus_publish(&out);

    if (rc != 0) {
        LOG_ERR("cmd publish failed! drops=%u", app_bus_drop_count());
    } else {
        LOG_INF("publish_cmd succeeded");
    }
}

static void set_mode(enum app_mode new_mode) {

    if (new_mode >= APP_MODE_MAX) {
        return;
    }

    if (new_mode != g_mode) {
        g_mode = new_mode;
        LOG_INF("mode -> %d", g_mode);
        publish_cmd(APP_CMD_SET_MODE, (uint32_t)g_mode);
    }
}

static void handle_button_event(const struct app_button_payload *b) {

    LOG_INF("handle_button_event: id=%u pressed=%u", b->button_id, b->pressed);
    
    /* Send BLE notification for both press and release */
    comms_ble_notify_button(b->button_id, b->pressed, (uint32_t)k_uptime_get());
    
    LOG_INF("BLE notify returned");

    if (!b ->pressed) {
        return; // acts only when pressed not release
    }

    if (b->pressed < 16) {
        g_button_press_count[b->button_id]++;
    }

    switch(b->button_id) {

        case 0:
            LOG_INF("button 0: toggling LED");
            actuator_led_toggle(0);
            break;

        case 1:
            LOG_INF("button 1: toggling LED");
            actuator_led_toggle(1);
            break;
        
        case 2:
            actuator_led_toggle(2);
            set_mode((enum app_mode)((g_mode + 1) % APP_MODE_MAX));
            break;
        
        case 3:
            actuator_led_toggle(3);
            for (int i = 0; i < 16; i++) {
                g_button_press_count[i] = 0;
            }

            publish_cmd(APP_CMD_RESET_STATS, 0);
            LOG_INF("stats reset");
            break;
        
        default:
            LOG_INF("btn %u pressed (count=%u)", 
                    b->button_id,
                    (b->button_id < 16) ? g_button_press_count[b->button_id] : 0);
            break;
    }
}

static void controller_thread(void) {

    LOG_INF("controller start");

    while (1) {
        
        struct app_msg msg;

        LOG_INF("controller waiting for message");
        int rc = app_bus_get(&msg, K_FOREVER);

        if (rc != 0) {
            LOG_ERR("app_bus_get failed: %d", rc);
            continue;
        }

        LOG_INF("controller got msg type=%d", msg.type);

        switch (msg.type) {

            case APP_MSG_BUTTON_EVENT:
                handle_button_event(&msg.data.button);
                break;

            case APP_MSG_COMMAND:
                if (msg.source == APP_SRC_COMMS && 
                    msg.data.command.command_id == APP_CMD_SET_MODE) {
                        set_mode((enum app_mode)msg.data.command.value);
                } else {
                    /* Re-publish commands we don't handle so actuator can process them */
                    app_bus_publish(&msg);
                }
                break;

            case APP_MSG_STATUS:
                break;

            default:
                break;
        }
    }
}

K_THREAD_DEFINE(controller_tid, 1024, controller_thread, NULL, NULL, NULL, 7, 0, 0);