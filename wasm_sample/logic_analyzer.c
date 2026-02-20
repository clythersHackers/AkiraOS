/*
 * Logic Analyzer for AkiraOS
 * 4-Channel GPIO Logic Analyzer with real-time waveform display
 * 
 * Channels:
 *  CH1 - GPIO 42 (Green)
 *  CH2 - GPIO 48 (Cyan)
 *  CH3 - GPIO 20 (Yellow)
 *  CH4 - GPIO 19 (Magenta)
 * 
 * Controls:
 *  UP    - Increase sampling speed (decrease delay)
 *  DOWN  - Decrease sampling speed (increase delay)
 *  LEFT  - Pause/Resume capture
 *  RIGHT - Exit application
 * 
 * Note: Runs for ~1-2 seconds then auto-exits to prevent system hang
 */

#include "akira_api.h"

// Pin assignments
#define CH1_PIN 42
#define CH2_PIN 48
#define CH3_PIN 20
#define CH4_PIN 19

// Button pins (DPAD)
#define BTN_UP    38
#define BTN_DOWN  39
#define BTN_LEFT  37  // Pause/Resume
#define BTN_RIGHT 36  // Exit app

// Display configuration
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define CHANNEL_COUNT 4
#define CHANNEL_HEIGHT 60  // 240 / 4 channels
#define LABEL_WIDTH 55     // Reserve space for channel labels
#define WAVEFORM_WIDTH (SCREEN_WIDTH - LABEL_WIDTH)  // 265 pixels

// Sampling configuration - reduced buffer size to avoid WASM issues
#define BUFFER_SIZE 256  // Power of 2 for efficient modulo
#define SAMPLE_DELAY_US 5000  // 5ms = 200 Hz sampling rate (was 1ms)
#define MAX_ITERATIONS 200  // Very short runtime (~1 second max)

// Sample buffer - circular buffer for each channel
// Using smaller buffer to avoid WASM "invalid local type" errors
static unsigned char samples[CHANNEL_COUNT][BUFFER_SIZE];
static int sample_pos = 0;
static int paused = 0;
static int exit_app = 0;
static int delay_us = SAMPLE_DELAY_US;

// Channel configuration
static const int channel_pins[CHANNEL_COUNT] = {CH1_PIN, CH2_PIN, CH3_PIN, CH4_PIN};

// Colors for waveforms
static const unsigned short channel_colors[CHANNEL_COUNT] = {
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_YELLOW,
    COLOR_MAGENTA
};

// Button state tracking
static unsigned char btn_last_state[4] = {1, 1, 1, 1};  // Assume pull-up (active low)

/**
 * Draw channel labels
 */
void draw_labels(void) {
    // Channel labels - draw with numbers
    display_text(3, 5, "CH1", COLOR_GREEN);
    display_text(3, 65, "CH2", COLOR_CYAN);
    display_text(3, 125, "CH3", COLOR_YELLOW);
    display_text(3, 185, "CH4", COLOR_MAGENTA);
    
    // Pin numbers
    display_text(3, 18, "P42", COLOR_WHITE);
    display_text(3, 78, "P48", COLOR_WHITE);
    display_text(3, 138, "P20", COLOR_WHITE);
    display_text(3, 198, "P19", COLOR_WHITE);
}

/**
 * Initialize the display with static elements (channel labels and grid)
 */
void init_display(void) {
    // Clear screen to black
    display_clear(COLOR_BLACK);
    
    // Draw channel labels
    draw_labels();
    
    // Draw channel separator lines and baselines
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        int y_base = ch * CHANNEL_HEIGHT;
        
        // Draw horizontal grid line (separator between channels)
        if (ch > 0) {
            display_rect(0, y_base, SCREEN_WIDTH, 1, COLOR_DARK_GRAY);
        }
        
        // Draw baseline reference lines (middle of channel area)
        int y_mid = y_base + CHANNEL_HEIGHT / 2;
        for (int x = LABEL_WIDTH; x < SCREEN_WIDTH; x += 20) {
            display_rect(x, y_mid, 2, 1, COLOR_DARK_GRAY);
        }
    }
    
    // Draw vertical line separating labels from waveform area
    display_rect(LABEL_WIDTH - 1, 0, 2, SCREEN_HEIGHT, COLOR_WHITE);
}

/**
 * Configure GPIO pins for input
 */
