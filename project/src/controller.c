#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <app/app_bus.h>
#include <app/app_msg.h>
#include <app/comms_ble.h>
#include <app/actuator.h>

LOG_MODULE_REGISTER(controller, LOG_LEVEL_INF); // Enable logging

static enum app_mode g_mode = APP_MODE_IDLE;
static uint32_t g_button_press_count[16];

/**
 * @brief Publish a command to the message bus
 * 
 * Creates and publishes a command message to the application bus.
 * Logs errors if the bus is full.
 * 
 * @param cmd_id Command identifier
 * @param value Command value parameter
 */
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

/**
 * @brief Set the system operating mode
 * 
 * Changes the current system mode and logs the transition.
 * 
 * @param new_mode Desired operating mode
 */
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

/**
 * @brief Handle button press/release events
 * 
 * Processes button events from the sensor module. Sends BLE notifications for all events
 * and triggers LED toggles or other actions for button presses. Maintains press counters
 * for each button.
 * 
 * @param b Pointer to the button event payload
 */
static void handle_button_event(const struct app_button_payload *b) {

    LOG_INF("handle_button_event: id=%u pressed=%u", b->button_id, b->pressed);
    
    // Send BLE notification for both press and release
    comms_ble_notify_button(b->button_id, b->pressed, (uint32_t)k_uptime_get());
    
    LOG_INF("BLE notify returned");

    // Ignore button release events; only process presses
    if (!b ->pressed) {
        return; // acts only when pressed not release
    }

    // Track button press count (if button ID is in range)
    if (b->pressed < 16) {
        g_button_press_count[b->button_id]++;
    }

    switch(b->button_id) {

        case 0:
            // Button 0: toggle LED 0
            LOG_INF("button 0: toggling LED");
            actuator_led_toggle(0);
            break;

        case 1:
            // Button 1: toggle LED 1
            LOG_INF("button 1: toggling LED");
            actuator_led_toggle(1);
            break;
        
        case 2:
            // Button 2: toggle LED 2 and cycle to next mode (IDLE → ACTIVE → DIAG → IDLE...)
            actuator_led_toggle(2);
            set_mode((enum app_mode)((g_mode + 1) % APP_MODE_MAX));
            break;
        
        case 3:
            // Button 3: toggle LED 3, reset all button counters, and publish reset command
            actuator_led_toggle(3);
            for (int i = 0; i < 16; i++) {
                g_button_press_count[i] = 0;
            }

            publish_cmd(APP_CMD_RESET_STATS, 0);
            LOG_INF("stats reset");
            break;
        
        default:
            // Any other button: just log the press event with its counter
            LOG_INF("btn %u pressed (count=%u)", 
                    b->button_id,
                    (b->button_id < 16) ? g_button_press_count[b->button_id] : 0);
            break;
    }
}

/**
 * @brief Controller thread main function
 * 
 * Main event loop that processes button events and commands from the message bus.
 * Coordinates LED control, mode changes, and BLE notifications.
 * 
 * Thread priority: 7 (higher than actuator, lower than sensor)
 */
static void controller_thread(void) {

    LOG_INF("controller start");

    // Main event loop: wait for and dispatch button events and commands
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
                // Handle button press/release and send BLE notifications
                handle_button_event(&msg.data.button);
                break;

            case APP_MSG_COMMAND:
                // Handle SET_MODE commands from BLE (comms), pass others to actuator
                if (msg.source == APP_SRC_COMMS && 
                    msg.data.command.command_id == APP_CMD_SET_MODE) {
                        set_mode((enum app_mode)msg.data.command.value);
                } else {
                    // Re-publish commands that are not handled so actuator can process them
                    app_bus_publish(&msg);
                }
                break;

            case APP_MSG_STATUS:
                // Status messages not yet implemented
                break;

            default:
                break;
        }
    }
}

// Create and start the controller thread with 1024-byte stack, priority 7 (between sensor and actuator)
K_THREAD_DEFINE(controller_tid, 1024, controller_thread, NULL, NULL, NULL, 7, 0, 0);