#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <app/app_bus.h>

#define APP_BUS_LEN 128

// Static app message queue: APP_BUS_LEN entries of struct app_msg, 4-byte aligned
K_MSGQ_DEFINE(app_bus_q, sizeof(struct app_msg), APP_BUS_LEN, 4);

static atomic_t g_drop_count; // Atomic such that incrementation is thread-safe

/**
 * @brief Publish a message to the application message bus
 * 
 * Attempts to add a message to the shared message queue. If the queue is full,
 * the message is dropped and the drop counter is incremented.
 * 
 * @param msg Pointer to the message to publish
 * @return 0 on success, negative error code if queue is full
 */
int app_bus_publish(const struct app_msg *msg) {

    int rc = k_msgq_put(&app_bus_q, msg, K_NO_WAIT);

    if(rc != 0) {
        atomic_inc(&g_drop_count);
    }

    return rc;
}

/**
 * @brief Retrieve a message from the application message bus
 * 
 * Blocks until a message is available in the queue or timeout expires.
 * 
 * @param out Pointer to buffer where the message will be copied
 * @param timeout Maximum time to wait for a message (K_FOREVER, K_NO_WAIT, or specific timeout)
 * @return 0 on success, negative error code on failure or timeout
 */
int app_bus_get(struct app_msg *out, k_timeout_t timeout) {
    return k_msgq_get(&app_bus_q, out, timeout);
}

/**
 * @brief Get the total number of dropped messages
 * 
 * Returns the count of messages that could not be published due to queue being full.
 * 
 * @return Total number of dropped messages since boot
 */
uint32_t app_bus_drop_count(void) {
    return (uint32_t)atomic_get(&g_drop_count);
}