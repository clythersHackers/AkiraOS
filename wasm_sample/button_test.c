/**
 * @file button_test.c
 * @brief WASM app to test GPIO and button input APIs
 * 
 * This app demonstrates:
 * - Reading button states using the input API
 * - Controlling GPIO pins
 * 
 * Required capabilities in manifest:
 * - "input.read" for reading buttons
 * - "gpio.read" for reading GPIO pins  
 * - "gpio.write" for controlling GPIO outputs
 */

#include <stdint.h>

// Import native functions from AkiraOS
extern void log_info(const char *message);
extern void sleep_ms(int ms);

// Input API
extern int input_read_buttons(void);
extern int input_button_pressed(int button_id);

// GPIO API
extern int gpio_configure(int pin, int flags);
extern int gpio_read(int pin);
extern int gpio_write(int pin, int value);

// Button IDs
#define AKIRA_BTN_UP       2
#define AKIRA_BTN_DOWN     3
#define AKIRA_BTN_LEFT     4
#define AKIRA_BTN_RIGHT    5
#define AKIRA_BTN_A        6
#define AKIRA_BTN_B        7
#define AKIRA_BTN_X        8
#define AKIRA_BTN_Y        9

// GPIO flags
#define AKIRA_GPIO_INPUT            (1U << 0)
#define AKIRA_GPIO_OUTPUT           (1U << 1)
#define AKIRA_GPIO_OUTPUT_INIT_LOW  (1U << 2)
#define AKIRA_GPIO_OUTPUT_INIT_HIGH (1U << 3)
#define AKIRA_GPIO_PULL_UP          (1U << 4)

// Simple string buffer for logging
static char msg_buffer[128];

// Simple integer to string conversion
void int_to_str(int value, char *str) {
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }
    
    int is_negative = 0;
    if (value < 0) {
        is_negative = 1;
        value = -value;
    }
    
    char temp[16];
    int i = 0;
    while (value > 0) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    
    int j = 0;
    if (is_negative) {
        str[j++] = '-';
    }
    while (i > 0) {
        str[j++] = temp[--i];
    }
    str[j] = '\0';
}

// Simple string copy
void str_copy(char *dest, const char *src) {
    while (*src) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

// Simple string append
void str_append(char *dest, const char *src) {
    while (*dest) dest++;
    str_copy(dest, src);
}

int main(void) {
    log_info("=== Button & GPIO Test App ===");
    log_info("Press buttons to see input...");
    
    // Optional: Configure an LED output on GPIO 2 (if available)
    int led_pin = 2;
    int ret = gpio_configure(led_pin, AKIRA_GPIO_OUTPUT | AKIRA_GPIO_OUTPUT_INIT_LOW);
    if (ret == 0) {
        log_info("LED configured on GPIO 2");
    } else {
        log_info("Could not configure LED (no gpio.write capability or invalid pin)");
    }
    
    int led_state = 0;
    int last_buttons = 0;
    int loop_count = 0;
    
    // Main loop
    while (1) {
        // Read all button states
        int buttons = input_read_buttons();
        
        // Only log when buttons change
        if (buttons != last_buttons && buttons != 0) {
            str_copy(msg_buffer, "Buttons pressed: 0x");
            
            // Convert button mask to hex string manually
            char hex[9];
            for (int i = 7; i >= 0; i--) {
                int nibble = (buttons >> (i * 4)) & 0xF;
                hex[7-i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
            }
            hex[8] = '\0';
            str_append(msg_buffer, hex);
            log_info(msg_buffer);
            
            // Check specific buttons
            if (input_button_pressed(AKIRA_BTN_A)) {
                log_info("  - Button A pressed!");
                // Toggle LED
                led_state = !led_state;
                gpio_write(led_pin, led_state);
            }
            if (input_button_pressed(AKIRA_BTN_B)) {
                log_info("  - Button B pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_X)) {
                log_info("  - Button X pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_Y)) {
                log_info("  - Button Y pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_UP)) {
                log_info("  - D-Pad UP pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_DOWN)) {
                log_info("  - D-Pad DOWN pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_LEFT)) {
                log_info("  - D-Pad LEFT pressed!");
            }
            if (input_button_pressed(AKIRA_BTN_RIGHT)) {
                log_info("  - D-Pad RIGHT pressed!");
            }
            
            last_buttons = buttons;
        } else if (buttons == 0 && last_buttons != 0) {
            log_info("All buttons released");
            last_buttons = 0;
        }
        
        // Blink LED every 10 loops (about 500ms)
        loop_count++;
        if (loop_count >= 10) {
            loop_count = 0;
            // Could pulse LED here
        }
        
        // Small delay to avoid polling too fast
        sleep_ms(50);
    }
    
    return 0;
}
