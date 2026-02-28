/*
 * Copyright (c) 2025 AkiraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */

#define LOG_MODULE_NAME akira_lifecycle_api
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(akira_lifecycle_api, CONFIG_AKIRA_LOG_LEVEL);

/**
 * @file akira_lifecycle_api.c
 * @brief App Lifecycle WASM native API
 *
 * Routes all calls through app_manager_* (never directly through
 * akira_runtime_*) so the persistent registry and crash counters
 * are always kept consistent.
 *
 * Self-start prevention: if the caller and the target share the same
 * name, the call is rejected with -EINVAL to prevent recursive instantiation.
 */

#include "akira_lifecycle_api.h"
#include <runtime/security.h>
#include <zephyr/kernel.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef CONFIG_AKIRA_APP_MANAGER
#include <runtime/app_manager/app_manager.h>
#endif

/* ── Internal helper ─────────────────────────────────────────────────────── */

/**
 * @brief Resolve the calling app's name from the exec environment.
 * @return 0 on success, negative on error (buf filled with empty string on error)
 */
static int get_caller_name(wasm_exec_env_t exec_env, char *buf, size_t buflen)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    if (!inst) {
        buf[0] = '\0';
        return -EINVAL;
    }
    return akira_runtime_get_name_for_module_inst(inst, buf, buflen);
}

/* ── WASM Exports ────────────────────────────────────────────────────────── */

int akira_native_app_start(wasm_exec_env_t exec_env, const char *name)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    if (!name || name[0] == '\0') {
        return -EINVAL;
    }

    /* Prevent self-start (recursive instantiation) */
    char self[APP_NAME_MAX_LEN] = {0};
    get_caller_name(exec_env, self, sizeof(self));
    if (self[0] != '\0' && strncmp(self, name, APP_NAME_MAX_LEN) == 0) {
        LOG_WRN("app_start: self-start blocked for '%s'", name);
        return -EINVAL;
    }

#ifdef CONFIG_AKIRA_APP_MANAGER
    int ret = app_manager_start(name);
    if (ret < 0) {
        LOG_ERR("app_start('%s') failed: %d", name, ret);
    }
    return ret;
#else
    return -ENOTSUP;
#endif
}

int akira_native_app_stop(wasm_exec_env_t exec_env, const char *name)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    if (!name || name[0] == '\0') {
        return -EINVAL;
    }

#ifdef CONFIG_AKIRA_APP_MANAGER
    int ret = app_manager_stop(name);
    if (ret < 0) {
        LOG_ERR("app_stop('%s') failed: %d", name, ret);
    }
    return ret;
#else
    return -ENOTSUP;
#endif
}

int akira_native_app_restart(wasm_exec_env_t exec_env, const char *name)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    if (!name || name[0] == '\0') {
        return -EINVAL;
    }

    /* Prevent self-restart */
    char self[APP_NAME_MAX_LEN] = {0};
    get_caller_name(exec_env, self, sizeof(self));
    if (self[0] != '\0' && strncmp(self, name, APP_NAME_MAX_LEN) == 0) {
        LOG_WRN("app_restart: self-restart blocked for '%s'", name);
        return -EINVAL;
    }

#ifdef CONFIG_AKIRA_APP_MANAGER
    int ret = app_manager_restart(name);
    if (ret < 0) {
        LOG_ERR("app_restart('%s') failed: %d", name, ret);
    }
    return ret;
#else
    return -ENOTSUP;
#endif
}

int akira_native_app_get_status(wasm_exec_env_t exec_env, const char *name)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    if (!name || name[0] == '\0') {
        return -EINVAL;
    }

#ifdef CONFIG_AKIRA_APP_MANAGER
    return (int)app_manager_get_state(name);
#else
    return -ENOTSUP;
#endif
}

int akira_native_app_list(wasm_exec_env_t exec_env,
                          uint32_t buf_ptr, uint32_t buf_len)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return -EINVAL;
    }

    if (buf_len == 0) {
        return -EINVAL;
    }

    char *wasm_buf = (char *)wasm_runtime_addr_app_to_native(module_inst, buf_ptr);
    if (!wasm_buf) {
        return -EFAULT;
    }

#ifdef CONFIG_AKIRA_APP_MANAGER
    /* Retrieve all apps into a local stack buffer — avoid PSRAM alloc here */
#define LIST_MAX 16
    app_info_t apps[LIST_MAX];
    int count = app_manager_list(apps, LIST_MAX);
    if (count < 0) {
        return count;
    }

    size_t pos = 0;
    for (int i = 0; i < count && pos < buf_len - 1; i++) {
        const char *state_str = app_state_to_str(apps[i].state);
        int n = snprintf(wasm_buf + pos, buf_len - pos,
                         "%s:%s\n", apps[i].name, state_str);
        if (n < 0 || (size_t)n >= buf_len - pos) {
            break;
        }
        pos += n;
    }
    wasm_buf[pos] = '\0';
    return count;
#else
    (void)wasm_buf;
    (void)buf_len;
    return -ENOTSUP;
#endif
}

int akira_native_app_get_self_name(wasm_exec_env_t exec_env,
                                   uint32_t buf_ptr, uint32_t buf_len)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_APP_CONTROL, -EPERM);

    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return -EINVAL;
    }

    if (buf_len < 2) {
        return -EINVAL;
    }

    char *wasm_buf = (char *)wasm_runtime_addr_app_to_native(module_inst, buf_ptr);
    if (!wasm_buf) {
        return -EFAULT;
    }

    char name[APP_NAME_MAX_LEN] = {0};
    int ret = akira_runtime_get_name_for_module_inst(module_inst, name, sizeof(name));
    if (ret < 0) {
        wasm_buf[0] = '\0';
        return ret;
    }

    size_t nlen = strnlen(name, sizeof(name));
    size_t copy = (nlen < buf_len - 1) ? nlen : buf_len - 1;
    memcpy(wasm_buf, name, copy);
    wasm_buf[copy] = '\0';
    return (int)copy;
}
