#ifndef APP_MSG_H
#define APP_MSG_H

#include <stdint.h>

// Messages Types
enum app_msg_type {
    APP_MSG_BUTTON_EVENT,
    APP_MSG_COMMAND,
    APP_MSG_STATUS,
};

// Messages Sources
enum app_msg_source {
    APP_SRC_SENSOR,
    APP_SRC_COMMS,
    APP_SRC_SYSTEM,
    APP_SRC_BUTTONS,
    APP_SRC_CONTROLLER,
    APP_SRC_ACTUATOR,
};

// System Modes
enum app_mode {
    APP_MODE_IDLE,
    APP_MODE_ACTIVE,
    APP_MODE_DIAG,
    APP_MODE_MAX,
};

// Commands
enum app_command_id {
    APP_CMD_LED_TOGGLE = 1,
    APP_CMD_LED_SET,
    APP_CMD_SET_MODE,
    APP_CMD_RESET_STATS,
};

// Payloads
struct app_button_payload {
    uint8_t button_id;
    uint8_t pressed;
};

struct app_command_payload {
    uint8_t command_id;
    uint32_t value;
};

struct app_status_payload {
    uint32_t uptime_ms;
};

/*
Main Message:
32-bit layout: type 4B, source 4B, timestamp 4B, union 8B 
*/
struct app_msg {
    enum app_msg_type type;
    enum app_msg_source source;
    uint32_t timestamp_ms;

    union {
        struct app_button_payload button;
        struct app_command_payload command;
        struct app_status_payload status;
    } data;
};

// Convert message type enum to a short label for logs/printing.
static inline const char *app_msg_type_str(enum app_msg_type t) {

    switch(t) {
        case APP_MSG_BUTTON_EVENT:  return "BUTTON";
        case APP_MSG_COMMAND:       return "COMMAND";
        case APP_MSG_STATUS:        return "STATUS";
        default:                    return "UNKNOWN";
    }
}

// Convert message source enum to a short label for logs/printing.
static inline const char *app_msg_source_str(enum app_msg_source s) {

    switch(s) {
        case APP_SRC_SENSOR: return "SENSOR";
        case APP_SRC_COMMS:  return "COMMS";
        case APP_SRC_SYSTEM: return "SYSTEM";
        default:             return "UNKNOWN";
    }
}

#endif /* APP_MSG_H */