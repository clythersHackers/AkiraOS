/**
 * @file akira_display_api.c
 * @brief Display API implementation for WASM exports
 */
#include "akira_api.h"
#include <runtime/security.h>
#include <drivers/platform_hal.h>
#include "../drivers/display/fonts.h"
#include <zephyr/drivers/display.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(akira_display_api, LOG_LEVEL_INF);

/* Platform-agnostic display primitives. All hardware calls go through
 * platform_hal.h. No security checks here - only in native exports.
 */

void akira_display_clear(uint16_t color)
{
#if AKIRA_PLATFORM_NATIVE_SIM
    for (int y = 0; y < 320; y++)
        for (int x = 0; x < 240; x++)
            akira_sim_draw_pixel(x, y, color);
    akira_sim_show_display();
#else
    struct display_capabilities caps;
    if (akira_display_hal_get_capabilities(&caps) < 0) {
        LOG_ERR("Failed to get display capabilities");
        return;
    }
    
    uint16_t *fb = akira_framebuffer_get();
    if (fb)
    {
        int pixels = caps.x_resolution * caps.y_resolution;
        for (int i = 0; i < pixels; i++)
            fb[i] = color;
    }
    else
    {
        LOG_WRN("No display framebuffer available");
    }
#endif
}

void akira_display_pixel(int x, int y, uint16_t color)
{
#if AKIRA_PLATFORM_NATIVE_SIM
    if (x < 0 || x >= 240 || y < 0 || y >= 320)
        return;
    akira_sim_draw_pixel(x, y, color);
#else
    struct display_capabilities caps;
    if (akira_display_hal_get_capabilities(&caps) < 0)
        return;
    
    if (x < 0 || x >= caps.x_resolution || y < 0 || y >= caps.y_resolution)
        return;

    uint16_t *fb = akira_framebuffer_get();
    if (fb)
        fb[y * caps.x_resolution + x] = color;
#endif
}

void akira_display_rect(int x, int y, int w, int h, uint16_t color)
{
    if (w <= 0 || h <= 0)
        return;
    
    int x_end = x + w;
    int y_end = y + h;
    for (int py = y; py < y_end; py++)
    {
        for (int px = x; px < x_end; px++)
        {
            akira_display_pixel(px, py, color);
        }
    }
}

void akira_display_text(int x, int y, const char *text, uint16_t color)
{
    if (!text) return;
    draw_string(x, y, text, color, akira_display_pixel, FONT_7X10);
}

void akira_display_text_large(int x, int y, const char *text, uint16_t color)
{
    if (!text) return;
    draw_string(x, y, text, color, akira_display_pixel, FONT_11X18);
}

void akira_display_flush(void)
{
    //LOG_INF("akira_display_flush() called");
#if AKIRA_PLATFORM_NATIVE_SIM
    LOG_INF("Using sim display path");
    akira_sim_show_display();
#else
    //LOG_INF("Using hardware display path");
    akira_display_hal_flush();
#endif
    //LOG_INF("akira_display_flush() completed");
}

void akira_display_get_size(int *width, int *height)
{
#if AKIRA_PLATFORM_NATIVE_SIM
    if (width) *width = 240;
    if (height) *height = 320;
#else
    struct display_capabilities caps;
    if (akira_display_hal_get_capabilities(&caps) < 0) {
        LOG_ERR("Failed to get display capabilities");
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    
    if (width) *width = caps.x_resolution;
    if (height) *height = caps.y_resolution;
#endif
}

/* WASM Native export API */

#ifdef CONFIG_AKIRA_WASM_RUNTIME

int akira_native_display_rect(wasm_exec_env_t exec_env, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_DISPLAY_WRITE, -EPERM);
    
    akira_display_rect(x, y, w, h, (uint16_t)color);
    return 0;
}

int akira_native_display_text(wasm_exec_env_t exec_env, int32_t x, int32_t y, const char *text, uint32_t color)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_DISPLAY_WRITE, -EPERM);
    
    akira_display_text(x, y, text, (uint16_t)color);
    return 0;
}

int akira_native_display_text_large(wasm_exec_env_t exec_env, int x, int y, const char *text, uint16_t color)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_DISPLAY_WRITE, -EPERM);
    
    akira_display_text_large(x, y, text, color);
    return 0;
}

int akira_native_display_clear(wasm_exec_env_t exec_env, uint32_t color)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_DISPLAY_WRITE, -EPERM);
    
    akira_display_clear((uint16_t)color);
    return 0;
}

int akira_native_display_pixel(wasm_exec_env_t exec_env, int32_t x, int32_t y, uint32_t color)
{
    AKIRA_CHECK_CAP_OR_RETURN(exec_env, AKIRA_CAP_DISPLAY_WRITE, -EPERM);
    
    akira_display_pixel(x, y, (uint16_t)color);
    return 0;
}

#endif