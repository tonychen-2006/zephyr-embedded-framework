#ifndef COMMS_BLE_H
#define COMMS_BLE_H

#include <stdint.h>

int comms_ble_start(void);
void comms_ble_notify_button(uint8_t button_id, uint8_t pressed, uint32_t timestamp_ms);

#endif /* COMMS_BLE_H */