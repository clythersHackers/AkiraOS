#include "akira_api.h"
#include "akira_storage_api.h"
#include <runtime/akira_runtime.h>
#include <runtime/security.h>
#include <zephyr/logging/log.h>
#include <stdbool.h>

#include <wasm_export.h>

bool akira_register_native_apis()
{   
    static NativeSymbol native_syms[] = {
        #ifdef CONFIG_AKIRA_WASM_API
        {"printf", (void *)akira_native_printf, "($)i", NULL},
        {"delay", (void *)akira_native_delay, "(i)i", NULL},
        #endif

        #ifdef CONFIG_DISPLAY
        {"display_rect",                (void *)akira_native_display_rect,                "(iiiii)i",  NULL},
        {"display_text",                (void *)akira_native_display_text,                "(ii$i)i",   NULL},
        {"display_text_large",          (void *)akira_native_display_text_large,          "(ii$i)i",   NULL},
        {"display_clear",               (void *)akira_native_display_clear,               "(i)i",      NULL},
        {"display_pixel",               (void *)akira_native_display_pixel,               "(iii)i",    NULL},
        {"display_flush",               (void *)akira_native_display_flush,               "()i",       NULL},
        {"display_get_size",            (void *)akira_native_display_get_size,            "(**)i",     NULL},
        {"display_line",                (void *)akira_native_display_line,                "(iiiii)i",  NULL},
        {"display_circle",              (void *)akira_native_display_circle,              "(iiii)i",   NULL},
        {"display_circle_fill",         (void *)akira_native_display_circle_fill,         "(iiii)i",   NULL},
        {"display_triangle",            (void *)akira_native_display_triangle,            "(iiiiiii)i",NULL},
        {"display_triangle_fill",       (void *)akira_native_display_triangle_fill,       "(iiiiiii)i",NULL},
        {"display_rect_outline",        (void *)akira_native_display_rect_outline,        "(iiiii)i",  NULL},
        {"display_bitmap",              (void *)akira_native_display_bitmap,              "(iiii*~)i", NULL},
        {"display_bitmap_transparent",  (void *)akira_native_display_bitmap_transparent,  "(iiii*~i)i",NULL},
        /* Phase 4 — UI helper primitives */
        {"display_hline",               (void *)akira_native_display_hline,               "(iiii)i",   NULL},
        {"display_vline",               (void *)akira_native_display_vline,               "(iiii)i",   NULL},
        {"display_number",              (void *)akira_native_display_number,              "(iiii)i",   NULL},
        {"display_progress_bar",        (void *)akira_native_display_progress_bar,        "(iiiiiiii)i",NULL},
        {"display_rounded_rect",        (void *)akira_native_display_rounded_rect,        "(iiiiii)i", NULL},
        {"display_rounded_rect_fill",   (void *)akira_native_display_rounded_rect_fill,   "(iiiiii)i", NULL},
        #endif

        #if defined(CONFIG_AKIRA_WASM_API) && defined(CONFIG_GPIO)
        {"gpio_configure", (void *)akira_native_gpio_configure, "(ii)i", NULL},
        {"gpio_read", (void *)akira_native_gpio_read, "(i)i", NULL},
        {"gpio_write", (void *)akira_native_gpio_write, "(ii)i", NULL},
        #endif

        #if defined(CONFIG_AKIRA_WASM_API) && defined(CONFIG_AKIRA_MODULE_RF) && defined(CONFIG_AKIRA_RF_FRAMEWORK)
        {"rf_set_frequency", (void *)akira_native_rf_set_frequency, "(i)i", NULL},
        {"rf_set_power", (void *)akira_native_rf_set_power, "(i)i", NULL},
        {"rf_get_rssi", (void *)akira_native_rf_get_rssi, "(i)i", NULL},
        {"rf_send", (void *)akira_native_rf_send, "(*i)i", NULL},
        #endif

        #if defined(CONFIG_AKIRA_WASM_API) && defined(CONFIG_SENSOR)
        {"sensor_read", (void *)akira_native_sensor_read, "(i)i", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_MEMORY
        {"mem_alloc", (void *)akira_native_mem_alloc, "(i)i", NULL},
        {"mem_free", (void *)akira_native_mem_free, "(i)", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_BLE
        {"ble_init",                 (void *)akira_native_ble_init,                 "()i",     NULL},
        {"ble_deinit",               (void *)akira_native_ble_deinit,               "()i",     NULL},
        {"ble_set_local_name",       (void *)akira_native_ble_set_local_name,       "($)i",    NULL},
        {"ble_service_create",       (void *)akira_native_ble_service_create,       "($)i",    NULL},
        {"ble_char_create",          (void *)akira_native_ble_char_create,          "($ii)i",  NULL},
        {"ble_service_add_char",     (void *)akira_native_ble_service_add_char,     "(ii)i",   NULL},
        {"ble_add_service",          (void *)akira_native_ble_add_service,          "(i)i",    NULL},
        {"ble_set_advertised_service",(void *)akira_native_ble_set_advertised_service,"(i)i", NULL},
        {"ble_advertise",            (void *)akira_native_ble_advertise,            "()i",     NULL},
        {"ble_stop_advertise",       (void *)akira_native_ble_stop_advertise,       "()i",     NULL},
        {"ble_is_connected",         (void *)akira_native_ble_is_connected,         "()i",     NULL},
        {"ble_char_write",           (void *)akira_native_ble_char_write,           "(i*~)i",  NULL},
        {"ble_char_read",            (void *)akira_native_ble_char_read,            "(i*~)i",  NULL},
        {"ble_event_pop",            (void *)akira_native_ble_event_pop,            "(*~)i",   NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_HID
        {"hid_enable",             (void *)akira_native_hid_enable,             "()i",    NULL},
        {"hid_disable",            (void *)akira_native_hid_disable,            "()i",    NULL},
        {"hid_is_connected",       (void *)akira_native_hid_is_connected,       "()i",    NULL},
        {"hid_key_press",          (void *)akira_native_hid_key_press,          "(i)i",   NULL},
        {"hid_key_release",        (void *)akira_native_hid_key_release,        "(i)i",   NULL},
        {"hid_key_release_all",    (void *)akira_native_hid_key_release_all,    "()i",    NULL},
        {"hid_type_string",        (void *)akira_native_hid_type_string,        "($)i",   NULL},
        {"hid_gamepad_press",      (void *)akira_native_hid_gamepad_press,      "(i)i",   NULL},
        {"hid_gamepad_release",    (void *)akira_native_hid_gamepad_release,    "(i)i",   NULL},
        {"hid_gamepad_set_axis",   (void *)akira_native_hid_gamepad_set_axis,   "(ii)i",  NULL},
        {"hid_gamepad_set_dpad",   (void *)akira_native_hid_gamepad_set_dpad,   "(i)i",   NULL},
        {"hid_gamepad_reset",      (void *)akira_native_hid_gamepad_reset,      "()i",    NULL},
        {"hid_mouse_move",         (void *)akira_native_hid_mouse_move,         "(ii)i",  NULL},
        {"hid_mouse_btn_press",    (void *)akira_native_hid_mouse_btn_press,    "(i)i",   NULL},
        {"hid_mouse_btn_release",  (void *)akira_native_hid_mouse_btn_release,  "(i)i",   NULL},
        {"hid_mouse_scroll",       (void *)akira_native_hid_mouse_scroll,       "(i)i",   NULL},
        {"hid_consumer_send",      (void *)akira_native_hid_consumer_send,      "(i)i",   NULL},
        {"hid_send_raw_report",    (void *)akira_native_hid_send_raw_report,    "(i*~)i", NULL},
        {"hid_action_register",    (void *)akira_native_hid_action_register,    "($ii)i", NULL},
        {"hid_action_trigger",     (void *)akira_native_hid_action_trigger,     "($)i",   NULL},
        {"hid_set_transport",      (void *)akira_native_hid_set_transport,      "(i)i",   NULL},
        {"hid_set_device_types",   (void *)akira_native_hid_set_device_types,   "(i)i",   NULL},
        {"hid_init",               (void *)akira_native_hid_init,               "(ii)i",  NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_LIFECYCLE
        {"app_get_status",    (void *)akira_native_app_get_status,    "($)i",   NULL},
        {"app_list",          (void *)akira_native_app_list,          "(ii)i",  NULL},
        {"app_get_self_name", (void *)akira_native_app_get_self_name, "(ii)i",  NULL},
        /* Write-control — requires AKIRA_CAP_APP_CONTROL */
        {"app_start",         (void *)akira_native_app_start,         "($)i",   NULL},
        {"app_stop",          (void *)akira_native_app_stop,          "($)i",   NULL},
        /* Lightweight handoff — requires AKIRA_CAP_APP_SWITCH or APP_CONTROL */
        {"app_switch",        (void *)akira_native_app_switch,        "($)i",   NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_IPC
        {"msg_subscribe",   (void *)akira_native_msg_subscribe,   "($)i",    NULL},
        {"msg_unsubscribe", (void *)akira_native_msg_unsubscribe,  "($)i",    NULL},
        {"msg_publish",     (void *)akira_native_msg_publish,     "($*~)i",  NULL},
        {"msg_recv",        (void *)akira_native_msg_recv,        "($*~i)i", NULL},
        {"msg_try_recv",    (void *)akira_native_msg_try_recv,    "($*~)i",  NULL},
        {"msg_pending",     (void *)akira_native_msg_pending,     "($)i",    NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_TIMER
        {"timer_create",  (void *)akira_native_timer_create,  "()i",   NULL},
        {"timer_start",   (void *)akira_native_timer_start,   "(i)i",  NULL},
        {"timer_stop",    (void *)akira_native_timer_stop,    "(i)i",  NULL},
        {"timer_elapsed", (void *)akira_native_timer_elapsed, "(i)i",  NULL},
        {"timer_free",    (void *)akira_native_timer_free,    "(i)i",  NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_UART
        {"uart_open",  (void *)akira_native_uart_open,  "(ii)i",   NULL},
        {"uart_write", (void *)akira_native_uart_write, "(i*~)i",  NULL},
        {"uart_read",  (void *)akira_native_uart_read,  "(i*~)i",  NULL},
        {"uart_close", (void *)akira_native_uart_close, "(i)i",    NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_I2C
        {"i2c_write_reg", (void *)akira_native_i2c_write_reg, "(iii*~)i", NULL},
        {"i2c_read_reg",  (void *)akira_native_i2c_read_reg,  "(iii*~)i", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_PWM
        {"pwm_set",     (void *)akira_native_pwm_set,     "(iii)i", NULL},
        {"pwm_disable", (void *)akira_native_pwm_disable, "(i)i",   NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_STORAGE
        {"storage_open",   (void *)akira_native_storage_open,   "($i)i",   NULL},
        {"storage_read",   (void *)akira_native_storage_read,   "(i*~)i",  NULL},
        {"storage_write",  (void *)akira_native_storage_write,  "(i*~)i",  NULL},
        {"storage_close",  (void *)akira_native_storage_close,  "(i)v",    NULL},
        {"storage_delete", (void *)akira_native_storage_delete, "($)i",    NULL},
        {"storage_list",   (void *)akira_native_storage_list,   "($*~)i",  NULL},
        #endif

    };

    return wasm_runtime_register_natives("env", native_syms, sizeof(native_syms) / sizeof(NativeSymbol));
}