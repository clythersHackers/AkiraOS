#include "ccsds_tm_udp_route.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#ifdef CONFIG_NETWORKING
#include <zephyr/net/socket.h>
#endif

#include <zephyr/sys/util.h>

#include "ccsds_tm_frame.h"

#ifdef CONFIG_NETWORKING
static int udp_route_fd = -1;

static int udp_route(uint8_t vcid, const uint8_t *frame, size_t frame_len,
                     void *user_data)
{
    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons(CONFIG_AKIRA_CCSDS_TM_UDP_DEST_PORT),
    };
    ssize_t sent;
    int ret;

    ARG_UNUSED(vcid);
    ARG_UNUSED(user_data);

    if (frame == NULL) {
        return -EINVAL;
    }

    ret = zsock_inet_pton(AF_INET, CONFIG_AKIRA_CCSDS_TM_UDP_DEST_IP,
                          &dest.sin_addr);
    if (ret != 1) {
        return -EINVAL;
    }

    if (udp_route_fd < 0) {
        int broadcast = 1;

        udp_route_fd = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (udp_route_fd < 0) {
            return -errno;
        }

        ret = zsock_setsockopt(udp_route_fd, SOL_SOCKET, SO_BROADCAST,
                               &broadcast, sizeof(broadcast));
        if (ret < 0) {
            ret = -errno;
            (void)zsock_close(udp_route_fd);
            udp_route_fd = -1;
            return ret;
        }
    }

    sent = zsock_sendto(udp_route_fd, frame, frame_len, 0,
                        (const struct sockaddr *)&dest, sizeof(dest));
    if (sent < 0) {
        return -errno;
    }

    return sent == (ssize_t)frame_len ? 0 : -EIO;
}
#endif

bool ccsds_tm_udp_route_available(void)
{
#ifdef CONFIG_NETWORKING
    return true;
#else
    return false;
#endif
}

int ccsds_tm_udp_route_register(void)
{
#ifdef CONFIG_NETWORKING
    if (udp_route_fd >= 0) {
        (void)zsock_close(udp_route_fd);
        udp_route_fd = -1;
    }

    return ccsds_tm_frame_register_route(CCSDS_TM_ROUTE_UDP, udp_route, NULL);
#else
    return -ENOTSUP;
#endif
}
