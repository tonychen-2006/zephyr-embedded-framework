#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <app/app_bus.h>
#include <app/app_msg.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF); // Enables logging

// Get device tree node handles for buttons sw0-sw3 from device tree aliases
#define SW0_NODE DT_ALIAS(sw0)
#define SW1_NODE DT_ALIAS(sw1)
#define SW2_NODE DT_ALIAS(sw2)
#define SW3_NODE DT_ALIAS(sw3)

// Verify all button aliases exist and are enabled in the device tree at compile-time
#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "sw0 alias not found in DeviceTree."
#endif
#if !DT_NODE_HAS_STATUS(SW1_NODE, okay)
#error "sw1 alias not found in DeviceTree."
#endif
#if !DT_NODE_HAS_STATUS(SW2_NODE, okay)
#error "sw2 alias not found in DeviceTree."
#endif
#if !DT_NODE_HAS_STATUS(SW3_NODE, okay)
#error "sw3 alias not found in DeviceTree."
#endif

// GPIO descriptors for buttons sw0-sw3 (pulled from device tree aliases)
static const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};

/**
 * @brief Sensor polling thread
 * 
 * Continuously polls button GPIO states and publishes button press/release events
 * to the application message bus. Detects state changes by comparing current state
 * with previous state. Logs warnings if message bus is full.
 * 
 * Thread priority: 5 (highest priority - ensures button events are captured)
 * Polling interval: 10ms
 */
static void sensor_thread(void) {

    for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
        // Quits if the GPIO controller for this button isn't ready
        if (!device_is_ready(buttons[i].port)) return;

        // Configure this button pin as an input per its DT flags
        gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
    }

    uint8_t last[ARRAY_SIZE(buttons)];
    for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
        // Set last[] with current button state at startup
        last[i] = gpio_pin_get_dt(&buttons[i]);
    }

    while(1) {

        for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
            // Read current button state
            int cur = gpio_pin_get_dt(&buttons[i]);

            if (cur != last[i]) {
                // State changed, update last-seen value
                last[i] = cur;

                struct app_msg msg = {0};

                // Populate button event message
                msg.type = APP_MSG_BUTTON_EVENT;
                msg.source = APP_SRC_SENSOR;
                msg.timestamp_ms = k_uptime_get_32(); // uptime (ms, 32-bit)
                msg.data.button.button_id = i;
                msg.data.button.pressed = (cur == 0) ? 1 : 0;

                // Publish to app bus and log outcome
                int send_rc = app_bus_publish(&msg);
                if (send_rc != 0) {
                    LOG_WRN("bus full (drops=%u)", app_bus_drop_count());
                } else {
                    LOG_INF("button event: pressed=%d", msg.data.button.pressed);
                }
            }
        }

        k_sleep(K_MSEC(10)); // Poll interval, sleep 10 ms between scans
    }
}

// Start sensor polling thread (stack 1024 bytes, priority 5)
K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 5, 0, 0);