#include "ccsds_shell.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include "ccsds_time_packet.h"
#include "ccsds_tm_frame.h"
#include "ccsds_tm_udp_route.h"

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
#define CCSDS_SHELL_ROUTE_NAME_MAX_LEN 8u
#define CCSDS_SHELL_TM_VC_COUNT 8u

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

static bool shell_tm_is_initialized(void)
{
    bool initialized;

    status_lock_init_once();
    k_mutex_lock(&status_lock, K_FOREVER);
    initialized = tm_status.initialized;
    k_mutex_unlock(&status_lock);

    return initialized;
}

static bool shell_tm_route_mask_available(ccsds_tm_route_mask_t route_mask)
{
    if ((route_mask & ~(CCSDS_TM_ROUTE_LOG | CCSDS_TM_ROUTE_UDP)) != 0u) {
        return false;
    }

    if ((route_mask & CCSDS_TM_ROUTE_UDP) != 0u &&
        !ccsds_tm_udp_route_available()) {
        return false;
    }

    return true;
}

bool ccsds_shell_tm_route_available(ccsds_tm_route_mask_t route_bit)
{
    if (route_bit == CCSDS_TM_ROUTE_NONE ||
        (route_bit & (route_bit - 1u)) != 0u) {
        return false;
    }

    return shell_tm_route_mask_available(route_bit);
}

static int route_name_to_mask(const char *name, size_t name_len,
                              ccsds_tm_route_mask_t *route_mask)
{
    if (name_len == strlen("log") && strncmp(name, "log", name_len) == 0) {
        *route_mask = CCSDS_TM_ROUTE_LOG;
        return 0;
    }

    if (name_len == strlen("udp") && strncmp(name, "udp", name_len) == 0) {
        *route_mask = CCSDS_TM_ROUTE_UDP;
        return 0;
    }

    return -EINVAL;
}

int ccsds_shell_tm_parse_route_mask(const char *routes,
                                    ccsds_tm_route_mask_t *route_mask)
{
    ccsds_tm_route_mask_t parsed_mask = CCSDS_TM_ROUTE_NONE;
    const char *token_start = NULL;
    size_t token_len = 0u;

    if (routes == NULL || route_mask == NULL) {
        return -EINVAL;
    }

    for (const char *p = routes;; p++) {
        bool separator = *p == '\0' || *p == ',' || *p == '|' || *p == '+';

        if (!separator && isspace((unsigned char)*p) == 0) {
            if (token_start == NULL) {
                token_start = p;
            }
            token_len++;
            continue;
        }

        if (token_start != NULL) {
            ccsds_tm_route_mask_t bit;
            int ret = route_name_to_mask(token_start, token_len, &bit);

            if (ret != 0) {
                return ret;
            }

            parsed_mask |= bit;
            token_start = NULL;
            token_len = 0u;
        }

        if (*p == '\0') {
            break;
        }
    }

    if (parsed_mask == CCSDS_TM_ROUTE_NONE) {
        return -EINVAL;
    }

    *route_mask = parsed_mask;

    return 0;
}

int ccsds_shell_tm_route_set(uint8_t vcid, ccsds_tm_route_mask_t route_mask)
{
    if (!shell_tm_is_initialized()) {
        return -EACCES;
    }

    if (!shell_tm_route_mask_available(route_mask)) {
        return -ENOTSUP;
    }

    return ccsds_tm_frame_set_vc_route(vcid, route_mask);
}

int ccsds_shell_tm_route_add(uint8_t vcid, ccsds_tm_route_mask_t route_mask)
{
    ccsds_tm_route_mask_t current;
    int ret;

    if (!shell_tm_is_initialized()) {
        return -EACCES;
    }

    if (!shell_tm_route_mask_available(route_mask)) {
        return -ENOTSUP;
    }

    ret = ccsds_tm_frame_get_vc_route(vcid, &current);
    if (ret != 0) {
        return ret;
    }

    return ccsds_tm_frame_set_vc_route(vcid, current | route_mask);
}

int ccsds_shell_tm_route_del(uint8_t vcid, ccsds_tm_route_mask_t route_mask)
{
    ccsds_tm_route_mask_t current;
    int ret;

    if (!shell_tm_is_initialized()) {
        return -EACCES;
    }

    if (!shell_tm_route_mask_available(route_mask)) {
        return -ENOTSUP;
    }

    ret = ccsds_tm_frame_get_vc_route(vcid, &current);
    if (ret != 0) {
        return ret;
    }

    return ccsds_tm_frame_set_vc_route(vcid, current & ~route_mask);
}

int ccsds_shell_tm_route_clear(uint8_t vcid)
{
    if (!shell_tm_is_initialized()) {
        return -EACCES;
    }

    return ccsds_tm_frame_set_vc_route(vcid, CCSDS_TM_ROUTE_NONE);
}

