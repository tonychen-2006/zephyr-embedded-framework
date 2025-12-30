# zephyr-embedded-framework
## BLE Commands

The device exposes a custom GATT service with two characteristics:

### Notify Characteristic
Sends button press/release events to connected devices.

**Format (7 bytes):**
- Byte 0: Message type (0 = button event)
- Byte 1: Button ID (0-3)
- Byte 2: Pressed state (0 = released, 1 = pressed)
- Bytes 3-6: Timestamp (milliseconds since boot, little-endian)

### Write Characteristic
Receives commands from connected devices to control LEDs.

**Format (5 bytes):**
- Byte 0: Command ID
- Bytes 1-4: Value (32-bit integer, little-endian)

**Available Commands:**

#### Toggle LED (Command ID = 1)
Toggles the specified LED on/off.
- `01 00 00 00 00` - Toggle LED 0
- `01 01 00 00 00` - Toggle LED 1
- `01 02 00 00 00` - Toggle LED 2
- `01 03 00 00 00` - Toggle LED 3

#### Set LED (Command ID = 2)
Sets the specified LED to a specific state.
- `02 00 00 00 00` - Turn OFF LED 0
- `02 01 00 00 00` - Turn ON LED 0
- `02 00 01 00 00` - Turn OFF LED 1
- `02 01 01 00 00` - Turn ON LED 1
- `02 00 02 00 00` - Turn OFF LED 2
- `02 01 02 00 00` - Turn ON LED 2

#### Set Mode (Command ID = 3)
Changes the system operating mode (0 = Idle, 1 = Active, 2 = Diagnostic).
- `03 00 00 00 00` - Set Idle mode
- `03 01 00 00 00` - Set Active mode
- `03 02 00 00 00` - Set Diagnostic mode

#### Reset Stats (Command ID = 4)
Resets button press counters.
- `04 00 00 00 00` - Reset statistics

## Connection
- **Device Name:** ZephyrDevice
- **Advertising:** Connectable, includes device name

Use nRF Connect (iOS/Android) or similar BLE apps to connect and control the device.

## Connecting via BLE (nRF Connect example)
1) Power the board and wait for advertising (`Advertising started` in logs). Name is **ZephyrDevice**.
2) In nRF Connect, tap Scan and find **ZephyrDevice**, then Connect.
3) Expand the custom service:
	- Service UUID: `1a2b3c4d-1111-2222-3333-1234567890ab`
	- Notify characteristic UUID: `1a2b3c4d-1111-2222-3333-1234567890ac`
	- Write characteristic UUID: `1a2b3c4d-1111-2222-3333-1234567890ad`
4) Enable notifications on the Notify characteristic (toggle the bell icon). You should see "notify enabled" in the device log.
5) Press board buttons to see live Notify updates (7-byte payload described above).
6) To control LEDs from the phone, write to the Write characteristic using the command formats listed above (5-byte payload).
7) If characteristics are not visible or notify cannot be enabled, clear the phone's BLE cache (forget device / clear app cache) and reconnect.