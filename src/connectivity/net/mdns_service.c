/**
 * @file mdns_service.c
 * @brief mDNS / DNS-SD service registration for AkiraOS
 *
 * Registers two DNS-SD services:
 *   _akiraos._tcp  — primary AkiraOS management API (port HTTP_SERVER_PORT)
 *   _http._tcp     — standard HTTP (same port, for browser discovery)
 *
 * The mDNS A record is automatically served by Zephyr's CONFIG_MDNS_RESPONDER
 * for whatever hostname CONFIG_NET_HOSTNAME is set to (default: "akiraos").
 * The device is reachable at:
 *   http://akiraos.local:8080/
 */

#include "mdns_service.h"
#include "http/http_server.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#ifdef CONFIG_DNS_SD
#include <zephyr/net/dns_sd.h>
#endif

#ifdef CONFIG_NET_HOSTNAME
#include <zephyr/net/hostname.h>
#endif

LOG_MODULE_REGISTER(mdns_service, CONFIG_AKIRA_LOG_LEVEL);

/*===========================================================================*/
/* DNS-SD compile-time service records                                      */
/*===========================================================================*/

#ifdef CONFIG_DNS_SD

/*
 * DNS_SD_REGISTER_TCP_SERVICE() creates a static service record that the
 * Zephyr mDNS/DNS-SD stack picks up automatically.  The instance name is
 * the first argument to the advertised service.  The hostname (".local")
 * part comes from CONFIG_NET_HOSTNAME.
 */

/* AkiraOS management API service */
DNS_SD_REGISTER_TCP_SERVICE(akiraos_service,
                            CONFIG_AKIRA_DEVICE_NAME,
                            "_akiraos",
                            "local",
                            DNS_SD_EMPTY_TXT,
                            HTTP_SERVER_PORT);

/* Standard _http._tcp entry so browsers / Bonjour can discover us */
DNS_SD_REGISTER_TCP_SERVICE(akiraos_http_service,
                            CONFIG_AKIRA_DEVICE_NAME,
                            "_http",
                            "local",
                            DNS_SD_EMPTY_TXT,
                            HTTP_SERVER_PORT);

#endif /* CONFIG_DNS_SD */

/*===========================================================================*/
/* Runtime state                                                             */
/*===========================================================================*/

static bool mdns_started = false;

/*===========================================================================*/
/* Public API                                                                */
/*===========================================================================*/

void akira_mdns_start(const char *device_name)
{
    ARG_UNUSED(device_name); /* instance name is baked in at compile time */

    if (mdns_started)
    {
        return;
    }

#ifdef CONFIG_NET_HOSTNAME
    /* Log the effective mDNS hostname so it shows up in boot logs */
    const char *hostname = net_hostname_get();
    LOG_INF("mDNS: device reachable at http://%s.local:%d/",
            hostname ? hostname : CONFIG_NET_HOSTNAME,
            HTTP_SERVER_PORT);
#else
    LOG_INF("mDNS: device reachable at http://%s.local:%d/",
            CONFIG_NET_HOSTNAME, HTTP_SERVER_PORT);
#endif

#ifdef CONFIG_DNS_SD
    LOG_INF("DNS-SD: advertising \"%s\" on _akiraos._tcp and _http._tcp",
            CONFIG_AKIRA_DEVICE_NAME);
#else
    LOG_WRN("DNS-SD not enabled — set CONFIG_DNS_SD=y and "
            "CONFIG_MDNS_RESPONDER=y for .local discovery");
#endif

    mdns_started = true;
}

void akira_mdns_stop(void)
{
    if (!mdns_started)
    {
        return;
    }
    /* Zephyr's static DNS-SD records cannot be unregistered at runtime;
     * we simply mark the service as inactive. */
    mdns_started = false;
    LOG_INF("mDNS: service stopped");
}
