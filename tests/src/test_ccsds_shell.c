#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#include "ccsds/ccsds_shell.h"
#include "ccsds/ccsds_tm_frame.h"

bool ccsds_tm_frame_test_is_running(void);
bool ccsds_tm_frame_test_run_cycle(k_timeout_t *next_delay, uint8_t *vcid);

#define TEST_CCSDS_SHELL_IDLE_VCID 7u
#define TEST_CCSDS_SHELL_IDLE_FHP 0x7feu

static void shell_setup(void *fixture)
{
    ARG_UNUSED(fixture);

    zassert_ok(ccsds_shell_tm_stop());
}

ZTEST(ccsds_shell, test_init_registers_log_route_for_idle_vc)
{
    struct ccsds_shell_tm_status status;
    uint8_t vcid;

    zassert_ok(ccsds_shell_tm_init());

    ccsds_shell_tm_get_status(&status);
    zassert_true(status.initialized);
    zassert_false(status.running);
    zassert_equal(status.log_route_calls, 0u);

    zassert_false(ccsds_tm_frame_test_run_cycle(NULL, &vcid));
    zassert_equal(vcid, TEST_CCSDS_SHELL_IDLE_VCID);

    ccsds_shell_tm_get_status(&status);
    zassert_equal(status.log_route_calls, 1u);
    zassert_equal(status.last_vcid, TEST_CCSDS_SHELL_IDLE_VCID);
    zassert_true(status.last_len > 0u);
    zassert_equal(status.last_mcfc, 0u);
    zassert_equal(status.last_vcfc, 0u);
    zassert_equal(status.last_fhp, TEST_CCSDS_SHELL_IDLE_FHP);
#ifdef CONFIG_AKIRA_CCSDS_RS
    zassert_true(status.last_cadu);
#else
    zassert_false(status.last_cadu);
#endif
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

ZTEST_SUITE(ccsds_shell, NULL, NULL, shell_setup, shell_setup, NULL);
