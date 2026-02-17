/**
 * @file display_hal.c
 * @brief Hardware Display Abstraction Layer for Zephyr Display API
 *
 * Integrates Zephyr's display subsystem with Akira's framebuffer-based
 * display API. Provides a translation layer between the hardware-agnostic
 * akira_display_* API and the physical display hardware.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include "../platform_hal.h"

LOG_MODULE_REGISTER(display_hal, LOG_LEVEL_INF);

/* Display device handle */
static const struct device *display_dev = NULL;

/* Display capabilities */
static struct display_capabilities display_caps = {0};

/* Backlight GPIO - GPIO3 on ESP32-S3 */
#define BACKLIGHT_GPIO_NODE DT_NODELABEL(gpio0)
#define BACKLIGHT_GPIO_PIN  3

static const struct device *backlight_gpio_dev = NULL;

/* Display reset is handled by the ST7789V driver via device tree reset-gpios.
 * Do not manually control GPIO15 (RESET) - driver manages hardware reset timing. */

/**
 * @brief Initialize the hardware display
 * @return 0 on success, negative errno on error
 */
int akira_display_hal_init(void)
{
#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display))

    display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));

    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }

    /* Enable backlight */
    backlight_gpio_dev = DEVICE_DT_GET(BACKLIGHT_GPIO_NODE);
    if (device_is_ready(backlight_gpio_dev)) {
        gpio_pin_configure(backlight_gpio_dev, BACKLIGHT_GPIO_PIN, GPIO_OUTPUT_ACTIVE);
        gpio_pin_set(backlight_gpio_dev, BACKLIGHT_GPIO_PIN, 1);
    }

    /* Get display capabilities and store them */
    display_get_capabilities(display_dev, &display_caps);

    LOG_INF("Display ready: %dx%d", display_caps.x_resolution, display_caps.y_resolution);

    /* Enable display output */
    int ret = display_blanking_off(display_dev);
    if (ret < 0) {
        LOG_ERR("Failed to enable display: %d", ret);
        return ret;
    }

    return 0;

#else
    LOG_WRN("No display device configured in device tree");
    return -ENOTSUP;
#endif
}

/**
 * @brief Flush framebuffer to physical display
 *
 * Transfers the contents of the Akira framebuffer to the physical display
 * hardware using Zephyr's display_write() API.
 */
void akira_display_hal_flush(void)
{
#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display))
    if (display_dev == NULL) {
        LOG_ERR("Display not initialized");
        return;
    }

    uint16_t *fb = akira_framebuffer_get();
    if (fb == NULL) {
        LOG_ERR("Framebuffer is NULL");
        return;
    }

    struct display_buffer_descriptor desc = {
        .buf_size = display_caps.x_resolution * display_caps.y_resolution * 2,
        .width    = display_caps.x_resolution,
        .height   = display_caps.y_resolution,
        .pitch    = display_caps.x_resolution,
    };

    int ret = display_write(display_dev, 0, 0, &desc, fb);
    if (ret < 0) {
        LOG_ERR("Display write failed: %d", ret);
    }
#endif
}

/**
 * @brief Get display capabilities
 * @param caps Pointer to capabilities structure to fill
 * @return 0 on success, negative errno on error
 */
int akira_display_hal_get_capabilities(struct display_capabilities *caps)
{
    if (caps == NULL) {
        return -EINVAL;
    }

#if DT_NODE_EXISTS(DT_CHOSEN(zephyr_display))
    if (display_dev == NULL) {
        return -ENODEV;
    }

    *caps = display_caps;
    return 0;
#else
    return -ENOTSUP;
#endif
}

/**
 * @brief Set display backlight brightness (if supported)
 * @param brightness Brightness level (0-100)
 */
void akira_display_hal_set_brightness(uint8_t brightness)
{
    if (backlight_gpio_dev && device_is_ready(backlight_gpio_dev)) {
        gpio_pin_set(backlight_gpio_dev, BACKLIGHT_GPIO_PIN, (brightness > 0) ? 1 : 0);
    }
}