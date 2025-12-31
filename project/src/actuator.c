#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <app/app_bus.h>
#include <app/app_msg.h>

LOG_MODULE_REGISTER(actuator, LOG_LEVEL_INF); // Enable logging

// Get device tree node handles for LEDS led0-led3 from device tree aliases
#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay) || !DT_NODE_HAS_STATUS(LED1_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED2_NODE, okay) || !DT_NODE_HAS_STATUS(LED3_NODE, okay)
#error "One or more LED aliases are missing/disabled in the devicetree."
#endif

// GPIO descriptors for LEDS led0-led3 (pulled from device tree aliases)
static const struct gpio_dt_spec leds[4] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

static uint8_t led_state[4];

/**
 * @brief Apply LED state change
 * 
 * Sets the specified LED to the requested on/off state and updates the internal state tracking.
 * 
 * @param id LED identifier (0-3)
 * @param on Desired state: 1 for on, 0 for off
 * @return 0 on success, -EINVAL if id invalid, -ENODEV if device not ready
 */
static int led_apply(uint8_t id, uint8_t on) {

    // Validate LED id is in range [0, 3]
    if (id >= 4) {
        return -EINVAL;
    }

    // Validate GPIO device is initialized and ready
    if (!device_is_ready(leds[id].port)) {
        return -ENODEV;
    }

    // Set GPIO pin to the requested level (1=on, 0=off)
    int rc = gpio_pin_set_dt(&leds[id], on ? 1 : 0);

    if (rc == 0) {
        led_state[id] = on ? 1 : 0;
    }

    return rc;
}

/**
 * @brief Toggle the state of the specified LED
 * 
 * Public interface for toggling LEDs. Can be called directly from other modules
 * to control LEDs without using the message bus.
 * 
 * @param led_id LED identifier (0-3)
 */
void actuator_led_toggle(uint8_t led_id)
{
    if (led_id < 4) {
        (void)led_apply(led_id, (uint8_t)!led_state[led_id]);
        LOG_INF("LED%u toggle -> %u", led_id, led_state[led_id]);
    }
}

/**
 * @brief Handle incoming command messages
 * 
 * Processes command messages from the message bus and executes the appropriate action.
 * Supports LED control, mode changes, and statistics reset.
 * 
 * @param cmd Pointer to the command payload
 */
static void handle_cmd(const struct app_command_payload *cmd) {

    uint8_t id;
    uint8_t on;

    switch (cmd->command_id) {

        case APP_CMD_LED_TOGGLE:
            // Toggle the specified LED: extract ID from lower byte of value
            id = (uint8_t)cmd->value;

            if (id < 4) {
                (void)led_apply(id, (uint8_t)!led_state[id]);
                LOG_INF("LED%u toggle -> %u", id, led_state[id]);
            }
            break;

        case APP_CMD_LED_SET:
            // Set LED state explicitly: upper byte = ID, lower byte = on/off state
            id = (uint8_t)((cmd->value >> 8) & 0xFF);
            on = (uint8_t)(cmd->value & 0xFF);
            
            if (id < 4) {
                (void)led_apply(id, on ? 1 : 0);
                LOG_INF("LED%u toggle -> %u", id, led_state[id]);
            }
            break;

        case APP_CMD_SET_MODE:
            // Change mode indicator: turn off all LEDs, then turn on the one matching the mode
            for (uint8_t i = 0; i < 4; i++) {
                (void)led_apply(i, 0);
            }

            switch ((enum app_mode)cmd->value) {

                case APP_MODE_IDLE:
                    (void)led_apply(0, 1);
                    break;

                case APP_MODE_ACTIVE:
                    (void)led_apply(1, 1);
                    break;
                
                case APP_MODE_DIAG:
                    (void)led_apply(2, 1);
                    break;

                default:
                    (void)led_apply(3, 1);
                    break;
            }

            LOG_INF("mode indicator -> %u", (unsigned)cmd->value);
            break;

        case APP_CMD_RESET_STATS:
            // Flash LED 3 briefly as reset acknowledgment (80 ms pulse)
            (void)led_apply(3, 1);
            k_sleep(K_MSEC(80));
            (void)led_apply(3, 0);
            LOG_INF("reset ack");
            break;

        default:
            break;
    }
}

/**
 * @brief Actuator thread main function
 * 
 * Initializes LED GPIO pins and enters main loop to process command messages
 * from the message bus. Controls LEDs based on received commands.
 * 
 * Thread priority: 8 (lower priority than controller)
 */
static void actuator_thread(void) {

    // Initialize all 4 LED GPIO pins as outputs, initially off
    for (int i = 0; i < 4; i++) {
        if(!device_is_ready(leds[i].port)) {
            LOG_ERR("LED%d device not ready", i);
            continue;
        }

        int rc = gpio_pin_configure_dt(&leds[i], GPIO_OUTPUT_INACTIVE);

        if (rc != 0) {
            LOG_ERR("LED%d configure failed (%d)", i, rc);
        } else {
            led_state[i] = 0;
        }
    }

    LOG_INF("actuator start");

    // Main event loop: wait for and process command messages from the bus
    while (1) {
        struct app_msg msg;

        LOG_DBG("actuator waiting for message");
        int rc = app_bus_get(&msg, K_FOREVER);

        if (rc != 0) {
            LOG_ERR("app_bus_get failed: %d", rc);
            continue;
        }

        LOG_DBG("actuator got msg type=%d", msg.type);
        // Only process command messages; ignore other message types
        if (msg.type == APP_MSG_COMMAND) {
            handle_cmd(&msg.data.command);
        }
    }
}

// Create and start the actuator thread with 1024-byte stack, priority 8 (higher priority than sensors)
K_THREAD_DEFINE(actuator_tid, 1024, actuator_thread, NULL, NULL, NULL, 8, 0, 0);