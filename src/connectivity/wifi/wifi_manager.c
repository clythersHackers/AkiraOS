/**
 * @file wifi_manager.c
 * @brief Centralized WiFi connection manager
 *
 * @copyright Copyright (c) 2026 PenEngineering S.R.L
 */

#include "wifi_manager.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_stats.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include "../../settings/settings.h"

LOG_MODULE_REGISTER(wifi_manager, CONFIG_AKIRA_LOG_LEVEL);

/* ── internal state ─────────────────────────────────────────────────────── */

struct wifi_mgr_listener {
    wifi_mgr_event_cb_t cb;
    void               *user_data;
};

static struct {
    wifi_mgr_state_t    state;
    struct wifi_mgr_listener listeners[CONFIG_AKIRA_WIFI_MANAGER_MAX_CBS];
    int64_t             connect_time_ms;   /* k_uptime_get() at IP assignment */
    struct net_mgmt_event_callback wifi_cb;
    struct net_mgmt_event_callback ipv4_cb;
    struct k_mutex      lock;
} mgr;

/* ── helpers ─────────────────────────────────────────────────────────────── */

static void fire_event(wifi_mgr_event_t evt)
{
    for (int i = 0; i < CONFIG_AKIRA_WIFI_MANAGER_MAX_CBS; i++) {
        if (mgr.listeners[i].cb) {
            mgr.listeners[i].cb(evt, mgr.listeners[i].user_data);
        }
    }
}

/* ── net_mgmt callbacks ──────────────────────────────────────────────────── */

static void wifi_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t event, struct net_if *iface)
{
    ARG_UNUSED(cb);
    ARG_UNUSED(iface);

    k_mutex_lock(&mgr.lock, K_FOREVER);

    switch (event) {
    case NET_EVENT_WIFI_CONNECT_RESULT: {
        const struct wifi_status *status =
            (const struct wifi_status *)cb->info;
        if (status && status->conn_status != WIFI_STATUS_CONN_SUCCESS) {
            LOG_WRN("WiFi connect failed (status=%d)", status->conn_status);
            mgr.state = WIFI_MGR_STATE_IDLE;
            k_mutex_unlock(&mgr.lock);
            fire_event(WIFI_MGR_EVT_CONNECT_FAILED);
            return;
        }
        /* L2 up — wait for IP assignment before declaring CONNECTED */
        LOG_DBG("WiFi L2 connected, waiting for IP");
        break;
    }

    case NET_EVENT_WIFI_DISCONNECT_RESULT:
        if (mgr.state == WIFI_MGR_STATE_CONNECTED ||
            mgr.state == WIFI_MGR_STATE_DISCONNECTING) {
            LOG_INF("WiFi disconnected");
            mgr.state = WIFI_MGR_STATE_IDLE;
            mgr.connect_time_ms = 0;
            k_mutex_unlock(&mgr.lock);
            fire_event(WIFI_MGR_EVT_DISCONNECTED);
            return;
        }
        break;

    default:
        break;
    }

    k_mutex_unlock(&mgr.lock);
}

static void ipv4_event_handler(struct net_mgmt_event_callback *cb,
                                uint64_t event, struct net_if *iface)
{
    ARG_UNUSED(cb);
    ARG_UNUSED(iface);

    if (event != NET_EVENT_IPV4_ADDR_ADD) {
        return;
    }

    k_mutex_lock(&mgr.lock, K_FOREVER);

    if (mgr.state == WIFI_MGR_STATE_CONNECTING) {
        mgr.state = WIFI_MGR_STATE_CONNECTED;
        mgr.connect_time_ms = k_uptime_get();
        LOG_INF("WiFi connected with IP");
        k_mutex_unlock(&mgr.lock);
        fire_event(WIFI_MGR_EVT_CONNECTED);
        return;
    }

    k_mutex_unlock(&mgr.lock);
}

/* ── public API ──────────────────────────────────────────────────────────── */

