/*
 * Copyright (c) 2025 AkiraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */

/**
 * @file akira_lifecycle_api.h
 * @brief App Lifecycle WASM API — start, stop, restart, status, list
 *
 * Allows a trusted WASM app to control the lifecycle of other installed apps.
 * Requires AKIRA_CAP_APP_CONTROL — elevated privilege; must not be granted
 * to untrusted apps.
 *
 * Manifest entry:  "capabilities": ["app.control"]
 */

#ifndef AKIRA_LIFECYCLE_API_H
#define AKIRA_LIFECYCLE_API_H

#include <stdint.h>
#include <wasm_export.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start an installed app by name
 * @param exec_env  WASM execution environment (caller context)
 * @param name      Target app name (null-terminated)
 * @return 0 on success, -EINVAL if name == self, -ENOENT if not found
 */
int akira_native_app_start(wasm_exec_env_t exec_env, const char *name);

/**
 * @brief Stop a running app by name
 * @param exec_env  WASM execution environment
 * @param name      Target app name
 * @return 0 on success, negative on error
 */
int akira_native_app_stop(wasm_exec_env_t exec_env, const char *name);

/**
 * @brief Restart an app (stop + start, resets crash counter)
 * @param exec_env  WASM execution environment
 * @param name      Target app name
 * @return 0 on success, negative on error
 */
int akira_native_app_restart(wasm_exec_env_t exec_env, const char *name);

/**
 * @brief Get the state of an app as an integer
 * @param exec_env  WASM execution environment
 * @param name      Target app name
 * @return app_state_t value (0-5), negative on error
 *   0=NEW, 1=INSTALLED, 2=RUNNING, 3=STOPPED, 4=ERROR, 5=FAILED
 */
int akira_native_app_get_status(wasm_exec_env_t exec_env, const char *name);

/**
 * @brief List all installed apps into a WASM buffer
 *
 * Writes newline-separated "name:state" entries into the WASM buffer.
 * Example output: "hello_world:RUNNING\ntetris:INSTALLED\n"
 *
 * @param exec_env  WASM execution environment
 * @param buf_ptr   WASM linear-memory pointer to output buffer
 * @param buf_len   Length of the output buffer
 * @return Number of apps written, negative on error
 */
int akira_native_app_list(wasm_exec_env_t exec_env,
                          uint32_t buf_ptr, uint32_t buf_len);

/**
 * @brief Get the calling app's own name
 * @param exec_env  WASM execution environment
 * @param buf_ptr   WASM linear-memory pointer to output buffer
 * @param buf_len   Buffer length (must be >= 32)
 * @return Length of the name string, negative on error
 */
int akira_native_app_get_self_name(wasm_exec_env_t exec_env,
                                   uint32_t buf_ptr, uint32_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_LIFECYCLE_API_H */
