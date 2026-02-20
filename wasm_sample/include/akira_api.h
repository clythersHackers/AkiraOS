/**
 * @file akira_api.h
 * @brief AkiraOS WASM SDK - Native API Declarations
 * 
 * This header provides function declarations and constants for WASM applications
 * running on the AkiraOS runtime. Include this header in your WASM apps to access
 * native functionality for display, GPIO, sensors, and more.
 * 
 * All native functions are declared as extern and automatically become imports
 * when compiled with WASI SDK and -nostdlib flag.
 * 
 * @copyright Copyright (c) 2026 AkiraOS Contributors
 * @license Apache-2.0
 */

#ifndef AKIRA_API_H
#define AKIRA_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * =============================================================================
 * COLOR CONSTANTS (RGB565 Format)
 * =============================================================================
 */

/** @brief Black color (RGB565: 0x0000) */
#define COLOR_BLACK       0x0000
/** @brief White color (RGB565: 0xFFFF) */
#define COLOR_WHITE       0xFFFF
/** @brief Red color (RGB565: 0xF800) */
#define COLOR_RED         0xF800
/** @brief Green color (RGB565: 0x07E0) */
#define COLOR_GREEN       0x07E0
/** @brief Blue color (RGB565: 0x001F) */
#define COLOR_BLUE        0x001F
/** @brief Yellow color (RGB565: 0xFFE0) */
#define COLOR_YELLOW      0xFFE0
/** @brief Cyan color (RGB565: 0x07FF) */
#define COLOR_CYAN        0x07FF
/** @brief Magenta color (RGB565: 0xF81F) */
#define COLOR_MAGENTA     0xF81F
/** @brief Gray color (RGB565: 0x7BEF) */
#define COLOR_GRAY        0x7BEF
/** @brief Dark gray color (RGB565: 0x39E7) */
#define COLOR_DARK_GRAY   0x39E7
/** @brief Light gray color (RGB565: 0xC618) */
#define COLOR_LIGHT_GRAY  0xC618
/** @brief Orange color (RGB565: 0xFD20) */
#define COLOR_ORANGE      0xFD20
/** @brief Purple color (RGB565: 0x801F) */
#define COLOR_PURPLE      0x801F

/*
 * =============================================================================
 * GPIO CONFIGURATION FLAGS
 * =============================================================================
 */

/** @brief Configure GPIO pin as input */
#define GPIO_INPUT            (1U << 0)
/** @brief Configure GPIO pin as output */
#define GPIO_OUTPUT           (1U << 1)
/** @brief Initialize output pin to low state */
#define GPIO_OUTPUT_INIT_LOW  (1U << 2)
/** @brief Initialize output pin to high state */
#define GPIO_OUTPUT_INIT_HIGH (1U << 3)
/** @brief Enable internal pull-up resistor */
#define GPIO_PULL_UP          (1U << 4)
/** @brief Enable internal pull-down resistor */
#define GPIO_PULL_DOWN        (1U << 5)
/** @brief Configure pin as active-low */
#define GPIO_ACTIVE_LOW       (1U << 6)
/** @brief Configure pin as active-high */
#define GPIO_ACTIVE_HIGH      (1U << 7)

/*
 * =============================================================================
 * SENSOR TYPES
 * =============================================================================
 */

/** @brief Temperature sensor */
#define SENSOR_TYPE_TEMP      0
/** @brief Humidity sensor */
#define SENSOR_TYPE_HUMIDITY  1
/** @brief Pressure sensor */
#define SENSOR_TYPE_PRESSURE  2
/** @brief Accelerometer X-axis */
#define SENSOR_TYPE_ACCEL_X   3
/** @brief Accelerometer Y-axis */
#define SENSOR_TYPE_ACCEL_Y   4
/** @brief Accelerometer Z-axis */
#define SENSOR_TYPE_ACCEL_Z   5
/** @brief Gyroscope X-axis */
#define SENSOR_TYPE_GYRO_X    6
/** @brief Gyroscope Y-axis */
#define SENSOR_TYPE_GYRO_Y    7
/** @brief Gyroscope Z-axis */
#define SENSOR_TYPE_GYRO_Z    8

