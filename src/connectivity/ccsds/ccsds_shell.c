#include "ccsds_shell.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include "ccsds_time_packet.h"
#include "ccsds_tm_frame.h"

LOG_MODULE_REGISTER(ccsds_shell, CONFIG_AKIRA_LOG_LEVEL);

#define CCSDS_SHELL_TM_ACTIVE_DELAY K_MSEC(100)
#define CCSDS_SHELL_TM_IDLE_DELAY K_SECONDS(1)
#define CCSDS_SHELL_TIME_VCID 0u
#define CCSDS_SHELL_IDLE_VCID 7u
#define CCSDS_SHELL_ASM_LEN 4u
#define CCSDS_SHELL_TM_PRIMARY_HDR_LEN 6u
#define CCSDS_SHELL_ASM0 0x1au
#define CCSDS_SHELL_ASM1 0xcfu
#define CCSDS_SHELL_ASM2 0xfcu
#define CCSDS_SHELL_ASM3 0x1du

static struct k_mutex status_lock;
static bool status_lock_initialized;
static struct ccsds_shell_tm_status tm_status;

static void status_lock_init_once(void)
{
    if (status_lock_initialized) {
        return;
    }

    k_mutex_init(&status_lock);
    status_lock_initialized = true;
}

static uint16_t read_be16(const uint8_t *buf)
{
    return ((uint16_t)buf[0] << 8) | buf[1];
}

static bool output_has_asm(const uint8_t *frame, size_t frame_len)
{
    return frame_len >= CCSDS_SHELL_ASM_LEN &&
           frame[0] == CCSDS_SHELL_ASM0 && frame[1] == CCSDS_SHELL_ASM1 &&
           frame[2] == CCSDS_SHELL_ASM2 && frame[3] == CCSDS_SHELL_ASM3;
}

static void parse_output_metadata(const uint8_t *frame, size_t frame_len,
                                  bool *cadu, uint8_t *mcfc, uint8_t *vcfc,
                                  uint16_t *fhp)
{
    size_t frame_offset;

    *cadu = output_has_asm(frame, frame_len);
    frame_offset = *cadu ? CCSDS_SHELL_ASM_LEN : 0u;

    *mcfc = 0u;
    *vcfc = 0u;
    *fhp = 0u;

    if (frame_len < frame_offset + CCSDS_SHELL_TM_PRIMARY_HDR_LEN) {
        return;
    }

    *mcfc = frame[frame_offset + 2u];
    *vcfc = frame[frame_offset + 3u];
    *fhp = read_be16(&frame[frame_offset + 4u]) & 0x07ffu;
}

static int log_route(uint8_t vcid, const uint8_t *frame, size_t frame_len,
                     void *user_data)
{
    bool cadu;
    uint8_t mcfc;
    uint8_t vcfc;
    uint16_t fhp;

    ARG_UNUSED(user_data);

    if (frame == NULL) {
        return -EINVAL;
    }

    parse_output_metadata(frame, frame_len, &cadu, &mcfc, &vcfc, &fhp);

    status_lock_init_once();
    k_mutex_lock(&status_lock, K_FOREVER);
    tm_status.log_route_calls++;
    tm_status.last_vcid = vcid;
    tm_status.last_len = frame_len;
    tm_status.last_mcfc = mcfc;
    tm_status.last_vcfc = vcfc;
    tm_status.last_fhp = fhp;
    tm_status.last_cadu = cadu;
    k_mutex_unlock(&status_lock);

    LOG_INF("tm vcid=%u len=%zu mcfc=%u vcfc=%u fhp=0x%03x cadu=%u",
            vcid, frame_len, mcfc, vcfc, fhp, cadu ? 1u : 0u);

    return 0;
}

