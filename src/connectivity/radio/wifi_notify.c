/**
 * @file wifi_notify.c
 * @brief WiFi IPv4-ready notifier
 *
 * Listens for NET_EVENT_IPV4_ADDR_ADD (fires when DHCP assigns an IP on
 * ESP32) and signals a dedicated thread to call akira_on_wifi_connected().
 *
 * The callback itself runs on the net_mgmt event thread — a shared resource.
 * Doing DNS + HTTP + WebSocket work there would block all other net_mgmt
 * events and risk deadlocking the network stack. Instead the callback just
 * gives a semaphore; the dedicated thread does the heavy lifting with its
 * own stack.
 *
 * AkiraOS default: akira_on_wifi_connected() is a no-op (main.c).
 * AkiraPlatform override: hub_register() + hub_ws_start().
 *
 * Stack size is controlled by CONFIG_AKIRA_WIFI_NOTIFY_STACK_SIZE.
 *
 * @copyright Copyright (c) 2026 PenEngineering S.R.L
 */

#include "../../akira.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(wifi_notify, LOG_LEVEL_INF);

/* Stack for the dedicated wifi-connected worker thread.
 * Must cover DNS resolution + any work done in akira_on_wifi_connected(). */
#define WIFI_NOTIFY_STACK_SIZE 4096

/* Semaphore: net_mgmt callback gives it, worker thread takes it.
 * Max count 1 — if WiFi reconnects rapidly, only one run is queued. */
static K_SEM_DEFINE(s_wifi_ready, 0, 1);

static void wifi_connected_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

	while (1) {
		k_sem_take(&s_wifi_ready, K_FOREVER);
		LOG_INF("wifi_notify: running akira_on_wifi_connected()");
		akira_on_wifi_connected();
	}
}

K_THREAD_DEFINE(wifi_notify_tid,
		WIFI_NOTIFY_STACK_SIZE,
		wifi_connected_thread, NULL, NULL, NULL,
		K_PRIO_COOP(7), 0, 0);

static struct net_mgmt_event_callback s_ipv4_cb;

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
			       uint32_t event, struct net_if *iface)
{
	ARG_UNUSED(cb);
	ARG_UNUSED(iface);

	if (!event) {
		return;
	}

	LOG_INF("IPv4 address assigned — signalling wifi_notify thread");
	k_sem_give(&s_wifi_ready);
}

static int wifi_notify_init(void)
{
	net_mgmt_init_event_callback(&s_ipv4_cb, ipv4_event_handler,
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&s_ipv4_cb);
	LOG_DBG("wifi_notify: IPv4 event handler registered");
	return 0;
}

SYS_INIT(wifi_notify_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

