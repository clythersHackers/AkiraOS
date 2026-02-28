#include "akira_api.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <runtime/security.h>

LOG_MODULE_REGISTER(akira_common_api, CONFIG_LOG_DEFAULT_LEVEL);

#ifdef CONFIG_AKIRA_WASM_RUNTIME
int akira_native_printf(wasm_exec_env_t exec_env, char *message)
{
    if (!message) {
        return -EINVAL;
    }

    LOG_INF("%s", message);
    return 0;
}

int akira_native_delay(wasm_exec_env_t exec_env, uint32_t microseconds)
{
    /* Note: delay() does not require capability checks - basic timing should always be allowed */
    k_usleep(microseconds);
    return 0;
}

#endif
