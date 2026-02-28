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
 * SENSOR CHANNEL IDs
 * =============================================================================
 * These values match Zephyr's enum sensor_channel exactly.
 * Pass them directly to sensor_read().
 */

/** @brief Accelerometer X-axis, m/s² */
#define SENSOR_CHAN_ACCEL_X        0
/** @brief Accelerometer Y-axis, m/s² */
#define SENSOR_CHAN_ACCEL_Y        1
/** @brief Accelerometer Z-axis, m/s² */
#define SENSOR_CHAN_ACCEL_Z        2
/** @brief Gyroscope X-axis, rad/s */
#define SENSOR_CHAN_GYRO_X         4
/** @brief Gyroscope Y-axis, rad/s */
#define SENSOR_CHAN_GYRO_Y         5
/** @brief Gyroscope Z-axis, rad/s */
#define SENSOR_CHAN_GYRO_Z         6
/** @brief Magnetometer X-axis, Gauss */
#define SENSOR_CHAN_MAGN_X         8
/** @brief Magnetometer Y-axis, Gauss */
#define SENSOR_CHAN_MAGN_Y         9
/** @brief Magnetometer Z-axis, Gauss */
#define SENSOR_CHAN_MAGN_Z         10
/** @brief Ambient temperature, °C */
#define SENSOR_CHAN_AMBIENT_TEMP   13
/** @brief Pressure, kPa */
#define SENSOR_CHAN_PRESS          14
/** @brief Relative humidity, % */
#define SENSOR_CHAN_HUMIDITY       16
/** @brief Altitude, m */
#define SENSOR_CHAN_ALTITUDE       23
/** @brief Voltage, V */
#define SENSOR_CHAN_VOLTAGE        33
/** @brief Current, A */
#define SENSOR_CHAN_CURRENT        35
/** @brief Power, W */
#define SENSOR_CHAN_POWER          36

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

/**
 * @brief Flush the framebuffer to the display hardware.
 *
 * Draw calls (rect, pixel, text, …) write into a back-buffer.  Call
 * display_flush() once you have finished composing a frame to push the
 * result to the screen.  An automatic flush fires 50 ms after the last draw
 * call, but calling this explicitly gives smoother animation.
 *
 * @return 0 on success, negative error code on failure
 */
extern int display_flush(void);

/**
 * @brief Get the display resolution.
 *
 * @param w_out  Pointer receives the display width  in pixels
 * @param h_out  Pointer receives the display height in pixels
 * @return 0 on success, negative error code on failure
 */
extern int display_get_size(int32_t *w_out, int32_t *h_out);

/**
 * @brief Draw a straight line between two points (Bresenham — no FPU).
 *
 * @param x0 Start X  @param y0 Start Y
 * @param x1 End X    @param y1 End Y
 * @param color RGB565 color
 * @return 0 on success
 */
extern int display_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

/**
 * @brief Draw a circle outline (midpoint algorithm — no FPU).
 *
 * @param cx Centre X  @param cy Centre Y  @param r Radius (pixels)
 * @param color RGB565 color
 * @return 0 on success
 */
extern int display_circle(int32_t cx, int32_t cy, int32_t r, uint32_t color);

/**
 * @brief Draw a filled circle.
 *
 * @param cx Centre X  @param cy Centre Y  @param r Radius (pixels)
 * @param color RGB565 fill color
 * @return 0 on success
 */
extern int display_circle_fill(int32_t cx, int32_t cy, int32_t r, uint32_t color);

/**
 * @brief Draw a triangle outline (three lines).
 *
 * @param x0,y0  @param x1,y1  @param x2,y2  Vertices
 * @param color RGB565 color
 * @return 0 on success
 */
extern int display_triangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                             int32_t x2, int32_t y2, uint32_t color);

/**
 * @brief Draw a filled triangle (scanline rasteriser).
 *
 * @param x0,y0  @param x1,y1  @param x2,y2  Vertices (any order)
 * @param color RGB565 fill color
 * @return 0 on success
 */
extern int display_triangle_fill(int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                  int32_t x2, int32_t y2, uint32_t color);

/**
 * @brief Draw a rectangle outline (four lines).
 *
 * @param x,y   Top-left corner  @param w Width  @param h Height
 * @param color RGB565 color
 * @return 0 on success
 */
