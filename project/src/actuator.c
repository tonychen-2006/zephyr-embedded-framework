#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <app/app_bus.h>
#include <app/app_msg.h>

LOG_MODULE_REGISTER(actuator, LOG_LEVEL_INF);

#define LED0_NODE DT_ALIAS(led0)
#define LED1_NODE DT_ALIAS(led1)
#define LED2_NODE DT_ALIAS(led2)
#define LED3_NODE DT_ALIAS(led3)

#if !DT_NODE_HAS_STATUS(LED0_NODE, okay) || !DT_NODE_HAS_STATUS(LED1_NODE, okay) || \
    !DT_NODE_HAS_STATUS(LED2_NODE, okay) || !DT_NODE_HAS_STATUS(LED3_NODE, okay)
#error "One or more LED aliases are missing/disabled in the devicetree."
#endif

static const struct gpio_dt_spec leds[4] = {
    GPIO_DT_SPEC_GET(LED0_NODE, gpios),
    GPIO_DT_SPEC_GET(LED1_NODE, gpios),
    GPIO_DT_SPEC_GET(LED2_NODE, gpios),
    GPIO_DT_SPEC_GET(LED3_NODE, gpios),
};

static uint8_t led_state[4];

static int led_apply(uint8_t id, uint8_t on) {

    if (id >= 4) {
        return -EINVAL;
    }

    if (!device_is_ready(leds[id].port)) {
        return -ENODEV;
    }

    int rc = gpio_pin_set_dt(&leds[id], on ? 1 : 0);

    if (rc == 0) {
        led_state[id] = on ? 1 : 0;
    }

    return rc;
}

void actuator_led_toggle(uint8_t led_id)
{
    if (led_id < 4) {
        (void)led_apply(led_id, (uint8_t)!led_state[led_id]);
        LOG_INF("LED%u toggle -> %u", led_id, led_state[led_id]);
    }
}

static void handle_cmd(const struct app_command_payload *cmd) {

    uint8_t id;
    uint8_t on;

    switch (cmd->command_id) {

        case APP_CMD_LED_TOGGLE:
            id = (uint8_t)cmd->value;

            if (id < 4) {
                (void)led_apply(id, (uint8_t)!led_state[id]);
                LOG_INF("LED%u toggle -> %u", id, led_state[id]);
            }
            break;

        case APP_CMD_LED_SET:
            id = (uint8_t)((cmd->value >> 8) & 0xFF);
            on = (uint8_t)(cmd->value & 0xFF);
            
            if (id < 4) {
                (void)led_apply(id, on ? 1 : 0);
                LOG_INF("LED%u toggle -> %u", id, led_state[id]);
            }
            break;

        case APP_CMD_SET_MODE:
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
            (void)led_apply(3, 1);
            k_sleep(K_MSEC(80));
            (void)led_apply(3, 0);
            LOG_INF("reset ack");
            break;

        default:
            break;
    }
}

static void actuator_thread(void) {

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

    while (1) {
        struct app_msg msg;

        LOG_DBG("actuator waiting for message");
        int rc = app_bus_get(&msg, K_FOREVER);

        if (rc != 0) {
            LOG_ERR("app_bus_get failed: %d", rc);
            continue;
        }

        LOG_DBG("actuator got msg type=%d", msg.type);
        if (msg.type == APP_MSG_COMMAND) {
            handle_cmd(&msg.data.command);
        }
    }
}

K_THREAD_DEFINE(actuator_tid, 1024, actuator_thread, NULL, NULL, NULL, 8, 0, 0);