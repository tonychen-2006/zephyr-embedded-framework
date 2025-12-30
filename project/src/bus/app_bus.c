#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <app/app_bus.h>

#define APP_BUS_LEN 128

K_MSGQ_DEFINE(app_bus_q, sizeof(struct app_msg), APP_BUS_LEN, 4);

static atomic_t g_drop_count;

int app_bus_publish(const struct app_msg *msg) {

    int rc = k_msgq_put(&app_bus_q, msg, K_NO_WAIT);

    if(rc != 0) {
        atomic_inc(&g_drop_count);
    }

    return rc;
}

int app_bus_get(struct app_msg *out, k_timeout_t timeout) {
    return k_msgq_get(&app_bus_q, out, timeout);
}

uint32_t app_bus_drop_count(void) {
    return (uint32_t)atomic_get(&g_drop_count);
}