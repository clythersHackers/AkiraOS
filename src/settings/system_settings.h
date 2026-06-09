/**
 * @file system_settings.h
 * @brief Centralized definitions for all system settings keys.
 * 
 */

#ifndef AKIRA_SYSTEM_SETTINGS_H
#define AKIRA_SYSTEM_SETTINGS_H

/* ------------------------------------------------------------------ */
/* System                                                               */
/* ------------------------------------------------------------------ */
#define AKIRA_SETTINGS_WIFI_SSID_KEY    "system/wifi/ssid"
#define AKIRA_SETTINGS_WIFI_PSK_KEY     "system/wifi/psk"
/* Combined credential key: "ssid\tpsk" — one NVS write guarantees atomicity */
#define AKIRA_SETTINGS_WIFI_CREDS_KEY   "system/wifi/creds"
#define AKIRA_SETTINGS_TIME_BASE_KEY    "system/time_base"

#endif /* AKIRA_SYSTEM_SETTINGS_H */