int wifi_manager_connect(void)
{
    k_mutex_lock(&mgr.lock, K_FOREVER);

    if (mgr.state == WIFI_MGR_STATE_CONNECTED ||
        mgr.state == WIFI_MGR_STATE_CONNECTING) {
        k_mutex_unlock(&mgr.lock);
        return -EALREADY;
    }

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        k_mutex_unlock(&mgr.lock);
        LOG_ERR("No network interface");
        return -ENODEV;
    }

    char ssid[MAX_VALUE_LEN];
    char psk[MAX_VALUE_LEN];

    /* Try the combined atomic key first; fall back to legacy individual keys
     * for devices that were provisioned before this format was introduced. */
    char combined[MAX_VALUE_LEN * 2 + 2];
    if (akira_settings_get(AKIRA_SETTINGS_WIFI_CREDS_KEY, combined, sizeof(combined)) == 0) {
        char *tab = strchr(combined, '\t');
        if (!tab) {
            k_mutex_unlock(&mgr.lock);
            LOG_ERR("Malformed combined WiFi credentials");
            return -EINVAL;
        }
        *tab = '\0';
        strncpy(ssid, combined, sizeof(ssid) - 1);
        ssid[sizeof(ssid) - 1] = '\0';
        strncpy(psk, tab + 1, sizeof(psk) - 1);
        psk[sizeof(psk) - 1] = '\0';
    } else if (akira_settings_get(AKIRA_SETTINGS_WIFI_SSID_KEY, ssid, sizeof(ssid)) != 0 ||
               akira_settings_get(AKIRA_SETTINGS_WIFI_PSK_KEY,  psk,  sizeof(psk))  != 0) {
        k_mutex_unlock(&mgr.lock);
        LOG_ERR("No WiFi credentials in NVS");
        return -ENOENT;
    }

    struct wifi_connect_req_params params = {
        .ssid        = (uint8_t *)ssid,
        .ssid_length = strlen(ssid),
        .psk         = (uint8_t *)psk,
        .psk_length  = strlen(psk),
        .channel     = WIFI_CHANNEL_ANY,
        .security    = strlen(psk) > 0 ? WIFI_SECURITY_TYPE_PSK
                                        : WIFI_SECURITY_TYPE_NONE,
        .mfp         = WIFI_MFP_OPTIONAL,
    };

    int ret = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(params));
    if (ret) {
        k_mutex_unlock(&mgr.lock);
        LOG_ERR("WiFi connect request failed: %d", ret);
        return ret;
    }

    mgr.state = WIFI_MGR_STATE_CONNECTING;
    LOG_INF("WiFi connecting to '%s'", ssid);

    k_mutex_unlock(&mgr.lock);
    return 0;
}

int wifi_manager_disconnect(void)
{
    k_mutex_lock(&mgr.lock, K_FOREVER);

    if (mgr.state == WIFI_MGR_STATE_IDLE ||
        mgr.state == WIFI_MGR_STATE_DISCONNECTING) {
        k_mutex_unlock(&mgr.lock);
        return -EALREADY;
    }

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        k_mutex_unlock(&mgr.lock);
        return -ENODEV;
    }

    int ret = net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);
    if (ret) {
        k_mutex_unlock(&mgr.lock);
        LOG_ERR("WiFi disconnect request failed: %d", ret);
        return ret;
    }

    mgr.state = WIFI_MGR_STATE_DISCONNECTING;
    k_mutex_unlock(&mgr.lock);
    return 0;
}

int wifi_manager_update_credentials(const char *ssid, const char *psk)
{
    if (!ssid || !psk) {
        return -EINVAL;
    }

    /* Write SSID and PSK as one tab-delimited value so the single NVS write is
     * atomic — a power-loss between two separate writes can leave stale PSK or
     * SSID and cause a permanent connection failure on next boot. */
    char combined[MAX_VALUE_LEN * 2 + 2];
    int n = snprintf(combined, sizeof(combined), "%s\t%s", ssid, psk);
    if (n < 0 || (size_t)n >= sizeof(combined)) {
        return -ENAMETOOLONG;
    }

    int ret = akira_settings_set(AKIRA_SETTINGS_WIFI_CREDS_KEY, combined, true);
    if (ret) {
        LOG_ERR("Failed to save WiFi credentials: %d", ret);
        return ret;
    }

    LOG_INF("WiFi credentials updated (ssid='%s')", ssid);
    return 0;
}