int ccsds_shell_tm_init(void)
{
    int ret;

    status_lock_init_once();

    (void)ccsds_time_packet_stop();

    ret = ccsds_tm_frame_init();
    if (ret != 0) {
        return ret;
    }

    ret = ccsds_tm_frame_register_route(CCSDS_TM_ROUTE_LOG, log_route, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = ccsds_tm_frame_set_vc_route(CCSDS_SHELL_TIME_VCID,
                                      CCSDS_TM_ROUTE_LOG);
    if (ret != 0) {
        return ret;
    }

    ret = ccsds_tm_frame_set_vc_route(CCSDS_SHELL_IDLE_VCID,
                                      CCSDS_TM_ROUTE_LOG);
    if (ret != 0) {
        return ret;
    }

    k_mutex_lock(&status_lock, K_FOREVER);
    memset(&tm_status, 0, sizeof(tm_status));
    tm_status.initialized = true;
    tm_status.time_vcid = CCSDS_SHELL_TIME_VCID;
    k_mutex_unlock(&status_lock);

    return 0;
}

int ccsds_shell_tm_start(void)
{
    int ret;

    status_lock_init_once();

    k_mutex_lock(&status_lock, K_FOREVER);
    if (!tm_status.initialized) {
        k_mutex_unlock(&status_lock);
        return -EACCES;
    }
    k_mutex_unlock(&status_lock);

    ret = ccsds_tm_frame_start(CCSDS_SHELL_TM_ACTIVE_DELAY,
                               CCSDS_SHELL_TM_IDLE_DELAY);
    if (ret != 0) {
        return ret;
    }

    ret = ccsds_time_packet_start(CCSDS_SHELL_TIME_VCID);
    if (ret != 0) {
        (void)ccsds_tm_frame_stop();
        return ret;
    }

    k_mutex_lock(&status_lock, K_FOREVER);
    tm_status.running = true;
    tm_status.time_vcid = CCSDS_SHELL_TIME_VCID;
    k_mutex_unlock(&status_lock);

    return 0;
}

int ccsds_shell_tm_stop(void)
{
    int ret;
    int tm_ret;

    ret = ccsds_time_packet_stop();
    tm_ret = ccsds_tm_frame_stop();
    if (ret == 0) {
        ret = tm_ret;
    }

    status_lock_init_once();
    k_mutex_lock(&status_lock, K_FOREVER);
    tm_status.running = false;
    k_mutex_unlock(&status_lock);

    return ret;
}

void ccsds_shell_tm_get_status(struct ccsds_shell_tm_status *status)
{
    if (status == NULL) {
        return;
    }

    status_lock_init_once();
    k_mutex_lock(&status_lock, K_FOREVER);
    *status = tm_status;
    k_mutex_unlock(&status_lock);
}

static int cmd_ccsds_tm_init(const struct shell *sh, size_t argc, char **argv)
{
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ret = ccsds_shell_tm_init();
    if (ret != 0) {
        shell_error(sh, "ccsds tm init failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds tm initialized");
    return 0;
}

static int cmd_ccsds_tm_start(const struct shell *sh, size_t argc, char **argv)
{
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ret = ccsds_shell_tm_start();
    if (ret != 0) {
        shell_error(sh, "ccsds tm start failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds tm started");
    return 0;
}

static int cmd_ccsds_tm_stop(const struct shell *sh, size_t argc, char **argv)
{
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ret = ccsds_shell_tm_stop();
    if (ret != 0) {
        shell_error(sh, "ccsds tm stop failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds tm stopped");
    return 0;
}

static int cmd_ccsds_tm_status(const struct shell *sh, size_t argc, char **argv)
{
    struct ccsds_shell_tm_status status;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ccsds_shell_tm_get_status(&status);

    shell_print(sh,
                "ccsds tm initialized=%u running=%u time_vc=%u log_calls=%u",
                status.initialized ? 1u : 0u, status.running ? 1u : 0u,
                status.time_vcid, status.log_route_calls);
    shell_print(sh, "last vcid=%u len=%zu mcfc=%u vcfc=%u fhp=0x%03x cadu=%u",
                status.last_vcid, status.last_len, status.last_mcfc,
                status.last_vcfc, status.last_fhp,
                status.last_cadu ? 1u : 0u);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tm, SHELL_CMD(init, NULL, "Initialize CCSDS TM", cmd_ccsds_tm_init),
    SHELL_CMD(start, NULL, "Start CCSDS TM", cmd_ccsds_tm_start),
    SHELL_CMD(stop, NULL, "Stop CCSDS TM", cmd_ccsds_tm_stop),
    SHELL_CMD(status, NULL, "Show CCSDS TM status", cmd_ccsds_tm_status),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds, SHELL_CMD(tm, &sub_ccsds_tm, "CCSDS TM commands", NULL),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ccsds, &sub_ccsds, "CCSDS commands", NULL);
