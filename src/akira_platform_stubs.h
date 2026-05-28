/**
 * @file akira_platform_stubs.h
 * @brief AkiraPlatform extension hook declarations.
 *
 * Weak no-ops called by AkiraOS at key lifecycle points.
 * AkiraPlatform (or any other overlay) provides strong overrides at link time.
 * AkiraOS builds and runs correctly when no override is present.
 *
 * @copyright Copyright (c) 2026 PenEngineering S.R.L
 */

#ifndef AKIRA_PLATFORM_STUBS_H
#define AKIRA_PLATFORM_STUBS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Called when a WASM application is successfully installed.
 *
 * Default: no-op. AkiraPlatform overrides to notify the hub.
 *
 * @param name  App name string (NUL-terminated).
 * @param id    Numeric app ID assigned by the app manager.
 */
void akira_on_app_installed(const char *name, int id);

/**
 * @brief Called when a WASM application is successfully uninstalled.
 *
 * Default: no-op. AkiraPlatform overrides to notify the hub.
 *
 * @param name  App name string (NUL-terminated).
 */
void akira_on_app_uninstalled(const char *name);

/**
 * @brief Called when a WASM application exits with a non-zero exit code
 *         (runtime exception, trap, or unhandled error).
 *
 * Default: no-op. AkiraPlatform overrides to forward the event to the hub.
 *
 * @param name       App name string (NUL-terminated).
 * @param exit_code  Non-zero exit code from the WASM runtime.
 */
void akira_on_app_crashed(const char *name, int exit_code);

/**
 * @brief Called when WiFi has an IPv4 address (DHCP done, DNS ready).
 *
 * Default: no-op. AkiraPlatform overrides to register/connect the hub.
 */
void akira_on_wifi_connected(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_PLATFORM_STUBS_H */