void init_gpio(void) {
    // Configure channel input pins
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        gpio_configure(channel_pins[ch], GPIO_INPUT);
    }
    
    // Configure button pins (active low with pull-up)
    gpio_configure(BTN_UP, GPIO_INPUT);
    gpio_configure(BTN_DOWN, GPIO_INPUT);
    gpio_configure(BTN_LEFT, GPIO_INPUT);
    gpio_configure(BTN_RIGHT, GPIO_INPUT);
}

/**
 * Handle button inputs
 */
void handle_buttons(void) {
    // Read button states (active low)
    int btn_up = !gpio_read(BTN_UP);
    int btn_down = !gpio_read(BTN_DOWN);
    int btn_left = !gpio_read(BTN_LEFT);
    int btn_right = !gpio_read(BTN_RIGHT);
    
    // UP: Increase speed (decrease delay)
    if (btn_up && btn_last_state[0]) {
        if (delay_us > 10) {
            delay_us -= 10;
        }
        btn_last_state[0] = 0;
    } else if (!btn_up) {
        btn_last_state[0] = 1;
    }
    
    // DOWN: Decrease speed (increase delay)
    if (btn_down && btn_last_state[1]) {
        if (delay_us < 10000) {
            delay_us += 50;
        }
        btn_last_state[1] = 0;
    } else if (!btn_down) {
        btn_last_state[1] = 1;
    }
    
    // LEFT: Pause/Resume
    if (btn_left && btn_last_state[2]) {
        paused = !paused;
        btn_last_state[2] = 0;
    } else if (!btn_left) {
        btn_last_state[2] = 1;
    }
    
    // RIGHT: Exit application
    if (btn_right && btn_last_state[3]) {
        exit_app = 1;
        btn_last_state[3] = 0;
    } else if (!btn_right) {
        btn_last_state[3] = 1;
    }
}

/**
 * Calculate Y coordinate for a signal level in a channel
 */
int get_y_for_signal(int channel, int level) {
    int y_base = channel * CHANNEL_HEIGHT;
    int y_mid = y_base + CHANNEL_HEIGHT / 2;
    
    if (level) {
        // HIGH - draw above midpoint
        return y_mid - 22;
    } else {
        // LOW - draw below midpoint
        return y_mid + 15;
    }
}

/**
 * Draw waveform column (simplified - less display calls)
 */
void draw_column(int screen_x, int buffer_idx) {
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        int y_base = ch * CHANNEL_HEIGHT;
        int y_mid = y_base + CHANNEL_HEIGHT / 2;
        
        // Clear entire column first
        display_rect(screen_x, y_base + 2, 1, CHANNEL_HEIGHT - 4, COLOR_BLACK);
        
        // Redraw baseline dot if on grid
        if ((screen_x - LABEL_WIDTH) % 20 == 0) {
            display_rect(screen_x, y_mid, 2, 1, COLOR_DARK_GRAY);
        }
        
        // Get and draw signal level
        int level = samples[ch][buffer_idx];
        int y = get_y_for_signal(ch, level);
        display_rect(screen_x, y, 1, 5, channel_colors[ch]);
    }
}

/**
 * Sample all GPIO channels and store in buffer
 */
void sample_channels(void) {
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        samples[ch][sample_pos] = gpio_read(channel_pins[ch]);
    }
}

/**
 * Main entry point
 */
int main(void) {
    // Initialize subsystems (minimal delays)
    init_gpio();
    init_display();
    
    // Clear sample buffer
    for (int ch = 0; ch < CHANNEL_COUNT; ch++) {
        for (int i = 0; i < BUFFER_SIZE; i++) {
            samples[ch][i] = 0;
        }
    }
    
    sample_pos = 0;
    int screen_x = LABEL_WIDTH;
    
    // Main loop - very limited runtime to prevent system hang
    while(1) {
        // Handle button inputs
        handle_buttons();
        
        // Exit early if requested
        if (exit_app) {
            break;
        }
        
        if (!paused) {
            // Sample all channels
            sample_channels();
            
            // Draw column with new sample (simplified drawing)
            draw_column(screen_x, sample_pos);
            
            // Advance positions
            sample_pos = (sample_pos + 1) & (BUFFER_SIZE - 1);
            screen_x++;
            
            // Wrap screen position
            if (screen_x >= SCREEN_WIDTH) {
                screen_x = LABEL_WIDTH;
            }
            
            // Delay for sampling rate
            delay(delay_us);
        } else {
            // When paused, check buttons more frequently
            delay(50000);  // 50ms
        }
    }
    
    // Clean exit (no delays)
    return 0;
}
