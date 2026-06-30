#include "ccsds_tc_udp_input.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

#include "ccsds_profile.h"

LOG_MODULE_REGISTER(ccsds_tc_udp_input, CONFIG_AKIRA_LOG_LEVEL);

#define CCSDS_TC_UDP_THREAD_PRIORITY CONFIG_AKIRA_CCSDS_TC_UDP_THREAD_PRIORITY
#define CCSDS_TC_UDP_THREAD_STACK_SIZE \
    CONFIG_AKIRA_CCSDS_TC_UDP_THREAD_STACK_SIZE

static K_THREAD_STACK_DEFINE(udp_thread_stack, CCSDS_TC_UDP_THREAD_STACK_SIZE);
static struct k_thread udp_thread;
static K_MUTEX_DEFINE(udp_lock);
static struct ccsds_tc_udp_input_stats udp_stats;
static int udp_fd = -1;
static bool udp_thread_started;

static uint8_t cltu_buf[CONFIG_AKIRA_CCSDS_MAX_CLTU_LEN + 1u];

static void process_datagram(struct ccsds_profile_tc_rx *profile,
                             const uint8_t *cltu, size_t cltu_len)
{
    int ret;

    k_mutex_lock(&udp_lock, K_FOREVER);
    udp_stats.datagrams_received++;
    k_mutex_unlock(&udp_lock);

    ret = ccsds_profile_tc_cltu_dispatch(profile, cltu, cltu_len);

    k_mutex_lock(&udp_lock, K_FOREVER);
    if (ret != 0) {
        udp_stats.last_error = ret;
    }
    k_mutex_unlock(&udp_lock);
}

static void udp_thread_fn(void *p1, void *p2, void *p3)
{
    int fd = (int)(intptr_t)p1;
    struct ccsds_profile_tc_rx *profile = p2;

    ARG_UNUSED(p3);

    while (true) {
        ssize_t received = zsock_recv(fd, cltu_buf, sizeof(cltu_buf), 0);

        if (received < 0) {
            int ret = -errno;
            bool running;

            k_mutex_lock(&udp_lock, K_FOREVER);
            running = udp_stats.running;
            udp_stats.last_error = ret;
            k_mutex_unlock(&udp_lock);

            if (!running) {
                break;
            }

            LOG_WRN("tc udp receive failed ret=%d", ret);
            continue;
        }

        process_datagram(profile, cltu_buf, (size_t)received);
    }

    k_mutex_lock(&udp_lock, K_FOREVER);
    if (udp_fd == fd) {
        (void)zsock_close(fd);
        udp_fd = -1;
    }
    udp_stats.running = false;
    udp_thread_started = false;
    k_mutex_unlock(&udp_lock);
}

bool ccsds_tc_udp_input_available(void)
{
    return true;
}

int ccsds_tc_udp_input_start(struct ccsds_profile_tc_rx *profile)
{
    struct sockaddr_in local = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_AKIRA_CCSDS_TC_UDP_LOCAL_PORT),
        .sin_addr = {
            .s_addr = htonl(INADDR_ANY),
        },
    };
    int fd;
    int ret;

    __ASSERT(profile != NULL, "TC UDP input profile is NULL");

    k_mutex_lock(&udp_lock, K_FOREVER);
    if (udp_stats.running || udp_thread_started) {
        k_mutex_unlock(&udp_lock);
        return -EALREADY;
    }
    k_mutex_unlock(&udp_lock);

    fd = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return -errno;
    }

    ret = zsock_bind(fd, (const struct sockaddr *)&local, sizeof(local));
    if (ret < 0) {
        ret = -errno;
        (void)zsock_close(fd);
        return ret;
    }

    k_mutex_lock(&udp_lock, K_FOREVER);
    memset(&udp_stats, 0, sizeof(udp_stats));
    udp_stats.running = true;
    udp_fd = fd;
    udp_thread_started = true;
    k_mutex_unlock(&udp_lock);

    k_thread_create(&udp_thread, udp_thread_stack,
                    K_THREAD_STACK_SIZEOF(udp_thread_stack), udp_thread_fn,
                    (void *)(intptr_t)fd, profile, NULL,
                    CCSDS_TC_UDP_THREAD_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&udp_thread, "ccsds_tc_udp");

    LOG_INF("tc udp input listening port=%u",
            CONFIG_AKIRA_CCSDS_TC_UDP_LOCAL_PORT);

    return 0;
}

int ccsds_tc_udp_input_stop(void)
{
    int fd;

    k_mutex_lock(&udp_lock, K_FOREVER);
    udp_stats.running = false;
    fd = udp_fd;
    udp_fd = -1;
    k_mutex_unlock(&udp_lock);

    if (fd >= 0) {
        (void)zsock_close(fd);
    }

    return 0;
}

void ccsds_tc_udp_input_get_stats(struct ccsds_tc_udp_input_stats *stats)
{
    __ASSERT(stats != NULL, "TC UDP input stats output is NULL");

    k_mutex_lock(&udp_lock, K_FOREVER);
    *stats = udp_stats;
    k_mutex_unlock(&udp_lock);
}
