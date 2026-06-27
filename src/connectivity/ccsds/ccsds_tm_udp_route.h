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

bool ccsds_tm_udp_route_available(void);
int ccsds_tm_udp_route_register(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_TM_UDP_ROUTE_H */