static void route_mask_to_names(ccsds_tm_route_mask_t route_mask, char *buf,
                                size_t buf_len)
{
    bool first = true;

    if (buf_len == 0u) {
        return;
    }

    buf[0] = '\0';

    if (route_mask == CCSDS_TM_ROUTE_NONE) {
        (void)snprintk(buf, buf_len, "none");
        return;
    }

    if ((route_mask & CCSDS_TM_ROUTE_LOG) != 0u) {
        (void)snprintk(buf, buf_len, "log");
        first = false;
    }

    if ((route_mask & CCSDS_TM_ROUTE_UDP) != 0u) {
        (void)snprintk(buf + strlen(buf), buf_len - strlen(buf),
                       "%sudp", first ? "" : ",");
        first = false;
    }

    route_mask &= ~(CCSDS_TM_ROUTE_LOG | CCSDS_TM_ROUTE_UDP);
    if (route_mask != CCSDS_TM_ROUTE_NONE) {
        (void)snprintk(buf + strlen(buf), buf_len - strlen(buf),
                       "%s0x%08x", first ? "" : ",", route_mask);
    }
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

    ret = ccsds_tm_udp_route_register();
    if (ret != 0 && ret != -ENOTSUP) {
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
    ccsds_tm_route_mask_t route_mask;
    char route_names[CCSDS_SHELL_ROUTE_NAME_MAX_LEN * 2u];

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

    if (status.initialized) {
        for (uint8_t vcid = 0u; vcid < CCSDS_SHELL_TM_VC_COUNT; vcid++) {
            if (ccsds_tm_frame_get_vc_route(vcid, &route_mask) != 0) {
                continue;
            }

            route_mask_to_names(route_mask, route_names, sizeof(route_names));
            shell_print(sh, "route vc=%u mask=0x%08x routes=%s", vcid,
                        route_mask, route_names);
        }
    }

    return 0;
}

static int parse_vcid(const char *arg, uint8_t *vcid)
{
    char *endptr;
    unsigned long value;

    if (arg == NULL || vcid == NULL || arg[0] == '\0') {
        return -EINVAL;
    }

    errno = 0;
    value = strtoul(arg, &endptr, 0);
    if (errno != 0 || *endptr != '\0' || value >= CCSDS_SHELL_TM_VC_COUNT) {
        return -EINVAL;
    }

    *vcid = (uint8_t)value;

    return 0;
}

static int parse_route_args(size_t argc, char **argv, size_t first_route_arg,
                            ccsds_tm_route_mask_t *route_mask)
{
    ccsds_tm_route_mask_t parsed_mask = CCSDS_TM_ROUTE_NONE;

    if (argc <= first_route_arg) {
        return -EINVAL;
    }

    for (size_t i = first_route_arg; i < argc; i++) {
        ccsds_tm_route_mask_t token_mask;
        int ret = ccsds_shell_tm_parse_route_mask(argv[i], &token_mask);

        if (ret != 0) {
            return ret;
        }

        parsed_mask |= token_mask;
    }

    *route_mask = parsed_mask;

    return 0;
}

static int cmd_ccsds_tm_route_list(const struct shell *sh, size_t argc,
                                   char **argv)
{
    ccsds_tm_route_mask_t route_mask;
    char route_names[CCSDS_SHELL_ROUTE_NAME_MAX_LEN * 2u];

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (!shell_tm_is_initialized()) {
        shell_error(sh, "ccsds tm route list requires ccsds tm init");
        return -EACCES;
    }

    for (uint8_t vcid = 0u; vcid < CCSDS_SHELL_TM_VC_COUNT; vcid++) {
        int ret = ccsds_tm_frame_get_vc_route(vcid, &route_mask);

        if (ret != 0) {
            shell_error(sh, "ccsds tm route list failed for vc %u: %d",
                        vcid, ret);
            return ret;
        }

        route_mask_to_names(route_mask, route_names, sizeof(route_names));
        shell_print(sh, "vc %u: 0x%08x %s", vcid, route_mask, route_names);
    }

    return 0;
}

static int cmd_ccsds_tm_route_info(const struct shell *sh, size_t argc,
                                   char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "dest log: available=1 route=log");
    if (ccsds_tm_udp_route_available()) {
#ifdef CONFIG_NETWORKING
        shell_print(sh, "dest udp: available=1 route=udp peer=%s:%u",
                    CONFIG_AKIRA_CCSDS_TM_UDP_DEST_IP,
                    CONFIG_AKIRA_CCSDS_TM_UDP_DEST_PORT);
#endif
    } else {
        shell_print(sh,
                    "dest udp: available=0 route=udp reason=networking-disabled");
    }

    return 0;
}

