#ifndef APP_BUS_H
#define APP_BUS_H

#include <zephyr/kernel.h>
#include <app/app_msg.h>

#ifdef __cplusplus

extern "C" {
#endif

int app_bus_publish(const struct app_msg *msg);

int app_bus_get(struct app_msg *out, k_timeout_t timeout);

uint32_t app_bus_drop_count(void);

#ifdef __cplusplus
}
#endif

#endif