extern int display_rect_outline(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

/**
 * @brief Blit an RGB565 bitmap to the display.
 *
 * @param x,y        Top-left destination corner
 * @param w,h        Width and height in pixels
 * @param data       Pointer to RGB565 pixel data (row-major, w*h*2 bytes)
 * @param data_size  Size of @p data in bytes (must be >= w*h*2)
 * @return 0 on success, -EINVAL if data_size is too small
 */
extern int display_bitmap(int32_t x, int32_t y, int32_t w, int32_t h,
                           const uint16_t *data, uint32_t data_size);

/**
 * @brief Blit an RGB565 bitmap with a transparent colour key.
 *
 * Pixels whose value equals @p key are not written to the framebuffer.
 *
 * @param x,y        Top-left destination corner
 * @param w,h        Width and height in pixels
 * @param data       Pointer to RGB565 pixel data (row-major, w*h*2 bytes)
 * @param data_size  Size of @p data in bytes (must be >= w*h*2)
 * @param key        RGB565 transparent colour value
 * @return 0 on success
 */
extern int display_bitmap_transparent(int32_t x, int32_t y, int32_t w, int32_t h,
                                       const uint16_t *data, uint32_t data_size,
                                       uint32_t key);

/**
 * @brief Draw a horizontal line (optimised run).
 * @param x,y  Start coordinate  @param len  Length in pixels
 * @param color  RGB565 color
 * @return 0 on success
 */
extern int display_hline(int32_t x, int32_t y, int32_t len, uint32_t color);

/**
 * @brief Draw a vertical line (optimised run).
 * @param x,y  Start coordinate  @param len  Length in pixels
 * @param color  RGB565 color
 * @return 0 on success
 */
extern int display_vline(int32_t x, int32_t y, int32_t len, uint32_t color);

/**
 * @brief Render an integer as decimal text using the small font.
 *
 * No stdlib required — the conversion is done natively.
 *
 * @param x,y    Top-left of the first digit
 * @param value  Value to display (positive or negative)
 * @param color  RGB565 color
 * @return 0 on success
 */
extern int display_number(int32_t x, int32_t y, int32_t value, uint32_t color);

/**
 * @brief Draw a horizontal progress bar.
 *
 * Draws a @p bg filled rectangle, then a @p fg rectangle proportional to
 * @p value / @p max_val, then an outline in @p fg.
 *
 * @param x,y      Top-left corner
 * @param w,h      Width and height in pixels
 * @param value    Current fill value (clamped to [0, max_val])
 * @param max_val  Maximum value (bar is full when value == max_val)
 * @param fg       Foreground / fill RGB565 color
 * @param bg       Background RGB565 color
 * @return 0 on success
 */
extern int display_progress_bar(int32_t x, int32_t y, int32_t w, int32_t h,
                                 int32_t value, int32_t max_val,
                                 uint32_t fg, uint32_t bg);

/**
 * @brief Draw a rounded rectangle outline.
 *
 * @param x,y    Top-left corner
 * @param w,h    Width and height in pixels
 * @param radius Corner arc radius in pixels (clamped to min(w,h)/2)
 * @param color  RGB565 color
 * @return 0 on success
 */
extern int display_rounded_rect(int32_t x, int32_t y, int32_t w, int32_t h,
                                 int32_t radius, uint32_t color);

/**
 * @brief Draw a filled rounded rectangle.
 *
 * @param x,y    Top-left corner
 * @param w,h    Width and height in pixels
 * @param radius Corner arc radius in pixels (clamped to min(w,h)/2)
 * @param color  RGB565 fill color
 * @return 0 on success
 */
extern int display_rounded_rect_fill(int32_t x, int32_t y, int32_t w, int32_t h,
                                      int32_t radius, uint32_t color);

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
 * @brief Error sentinel returned by sensor_read() when the sensor is
 * unavailable or an I/O error occurred.
 *
 * Compare the return value against this constant rather than testing for
 * arbitrary negative numbers, because valid near-zero readings (e.g.
 * -0.001 m/s² = -1) are also small negative integers.
 *
 * Example:
 *   int ax = sensor_read(SENSOR_CHAN_ACCEL_X);
 *   if (ax == AKIRA_SENSOR_ERROR) { ... handle error ... }
 */
#define AKIRA_SENSOR_ERROR  (-2147483647 - 1)   /* INT32_MIN */

/**
 * @brief Read a sensor channel value.
 *
 * Iterates all DT-enabled sensor devices and returns the first that answers
 * the requested channel. On success, returns the reading scaled by 1000
 * (divide by 1000.0 to recover the physical value). On failure, returns
 * AKIRA_SENSOR_ERROR.
 *
 * @param channel  Sensor channel ID (SENSOR_CHAN_* constant).
 * @return Reading x1000 on success, AKIRA_SENSOR_ERROR on failure.
 */
extern int sensor_read(int32_t channel);

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
 * TIMER API
 * =============================================================================
 * Required capability: timer
 *
 * Polling-only timers backed by the OS uptime counter.
 * Values are in milliseconds. Use delay() to yield between polls.
 */

/**
 * @brief Allocate a new timer handle.
 * @return Handle index ≥0 on success, -ENOMEM if pool is full.
 */
extern int timer_create(void);

/**
 * @brief Start (or restart) a timer, resetting elapsed time to 0.
 * @param handle Handle returned by timer_create().
 * @return 0 on success, negative error code on failure.
 */
extern int timer_start(int32_t handle);

/**
 * @brief Stop a running timer, preserving the elapsed time.
 * @param handle Handle returned by timer_create().
 * @return 0 on success, negative error code on failure.
 */
extern int timer_stop(int32_t handle);

/**
 * @brief Return elapsed milliseconds.
 * Running timer: time since last timer_start().
 * Stopped timer: time between last timer_start() and timer_stop().
 * @param handle Handle returned by timer_create().
 * @return Elapsed milliseconds (int32), negative error code on failure.
 */
extern int timer_elapsed(int32_t handle);

/**
 * @brief Release a timer handle back to the pool.
 * @param handle Handle returned by timer_create().
 * @return 0 on success, negative error code on failure.
 */
extern int timer_free(int32_t handle);

/*
 * =============================================================================
 * UART API
 * =============================================================================
 * Required capability: uart
 *
 * Full-duplex UART access. UART0 is reserved for the system shell.
 * port_id 0 = first secondary UART (UART1).
 * uart_read() is non-blocking — returns 0 if no data is available.
 */

/**
 * @brief Open a secondary UART port.
 * @param port_id  0-indexed secondary UART (0 = UART1).
 * @param baud_rate Desired baud rate (e.g. 115200).
 * @return Handle index ≥0 on success, negative error code on failure.
 */
extern int uart_open(int32_t port_id, int32_t baud_rate);

/**
 * @brief Write bytes to an open UART.
 * @param handle    Handle from uart_open().
 * @param buf       Pointer to data buffer.
 * @param len       Number of bytes to write.
 * @return Bytes written, or negative error code.
 */
extern int uart_write(int32_t handle, const uint8_t *buf, uint32_t len);

/**
 * @brief Non-blocking read from UART RX ring buffer.
 * Returns 0 immediately if no data is available; use delay() to poll.
 * @param handle    Handle from uart_open().
 * @param buf       Destination buffer.
 * @param max_len   Maximum bytes to read.
 * @return Bytes read (0 = no data), or negative error code.
 */
extern int uart_read(int32_t handle, uint8_t *buf, uint32_t max_len);

/**
 * @brief Close a UART handle and release its resources.
 * @param handle Handle from uart_open().
 * @return 0 on success, negative error code on failure.
 */
extern int uart_close(int32_t handle);

/*
 * =============================================================================
 * I2C API
 * =============================================================================
 * Required capability: i2c
 *
 * Stateless raw register access. The LSM6DS3 IMU is on bus 0 at address 0x6A.
 * All lengths are capped at 256 bytes; addresses must be 7-bit (≤ 0x7F).
 */

/**
 * @brief Write bytes to an I2C device register.
 * @param bus_id   I2C bus index (0 = i2c0, 1 = i2c1).
 * @param dev_addr 7-bit I2C device address.
 * @param reg_addr Register address to write to.
 * @param buf      Pointer to data buffer.
 * @param len      Number of bytes to write (max 256).
 * @return 0 on success, negative error code on failure.
 */
extern int i2c_write_reg(int32_t bus_id, int32_t dev_addr, int32_t reg_addr,
                          const uint8_t *buf, uint32_t len);

/**
 * @brief Read bytes from an I2C device register.
 * @param bus_id   I2C bus index (0 = i2c0, 1 = i2c1).
 * @param dev_addr 7-bit I2C device address.
 * @param reg_addr Register address to read from.
 * @param buf      Destination buffer.
 * @param len      Number of bytes to read (max 256).
 * @return Bytes read on success, negative error code on failure.
 */
extern int i2c_read_reg(int32_t bus_id, int32_t dev_addr, int32_t reg_addr,
                         uint8_t *buf, uint32_t len);

/*
 * =============================================================================
 * PWM API
 * =============================================================================
 * Required capability: pwm
 *
 * Channel-indexed PWM control. Channel 0 is the first available PWM output.
 */

/**
 * @brief Set a PWM channel's frequency and duty cycle.
 * @param channel  Logical PWM channel index (0-based).
 * @param freq_hz  Frequency in Hertz (1–10,000,000).
 * @param duty_pct Duty cycle percentage (0–100).
 * @return 0 on success, negative error code on failure.
 */
extern int pwm_set(int32_t channel, int32_t freq_hz, int32_t duty_pct);

/**
 * @brief Disable a PWM channel (output held low).
 * @param channel Logical PWM channel index.
 * @return 0 on success, negative error code on failure.
 */
extern int pwm_disable(int32_t channel);

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