static int route_command_common(const struct shell *sh, size_t argc, char **argv,
                                int (*op)(uint8_t, ccsds_tm_route_mask_t),
                                const char *op_name)
{
    ccsds_tm_route_mask_t current_mask;
    ccsds_tm_route_mask_t route_mask;
    char route_names[CCSDS_SHELL_ROUTE_NAME_MAX_LEN * 2u];
    uint8_t vcid;
    int ret;

    ret = parse_vcid(argv[1], &vcid);
    if (ret != 0) {
        shell_error(sh, "invalid VCID: %s", argv[1]);
        return ret;
    }

    ret = parse_route_args(argc, argv, 2u, &route_mask);
    if (ret != 0) {
        shell_error(sh, "invalid route name; supported routes: log udp");
        return ret;
    }

    ret = op(vcid, route_mask);
    if (ret != 0) {
        if (ret == -ENOTSUP) {
            shell_error(sh, "route unavailable in this build");
        } else {
            shell_error(sh, "ccsds tm route %s failed: %d", op_name, ret);
        }
        return ret;
    }

    ret = ccsds_tm_frame_get_vc_route(vcid, &current_mask);
    if (ret != 0) {
        shell_error(sh, "ccsds tm route %s readback failed: %d", op_name, ret);
        return ret;
    }

    route_mask_to_names(current_mask, route_names, sizeof(route_names));
    shell_print(sh, "ccsds tm route %s vc %u mask 0x%08x routes=%s",
                op_name, vcid, current_mask, route_names);

    return 0;
}

static int cmd_ccsds_tm_route_set(const struct shell *sh, size_t argc,
                                  char **argv)
{
    return route_command_common(sh, argc, argv, ccsds_shell_tm_route_set,
                                "set");
}

static int cmd_ccsds_tm_route_add(const struct shell *sh, size_t argc,
                                  char **argv)
{
    return route_command_common(sh, argc, argv, ccsds_shell_tm_route_add,
                                "add");
}

static int cmd_ccsds_tm_route_del(const struct shell *sh, size_t argc,
                                  char **argv)
{
    return route_command_common(sh, argc, argv, ccsds_shell_tm_route_del,
                                "del");
}

static int cmd_ccsds_tm_route_clear(const struct shell *sh, size_t argc,
                                    char **argv)
{
    ccsds_tm_route_mask_t current_mask;
    char route_names[CCSDS_SHELL_ROUTE_NAME_MAX_LEN * 2u];
    uint8_t vcid;
    int ret;

    ARG_UNUSED(argc);

    ret = parse_vcid(argv[1], &vcid);
    if (ret != 0) {
        shell_error(sh, "invalid VCID: %s", argv[1]);
        return ret;
    }

    ret = ccsds_shell_tm_route_clear(vcid);
    if (ret != 0) {
        shell_error(sh, "ccsds tm route clear failed: %d", ret);
        return ret;
    }

    ret = ccsds_tm_frame_get_vc_route(vcid, &current_mask);
    if (ret != 0) {
        shell_error(sh, "ccsds tm route clear readback failed: %d", ret);
        return ret;
    }

    route_mask_to_names(current_mask, route_names, sizeof(route_names));
    shell_print(sh, "ccsds tm route clear vc %u mask 0x%08x routes=%s",
                vcid, current_mask, route_names);

    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tm_route,
    SHELL_CMD(info, NULL, "Show available CCSDS TM destinations",
              cmd_ccsds_tm_route_info),
    SHELL_CMD(list, NULL, "List CCSDS TM route masks", cmd_ccsds_tm_route_list),
    SHELL_CMD_ARG(set, NULL, "Set VC routes", cmd_ccsds_tm_route_set, 3, 8),
    SHELL_CMD_ARG(add, NULL, "Add VC routes", cmd_ccsds_tm_route_add, 3, 8),
    SHELL_CMD_ARG(del, NULL, "Delete VC routes", cmd_ccsds_tm_route_del, 3, 8),
    SHELL_CMD_ARG(clear, NULL, "Clear VC routes", cmd_ccsds_tm_route_clear, 2,
                  0),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tm, SHELL_CMD(init, NULL, "Initialize CCSDS TM", cmd_ccsds_tm_init),
    SHELL_CMD(route, &sub_ccsds_tm_route, "Configure CCSDS TM routes", NULL),
    SHELL_CMD(start, NULL, "Start CCSDS TM", cmd_ccsds_tm_start),
    SHELL_CMD(stop, NULL, "Stop CCSDS TM", cmd_ccsds_tm_stop),
    SHELL_CMD(status, NULL, "Show CCSDS TM status", cmd_ccsds_tm_status),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds, SHELL_CMD(tm, &sub_ccsds_tm, "CCSDS TM commands", NULL),
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ccsds, &sub_ccsds, "CCSDS commands", NULL);
