#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ccsds/ccsds_space_packet.h"
#include "ccsds/ccsds_time_packet.h"

static uint32_t read_be32(const uint8_t *buf)
{
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) | buf[3];
}

static uint16_t read_be16(const uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | buf[1];
}

ZTEST(ccsds_time_packet, test_builder_encodes_apid0_space_packet)
{
    uint8_t buf[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                CCSDS_TIME_PACKET_PAYLOAD_LEN];
    struct ccsds_space_packet packet;
    size_t len;

    zassert_ok(ccsds_time_packet_build(42u, 0x1234u, 3u, buf,
                                       sizeof(buf), &len));
    zassert_equal(len, sizeof(buf));
    zassert_ok(ccsds_space_packet_decode(buf, len, &packet));

    zassert_equal(packet.version, 0u);
    zassert_equal(packet.type, CCSDS_PACKET_TYPE_TM);
    zassert_false(packet.secondary_header);
    zassert_equal(packet.apid, CCSDS_TIME_PACKET_APID);
    zassert_equal(packet.sequence_flags, CCSDS_SEQUENCE_UNSEGMENTED);
    zassert_equal(packet.sequence_count, 3u);
    zassert_equal(packet.payload_len, CCSDS_TIME_PACKET_PAYLOAD_LEN);
}

ZTEST(ccsds_time_packet, test_payload_is_big_endian_seconds_and_cuc_fine_time)
{
    uint8_t buf[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                CCSDS_TIME_PACKET_PAYLOAD_LEN];
    struct ccsds_space_packet packet;
    size_t len;

    zassert_ok(ccsds_time_packet_build(0x01020304u, 0x0506u, 0u, buf,
                                       sizeof(buf), &len));
    zassert_ok(ccsds_space_packet_decode(buf, len, &packet));

    zassert_equal(read_be32(&packet.payload[0]), 0x01020304u);
    zassert_equal(read_be16(&packet.payload[4]), 0x0506u);
}

ZTEST(ccsds_time_packet, test_build_now_time_is_sane_and_monotonic)
{
    uint8_t first[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                  CCSDS_TIME_PACKET_PAYLOAD_LEN];
    uint8_t second[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                   CCSDS_TIME_PACKET_PAYLOAD_LEN];
    struct ccsds_space_packet first_packet;
    struct ccsds_space_packet second_packet;
    uint64_t first_fine_ticks;
    uint64_t second_fine_ticks;
    size_t len;

    zassert_ok(ccsds_time_packet_build_now(0u, first, sizeof(first), &len));
    zassert_ok(ccsds_space_packet_decode(first, len, &first_packet));
    k_sleep(K_MSEC(20));
    zassert_ok(ccsds_time_packet_build_now(1u, second, sizeof(second), &len));
    zassert_ok(ccsds_space_packet_decode(second, len, &second_packet));

    first_fine_ticks =
        (uint64_t)read_be32(&first_packet.payload[0]) *
            CCSDS_TIME_PACKET_FINE_UNITS +
        read_be16(&first_packet.payload[4]);
    second_fine_ticks =
        (uint64_t)read_be32(&second_packet.payload[0]) *
            CCSDS_TIME_PACKET_FINE_UNITS +
        read_be16(&second_packet.payload[4]);

    zassert_true(second_fine_ticks >= first_fine_ticks);
}

ZTEST_SUITE(ccsds_time_packet, NULL, NULL, NULL, NULL, NULL);