/*
 * =============================================================================
 * LOG LEVELS
 * =============================================================================
 */

/** @brief Error log level */
#define LOG_LEVEL_ERR     1
/** @brief Warning log level */
#define LOG_LEVEL_WRN     2
/** @brief Info log level */
#define LOG_LEVEL_INF     3

/*
 * =============================================================================
 * LOGGING API
 * =============================================================================
 * Required capability: input.read
 */

/**
 * @brief Log a message to the AkiraOS console
 * 
 * @param level Log level (LOG_LEVEL_ERR, LOG_LEVEL_WRN, LOG_LEVEL_INF, LOG_LEVEL_DBG)
 * @param message Null-terminated string message to log
 * @return 0 on success, negative error code on failure
 */
extern int printf(const char *message);

/**
 * @brief Delay execution for a specified number of microseconds
 */
extern int delay(uint32_t microseconds);


/*
 * =============================================================================
 * DISPLAY API
 * =============================================================================
 * Required capability: display.write
 * 
 * All display functions use RGB565 color format (16-bit color).
 * Coordinates start at (0,0) in the top-left corner.
 */

/**
 * @brief Clear the entire display with a solid color
 * 
 * @param color RGB565 color value
 * @return 0 on success, negative error code on failure
 */
extern int display_clear(uint32_t color);

/**
 * @brief Set a single pixel on the display
 * 
 * @param x X coordinate (horizontal position)
 * @param y Y coordinate (vertical position)
 * @param color RGB565 color value
 * @return 0 on success, negative error code on failure
 */
extern int display_pixel(int32_t x, int32_t y, uint32_t color);

/**
 * @brief Draw a filled rectangle on the display
 * 
 * @param x X coordinate of top-left corner
 * @param y Y coordinate of top-left corner
 * @param w Width in pixels
 * @param h Height in pixels
 * @param color RGB565 color value
 * @return 0 on success, negative error code on failure
 */
extern int display_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/**
 * @brief Display text using the small font (7x10 pixels per character)
 * 
 * @param x X coordinate of text starting position
 * @param y Y coordinate of text starting position
 * @param text Null-terminated string to display
 * @param color RGB565 color value
 * @return 0 on success, negative error code on failure
 */
extern int display_text(int32_t x, int32_t y, const char *text, uint32_t color);

/**
 * @brief Display text using the large font (11x18 pixels per character)
 * 
 * @param x X coordinate of text starting position
 * @param y Y coordinate of text starting position
 * @param text Null-terminated string to display
 * @param color RGB565 color value
 * @return 0 on success, negative error code on failure
 */
extern int display_text_large(int32_t x, int32_t y, const char *text, uint32_t color);

/*
 * =============================================================================
 * GPIO API
 * =============================================================================
 * Required capabilities: gpio.read and/or gpio.write
 * 
 * GPIO operations allow control of digital input/output pins.
 * Pin numbers are logical and mapped by the runtime to physical pins.
 */

/**
 * @brief Configure a GPIO pin
 * 
 * @param pin GPIO pin number
 * @param flags Configuration flags (GPIO_INPUT, GPIO_OUTPUT, etc.)
 * @return 0 on success, negative error code on failure
 */
extern int gpio_configure(uint32_t pin, uint32_t flags);

/**
 * @brief Read the state of a GPIO input pin
 * 
 * @param pin GPIO pin number
 * @return Pin state (0 = low, 1 = high), negative error code on failure
 */
extern int gpio_read(uint32_t pin);

/**
 * @brief Write a value to a GPIO output pin
 * 
 * @param pin GPIO pin number
 * @param value Value to write (0 = low, 1 = high)
 * @return 0 on success, negative error code on failure
 */
extern int gpio_write(uint32_t pin, uint32_t value);

