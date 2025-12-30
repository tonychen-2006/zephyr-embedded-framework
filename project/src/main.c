#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <app/comms_ble.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

int main(void) {
    LOG_INF("system boot");

    comms_ble_start();

    while(1) {
        k_sleep(K_SECONDS(10));
    }
}