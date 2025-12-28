#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

// int main(void)
// {
//         return 0;
// }

#define LED3_NODE DT_ALIAS(led3)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED3_NODE, gpios);

int main(void)
{
    if (!device_is_ready(led.port)) return 0;
    gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);

    while (1) {
        gpio_pin_toggle_dt(&led);
        k_sleep(K_MSEC(500));
    }
}