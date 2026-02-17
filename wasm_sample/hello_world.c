/**
 * @file hello_world.c
 * @brief Simple "Hello World" WASM application for AkiraOS
 * 
 * Minimal example demonstrating logging API.
 * 
 * @copyright Copyright (c) 2026 AkiraOS Contributors  
 * @license Apache-2.0
 */

#include "akira_api.h"

int main(void)
{
    // Log messages at different levels
    printk(LOG_LEVEL_INF, "=================================");
    printk(LOG_LEVEL_INF, "  Hello from AkiraOS WASM!      ");
    printk(LOG_LEVEL_INF, "=================================");
    printk(LOG_LEVEL_INF, "");
    
    printk(LOG_LEVEL_INF, "[INFO]  This is an info message");
    printk(LOG_LEVEL_WRN, "[WARN]  This is a warning message");
    printk(LOG_LEVEL_ERR, "[ERROR] This is an error message");
    
    printk(LOG_LEVEL_INF, "");
    printk(LOG_LEVEL_INF, "WASM app executed successfully!");
    printk(LOG_LEVEL_INF, "SDK Version: 1.0.0");
    printk(LOG_LEVEL_INF, "Runtime: AkiraOS WAMR");
    
    return 0;
}
