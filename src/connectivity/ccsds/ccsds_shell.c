#include "ccsds_shell.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>
#include <zephyr/sys/byteorder.h>

#include "ccsds_profile.h"
#ifdef CONFIG_NETWORKING
#include "ccsds_udp.h"
#endif
#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
#ifdef CONFIG_AKIRA_CCSDS_TM_RND
#include "ccsds_rnd.h"
#endif
#include "ccsds_router.h"
#include "ccsds_time_service.h"
#include "ccsds_tm_frame.h"
#include "ccsds_tm_udp_route.h"
#endif
#ifdef CONFIG_AKIRA_CCSDS_CFDP
#include "akira_cfdp_service.h"
#include "ccsds_router.h"
#endif

LOG_MODULE_REGISTER(ccsds_shell, CONFIG_AKIRA_LOG_LEVEL);

#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
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
static struct ccsds_router tc_router;
static struct ccsds_profile_tc_rx tc_rx_profile;
static struct ccsds_profile_input tc_input_profile;
static bool tc_rx_profile_initialized;

static void status_lock_init_once(void)
{
    if (status_lock_initialized) {
        return;
    }

    k_mutex_init(&status_lock);
    status_lock_initialized = true;
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
    const uint8_t *tm_header;
#ifdef CONFIG_AKIRA_CCSDS_TM_RND
    uint8_t decoded_header[CCSDS_SHELL_TM_PRIMARY_HDR_LEN];
#endif
    size_t frame_offset;

    *cadu = output_has_asm(frame, frame_len);
    frame_offset = *cadu ? CCSDS_SHELL_ASM_LEN : 0u;

    *mcfc = 0u;
    *vcfc = 0u;
    *fhp = 0u;

    if (frame_len < frame_offset + CCSDS_SHELL_TM_PRIMARY_HDR_LEN) {
        return;
    }

    tm_header = &frame[frame_offset];
#ifdef CONFIG_AKIRA_CCSDS_TM_RND
    if (*cadu) {
        memcpy(decoded_header, tm_header, sizeof(decoded_header));
        ccsds_rnd_apply(decoded_header, sizeof(decoded_header));
        tm_header = decoded_header;
    }
#endif

    *mcfc = tm_header[2];
    *vcfc = tm_header[3];
    *fhp = sys_get_be16(&tm_header[4]) & 0x07ffu;
}

