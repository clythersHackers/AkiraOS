/**
 * @file ccsds_time_packet.h
 * @brief APID 0 CCSDS spacecraft time telemetry packet producer.
 */

#ifndef AKIRA_CCSDS_TIME_PACKET_H
#define AKIRA_CCSDS_TIME_PACKET_H

#include "ccsds_types.h"

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CCSDS_TIME_PACKET_APID 0u
#define CCSDS_TIME_PACKET_FINE_BYTES 2u
#define CCSDS_TIME_PACKET_FINE_UNITS (1u << (8u * CCSDS_TIME_PACKET_FINE_BYTES))
#define CCSDS_TIME_PACKET_PAYLOAD_LEN 6u
#define CCSDS_TIME_PACKET_PERIOD K_SECONDS(10)

/**
 * @brief Build an APID 0 spacecraft time Space Packet.
 *
 * Payload layout, all fixed-width big-endian binary:
 * - bytes 0..3: uint32 spacecraft time seconds
 * - bytes 4..5: uint16 CUC-style fine time, units of 1/65536 second
 *
 * @param seconds Coarse spacecraft time seconds.
 * @param fine_time 16-bit binary fraction of one second.
 * @param sequence_count Space Packet sequence count, 0 through 0x3fff.
 * @param buf Output buffer for the encoded Space Packet.
 * @param cap Output buffer capacity in bytes.
 * @param len Written encoded packet length.
 *
 * @return 0 on success, or a negative errno.
 */
int ccsds_time_packet_build(uint32_t seconds, uint16_t fine_time,
                            uint16_t sequence_count, uint8_t *buf,
                            size_t cap, size_t *len);

/**
 * @brief Build an APID 0 time packet from Zephyr kernel uptime ticks.
 *
 * Fine time is quantized to the kernel tick rate, then represented as a
 * 16-bit binary fraction of one second.
 *
 * @return 0 on success, or a negative errno.
 */
int ccsds_time_packet_build_now(uint16_t sequence_count, uint8_t *buf,
                                size_t cap, size_t *len);

/**
 * @brief Start the explicit 10-second APID 0 time packet producer.
 *
 * The producer enqueues normal Space Packets onto @p vcid. It does not start
 * the TM frame generator and is not auto-started at boot.
 *
 * @return 0 on success, or -EINVAL for an invalid VCID.
 */
int ccsds_time_packet_start(uint8_t vcid);

/**
 * @brief Stop the APID 0 time packet producer.
 *
 * @return 0 on success.
 */
int ccsds_time_packet_stop(void);

#ifdef CONFIG_ZTEST
/**
 * @brief Run one test-local producer cycle synchronously.
 *
 * @return 0 on success.
 */
int ccsds_time_packet_test_enqueue_now(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_TIME_PACKET_H */
