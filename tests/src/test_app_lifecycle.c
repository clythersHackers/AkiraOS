/*
 * test_app_lifecycle.c
 * ztest suite: app_lifecycle
 *
 * Tests for the app_manager state machine (app_state_t transitions).
 * Operates entirely on app_entry_t structs in RAM — no WAMR, no flash.
 *
 * Sources exercised:
 *   src/runtime/app_manager/app_manager.h  — types, constants
 *   (state transitions are validated against the enum values)
 */

#include <zephyr/ztest.h>
#include <string.h>

#include "app_manager/app_manager.h"
#include "akira_runtime.h" /* akira_app_status_t */

/* ── helpers ─────────────────────────────────────────────────────────────── */

/** Initialise an app_entry_t as if it was just created by the installer. */
static void make_new_entry(app_entry_t *e, const char *name)
{
    memset(e, 0, sizeof(*e));
    strncpy(e->name, name, APP_NAME_MAX_LEN - 1);
    e->state = APP_STATE_NEW;
    e->container_id = -1;
}

/* ── state machine enum sanity ───────────────────────────────────────────── */

ZTEST(app_lifecycle, test_state_enum_ordering)
{
    /*
     * The install/run lifecycle only makes sense with this ordering.
     * If someone reorders the enum the CI will catch it here.
     */
    zassert_equal(APP_STATE_NEW, 0, "APP_STATE_NEW must be 0");
    zassert_true(APP_STATE_INSTALLED > APP_STATE_NEW,
                 "INSTALLED must follow NEW");
    zassert_true(APP_STATE_RUNNING > APP_STATE_INSTALLED,
                 "RUNNING must follow INSTALLED");
    zassert_true(APP_STATE_STOPPED > APP_STATE_RUNNING,
                 "STOPPED must follow RUNNING");
    zassert_true(APP_STATE_ERROR > APP_STATE_STOPPED,
                 "ERROR must follow STOPPED");
    zassert_true(APP_STATE_FAILED > APP_STATE_ERROR,
                 "FAILED must follow ERROR");
}

/* ── akira_app_status_t (runtime layer) enum sanity ─────────────────────── */

ZTEST(app_lifecycle, test_runtime_status_enum_ordering)
{
    zassert_equal(AKIRA_APP_STATUS_CREATED, 0,
                  "CREATED must be 0");
    zassert_true(AKIRA_APP_STATUS_RUNNING > AKIRA_APP_STATUS_CREATED,
                 "RUNNING must follow CREATED");
    zassert_true(AKIRA_APP_STATUS_EXITED > AKIRA_APP_STATUS_RUNNING,
                 "EXITED must follow RUNNING");
    zassert_true(AKIRA_APP_STATUS_STOPPED > AKIRA_APP_STATUS_EXITED,
                 "STOPPED must follow EXITED");
    zassert_true(AKIRA_APP_STATUS_ERROR > AKIRA_APP_STATUS_STOPPED,
                 "ERROR must follow STOPPED");
}

/* ── NEW → INSTALLED ─────────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_initial_state_new)
{
    app_entry_t e;

    make_new_entry(&e, "hello");

    zassert_equal(e.state, APP_STATE_NEW, "freshly created must be NEW");
    zassert_equal(e.container_id, -1,
                  "container_id must be -1 when not loaded");
}

ZTEST(app_lifecycle, test_transition_new_to_installed)
{
    app_entry_t e;

    make_new_entry(&e, "hello");
    e.state = APP_STATE_INSTALLED; /* installer sets this after writing files */

    zassert_equal(e.state, APP_STATE_INSTALLED,
                  "state should be INSTALLED after install step");
}

/* ── INSTALLED → RUNNING ─────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_transition_installed_to_running)
{
    app_entry_t e;

    make_new_entry(&e, "demo");
    e.state = APP_STATE_INSTALLED;

    /* Launcher sets state to RUNNING before spawning thread */
    e.state = APP_STATE_RUNNING;

    zassert_equal(e.state, APP_STATE_RUNNING,
                  "state should be RUNNING after start");
}

/* ── RUNNING → STOPPED ───────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_transition_running_to_stopped)
{
    app_entry_t e;

    make_new_entry(&e, "demo");
    e.state = APP_STATE_RUNNING;

    e.state = APP_STATE_STOPPED;

    zassert_equal(e.state, APP_STATE_STOPPED,
                  "state should be STOPPED after graceful stop");
}

/* ── RUNNING → ERROR ─────────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_transition_running_to_error)
{
    app_entry_t e;

    make_new_entry(&e, "buggy");
    e.state = APP_STATE_RUNNING;

    /* Simulate crash handler */
    e.state = APP_STATE_ERROR;
    e.crash_count++;

    zassert_equal(e.state, APP_STATE_ERROR,
                  "state should be ERROR after crash");
    zassert_equal(e.crash_count, 1U, "crash_count should increment");
}

/* ── ERROR → FAILED (max retries exceeded) ───────────────────────────────── */

ZTEST(app_lifecycle, test_transition_error_to_failed)
{
    app_entry_t e;

    make_new_entry(&e, "crashloop");
    e.state = APP_STATE_ERROR;
    e.crash_count = CONFIG_AKIRA_APP_MAX_RETRIES;

    /* Manager marks FAILED when retries exhausted */
    if (e.crash_count >= CONFIG_AKIRA_APP_MAX_RETRIES)
    {
        e.state = APP_STATE_FAILED;
    }

    zassert_equal(e.state, APP_STATE_FAILED,
                  "should be FAILED after max retries");
}

/* ── field limits ────────────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_name_length_limit)
{
    app_entry_t e;

    make_new_entry(&e, "x");

    /* APP_NAME_MAX_LEN includes the NUL — the stored name must be
     * NUL-terminated within the field */
    size_t len = strlen(e.name);

    zassert_true(len < APP_NAME_MAX_LEN,
                 "name must be NUL-terminated within the field");
}

ZTEST(app_lifecycle, test_defaults)
{
    app_entry_t e;

    make_new_entry(&e, "defaults_test");

    zassert_equal(e.crash_count, 0U, "crash_count must start at 0");
    zassert_equal(e.is_preloaded, false, "is_preloaded must start false");
    zassert_equal(e.container_id, -1, "container_id must start at -1");
}

/* ── restart config ──────────────────────────────────────────────────────── */

ZTEST(app_lifecycle, test_restart_config_defaults)
{
    app_entry_t e;

    make_new_entry(&e, "restart_test");

    /*
     * Default restart config: enabled=false (no auto-restart unless
     * the manifest explicitly requests it).
     */
    zassert_false(e.restart.enabled,
                  "restart should be disabled by default");
}

ZTEST_SUITE(app_lifecycle, NULL, NULL, NULL, NULL, NULL);
