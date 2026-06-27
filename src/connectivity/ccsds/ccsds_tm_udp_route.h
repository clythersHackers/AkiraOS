/**
 * @file ccsds_tm_udp_route.h
 * @brief Development UDP destination for CCSDS TM output.
 */

#ifndef AKIRA_CCSDS_TM_UDP_ROUTE_H
#define AKIRA_CCSDS_TM_UDP_ROUTE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return whether UDP routing is available in this build.
 *
 * @return true when CONFIG_NETWORKING enables the UDP route implementation.
 */
bool ccsds_tm_udp_route_available(void);

/**
 * @brief Register the development UDP TM output route.
 *
 * @return 0 on success, -ENOTSUP when networking is disabled, or a negative
 *         errno from TM route registration.
 */
int ccsds_tm_udp_route_register(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_TM_UDP_ROUTE_H */
