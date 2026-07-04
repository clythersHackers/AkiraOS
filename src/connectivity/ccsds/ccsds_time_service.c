#include "ccsds_time_service.h"

#include <errno.h>
#include <string.h>

#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include "ccsds_space_packet.h"
#include "ccsds_tm_frame.h"

LOG_MODULE_REGISTER(ccsds_time_service, CONFIG_AKIRA_LOG_LEVEL);

#define TIME_FUNC_SET 0x01u
#define TIME_FUNC_ADJUST 0x02u
#define TIME_FUNC_PING 0xFFu

static struct k_work_delayable time_service_work;
static bool time_service_work_initialized;
static bool time_service_running;
static uint8_t time_service_vcid;
static uint16_t time_service_sequence_count;
static uint8_t time_service_buf[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                                CCSDS_TIME_SERVICE_PAYLOAD_LEN + 1u];

/* Simple service-level offset from kernel uptime */
static int64_t time_offset_ticks;

static int build_tm_packet(uint8_t func_id, uint8_t *buf, size_t cap,
                           size_t *len)
{
    uint8_t payload[CCSDS_TIME_SERVICE_PAYLOAD_LEN + 1u];
    struct ccsds_space_packet packet = {
        .version = 0u,
        .type = CCSDS_PACKET_TYPE_TM,
        .secondary_header = false,
        .apid = CCSDS_TIME_SERVICE_APID,
        .sequence_flags = CCSDS_SEQUENCE_UNSEGMENTED,
        .sequence_count = time_service_sequence_count,
        .payload = payload,
        .payload_len = 0,
    };

    int64_t now_ticks = k_uptime_ticks() + time_offset_ticks;
    uint32_t s = (uint32_t)(now_ticks / CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    uint32_t f_ticks = (uint32_t)(now_ticks % CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    uint16_t f = (uint16_t)(((uint64_t)f_ticks * CCSDS_TIME_SERVICE_FINE_UNITS) /
                            CONFIG_SYS_CLOCK_TICKS_PER_SEC);

    payload[0] = func_id;
    sys_put_be32(s, &payload[1]);
    sys_put_be16(f, &payload[5]);
    packet.payload_len = 7u;

    int ret = ccsds_space_packet_encode(&packet, buf, cap, len);
    if (ret == 0) {
        time_service_sequence_count = (time_service_sequence_count + 1u) & 0x3fffu;
    }
    return ret;
}

static void time_service_work_handler(struct k_work *work)
{
    size_t len;
    ARG_UNUSED(work);

    if (!time_service_running) {
        return;
    }

    /* Periodic telemetry uses 0x00 as function ID (Implicit Time Report) */
    if (build_tm_packet(0x00u, time_service_buf, sizeof(time_service_buf), &len) == 0) {
        (void)ccsds_tm_frame_add(time_service_vcid, time_service_buf, len, K_NO_WAIT);
    }

    if (time_service_running) {
        (void)k_work_schedule(&time_service_work, CCSDS_TIME_SERVICE_PERIOD);
    }
}

static int handle_tc_packet(const struct ccsds_space_packet *packet,
                            void *user_data)
{
    ARG_UNUSED(user_data);

    if (packet->payload_len < 1u) {
        return -EINVAL;
    }

    uint8_t func = packet->payload[0];

    switch (func) {
    case TIME_FUNC_PING: {
        size_t len;
        LOG_INF("Time Service Ping");
        if (build_tm_packet(0xFFu, time_service_buf, sizeof(time_service_buf), &len) == 0) {
            (void)ccsds_tm_frame_add(time_service_vcid, time_service_buf, len, K_NO_WAIT);
        }
        break;
    }

    case TIME_FUNC_SET: {
        if (packet->payload_len < 7u) {
            return -EINVAL;
        }
        uint32_t s = sys_get_be32(&packet->payload[1]);
        uint16_t f = sys_get_be16(&packet->payload[5]);
        int64_t target_ticks = (int64_t)s * CONFIG_SYS_CLOCK_TICKS_PER_SEC +
                               ((int64_t)f * CONFIG_SYS_CLOCK_TICKS_PER_SEC) /
                                   CCSDS_TIME_SERVICE_FINE_UNITS;
        time_offset_ticks = target_ticks - k_uptime_ticks();
        LOG_INF("Time Service Set: s=%u f=%u", s, f);
        break;
    }

    case TIME_FUNC_ADJUST: {
        if (packet->payload_len < 7u) {
            return -EINVAL;
        }
        int32_t s_adj = (int32_t)sys_get_be32(&packet->payload[1]);
        int16_t f_adj = (int16_t)sys_get_be16(&packet->payload[5]);
        int64_t adj_ticks = (int64_t)s_adj * CONFIG_SYS_CLOCK_TICKS_PER_SEC +
                            ((int64_t)f_adj * CONFIG_SYS_CLOCK_TICKS_PER_SEC) /
                                CCSDS_TIME_SERVICE_FINE_UNITS;
        time_offset_ticks += adj_ticks;
        LOG_INF("Time Service Adjust: s=%d f=%d", s_adj, f_adj);
        break;
    }

    default:
        LOG_WRN("Time Service unknown func: 0x%02x", func);
        return -ENOTSUP;
    }

    return 0;
}

int ccsds_time_service_init(struct ccsds_router *router)
{
    if (!time_service_work_initialized) {
        k_work_init_delayable(&time_service_work, time_service_work_handler);
        time_service_work_initialized = true;
    }
    return ccsds_router_register_apid(router, CCSDS_TIME_SERVICE_APID,
                                      handle_tc_packet, NULL);
}

int ccsds_time_service_start(uint8_t vcid)
{
    if (vcid > 7u) {
        return -EINVAL;
    }

    time_service_vcid = vcid;
    time_service_running = true;

    if (!time_service_work_initialized) {
        k_work_init_delayable(&time_service_work, time_service_work_handler);
        time_service_work_initialized = true;
    }

    (void)k_work_schedule(&time_service_work, K_NO_WAIT);
    return 0;
}

void ccsds_time_service_stop(void)
{
    time_service_running = false;
    (void)k_work_cancel_delayable(&time_service_work);
}
