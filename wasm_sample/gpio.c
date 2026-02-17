/**
 * @file gpio.c
 * @brief GPIO control WASM application for AkiraOS
 * 
 * This application demonstrates GPIO functionality:
 * - Configuring GPIO pins as input and output
 * - Reading from input pins
 * - Writing to output pins
 * - Creating a simple input-to-output mirror application
 * 
 * The app reads from GPIO 15 (input) and mirrors the state to GPIO 40 (output).
 * 
 * @copyright Copyright (c) 2026 AkiraOS Contributors
 * @license Apache-2.0
 */

#include "akira_api.h"

// GPIO pin definitions
#define GPIO_INPUT_PIN   15  /**< Input pin number */
#define GPIO_OUTPUT_PIN  40  /**< Output pin number */

// Loop configuration
#define LOOP_ITERATIONS  100  /**< Number of iterations to run */

/**
 * @brief Simple delay function
 * 
 * Creates a busy-wait delay. Not precise, but sufficient for demonstration.
 * 
 * @param count Number of iterations (approximate delay)
 */
static void simple_delay(uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        // Busy wait
        __asm__ volatile ("" ::: "memory");
    }
}

/**
 * @brief Main entry point for the GPIO WASM application
 * 
 * Configures GPIO pins and runs a loop that reads from an input pin
 * and mirrors the state to an output pin.
 * 
 * @return 0 on success, negative error code on failure
 */
int main(void)
{
    int ret;
    
    printk(LOG_LEVEL_INF, "=================================");
    printk(LOG_LEVEL_INF, "  AkiraOS GPIO Test Application ");
    printk(LOG_LEVEL_INF, "=================================");
    
    // Configure GPIO 15 as input with pull-up resistor
    printk(LOG_LEVEL_INF, "Configuring GPIO 15 as INPUT with pull-up...");
    ret = gpio_configure(GPIO_INPUT_PIN, GPIO_INPUT | GPIO_PULL_UP | GPIO_ACTIVE_HIGH);
    if (ret < 0) {
        printk(LOG_LEVEL_ERR, "Failed to configure GPIO input pin!");
        return ret;
    }
    printk(LOG_LEVEL_INF, "GPIO 15 configured successfully");
    
    // Configure GPIO 40 as output, initialized to LOW
    printk(LOG_LEVEL_INF, "Configuring GPIO 40 as OUTPUT (init LOW)...");
    ret = gpio_configure(GPIO_OUTPUT_PIN, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW | GPIO_ACTIVE_HIGH);
    if (ret < 0) {
        printk(LOG_LEVEL_ERR, "Failed to configure GPIO output pin!");
        return ret;
    }
    printk(LOG_LEVEL_INF, "GPIO 40 configured successfully");
    
    printk(LOG_LEVEL_INF, "");
    printk(LOG_LEVEL_INF, "Starting GPIO mirror loop...");
    printk(LOG_LEVEL_INF, "Reading GPIO 15 -> Writing to GPIO 40");
    printk(LOG_LEVEL_INF, "");
    
    int last_state = -1;
    
    // Main loop: read input and mirror to output
    for (int i = 0; i < LOOP_ITERATIONS; i++) {
        // Read the input pin state
        int input_state = gpio_read(GPIO_INPUT_PIN);
        if (input_state < 0) {
            printk(LOG_LEVEL_ERR, "Failed to read GPIO input!");
            return input_state;
        }
        
        // Only log and update output when state changes
        if (input_state != last_state) {
            // Log the state change
            if (input_state == 1) {
                printk(LOG_LEVEL_INF, "Input HIGH -> Setting output HIGH");
            } else {
                printk(LOG_LEVEL_INF, "Input LOW  -> Setting output LOW");
            }
            
            // Write the inverted state to output (mirror)
            ret = gpio_write(GPIO_OUTPUT_PIN, input_state);
            if (ret < 0) {
                printk(LOG_LEVEL_ERR, "Failed to write to GPIO output!");
                return ret;
            }
            
            last_state = input_state;
        }
        
        // Small delay between reads
        simple_delay(100000);
    }
    
    printk(LOG_LEVEL_INF, "");
    printk(LOG_LEVEL_INF, "GPIO test completed successfully!");
    printk(LOG_LEVEL_INF, "Total iterations: 100");
    
    // Set output to LOW before exiting
    gpio_write(GPIO_OUTPUT_PIN, 0);
    printk(LOG_LEVEL_INF, "Output pin reset to LOW");
    
    return 0;
}
