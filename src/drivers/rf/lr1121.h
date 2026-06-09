/**
 * @file lr1121.h
 * @brief LR1121 LoRa/GFSK Transceiver Driver
 * @stability experimental
 * @since 1.4
 */

#ifndef AKIRA_LR1121_H
#define AKIRA_LR1121_H

#include "rf_framework.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief LR1121 configuration (legacy - kept for API compatibility)
     * @note This is no longer used - driver uses device tree configuration
     */
    struct lr1121_config
    {
        void *reserved;  /* Not used - kept for compatibility */
    };

    /**
     * @brief Initialize LR1121 driver
     * @param config Hardware configuration (ignored - uses device tree)
     * @return 0 on success
     */
    int lr1121_init_with_config(const struct lr1121_config *config);

    /**
     * @brief Get LR1121 driver interface
     * @return Driver interface pointer
     */
    const struct akira_rf_driver *lr1121_get_driver(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_LR1121_H */
