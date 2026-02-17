/**
 * @file shell_display_cmds.c
 * @brief Shell commands for display control and testing
 */

#include <zephyr/shell/shell.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <stdlib.h>
#include "../api/akira_api.h"
#include "../drivers/platform_hal.h"

/* RGB565 color definitions */
#define COLOR_BLACK   0x0000
#define COLOR_WHITE   0xFFFF
#define COLOR_RED     0xF800
#define COLOR_GREEN   0x07E0
#define COLOR_BLUE    0x001F
#define COLOR_YELLOW  0xFFE0
#define COLOR_CYAN    0x07FF
#define COLOR_MAGENTA 0xF81F

/* Helper to parse hex color */
static uint16_t parse_color(const char *str)
{
    if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) {
        return (uint16_t)strtoul(str, NULL, 16);
    }
    
    /* Named colors */
    if (strcmp(str, "black") == 0) return COLOR_BLACK;
    if (strcmp(str, "white") == 0) return COLOR_WHITE;
    if (strcmp(str, "red") == 0) return COLOR_RED;
    if (strcmp(str, "green") == 0) return COLOR_GREEN;
    if (strcmp(str, "blue") == 0) return COLOR_BLUE;
    if (strcmp(str, "yellow") == 0) return COLOR_YELLOW;
    if (strcmp(str, "cyan") == 0) return COLOR_CYAN;
    if (strcmp(str, "magenta") == 0) return COLOR_MAGENTA;
    
    return (uint16_t)strtoul(str, NULL, 0);
}

/* display info */
static int cmd_display_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    struct display_capabilities caps;
    int ret = akira_display_hal_get_capabilities(&caps);
    
    if (ret < 0) {
        shell_error(sh, "Failed to get display capabilities: %d", ret);
        return ret;
    }

    shell_print(sh, "Display Information:");
    shell_print(sh, "  Resolution: %dx%d", caps.x_resolution, caps.y_resolution);
    shell_print(sh, "  Pixel format: %d", caps.current_pixel_format);
    shell_print(sh, "  Orientation: %d", caps.current_orientation);
    shell_print(sh, "  Screen info: 0x%02x", caps.screen_info);
    
    uint16_t *fb = akira_framebuffer_get();
    if (fb) {
        shell_print(sh, "  Framebuffer: %p", fb);
        shell_print(sh, "  Size: %d bytes", caps.x_resolution * caps.y_resolution * 2);
    } else {
        shell_print(sh, "  Framebuffer: not available");
    }

    return 0;
}

/* display clear <color> */
static int cmd_display_clear(const struct shell *sh, size_t argc, char **argv)
{
    uint16_t color = COLOR_BLACK;
    
    if (argc > 1) {
        color = parse_color(argv[1]);
    }

    akira_display_clear(color);
    akira_display_flush();
    
    shell_print(sh, "Display cleared to 0x%04X", color);
    return 0;
}

/* display fill <x> <y> <w> <h> <color> */
static int cmd_display_fill(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 6) {
        shell_error(sh, "Usage: display fill <x> <y> <w> <h> <color>");
        return -EINVAL;
    }

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    int w = atoi(argv[3]);
    int h = atoi(argv[4]);
    uint16_t color = parse_color(argv[5]);

    akira_display_rect(x, y, w, h, color);
    akira_display_flush();

    shell_print(sh, "Filled rectangle at (%d,%d) %dx%d with 0x%04X", x, y, w, h, color);
    return 0;
}

/* display pixel <x> <y> <color> */
static int cmd_display_pixel(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_error(sh, "Usage: display pixel <x> <y> <color>");
        return -EINVAL;
    }

    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    uint16_t color = parse_color(argv[3]);

    akira_display_pixel(x, y, color);
    akira_display_flush();

    shell_print(sh, "Set pixel (%d,%d) to 0x%04X", x, y, color);
    return 0;
}

/* display test */
static int cmd_display_test(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    /* Get display dimensions */
    struct display_capabilities caps;
    int ret = akira_display_hal_get_capabilities(&caps);
    if (ret < 0) {
        shell_error(sh, "Failed to get display capabilities");
        return ret;
    }

    shell_print(sh, "Running display test pattern...");

    /* Full screen colors */
    const uint16_t colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_WHITE, COLOR_BLACK};
    const char *names[] = {"RED", "GREEN", "BLUE", "WHITE", "BLACK"};
    
    for (int i = 0; i < 5; i++) {
        shell_print(sh, "  %s", names[i]);
        akira_display_clear(colors[i]);
        akira_display_flush();
        k_msleep(500);
    }

    /* Color bars */
    shell_print(sh, "  Color bars");
    int bar_height = caps.y_resolution / 8;
    const uint16_t bar_colors[] = {
        COLOR_WHITE, COLOR_YELLOW, COLOR_CYAN, COLOR_GREEN,
        COLOR_MAGENTA, COLOR_RED, COLOR_BLUE, COLOR_BLACK
    };
    
    for (int i = 0; i < 8; i++) {
        akira_display_rect(0, i * bar_height, caps.x_resolution, bar_height, bar_colors[i]);
    }
    akira_display_flush();
    k_msleep(1000);

    /* Checkerboard */
    shell_print(sh, "  Checkerboard");
    for (int y = 0; y < caps.y_resolution; y += 20) {
        for (int x = 0; x < caps.x_resolution; x += 20) {
            uint16_t color = ((x / 20 + y / 20) % 2) ? COLOR_WHITE : COLOR_BLACK;
            akira_display_rect(x, y, 20, 20, color);
        }
    }
    akira_display_flush();
    k_msleep(1000);

    /* Clear to black */
    akira_display_clear(COLOR_BLACK);
    akira_display_flush();

    shell_print(sh, "Test complete");
    return 0;
}

