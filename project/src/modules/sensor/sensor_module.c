#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include <app/app_bus.h>
#include <app/app_msg.h>

LOG_MODULE_REGISTER(sensor, LOG_LEVEL_INF);

#define SW0_NODE DT_ALIAS(sw0)

#if !DT_NODE_HAS_STATUS(SW0_NODE, okay)
#error "sw0 alias not found in DeviceTree."
#endif

static const struct gpio_dt_spec buttons[] = {
    GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw1), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw2), gpios),
    GPIO_DT_SPEC_GET(DT_ALIAS(sw3), gpios),
};

static void sensor_thread(void) {

    for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
        if (!device_is_ready(buttons[i].port)) return;

        gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
    }

    uint8_t last[ARRAY_SIZE(buttons)];
    for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
        last[i] = gpio_pin_get_dt(&buttons[i]);
    }

    while(1) {

        for (int i = 0; i < ARRAY_SIZE(buttons); i++) {
            int cur = gpio_pin_get_dt(&buttons[i]);

            if (cur != last[i]) {
                last[i] = cur;

                struct app_msg msg = {0};

                msg.type = APP_MSG_BUTTON_EVENT;
                msg.source = APP_SRC_SENSOR;
                msg.timestamp_ms = k_uptime_get_32();
                msg.data.button.button_id = i;
                msg.data.button.pressed = (cur == 0) ? 1 : 0;

                int send_rc = app_bus_publish(&msg);
                if (send_rc != 0) {
                    LOG_WRN("bus full (drops=%u)", app_bus_drop_count());
                } else {
                    LOG_INF("button event: pressed=%d", msg.data.button.pressed);
                }
            }
        }

        k_sleep(K_MSEC(10));
    }
}

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 5, 0, 0);