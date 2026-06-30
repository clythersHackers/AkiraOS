#include <errno.h>

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ccsds/ccsds_shell.h"
#include "ccsds/ccsds_tm_frame.h"

bool ccsds_tm_frame_test_is_running(void);
bool ccsds_tm_frame_test_run_cycle(k_timeout_t *next_delay, uint8_t *vcid);
int ccsds_tm_frame_test_get_clcw(uint32_t *clcw);

#define TEST_CCSDS_SHELL_IDLE_VCID 7u
#define TEST_CCSDS_SHELL_IDLE_FHP 0x7feu

static ccsds_tm_route_mask_t get_vc_route(uint8_t vcid)
{
    ccsds_tm_route_mask_t route_mask;

    zassert_ok(ccsds_tm_frame_get_vc_route(vcid, &route_mask));

    return route_mask;
}

static void shell_setup(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(ccsds_shell_tm_stop());
}

ZTEST(ccsds_shell, test_init_registers_log_destination_without_default_routes)
{
    struct ccsds_shell_tm_status status;
    uint8_t vcid;

    zassert_ok(ccsds_shell_tm_init());

    ccsds_shell_tm_get_status(&status);
    zassert_true(status.initialized);
    zassert_false(status.running);
    zassert_equal(status.log_route_calls, 0u);
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_NONE);
    zassert_equal(get_vc_route(TEST_CCSDS_SHELL_IDLE_VCID),
                  CCSDS_TM_ROUTE_NONE);

    zassert_false(ccsds_tm_frame_test_run_cycle(NULL, &vcid));
    zassert_equal(vcid, TEST_CCSDS_SHELL_IDLE_VCID);

    ccsds_shell_tm_get_status(&status);
    zassert_equal(status.log_route_calls, 0u);

    zassert_ok(ccsds_shell_tm_route_add(TEST_CCSDS_SHELL_IDLE_VCID,
                                        CCSDS_TM_ROUTE_LOG));
    zassert_false(ccsds_tm_frame_test_run_cycle(NULL, &vcid));
    zassert_equal(vcid, TEST_CCSDS_SHELL_IDLE_VCID);

    ccsds_shell_tm_get_status(&status);
    zassert_equal(status.log_route_calls, 1u);
    zassert_equal(status.last_vcid, TEST_CCSDS_SHELL_IDLE_VCID);
    zassert_true(status.last_len > 0u);
    zassert_equal(status.last_mcfc, 1u);
    zassert_equal(status.last_vcfc, 1u);
    zassert_equal(status.last_fhp, TEST_CCSDS_SHELL_IDLE_FHP);
    zassert_true(status.last_cadu);
}

ZTEST(ccsds_shell, test_init_registers_tc_clcw_provider_without_udp)
{
    uint32_t clcw = 0xffffffffu;

    zassert_ok(ccsds_shell_tm_init());
    zassert_ok(ccsds_tm_frame_test_get_clcw(&clcw));
    zassert_equal(clcw, 0x01000000u);
}

ZTEST(ccsds_shell, test_start_and_stop_control_tm_generator)
{
    struct ccsds_shell_tm_status status;

    zassert_ok(ccsds_shell_tm_init());
    zassert_ok(ccsds_shell_tm_start());
    zassert_true(ccsds_tm_frame_test_is_running());

    ccsds_shell_tm_get_status(&status);
    zassert_true(status.initialized);
    zassert_true(status.running);
    zassert_equal(status.time_vcid, 0u);

    zassert_ok(ccsds_shell_tm_stop());
    zassert_false(ccsds_tm_frame_test_is_running());

    ccsds_shell_tm_get_status(&status);
    zassert_true(status.initialized);
    zassert_false(status.running);
}