/* display flush */
static int cmd_display_flush(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    akira_display_flush();
    shell_print(sh, "Framebuffer flushed to display");
    return 0;
}

/* display brightness <0-100> */
static int cmd_display_brightness(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: display brightness <0-100>");
        return -EINVAL;
    }

    int brightness = atoi(argv[1]);
    if (brightness < 0 || brightness > 100) {
        shell_error(sh, "Brightness must be 0-100");
        return -EINVAL;
    }

    akira_display_hal_set_brightness((uint8_t)brightness);
    shell_print(sh, "Brightness set to %d%%", brightness);
    return 0;
}

/* display blanking <on|off> */
static int cmd_display_blanking(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: display blanking <on|off>");
        return -EINVAL;
    }

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display))
    const struct device *dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    
    if (!device_is_ready(dev)) {
        shell_error(sh, "Display not ready");
        return -ENODEV;
    }

    int ret;
    if (strcmp(argv[1], "on") == 0) {
        ret = display_blanking_on(dev);
        shell_print(sh, "Display blanking enabled (screen off)");
    } else if (strcmp(argv[1], "off") == 0) {
        ret = display_blanking_off(dev);
        shell_print(sh, "Display blanking disabled (screen on)");
    } else {
        shell_error(sh, "Invalid argument. Use 'on' or 'off'");
        return -EINVAL;
    }

    if (ret < 0) {
        shell_error(sh, "Blanking command failed: %d", ret);
        return ret;
    }
#else
    shell_error(sh, "No display configured");
#endif

    return 0;
}

/* display line <x0> <y0> <x1> <y1> <color> */
static int cmd_display_line(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 6) {
        shell_error(sh, "Usage: display line <x0> <y0> <x1> <y1> <color>");
        return -EINVAL;
    }

    int x0 = atoi(argv[1]);
    int y0 = atoi(argv[2]);
    int x1 = atoi(argv[3]);
    int y1 = atoi(argv[4]);
    uint16_t color = parse_color(argv[5]);

    /* Simple Bresenham's line algorithm */
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (1) {
        akira_display_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }

    akira_display_flush();
    shell_print(sh, "Drew line");
    return 0;
}

/* display rotate <rotation> - Set display rotation */
static int cmd_display_rotate(const struct shell *sh, size_t argc, char **argv)
{
    if (argc != 2) {
        shell_error(sh, "Usage: display rotate <0|1|2|3>");
        shell_print(sh, "  0 = 0° (Portrait)");
        shell_print(sh, "  1 = 90° (Landscape)");
        shell_print(sh, "  2 = 180° (Portrait inverted)");
        shell_print(sh, "  3 = 270° (Landscape inverted)");
        return -EINVAL;
    }

    int rotation = atoi(argv[1]);
    if (rotation < 0 || rotation > 3) {
        shell_error(sh, "Invalid rotation: %d (must be 0-3)", rotation);
        return -EINVAL;
    }

    int ret = akira_display_hal_set_rotation((uint8_t)rotation);
    if (ret == -ENOTSUP) {
        shell_warn(sh, "Runtime rotation not supported");
        shell_print(sh, "To change rotation, edit 'mdac' in device tree:");
        shell_print(sh, "  0° (Portrait):     mdac = <0x08>;");
        shell_print(sh, "  90° (Landscape):   mdac = <0x68>;");
        shell_print(sh, "  180° (Inverted):   mdac = <0xC8>;");
        shell_print(sh, "  270° (Landscape2): mdac = <0xA8>;");
        return 0;
    } else if (ret < 0) {
        shell_error(sh, "Rotation failed: %d", ret);
        return ret;
    }

    shell_print(sh, "Display rotated to %d degrees", rotation * 90);
    return 0;
}

/* Subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_display,
    SHELL_CMD_ARG(info, NULL, "Show display information", cmd_display_info, 1, 0),
    SHELL_CMD_ARG(clear, NULL, "Clear display [color]", cmd_display_clear, 1, 1),
    SHELL_CMD_ARG(fill, NULL, "Fill rectangle <x> <y> <w> <h> <color>", cmd_display_fill, 6, 0),
    SHELL_CMD_ARG(pixel, NULL, "Draw pixel <x> <y> <color>", cmd_display_pixel, 4, 0),
    SHELL_CMD_ARG(line, NULL, "Draw line <x0> <y0> <x1> <y1> <color>", cmd_display_line, 6, 0),
    SHELL_CMD_ARG(test, NULL, "Run display test pattern", cmd_display_test, 1, 0),
    SHELL_CMD_ARG(flush, NULL, "Flush framebuffer to display", cmd_display_flush, 1, 0),
    SHELL_CMD_ARG(brightness, NULL, "Set brightness <0-100>", cmd_display_brightness, 2, 0),
    SHELL_CMD_ARG(blanking, NULL, "Control blanking <on|off>", cmd_display_blanking, 2, 0),
    SHELL_CMD_ARG(rotate, NULL, "Set rotation <0|1|2|3> (0/90/180/270°)", cmd_display_rotate, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(display, &sub_display, "Display control commands", NULL);
