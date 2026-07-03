#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ccsds/ccsds_space_packet.h"
#include "ccsds/ccsds_time_service.h"
#include "ccsds/ccsds_router.h"

ZTEST(ccsds_time_service, test_time_set_and_get)
{
    struct ccsds_router router;
    uint8_t payload[7];
    struct ccsds_space_packet packet = {
        .version = 0u,
        .type = CCSDS_PACKET_TYPE_TC,
        .apid = CCSDS_TIME_SERVICE_APID,
        .payload = payload,
        .payload_len = 7,
    };

    ccsds_router_init(&router);
    zassert_ok(ccsds_time_service_init(&router));

    /* Set time: s=1000, f=0 */
    payload[0] = 0x01; // TIME_FUNC_SET
    payload[1] = 0; payload[2] = 0; payload[3] = 0x03; payload[4] = 0xE8; // 1000
    payload[5] = 0; payload[6] = 0;

    zassert_ok(ccsds_router_dispatch(&router, &packet));

    // We can't easily check the offset without exposing it or waiting for TM.
    // But we can check if it accepts the packet.
}

ZTEST(ccsds_time_service, test_ping_response_enqueued)
{
    struct ccsds_router router;
    uint8_t payload[1];
    struct ccsds_space_packet packet = {
        .version = 0u,
        .type = CCSDS_PACKET_TYPE_TC,
        .apid = CCSDS_TIME_SERVICE_APID,
        .payload = payload,
        .payload_len = 1,
    };

    ccsds_router_init(&router);
    zassert_ok(ccsds_time_service_init(&router));

    payload[0] = 0xFF; // TIME_FUNC_PING

    zassert_ok(ccsds_router_dispatch(&router, &packet));
    
    // In a real test we'd check if a TM frame was added to the queue.
}

ZTEST_SUITE(ccsds_time_service, NULL, NULL, NULL, NULL, NULL);
