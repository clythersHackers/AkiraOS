/**
 * @file ccsds_time_service.h
 * @brief APID 0 Time & Diagnostic Service (TM Telemetry + TC Command).
 */

#ifndef AKIRA_CCSDS_TIME_SERVICE_H
#define AKIRA_CCSDS_TIME_SERVICE_H

#include "ccsds_router.h"
#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CCSDS_TIME_SERVICE_APID 0u
#define CCSDS_TIME_SERVICE_FINE_BYTES 2u
#define CCSDS_TIME_SERVICE_FINE_UNITS (1u << (8u * CCSDS_TIME_SERVICE_FINE_BYTES))
#define CCSDS_TIME_SERVICE_PAYLOAD_LEN 7u
#define CCSDS_TIME_SERVICE_PERIOD K_SECONDS(10)

/**
 * @brief Initialize the Time Service and register it with the TC router.
 *
 * @param router TC APID router.
 * @return 0 on success.
 */
int ccsds_time_service_init(struct ccsds_router *router);

/**
 * @brief Start periodic TM time telemetry.
 *
 * @param vcid Virtual Channel ID for TM packets.
 * @return 0 on success.
 */
int ccsds_time_service_start(uint8_t vcid);

/**
 * @brief Stop periodic TM time telemetry.
 */
void ccsds_time_service_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_TIME_SERVICE_H */
