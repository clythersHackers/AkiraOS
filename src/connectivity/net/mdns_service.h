/**
 * @file mdns_service.h
 * @brief mDNS / DNS-SD service registration for AkiraOS
 *
 * Advertises the device on the local network as:
 *   {device_name}._akiraos._tcp.local  (DNS-SD service)
 *   {CONFIG_NET_HOSTNAME}.local         (mDNS A record)
 *
 * Requires:
 *   CONFIG_MDNS_RESPONDER=y
 *   CONFIG_DNS_SD=y
 *   CONFIG_NET_HOSTNAME="akiraos"  (or any desired hostname)
 * @stability experimental
 * @since 1.5
 */

#ifndef AKIRA_MDNS_SERVICE_H
#define AKIRA_MDNS_SERVICE_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Start mDNS/DNS-SD advertisement.
     *
     * Should be called once the network interface has a valid IP address.
     * Subsequent calls are no-ops.
     *
     * @param device_name  Human-readable instance name for DNS-SD
     *                     (e.g. "AkiraOS Device").  Pass NULL to use
     *                     CONFIG_AKIRA_DEVICE_NAME.
     */
    void akira_mdns_start(const char *device_name);

    /**
     * @brief Stop mDNS/DNS-SD advertisement.
     */
    void akira_mdns_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_MDNS_SERVICE_H */
