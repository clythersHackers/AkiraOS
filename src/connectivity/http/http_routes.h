/**
 * @file http_routes.h
 * @brief AkiraOS HTTP API Route Registration
 *
 * Registers all REST API routes onto the HTTP server:
 *   POST   /upload                  OTA firmware flash
 *   POST   /api/apps/install        WASM app install (streaming)
 *   GET    /api/v1/info             Device info (name, fw version, chip)
 *   GET    /api/v1/status           Device status (storage, RAM, uptime)
 *   GET    /api/v1/apps             Installed app list
 *   GET    /api/v1/logs             Recent log entries
 *   POST   /api/v1/apps/start       Start an app  (?name=)
 *   POST   /api/v1/apps/stop        Stop an app   (?name=)
 *   DELETE /api/v1/apps             Uninstall an app (?name=)
 *
 * @stability experimental
 * @since 1.5
 */

#ifndef AKIRA_HTTP_ROUTES_H
#define AKIRA_HTTP_ROUTES_H

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief Register all AkiraOS HTTP API routes
     *
     * Must be called after akira_http_server_init() and before
     * akira_http_server_start().
     *
     * @return 0 on success, negative errno on failure
     */
    int akira_http_routes_init(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_HTTP_ROUTES_H */
