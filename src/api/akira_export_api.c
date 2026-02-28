#include "akira_api.h"
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

        #ifdef CONFIG_AKIRA_WASM_GPIO
        {"gpio_configure", (void *)akira_native_gpio_configure, "(ii)i", NULL},
        {"gpio_read", (void *)akira_native_gpio_read, "(i)i", NULL},
        {"gpio_write", (void *)akira_native_gpio_write, "(ii)i", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_RF
        {"rf_set_frequency", (void *)akira_native_rf_set_frequency, "(i)i", NULL},
        {"rf_set_power", (void *)akira_native_rf_set_power, "(i)i", NULL},
        {"rf_get_rssi", (void *)akira_native_rf_get_rssi, "(i)i", NULL},
        {"rf_send", (void *)akira_native_rf_send, "(*i)i", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_SENSOR
        {"sensor_read", (void *)akira_native_sensor_read, "(i)i", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_MEMORY
        {"mem_alloc", (void *)akira_native_mem_alloc, "(i)i", NULL},
        {"mem_free", (void *)akira_native_mem_free, "(i)", NULL},
        #endif

        #ifdef CONFIG_AKIRA_WASM_BT_SHELL
        {"bt_shell_print", (void *)akira_native_bt_shell_send, "(i$)i", NULL},
        {"bt_shell_send_data", (void *)akira_native_bt_shell_send_data, "(i*i)i", NULL},
        {"bt_shell_is_ready", (void *)akira_native_bt_shell_is_ready, "()i", NULL},
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
    };

    return wasm_runtime_register_natives("env", native_syms, sizeof(native_syms) / sizeof(NativeSymbol));
}