/*
 * =============================================================================
 * SENSOR API
 * =============================================================================
 * Required capability: sensor.read
 * 
 * Sensor readings return scaled integer values. Refer to documentation for
 * scaling factors specific to each sensor type.
 */

/**
 * @brief Read a sensor value
 * 
 * @param type Sensor type (SENSOR_TYPE_TEMP, SENSOR_TYPE_HUMIDITY, etc.)
 * @return Sensor reading (scaled integer), negative error code on failure
 */
extern int sensor_read(int32_t type);

/*
 * =============================================================================
 * MEMORY API
 * =============================================================================
 * 
 * Dynamic memory allocation within WASM module's memory quota.
 * Memory is automatically freed when the WASM app terminates.
 */

/**
 * @brief Allocate memory from the WASM heap
 * 
 * @param size Number of bytes to allocate
 * @return WASM address of allocated memory, 0 on failure
 */
extern uint32_t mem_alloc(uint32_t size);

/**
 * @brief Free previously allocated memory
 * 
 * @param ptr WASM address returned by mem_alloc()
 */
extern void mem_free(uint32_t ptr);

/*
 * =============================================================================
 * BLUETOOTH SHELL API
 * =============================================================================
 * Required capability: bt_shell
 * 
 * Functions for sending data over Bluetooth shell interface.
 */

/**
 * @brief Send a text message over Bluetooth shell
 * 
 * @param message Null-terminated string to send
 * @return 0 on success, negative error code on failure
 */
extern int bt_shell_print(const char *message);

/**
 * @brief Send raw data over Bluetooth shell
 * 
 * @param data_ptr Pointer to data buffer
 * @param len Length of data in bytes
 * @return 0 on success, negative error code on failure
 */
extern int bt_shell_send_data(uint32_t data_ptr, uint32_t len);

/**
 * @brief Check if Bluetooth shell is ready
 * 
 * @return 1 if ready, 0 if not ready, negative error code on failure
 */
extern int bt_shell_is_ready(void);

/*
 * =============================================================================
 * RF TRANSCEIVER API
 * =============================================================================
 * Required capability: rf.transceive
 * 
 * Functions for controlling radio frequency transceivers (e.g., LoRa, LR1121).
 */

/**
 * @brief Set RF transceiver frequency
 * 
 * @param freq_hz Frequency in Hertz
 * @return 0 on success, negative error code on failure
 */
extern int rf_set_frequency(uint32_t freq_hz);

/**
 * @brief Set RF transmission power
 * 
 * @param dbm Power in dBm
 * @return 0 on success, negative error code on failure
 */
extern int rf_set_power(int8_t dbm);

/**
 * @brief Get received signal strength indicator (RSSI)
 * 
 * @param rssi Pointer to store RSSI value
 * @return 0 on success, negative error code on failure
 */
extern int rf_get_rssi(int16_t *rssi);

/**
 * @brief Send data over RF transceiver
 * 
 * @param payload_ptr Pointer to payload data
 * @param len Length of payload in bytes
 * @return 0 on success, negative error code on failure
 */
extern int rf_send(uint32_t payload_ptr, uint32_t len);

/*
 * =============================================================================
 * HELPER MACROS
 * =============================================================================
 */

/** 
 * Note: Export functions via linker flags (-Wl,--export=main), not source attributes.
 * This avoids duplicate export errors with the build system.
 */

/*
 * =============================================================================
 * ERROR CODES
 * =============================================================================
 */

#define EPERM           1   /**< Operation not permitted */
#define ENOENT          2   /**< No such file or directory */
#define EIO             5   /**< I/O error */
#define EBADF           9   /**< Bad file descriptor */
#define ENOMEM          12  /**< Out of memory */
#define EACCES          13  /**< Permission denied */
#define EFAULT          14  /**< Bad address */
#define EBUSY           16  /**< Device or resource busy */
#define EINVAL          22  /**< Invalid argument */
#define ENOSPC          28  /**< No space left on device */
#define ETIMEDOUT       110 /**< Connection timed out */

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_API_H */
