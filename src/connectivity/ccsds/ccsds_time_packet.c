#include "ccsds_time_packet.h"

#include <errno.h>

#include "ccsds_space_packet.h"
#include "ccsds_tm_frame.h"

static struct k_work_delayable time_packet_work;
static bool time_packet_work_initialized;
static bool time_packet_running;
static uint8_t time_packet_vcid;
static uint16_t time_packet_sequence_count;
static uint8_t time_packet_buf[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                               CCSDS_TIME_PACKET_PAYLOAD_LEN];

static void write_be32(uint8_t *buf, uint32_t value)
{
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

static void write_be16(uint8_t *buf, uint16_t value)
{
    buf[0] = (uint8_t)(value >> 8);
    buf[1] = (uint8_t)value;
}

/* Periodically build and enqueue APID 0 without driving TM frame emission. */
static void time_packet_work_handler(struct k_work *work)
{
    size_t len;

    ARG_UNUSED(work);

    if (!time_packet_running) {
        return;
    }

    if (ccsds_time_packet_build_now(time_packet_sequence_count,
                                    time_packet_buf,
                                    sizeof(time_packet_buf), &len) == 0) {
        if (ccsds_tm_frame_add(time_packet_vcid, time_packet_buf, len,
                               K_NO_WAIT) == 0) {
            time_packet_sequence_count =
                (time_packet_sequence_count + 1u) & 0x3fffu;
        }
    }

    if (time_packet_running) {
        (void)k_work_schedule(&time_packet_work, CCSDS_TIME_PACKET_PERIOD);
    }
}

/* Lazily initialize the delayable work item so tests can reset TM separately. */
static void time_packet_init_once(void)
{
    if (time_packet_work_initialized) {
        return;
    }

    k_work_init_delayable(&time_packet_work, time_packet_work_handler);
    time_packet_work_initialized = true;
}

int ccsds_time_packet_build(uint32_t seconds, uint16_t fine_time,
                            uint16_t sequence_count, uint8_t *buf,
                            size_t cap, size_t *len)
{
    uint8_t payload[CCSDS_TIME_PACKET_PAYLOAD_LEN];
    struct ccsds_space_packet packet = {
        .version = 0u,
        .type = CCSDS_PACKET_TYPE_TM,
        .secondary_header = false,
        .apid = CCSDS_TIME_PACKET_APID,
        .sequence_flags = CCSDS_SEQUENCE_UNSEGMENTED,
        .sequence_count = sequence_count,
        .payload = payload,
        .payload_len = sizeof(payload),
    };

    if (sequence_count > 0x3fffu) {
        return -EINVAL;
    }

    write_be32(&payload[0], seconds);
    write_be16(&payload[4], fine_time);

    return ccsds_space_packet_encode(&packet, buf, cap, len);
}

int ccsds_time_packet_build_now(uint16_t sequence_count, uint8_t *buf,
                                size_t cap, size_t *len)
{
    int64_t ticks = k_uptime_ticks();
    uint32_t seconds = (uint32_t)(ticks / CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    uint32_t subsecond_ticks =
        (uint32_t)(ticks % CONFIG_SYS_CLOCK_TICKS_PER_SEC);
    /*
     * Convert the current tick within the second to a two-byte CUC-style
     * binary fraction. This preserves the kernel clock's true tick resolution
     * without paying for or implying nanosecond precision.
     */
    uint16_t fine_time =
        (uint16_t)(((uint64_t)subsecond_ticks *
                    CCSDS_TIME_PACKET_FINE_UNITS) /
                   CONFIG_SYS_CLOCK_TICKS_PER_SEC);

    return ccsds_time_packet_build(seconds, fine_time, sequence_count, buf,
                                   cap, len);
}

int ccsds_time_packet_start(uint8_t vcid)
{
    if (vcid > 7u) {
        return -EINVAL;
    }

    time_packet_init_once();
    time_packet_vcid = vcid;
    time_packet_running = true;

    (void)k_work_schedule(&time_packet_work, CCSDS_TIME_PACKET_PERIOD);

    return 0;
}

int ccsds_time_packet_stop(void)
{
    time_packet_init_once();
    time_packet_running = false;
    (void)k_work_cancel_delayable(&time_packet_work);

    return 0;
}

#ifdef CONFIG_ZTEST
int ccsds_time_packet_test_enqueue_now(void)
{
    time_packet_init_once();
    time_packet_work_handler(&time_packet_work.work);

    return 0;
}
#endif
