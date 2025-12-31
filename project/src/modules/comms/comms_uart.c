#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <app/app_bus.h>

LOG_MODULE_REGISTER(comms, LOG_LEVEL_INF); // Enable logging

static uint32_t last_button_log_ms;
static uint8_t last_button_state;

static void comms_thread(void) {

    // Disabled to allow controller to process button events
    
    while (1) {
        k_sleep(K_FOREVER);
    }

    // struct app_msg msg;
    //
    // while (1) {
    //     if (app_bus_get(&msg, K_FOREVER) != 0) {
    //         continue;
    //     }
    //
    //     if (msg.type == APP_MSG_BUTTON_EVENT) {
    //
    //         uint32_t now = k_uptime_get_32();
    //         bool changed = (msg.data.button.pressed != last_button_state);
    //
    //         if (changed || (now - last_button_log_ms) >= 200U) {
    //             LOG_INF("rx BUTTON id=%d pressed=%d ts=%u",
    //                     msg.data.button.button_id,
    //                     msg.data.button.pressed,
    //                     msg.timestamp_ms);
    //
    //             last_button_state = msg.data.button.pressed;
    //             last_button_log_ms = now;
    //         }
    //     } else {
    //         LOG_INF("rx type=%d src=%d ts=%u",
    //                 msg.type,
    //                 msg.source,
    //                 msg.timestamp_ms);
    //     }
    // }
}

K_THREAD_DEFINE(comms_tid, 1024, comms_thread, NULL, NULL, NULL, 6, 0, 0);