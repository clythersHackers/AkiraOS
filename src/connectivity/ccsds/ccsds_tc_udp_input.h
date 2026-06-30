/**
 * @file ccsds_tc_udp_input.h
 * @brief Development UDP input for complete CCSDS TC CLTUs.
 */

#ifndef AKIRA_CCSDS_TC_UDP_INPUT_H
#define AKIRA_CCSDS_TC_UDP_INPUT_H

#include "ccsds_profile.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ccsds_tc_udp_input_stats {
    bool running;
    uint32_t datagrams_received;
    int last_error;
};

/**
 * @brief Return whether UDP TC input is available in this build.
 *
 * @return true. This module is only compiled when networking is enabled.
 */
bool ccsds_tc_udp_input_available(void);

/**
 * @brief Start listening for UDP datagrams that each contain one complete CLTU.
 *
 * @param profile TC receive profile used after UDP datagram acquisition.
 *
 * @return 0 on success, -EALREADY when already running, or a socket errno.
 */
int ccsds_tc_udp_input_start(struct ccsds_profile_tc_rx *profile);

/**
 * @brief Stop the UDP TC input listener.
 *
 * @return 0 on success.
 */
int ccsds_tc_udp_input_stop(void);

/**
 * @brief Copy current UDP TC input transport counters.
 *
 * @param stats Output snapshot.
 */
void ccsds_tc_udp_input_get_stats(struct ccsds_tc_udp_input_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_TC_UDP_INPUT_H */
