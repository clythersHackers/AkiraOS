/**
 * @file ccsds.h
 * @brief Public CCSDS API umbrella for AkiraOS.
 */

#ifndef AKIRA_CONNECTIVITY_CCSDS_H
#define AKIRA_CONNECTIVITY_CCSDS_H

#ifdef CONFIG_AKIRA_CCSDS_CFDP
#include "ccsds/akira_cfdp_service.h"
#include "ccsds/akira_cfdp_staging.h"
#endif
#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
#include "ccsds/ccsds_bch.h"
#include "ccsds/ccsds_cltu.h"
#endif
#include "ccsds/ccsds_profile.h"
#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
#include "ccsds/ccsds_rnd.h"
#include "ccsds/ccsds_rs.h"
#endif
#include "ccsds/ccsds_router.h"
#include "ccsds/ccsds_space_packet.h"
#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
#include "ccsds/ccsds_tc_frame.h"
#ifdef CONFIG_NETWORKING
#include "ccsds/ccsds_udp.h"
#endif
#include "ccsds/ccsds_time_packet.h"
#include "ccsds/ccsds_tm_frame.h"
#include "ccsds/ccsds_tm_udp_route.h"
#endif
#include "ccsds/ccsds_types.h"

#endif /* AKIRA_CONNECTIVITY_CCSDS_H */