int wifi_manager_get_stats(wifi_mgr_stats_t *out)
{
    if (!out) {
        return -EINVAL;
    }

    k_mutex_lock(&mgr.lock, K_FOREVER);

    if (mgr.state != WIFI_MGR_STATE_CONNECTED) {
        k_mutex_unlock(&mgr.lock);
        return -ENOTCONN;
    }

    memset(out, 0, sizeof(*out));

    struct net_if *iface = net_if_get_default();
    if (!iface) {
        k_mutex_unlock(&mgr.lock);
        return -ENODEV;
    }

    /* RSSI, channel, SSID from driver status */
    struct wifi_iface_status status;
    if (net_mgmt(NET_REQUEST_WIFI_IFACE_STATUS, iface,
                 &status, sizeof(status)) == 0) {
        out->rssi    = status.rssi;
        out->channel = status.channel;
        strncpy(out->ssid, (const char *)status.ssid,
                MIN(status.ssid_len, sizeof(out->ssid) - 1));
    }

    /* IP address — use the foreach API; direct struct access removed in Zephyr 4.x */
    struct in_addr *global = net_if_ipv4_get_global_addr(iface, NET_ADDR_PREFERRED);
    if (global) {
        net_addr_ntop(AF_INET, global, out->ip_addr, sizeof(out->ip_addr));
    }

    /* Time connected */
    out->time_connected_s = (uint32_t)((k_uptime_get() - mgr.connect_time_ms) / 1000);

    /* TX/RX bytes from net_if stats */
#if defined(CONFIG_NET_STATISTICS_ETHERNET)
    struct net_stats_eth stats_buf;
    if (net_mgmt(NET_REQUEST_STATS_GET_ETHERNET, iface,
                 &stats_buf, sizeof(stats_buf)) == 0) {
        out->tx_bytes = (uint32_t)stats_buf.bytes.sent;
        out->rx_bytes = (uint32_t)stats_buf.bytes.received;
    }
#endif

    k_mutex_unlock(&mgr.lock);
    return 0;
}

wifi_mgr_state_t wifi_manager_get_state(void)
{
    return mgr.state;
}

int wifi_manager_register_cb(wifi_mgr_event_cb_t cb, void *user_data)
{
    if (!cb) {
        return -EINVAL;
    }

    k_mutex_lock(&mgr.lock, K_FOREVER);

    int free_slot = -1;
    for (int i = 0; i < CONFIG_AKIRA_WIFI_MANAGER_MAX_CBS; i++) {
        if (mgr.listeners[i].cb == cb &&
            mgr.listeners[i].user_data == user_data) {
            k_mutex_unlock(&mgr.lock);
            return -EALREADY;
        }
        if (free_slot < 0 && mgr.listeners[i].cb == NULL) {
            free_slot = i;
        }
    }

    if (free_slot < 0) {
        k_mutex_unlock(&mgr.lock);
        LOG_WRN("Callback table full (max %d)", CONFIG_AKIRA_WIFI_MANAGER_MAX_CBS);
        return -ENOMEM;
    }

    mgr.listeners[free_slot].cb        = cb;
    mgr.listeners[free_slot].user_data = user_data;

    k_mutex_unlock(&mgr.lock);
    return 0;
}

int wifi_manager_unregister_cb(wifi_mgr_event_cb_t cb, void *user_data)
{
    if (!cb) {
        return -EINVAL;
    }

    k_mutex_lock(&mgr.lock, K_FOREVER);

    for (int i = 0; i < CONFIG_AKIRA_WIFI_MANAGER_MAX_CBS; i++) {
        if (mgr.listeners[i].cb == cb &&
            mgr.listeners[i].user_data == user_data) {
            mgr.listeners[i].cb        = NULL;
            mgr.listeners[i].user_data = NULL;
            k_mutex_unlock(&mgr.lock);
            return 0;
        }
    }

    k_mutex_unlock(&mgr.lock);
    return -ENOENT;
}

/* ── SYS_INIT ────────────────────────────────────────────────────────────── */

static int wifi_manager_init(void)
{
    memset(&mgr, 0, sizeof(mgr));
    k_mutex_init(&mgr.lock);
    mgr.state = WIFI_MGR_STATE_IDLE;

    net_mgmt_init_event_callback(&mgr.wifi_cb, wifi_event_handler,
                                 NET_EVENT_WIFI_CONNECT_RESULT |
                                 NET_EVENT_WIFI_DISCONNECT_RESULT);
    net_mgmt_add_event_callback(&mgr.wifi_cb);

    net_mgmt_init_event_callback(&mgr.ipv4_cb, ipv4_event_handler,
                                 NET_EVENT_IPV4_ADDR_ADD);
    net_mgmt_add_event_callback(&mgr.ipv4_cb);

    LOG_INF("WiFi manager initialized");
    return 0;
}

SYS_INIT(wifi_manager_init, APPLICATION, CONFIG_AKIRA_WIFI_MANAGER_INIT_PRIORITY);