static int log_route(uint8_t vcid, const uint8_t *frame, size_t frame_len,
                     void *user_data)
{
    bool cadu;
    uint8_t mcfc;
    uint8_t vcfc;
    uint16_t fhp;

    ARG_UNUSED(user_data);

    __ASSERT(frame != NULL, "TM log route frame is NULL");

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

static void ensure_tc_profile_initialized(void)
{
    if (tc_rx_profile_initialized) {
        return;
    }

    ccsds_router_init(&tc_router);
    ccsds_time_service_init(&tc_router);
    ccsds_profile_tc_rx_init(&tc_rx_profile, &tc_router);
    ccsds_profile_input_init(&tc_input_profile, &tc_router, &tc_rx_profile);

    tc_rx_profile_initialized = true;
}

static void register_tc_clcw_provider_if_ready(void)
{
    if (!tc_rx_profile_initialized || !shell_tm_is_initialized()) {
        return;
    }

    ccsds_tm_frame_set_clcw_provider(ccsds_profile_tc_clcw_provider,
                                     &tc_rx_profile);
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

    __ASSERT(routes != NULL, "TM route list is NULL");
    __ASSERT(route_mask != NULL, "TM route mask output is NULL");

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

    ccsds_time_service_stop();

    ccsds_tm_frame_init();

    ret = ccsds_tm_frame_register_route(CCSDS_TM_ROUTE_LOG, log_route, NULL);
    if (ret != 0) {
        return ret;
    }

    ret = ccsds_tm_udp_route_register();
    if (ret != 0 && ret != -ENOTSUP) {
        return ret;
    }

    ensure_tc_profile_initialized();

    k_mutex_lock(&status_lock, K_FOREVER);
    memset(&tm_status, 0, sizeof(tm_status));
    tm_status.initialized = true;
    tm_status.time_vcid = CCSDS_SHELL_TIME_VCID;
    k_mutex_unlock(&status_lock);

    register_tc_clcw_provider_if_ready();

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

    ccsds_tm_frame_start(CCSDS_SHELL_TM_ACTIVE_DELAY,
                         CCSDS_SHELL_TM_IDLE_DELAY);

    ret = ccsds_time_service_start(CCSDS_SHELL_TIME_VCID);
    if (ret != 0) {
        ccsds_tm_frame_stop();
        return ret;
    }

    k_mutex_lock(&status_lock, K_FOREVER);
    tm_status.running = true;
    tm_status.time_vcid = CCSDS_SHELL_TIME_VCID;
    k_mutex_unlock(&status_lock);

    return 0;
}

void ccsds_shell_tm_stop(void)
{
    ccsds_time_service_stop();
    ccsds_tm_frame_stop();

    status_lock_init_once();
    k_mutex_lock(&status_lock, K_FOREVER);
    tm_status.running = false;
    k_mutex_unlock(&status_lock);
}

void ccsds_shell_tm_get_status(struct ccsds_shell_tm_status *status)
{
    __ASSERT(status != NULL, "CCSDS shell TM status output is NULL");

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
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ccsds_shell_tm_stop();

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

static int cmd_ccsds_tc_start_udp(const struct shell *sh, size_t argc,
                                  char **argv)
{
#ifdef CONFIG_NETWORKING
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ensure_tc_profile_initialized();
    register_tc_clcw_provider_if_ready();

    ret = ccsds_udp_start(&tc_input_profile);
    if (ret != 0) {
        shell_error(sh, "ccsds tc udp start failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds tc udp listening on port %u",
                CONFIG_AKIRA_CCSDS_UDP_LOCAL_PORT);

    return 0;
#else
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_error(sh, "ccsds tc udp input unavailable: networking disabled");
    return -ENOTSUP;
#endif
}

static int cmd_ccsds_tc_stop_udp(const struct shell *sh, size_t argc,
                                 char **argv)
{
#ifdef CONFIG_NETWORKING
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ccsds_udp_stop();

    shell_print(sh, "ccsds tc udp stopped");
    return 0;
#else
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_error(sh, "ccsds tc udp input unavailable: networking disabled");
    return -ENOTSUP;
#endif
}

static int cmd_ccsds_tc_status_udp(const struct shell *sh, size_t argc,
                                   char **argv)
{
#ifdef CONFIG_NETWORKING
    struct ccsds_udp_stats udp_stats;
#endif

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

#ifdef CONFIG_NETWORKING
    ccsds_udp_get_stats(&udp_stats);

    shell_print(sh, "ccsds tc udp available=%u running=%u",
                ccsds_udp_available() ? 1u : 0u,
                udp_stats.running ? 1u : 0u);
    shell_print(sh, "local_port=%u peer=%s:%u",
                CONFIG_AKIRA_CCSDS_UDP_LOCAL_PORT,
                CONFIG_AKIRA_CCSDS_UDP_PEER_IP,
                CONFIG_AKIRA_CCSDS_UDP_PEER_PORT);
    shell_print(sh, "udp_rx=%u udp_tx=%u udp_last_error=%d",
                udp_stats.datagrams_received, udp_stats.datagrams_sent,
                udp_stats.last_error);
#else
    shell_print(sh, "ccsds tc udp available=0 running=0");
#endif

    return 0;
}

static int cmd_ccsds_tc_status(const struct shell *sh, size_t argc,
                               char **argv)
{
    struct ccsds_profile_tc_rx_stats rx_stats;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    ccsds_profile_tc_rx_get_stats(&rx_stats);

    shell_print(sh,
                "cltu_rx=%u oversize=%u cltu_fail=%u frame_reject=%u control=%u",
                rx_stats.cltus_received, rx_stats.cltus_oversize,
                rx_stats.cltu_decode_failures, rx_stats.tc_frame_rejects,
                rx_stats.control_frames_seen);
    shell_print(sh, "dispatch_ok=%u dispatch_fail=%u last_error=%d",
                rx_stats.packets_dispatched, rx_stats.dispatch_failures,
                rx_stats.last_error);
    shell_print(sh, "last_cltu_len=%zu last_tc_frame_len=%zu",
                rx_stats.last_cltu_len, rx_stats.last_tc_frame_len);

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
    sub_ccsds_tc_start,
    SHELL_CMD(udp, NULL, "Start CCSDS TC UDP input", cmd_ccsds_tc_start_udp),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tc_stop,
    SHELL_CMD(udp, NULL, "Stop CCSDS TC UDP input", cmd_ccsds_tc_stop_udp),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tc_status,
    SHELL_CMD(udp, NULL, "Show CCSDS TC UDP input status",
              cmd_ccsds_tc_status_udp),
    SHELL_SUBCMD_SET_END);

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_tc,
    SHELL_CMD(start, &sub_ccsds_tc_start, "Start CCSDS TC input", NULL),
    SHELL_CMD(stop, &sub_ccsds_tc_stop, "Stop CCSDS TC input", NULL),
    SHELL_CMD(status, &sub_ccsds_tc_status, "Show CCSDS TC input status",
              cmd_ccsds_tc_status),
    SHELL_SUBCMD_SET_END);
#endif /* CONFIG_AKIRA_CCSDS_FRAME_SUPPORT */

#ifdef CONFIG_AKIRA_CCSDS_CFDP
static akira_cfdp_service_config_t cfdp_config;
static bool cfdp_config_initialized;
static bool cfdp_started;
#if defined(CONFIG_NETWORKING) && !defined(CONFIG_AKIRA_CCSDS_FRAME_SUPPORT)
static struct ccsds_router cfdp_router;
static struct ccsds_profile_input cfdp_input;
#endif

static void cfdp_config_init_once(void)
{
    if (!cfdp_config_initialized) {
        akira_cfdp_service_config_defaults(&cfdp_config);
        cfdp_config_initialized = true;
    }
}

static uint8_t encoded_uint_len(uint64_t value)
{
    uint8_t len = 1u;

    while (len < sizeof(value) && (value >> (len * 8u)) != 0u) {
        len++;
    }
    return len;
}

int ccsds_shell_cfdp_configure(uint64_t local_entity_id,
                               uint64_t remote_entity_id, uint16_t apid)
{
    uint8_t entity_id_len;

    if (cfdp_started) {
        return -EALREADY;
    }
    if (local_entity_id == 0u || remote_entity_id == 0u || apid == 0u ||
        apid > 0x07ffu) {
        return -EINVAL;
    }

    entity_id_len = MAX(encoded_uint_len(local_entity_id),
                        encoded_uint_len(remote_entity_id));
    if (entity_id_len > CCSDS_CFDP_MAX_ENTITY_ID_LEN) {
        return -ERANGE;
    }

    cfdp_config_init_once();
    cfdp_config.local_entity_id = local_entity_id;
    cfdp_config.remote_entity_id = remote_entity_id;
    cfdp_config.entity_id_len = entity_id_len;
    cfdp_config.local_apid = apid;
    cfdp_config.remote_apid = apid;
    return 0;
}

void ccsds_shell_cfdp_get_config(akira_cfdp_service_config_t *config,
                                 bool *started)
{
    __ASSERT(config != NULL, "CFDP shell config output is NULL");
    __ASSERT(started != NULL, "CFDP shell started output is NULL");

    cfdp_config_init_once();
    *config = cfdp_config;
    *started = cfdp_started;
}

#if defined(CONFIG_NETWORKING) && !defined(CONFIG_AKIRA_CCSDS_FRAME_SUPPORT)
static uint64_t cfdp_now_ms(void *user)
{
    ARG_UNUSED(user);
    return (uint64_t)k_uptime_get();
}
#endif

int ccsds_shell_cfdp_start(void)
{
#if defined(CONFIG_NETWORKING) && !defined(CONFIG_AKIRA_CCSDS_FRAME_SUPPORT)
    enum ccsds_cfdp_status status;
    int ret;

    if (cfdp_started) {
        return -EALREADY;
    }

    cfdp_config_init_once();
    cfdp_config.send_packet = ccsds_udp_send;
    cfdp_config.send_user = NULL;
    cfdp_config.now_ms = cfdp_now_ms;
    cfdp_config.packet_type = CCSDS_PACKET_TYPE_TC;

    ccsds_router_init(&cfdp_router);
    status = akira_cfdp_service_init(&cfdp_config);
    if (status != CCSDS_CFDP_STATUS_OK) {
        return -(int)status;
    }
    ret = akira_cfdp_service_register_rx(&cfdp_router);
    if (ret != 0) {
        return ret;
    }

    ccsds_profile_input_init(&cfdp_input, &cfdp_router, NULL);
    ret = ccsds_udp_start(&cfdp_input);
    if (ret != 0) {
        return ret;
    }

    cfdp_started = true;
    return 0;
#else
    return -ENOTSUP;
#endif
}

static int parse_u64(const char *arg, uint64_t *value)
{
    char *endptr;
    unsigned long long parsed;

    if (arg == NULL || value == NULL || arg[0] == '\0' || arg[0] == '-') {
        return -EINVAL;
    }
    errno = 0;
    parsed = strtoull(arg, &endptr, 0);
    if (errno != 0 || *endptr != '\0') {
        return -EINVAL;
    }
    *value = (uint64_t)parsed;
    return 0;
}

static int cmd_ccsds_cfdp_config(const struct shell *sh, size_t argc,
                                 char **argv)
{
    uint64_t local;
    uint64_t remote;
    uint64_t apid;
    int ret;

    ARG_UNUSED(argc);
    if (strcmp(argv[1], "local") != 0 || strcmp(argv[3], "remote") != 0 ||
        strcmp(argv[5], "apid") != 0 || parse_u64(argv[2], &local) != 0 ||
        parse_u64(argv[4], &remote) != 0 || parse_u64(argv[6], &apid) != 0 ||
        apid > UINT16_MAX) {
        shell_error(sh,
                    "usage: ccsds cfdp config local <entity-id> remote <entity-id> apid <apid>");
        return -EINVAL;
    }

    ret = ccsds_shell_cfdp_configure(local, remote, (uint16_t)apid);
    if (ret != 0) {
        shell_error(sh, "ccsds cfdp config failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds cfdp configured local=%llu remote=%llu apid=0x%03x",
                (unsigned long long)local, (unsigned long long)remote,
                (unsigned int)apid);
    return 0;
}

static int cmd_ccsds_cfdp_start(const struct shell *sh, size_t argc,
                                char **argv)
{
    int ret;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    ret = ccsds_shell_cfdp_start();
    if (ret != 0) {
        shell_error(sh, "ccsds cfdp start failed: %d", ret);
        return ret;
    }

    shell_print(sh, "ccsds cfdp started");
    return 0;
}

static int cmd_ccsds_cfdp_put(const struct shell *sh, size_t argc, char **argv)
{
    ccsds_cfdp_transaction_id_t id;
    enum ccsds_cfdp_status status;

    ARG_UNUSED(argc);
    if (!cfdp_started) {
        shell_error(sh, "ccsds cfdp put requires ccsds cfdp start");
        return -EACCES;
    }

    status = akira_cfdp_service_send_path(argv[1], argv[2], &id);
    if (status != CCSDS_CFDP_STATUS_OK) {
        shell_error(sh, "ccsds cfdp put failed: %d", status);
        return -(int)status;
    }

    shell_print(sh, "ccsds cfdp put transaction=%llu:%llu",
                (unsigned long long)id.source_entity_id,
                (unsigned long long)id.transaction_sequence_number);
    return 0;
}

static int cmd_ccsds_cfdp_status(const struct shell *sh, size_t argc,
                                 char **argv)
{
    struct akira_cfdp_service_status status;

    ARG_UNUSED(argc);
    ARG_UNUSED(argv);
    akira_cfdp_service_get_status(&status);
    if (!status.valid) {
        shell_print(sh, "last transaction=none");
        return 0;
    }

    shell_print(sh,
                "last transaction=%llu:%llu event=%s source=%s dest=%s size=%u checksum=0x%08x checksum=%s status=%s cfdp_status=%s",
                (unsigned long long)status.transaction_id.source_entity_id,
                (unsigned long long)status.transaction_id.transaction_sequence_number,
                status.event_type == CCSDS_CFDP_EVENT_COMPLETE ? "COMPLETE" :
                                                                 "FAILED",
                status.source_path, status.destination_path, status.file_size,
                status.checksum, status.checksum_ok ? "OK" : "NOK",
                status.event_type == CCSDS_CFDP_EVENT_COMPLETE ? "COMPLETE" :
                                                                 "FAILED",
                akira_cfdp_service_status_name(status.status));
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds_cfdp,
    SHELL_CMD_ARG(config, NULL, "Configure CFDP entity IDs and APID",
                  cmd_ccsds_cfdp_config, 7, 0),
    SHELL_CMD(start, NULL, "Start the CFDP service", cmd_ccsds_cfdp_start),
    SHELL_CMD_ARG(put, NULL, "Send a file", cmd_ccsds_cfdp_put, 3, 0),
    SHELL_CMD(status, NULL, "Show last CFDP terminal status",
              cmd_ccsds_cfdp_status),
    SHELL_SUBCMD_SET_END);
#endif /* CONFIG_AKIRA_CCSDS_CFDP */

SHELL_STATIC_SUBCMD_SET_CREATE(
    sub_ccsds,
#ifdef CONFIG_AKIRA_CCSDS_FRAME_SUPPORT
    SHELL_CMD(tm, &sub_ccsds_tm, "CCSDS TM commands", NULL),
    SHELL_CMD(tc, &sub_ccsds_tc, "CCSDS TC commands", NULL),
#endif
#ifdef CONFIG_AKIRA_CCSDS_CFDP
    SHELL_CMD(cfdp, &sub_ccsds_cfdp, "CCSDS CFDP commands", NULL),
#endif
    SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(ccsds, &sub_ccsds, "CCSDS commands", NULL);