ZTEST(ccsds_shell, test_route_name_parsing_supports_log_and_udp)
{
    ccsds_tm_route_mask_t route_mask;

    zassert_ok(ccsds_shell_tm_parse_route_mask("log", &route_mask));
    zassert_equal(route_mask, CCSDS_TM_ROUTE_LOG);

    zassert_ok(ccsds_shell_tm_parse_route_mask("udp", &route_mask));
    zassert_equal(route_mask, CCSDS_TM_ROUTE_UDP);

    zassert_ok(ccsds_shell_tm_parse_route_mask("log,udp", &route_mask));
    zassert_equal(route_mask, CCSDS_TM_ROUTE_LOG | CCSDS_TM_ROUTE_UDP);

    zassert_equal(ccsds_shell_tm_parse_route_mask("serial", &route_mask),
                  -EINVAL);
}

ZTEST(ccsds_shell, test_route_availability_reports_build_destinations)
{
    zassert_true(ccsds_shell_tm_route_available(CCSDS_TM_ROUTE_LOG));
#ifdef CONFIG_NETWORKING
    zassert_true(ccsds_shell_tm_route_available(CCSDS_TM_ROUTE_UDP));
#else
    zassert_false(ccsds_shell_tm_route_available(CCSDS_TM_ROUTE_UDP));
#endif
    zassert_false(ccsds_shell_tm_route_available(CCSDS_TM_ROUTE_NONE));
    zassert_false(ccsds_shell_tm_route_available(CCSDS_TM_ROUTE_LOG |
                                                 CCSDS_TM_ROUTE_UDP));
}

ZTEST(ccsds_shell, test_route_set_add_del_and_clear_log_mask)
{
    zassert_ok(ccsds_shell_tm_init());

    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_NONE);

    zassert_ok(ccsds_shell_tm_route_add(0u, CCSDS_TM_ROUTE_LOG));
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_LOG);

    zassert_ok(ccsds_shell_tm_route_del(0u, CCSDS_TM_ROUTE_LOG));
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_NONE);

    zassert_ok(ccsds_shell_tm_route_set(0u, CCSDS_TM_ROUTE_LOG));
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_LOG);
}

ZTEST(ccsds_shell, test_route_add_reports_combined_masks_when_available)
{
    zassert_ok(ccsds_shell_tm_init());
    zassert_ok(ccsds_shell_tm_route_set(0u, CCSDS_TM_ROUTE_LOG));

#ifdef CONFIG_NETWORKING
    zassert_ok(ccsds_shell_tm_route_add(0u, CCSDS_TM_ROUTE_UDP));
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_LOG | CCSDS_TM_ROUTE_UDP);
#else
    zassert_equal(ccsds_shell_tm_route_add(0u, CCSDS_TM_ROUTE_UDP),
                  -ENOTSUP);
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_LOG);
#endif
}

ZTEST(ccsds_shell, test_route_helper_reports_current_masks)
{
    ccsds_tm_route_mask_t route_mask;

    zassert_ok(ccsds_shell_tm_init());
    zassert_ok(ccsds_shell_tm_route_clear(1u));
    zassert_ok(ccsds_tm_frame_get_vc_route(1u, &route_mask));
    zassert_equal(route_mask, CCSDS_TM_ROUTE_NONE);

    zassert_ok(ccsds_shell_tm_route_set(1u, CCSDS_TM_ROUTE_LOG));
    zassert_ok(ccsds_tm_frame_get_vc_route(1u, &route_mask));
    zassert_equal(route_mask, CCSDS_TM_ROUTE_LOG);

    zassert_equal(ccsds_tm_frame_get_vc_route(8u, &route_mask), -EINVAL);
}

ZTEST(ccsds_shell, test_udp_route_is_unavailable_without_networking)
{
    zassert_ok(ccsds_shell_tm_init());

#ifdef CONFIG_NETWORKING
    zassert_ok(ccsds_shell_tm_route_set(0u, CCSDS_TM_ROUTE_UDP));
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_UDP);
#else
    zassert_equal(ccsds_shell_tm_route_set(0u, CCSDS_TM_ROUTE_UDP),
                  -ENOTSUP);
    zassert_equal(get_vc_route(0u), CCSDS_TM_ROUTE_NONE);
#endif
}

ZTEST_SUITE(ccsds_shell, NULL, NULL, shell_setup, shell_setup, NULL);
