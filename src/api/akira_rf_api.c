/**
 * @file akira_rf_api.c
 * @brief RF API implementation for WASM exports
 */

#include "akira_api.h"
#include "akira_rf_api.h"
#include <runtime/security.h>
#include <zephyr/logging/log.h>
#include "../drivers/rf/rf_framework.h"
#include "../drivers/rf/lr1121.h"

LOG_MODULE_REGISTER(akira_rf_api, LOG_LEVEL_INF);

static akira_rf_chip_t g_active_chip = AKIRA_RF_CHIP_NONE;
static bool rf_framework_initialized = false;
static const struct akira_rf_driver *g_active_driver = NULL;

/* Ensure RF framework is initialized */
static int ensure_rf_framework(void)
{
    if (rf_framework_initialized) {
        return 0;
    }

    int ret = rf_framework_init();
    if (ret < 0) {
        LOG_ERR("RF framework init failed: %d", ret);
        return ret;
    }

    rf_framework_initialized = true;
    return 0;
}

/* Core RF API functions (no security checks) */

int akira_rf_init(akira_rf_chip_t chip)
{
    LOG_INF("RF init: chip=%d", chip);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    int ret;

    /* Ensure framework is initialized */
    ret = ensure_rf_framework();
    if (ret < 0) {
        return ret;
    }

    /* Map chip enum to RF framework type */
    rf_chip_type_t rf_type;
    switch (chip) {
        case AKIRA_RF_CHIP_LR1121:
            rf_type = RF_CHIP_LR1121;
            break;
        case AKIRA_RF_CHIP_CC1101:
            rf_type = RF_CHIP_CC1101;
            break;
        case AKIRA_RF_CHIP_NRF24L01:
            rf_type = RF_CHIP_NRF24L01;
            break;
        default:
            LOG_ERR("Unsupported chip type: %d", chip);
            return -EINVAL;
    }

    /* Get driver from framework */
    const struct akira_rf_driver *driver = rf_framework_get_driver(rf_type);
    if (!driver) {
        LOG_ERR("Driver not found for chip type %d", rf_type);
        return -ENODEV;
    }

    /* Initialize the driver */
    ret = driver->init();
    if (ret < 0) {
        LOG_ERR("Driver init failed: %d", ret);
        return ret;
    }

    /* Store active driver and chip */
    g_active_driver = driver;
    g_active_chip = chip;
    rf_framework_set_active_driver(rf_type);

    LOG_INF("RF driver '%s' initialized successfully", driver->name);
    return 0;
#else
    (void)chip;
    return -ENOSYS;
#endif
}

int akira_rf_deinit(void)
{
    LOG_INF("RF deinit");

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (g_active_driver && g_active_driver->deinit) {
        g_active_driver->deinit();
    }
    g_active_driver = NULL;
    rf_framework_set_active_driver(RF_CHIP_NONE);
#endif

    g_active_chip = AKIRA_RF_CHIP_NONE;
    return 0;
}

int akira_rf_send(const uint8_t *data, size_t len)
{
    if (g_active_chip == AKIRA_RF_CHIP_NONE || !g_active_driver)
    {
        LOG_ERR("RF not initialized");
        return -ENODEV;
    }

    if (!data || len == 0)
    {
        return -EINVAL;
    }

    LOG_DBG("RF send: %zu bytes", len);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (!g_active_driver->tx) {
        return -ENOSYS;
    }
    return g_active_driver->tx(data, len);
#else
    (void)data; (void)len;
    return -ENOSYS;
#endif
}

int akira_rf_receive(uint8_t *buffer, size_t max_len, uint32_t timeout_ms)
{
    if (g_active_chip == AKIRA_RF_CHIP_NONE || !g_active_driver)
    {
        LOG_ERR("RF not initialized");
        return -ENODEV;
    }

    if (!buffer || max_len == 0)
    {
        return -EINVAL;
    }

    LOG_DBG("RF receive: max=%zu, timeout=%u", max_len, timeout_ms);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (!g_active_driver->rx) {
        return -ENOSYS;
    }
    return g_active_driver->rx(buffer, max_len, timeout_ms);
#else
    (void)buffer; (void)max_len; (void)timeout_ms;
    return -ENOSYS;
#endif
}

int akira_rf_set_frequency(uint32_t freq_hz)
{
    LOG_INF("RF set frequency: %u Hz", freq_hz);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (!g_active_driver || !g_active_driver->set_frequency) {
        return -ENOSYS;
    }
    return g_active_driver->set_frequency(freq_hz);
#else
    (void)freq_hz;
    return -ENOSYS;
#endif
}

int akira_rf_set_power(int8_t dbm)
{
    LOG_INF("RF set power: %d dBm", dbm);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (!g_active_driver || !g_active_driver->set_power) {
        return -ENOSYS;
    }
    return g_active_driver->set_power(dbm);
#else
    (void)dbm;
    return -ENOSYS;
#endif
}

int akira_rf_get_rssi(int16_t *rssi)
{
    if (!rssi)
        return -EINVAL;

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    if (!g_active_driver || !g_active_driver->get_rssi) {
        *rssi = -100;
        return -ENOSYS;
    }
    return g_active_driver->get_rssi(rssi);
#else
    *rssi = -100;
    return -ENOSYS;
#endif
}

#ifdef CONFIG_AKIRA_WASM_RUNTIME

/* WASM Native export API */

int akira_native_rf_send(wasm_exec_env_t exec_env, uint32_t payload_ptr, uint32_t len)
{
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst)
        return -1;

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_RF_TRANSCEIVE, -EPERM);

    if (len == 0)
        return -1;

    uint8_t *ptr = (uint8_t *)wasm_runtime_addr_app_to_native(module_inst, payload_ptr);
    if (!ptr)
        return -1;

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    /* The API layer will dispatch to the proper radio driver */
    return akira_rf_send(ptr, len);
#else
    (void)ptr; (void)len;
    return -ENOSYS;
#endif
}

int akira_native_rf_receive(wasm_exec_env_t exec_env, uint32_t buffer_ptr, uint32_t max_len, uint32_t timeout_ms)
{
    wasm_module_inst_t module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!module_inst)
        return -1;

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_RF_TRANSCEIVE, -EPERM);

    if (max_len == 0)
        return -1;

    uint8_t *ptr = (uint8_t *)wasm_runtime_addr_app_to_native(module_inst, buffer_ptr);
    if (!ptr)
        return -1;

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    /* The API layer will dispatch to the proper radio driver */
    return akira_rf_receive(ptr, max_len, timeout_ms);
#else
    (void)ptr; (void)max_len; (void)timeout_ms;
    return -ENOSYS;
#endif
}

int akira_native_rf_set_frequency(wasm_exec_env_t exec_env, uint32_t freq_hz)
{

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_RF_TRANSCEIVE, -EPERM);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    return akira_rf_set_frequency(freq_hz);
#else
    (void)freq_hz;
    return -ENOSYS;
#endif
}

int akira_native_rf_get_rssi(wasm_exec_env_t exec_env, int16_t *rssi)
{

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_RF_TRANSCEIVE, -EPERM);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    return akira_rf_get_rssi(rssi);
#else
    (void)rssi;
    return -ENOSYS;
#endif
}

int akira_native_rf_set_power(wasm_exec_env_t exec_env, int8_t dbm)
{

    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_RF_TRANSCEIVE, -EPERM);

#ifdef CONFIG_AKIRA_RF_FRAMEWORK
    return akira_rf_set_power(dbm);
#else
    (void)dbm;
    return -ENOSYS;
#endif
}

#endif /* CONFIG_AKIRA_WASM_RUNTIME */