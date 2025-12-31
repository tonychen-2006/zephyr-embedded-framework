# Zephyr Embedded Framework

A Zephyr RTOS–based application demonstrating a multi-threaded, message-driven architecture for embedded systems. The project integrates GPIO button/LED control, BLE wireless communication, and event-driven message routing across independent, priority-scheduled threads.

## Architecture Overview

The application follows a **layered, message-passing architecture** with three main functional threads and a central application message bus:

```
┌─────────────────────────────────────────┐
│  BLE GATT Service (comms_ble)           │
│  - Advertising, connection mgmt         │
│  - Notify/Write characteristics         │
└────────────────┬────────────────────────┘
                 │ Notifications & Commands
                 ↓
        ┌────────────────────┐
        │  Application Bus   │
        │  (Message Queue)   │
        └────────────────────┘
                 ↑
     ┌───────────┼───────────┐
     ↑           ↑           ↑
┌─────────┐ ┌─────────┐ ┌──────────┐
│ Sensor  │ │Controller│ │ Actuator │
│ Module  │ │ (Logic)  │ │ (Output) │
│ (Input) │ │ Prio: 7  │ │ Prio: 8  │
│ Prio: 6 │ └─────────┘ └──────────┘
└─────────┘   Button      LED Control
              Events
```

### **Threads & Responsibilities** (by priority, lower number = higher priority)

#### **Sensor Module** (Priority 6)
- **File:** `src/modules/sensor/sensor_module.c`
- **Role:** Monitors hardware inputs (buttons via GPIO)
- **Task:** Polls button states every 10ms, detects press/release transitions, publishes button events to the message bus
- **Outputs:** `APP_MSG_BUTTON_EVENT` messages

#### **Controller** (Priority 7)
- **File:** `src/controller.c`
- **Role:** Central logic and coordination
- **Tasks:**
  - Receives button events and sends BLE notifications
  - Handles mode transitions and command routing
  - Processes commands from BLE (e.g., SET_MODE)
  - Routes other commands (e.g., LED control) to the actuator
- **Outputs:** `APP_MSG_COMMAND` messages, BLE notifications

#### **Actuator** (Priority 8)
- **File:** `src/actuator.c`
- **Role:** Controls hardware outputs (LEDs)
- **Task:** Receives commands from the bus and applies LED state changes
- **Inputs:** `APP_MSG_COMMAND` messages

#### **BLE Communications** (Asynchronous)
- **File:** `src/modules/comms/comms_ble.c`
- **Role:** Wireless interface to connected devices
- **Tasks:**
  - Advertises a custom GATT service with Notify/Write characteristics
  - Receives BLE write commands and publishes to the message bus
  - Sends button event notifications to connected clients
  - Manages connections (connect/disconnect callbacks)

### **Message Bus** (`app_bus`)
A lightweight, fixed-size queue for inter-thread communication:
- Defined in: `include/app/app_msg.h`, `include/app/app_bus.h`
- Message types: `BUTTON_EVENT`, `COMMAND`, `STATUS`
- Thread-safe enqueue/dequeue with overflow tracking

---

## Hardware Setup

- **Buttons:** 4 GPIO inputs (pulled from device tree aliases `button0`–`button3`)
- **LEDs:** 4 GPIO outputs (pulled from device tree aliases `led0`–`led3`)
- **BLE Radio:** nRF52840 Bluetooth interface

---

## System Modes

The device supports three operating modes, indicated by which LED is lit:

| Mode | LED | Description |
|------|-----|-------------|
| **IDLE** (0) | LED 0 | Default; awaiting input |
| **ACTIVE** (1) | LED 1 | Processing or communicating |
| **DIAG** (2) | LED 2 | Diagnostic/service mode |

Mode transitions are triggered by **Button 2** (cycles through modes) or BLE **SET_MODE** command.

---

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