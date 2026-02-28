/**
 * @file akira_bt_shell_api.c
 * @brief BT Shell API implementation for WASM exports
 */

#include "akira_api.h"
#include "akira_bt_shell_api.h"
#include <runtime/security.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "runtime/security.h"


#if defined(CONFIG_AKIRA_BT_SHELL)
#include "bt_shell.h"
#endif

LOG_MODULE_REGISTER(akira_bt_shell_api, LOG_LEVEL_INF);

/* Core BT Shell API functions (no security checks) */

int akira_bt_shell_send(const char *message)
{
    if (!message)
    {
        return -EINVAL;
    }

#if defined(CONFIG_AKIRA_BT_SHELL)
    return bt_shell_send_command(message);
#else
    (void)message;
    return -ENOSYS;
#endif
}

int akira_bt_shell_send_data(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
    {
        return -EINVAL;
    }

#if defined(CONFIG_AKIRA_BT_SHELL)
    return bt_shell_send_data(data, len);
#else
    (void)data;
    (void)len;
    return -ENOSYS;
#endif
}

int akira_bt_shell_is_ready(void)
{
#if defined(CONFIG_AKIRA_BT_SHELL)
    return bt_shell_notifications_enabled() ? 1 : 0;
#else
    return 0;
#endif
}

int akira_bt_shell_recv(uint8_t *buf, size_t len, int32_t timeout_ms)
{
    if (!buf || len == 0) {
        return -EINVAL;
    }

#if defined(CONFIG_AKIRA_BT_SHELL)
    k_timeout_t to = (timeout_ms < 0) ? K_FOREVER :
                     (timeout_ms == 0)  ? K_NO_WAIT  :
                                          K_MSEC(timeout_ms);
    return bt_shell_recv(buf, len, to);
#else
    ARG_UNUSED(timeout_ms);
    return -ENOSYS;
#endif
}

/* WASM Native export API */

int akira_native_bt_shell_send(wasm_exec_env_t exec_env, const char *message)
{
    /* Use inline capability check for <60ns overhead */
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_BT_SHELL, -EPERM);

    return akira_bt_shell_send(message);
}

int akira_native_bt_shell_send_data(wasm_exec_env_t exec_env, uint32_t data_ptr, uint32_t len)
{
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst)
        return -1;

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_BT_SHELL, -EPERM);

    if (len == 0)
        return -EINVAL;

    uint8_t *ptr = (uint8_t *)wasm_runtime_addr_app_to_native(module_inst, data_ptr);
    if (!ptr)
        return -EFAULT;

    return akira_bt_shell_send_data(ptr, len);
}

int akira_native_bt_shell_is_ready(wasm_exec_env_t exec_env)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_BT_SHELL, -EPERM);

    return akira_bt_shell_is_ready();
}

int akira_native_bt_shell_recv(wasm_exec_env_t exec_env,
                               uint32_t buf_ptr, uint32_t len,
                               int32_t timeout_ms)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_BT_SHELL, -EPERM);

    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst) {
        return -EINVAL;
    }

    if (len == 0) {
        return -EINVAL;
    }

    uint8_t *ptr = (uint8_t *)wasm_runtime_addr_app_to_native(module_inst, buf_ptr);
    if (!ptr) {
        return -EFAULT;
    }

    return akira_bt_shell_recv(ptr, len, timeout_ms);
}