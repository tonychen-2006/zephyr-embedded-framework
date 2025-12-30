#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <app/comms_ble.h>

LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

/**
 * @brief Application entry point
 * 
 * Initializes the BLE communication subsystem and enters idle loop.
 * Other subsystems (controller, actuator, sensor) are started automatically
 * via K_THREAD_DEFINE.
 * 
 * @return Does not return
 */
int main(void) {
    LOG_INF("system boot");

    comms_ble_start(); // Begin BLE controls

    while(1) {
        k_sleep(K_SECONDS(10));
    }
}