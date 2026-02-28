# AkiraOS WASM SDK & Example Applications

This directory contains the AkiraOS WASM Software Development Kit (SDK) and example applications demonstrating the native API functionality.

## Table of Contents

- [Overview](#overview)
- [Prerequisites](#prerequisites)
- [SDK Structure](#sdk-structure)
- [Example Applications](#example-applications)
- [Building WASM Apps](#building-wasm-apps)
- [Creating Your Own Apps](#creating-your-own-apps)
- [Manifest Format](#manifest-format)
- [API Reference](#api-reference)
- [AOT Compilation](#aot-compilation)
- [Troubleshooting](#troubleshooting)

---

## Overview

The AkiraOS WASM SDK provides a complete development environment for building WebAssembly applications that run on AkiraOS-powered devices. Applications are sandboxed and have capabilities-based access to system resources like GPIO, display, sensors, and more.

**Key Features:**
- ✅ Capability-based security model
- ✅ RGB565 graphics API
- ✅ GPIO control (input/output)
- ✅ Sensor access (IMU, environmental)
- ✅ Memory management
- ✅ Bluetooth shell integration
- ✅ RF transceiver support
- ✅ AOT compilation support for performance

---

## Prerequisites

### Required Software

1. **WASI SDK** (version 29 or later)
   - Download from: https://github.com/WebAssembly/wasi-sdk/releases
   - Default installation path: `/opt/wasi-sdk`
   - Set `WASI_SDK` environment variable if installed elsewhere

2. **AkiraOS Runtime**
   - Build and flash AkiraOS to your target device
   - See main project README for build instructions

### Installation

```bash
# Install WASI SDK (Linux example)
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-29/wasi-sdk-29.0-x86_64-linux.tar.gz
sudo tar xzf wasi-sdk-29.0-x86_64-linux.tar.gz -C /opt
sudo mv /opt/wasi-sdk-29.0 /opt/wasi-sdk

# Verify installation
/opt/wasi-sdk/bin/clang --version
```

---

## SDK Structure

```
wasm_sample/
├── include/
│   └── akira_api.h           # Main SDK header (import this in your apps)
├── bin/                       # Built WASM files (generated)
├── build_wasm_apps.sh         # Build script
├── README.md                  # This file
│
├── hello_world.c              # Example: Basic logging
├── hello_world.json           # Manifest for hello_world
│
├── gpio.c                     # Example: GPIO control
├── gpio.json                  # Manifest for gpio
│
├── display_test.c             # Example: Graphics/display
├── display_test.json          # Manifest for display_test
│
├── tetris.c                   # Example: Full game
└── tetris.json                # Manifest for tetris
```

### Key Files

- **`include/akira_api.h`**: The main SDK header providing function declarations, constants, and macros for WASM development
- **`build_wasm_apps.sh`**: Automated build script for compiling WASM applications
- **`*.json`**: Manifest files defining app metadata and required capabilities

---

## Example Applications

### 1. hello_world.c

**Purpose:** Minimal WASM app demonstrating basic logging functionality

**Features:**
- Logging at different levels (DEBUG, INFO, WARN, ERROR)
- Simple "Hello World" output
- Entry point pattern

**Capabilities Required:** `input.read`

**Run:**
```bash
app start hello_world
```

---

### 2. gpio.c

**Purpose:** GPIO input/output control demonstration

**Features:**
- Configure GPIO pins as input/output
- Read from input pin (GPIO 15)
- Write to output pin (GPIO 40)
- Mirror input state to output
- Pull-up/pull-down resistor configuration

**Capabilities Required:** `gpio.read`, `gpio.write`

**Hardware:**
- GPIO 15: Input (e.g., button)
- GPIO 40: Output (e.g., LED)

**Run:**
```bash
app start gpio
```

---

### 3. display_test.c

**Purpose:** Comprehensive display API demonstration

**Features:**
- Clear screen with colors
- Draw filled rectangles
- Draw individual pixels
- Render text (small and large fonts)
- Color palettes
- Visual patterns and graphics

**Capabilities Required:** `display.write`

**Run:**
```bash
app start display_test
```

---

### 4. tetris.c

**Purpose:** Full-featured Tetris game

**Features:**
- Seven classic tetromino pieces (I, O, T, S, Z, J, L)
- Piece rotation and movement
- Line clearing and scoring
- Level progression
- Next piece preview
- Game over detection
- Colorful RGB565 graphics
- Simple AI for auto-play demonstration

**Capabilities Required:** `display.write`, `input.read`, `gpio.read`

**Run:**
```bash
app start tetris
```

---

## Building WASM Apps

### Build All Applications

```bash
cd wasm_sample
./build_wasm_apps.sh
```

### Build Specific Application

```bash
./build_wasm_apps.sh hello_world
```

### Clean Build Artifacts

```bash
./build_wasm_apps.sh clean
```

### List Available Apps

```bash
./build_wasm_apps.sh list
```

### Build Output

Compiled WASM files are generated in `wasm_sample/bin/`:
```
bin/
├── hello_world.wasm
├── hello_world.json
├── gpio.wasm
├── gpio.json
├── display_test.wasm
├── display_test.json
├── tetris.wasm
└── tetris.json
```

---

## Creating Your Own Apps

### Step 1: Create Source File

Create a new `.c` file in the `wasm_sample/` directory:

```c
// my_app.c
#include "akira_api.h"

AKIRA_EXPORT(main)
int main(void)
{
    log(LOG_LEVEL_INF, "Hello from my custom app!");
    
    // Your code here
    display_clear(COLOR_BLUE);
    display_text(10, 10, "My Custom App", COLOR_WHITE);
    
    return 0;
}
```

### Step 2: Create Manifest

Create a corresponding `.json` manifest file:

```json
{
  "name": "my_app",
  "version": "1.0.0",
  "capabilities": [
    "display.write",
    "input.read"
  ],
  "memory_quota": 65536
}
```

### Step 3: Build

```bash
./build_wasm_apps.sh my_app
```

### Step 4: Deploy and Run

Copy to your AkiraOS device and run:
```bash
app start my_app
```

---

## Manifest Format

Manifest files define application metadata and security capabilities.

### Structure

```json
{
  "name": "app_name",
  "version": "1.0.0",
  "capabilities": [
    "capability1",
    "capability2"
  ],
  "memory_quota": 65536
}
```

### Available Capabilities

| Capability | Description |
|------------|-------------|
| `display.write` | Access to display/graphics functions |
| `input.read` | Read input devices and logging |
| `input.write` | Write to input subsystem |
| `gpio.read` | Read GPIO pins |
| `gpio.write` | Write GPIO pins |
| `sensor.read` | Read sensor data (IMU, environmental) |
| `rf.transceive` | RF transceiver control |
| `bt_shell` | Bluetooth shell access |
| `storage.read` | Read from storage |
| `storage.write` | Write to storage |
| `network` | Network access |

### Memory Quota

- **Default:** 64 KB (65536 bytes)
- **Maximum:** 128 KB (131072 bytes)
- Set to `0` for unlimited (use with caution)

**Example:**
```json
"memory_quota": 131072  // 128 KB for complex apps like games
```

---

## API Reference

For detailed API documentation, see [docs/api-reference/native-api.md](../docs/api-reference/native-api.md)

### Quick Reference

#### Logging

```c
int log(uint32_t level, const char *message);
// Levels: LOG_LEVEL_ERR, LOG_LEVEL_WRN, LOG_LEVEL_INF, LOG_LEVEL_DBG
```

#### Display (RGB565 Colors)

```c
int display_clear(uint32_t color);
int display_pixel(int32_t x, int32_t y, uint32_t color);
int display_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
int display_text(int32_t x, int32_t y, const char *text, uint32_t color);
int display_text_large(int32_t x, int32_t y, const char *text, uint32_t color);
```

**Color Constants:** `COLOR_RED`, `COLOR_GREEN`, `COLOR_BLUE`, `COLOR_WHITE`, `COLOR_BLACK`, etc.

#### GPIO

```c
int gpio_configure(uint32_t pin, uint32_t flags);
int gpio_read(uint32_t pin);
int gpio_write(uint32_t pin, uint32_t value);
```

**Flags:** `GPIO_INPUT`, `GPIO_OUTPUT`, `GPIO_PULL_UP`, `GPIO_PULL_DOWN`, etc.

#### Sensors

```c
int sensor_read(int32_t channel);
// Channels: SENSOR_CHAN_ACCEL_X/Y/Z, SENSOR_CHAN_GYRO_X/Y/Z,
//           SENSOR_CHAN_AMBIENT_TEMP, SENSOR_CHAN_HUMIDITY, SENSOR_CHAN_PRESS,
//           SENSOR_CHAN_VOLTAGE, SENSOR_CHAN_CURRENT, ... (Zephyr channel IDs)
// Returns value x1000 on success, AKIRA_SENSOR_ERROR on failure
```

#### Memory

```c
uint32_t mem_alloc(uint32_t size);
void mem_free(uint32_t ptr);
```

---

## AOT Compilation

Ahead-of-Time (AOT) compilation can significantly improve WASM execution performance (10-50x faster than interpreter mode).

### Prerequisites

- **wamrc** compiler from WAMR
- Target architecture specification

### Compilation

```bash
# For ESP32-S3 (Xtensa)
wamrc --target=xtensa --cpu=esp32s3 -o app.aot app.wasm

# For STM32/nRF (ARM Cortex-M)
wamrc --target=thumbv7 --cpu=cortex-m4 -o app.aot app.wasm

# For native_sim (x86_64)
wamrc --target=x86_64 -o app.aot app.wasm
```

### Loading AOT Files

AOT files (`.aot`) can be loaded by AkiraOS runtime just like `.wasm` files:

```bash
app start app.aot
```

### Performance Comparison

| Mode | Speed | Binary Size | Use Case |
|------|-------|-------------|----------|
| Interpreter | 70% native | Smaller | Development, testing |
| AOT | 10-50x faster | Larger | Production, games |

**Note:** AOT compilation requires `CONFIG_WAMR_AOT_SUPPORT=y` in AkiraOS build configuration.

---

## Troubleshooting

### Build Errors

**Error: `WASI SDK not found`**
- Solution: Install WASI SDK or set `WASI_SDK` environment variable
```bash
export WASI_SDK=/path/to/wasi-sdk
```

**Error: `undefined symbol: function_name`**
- Solution: Check that the function is declared in `akira_api.h` and enabled in runtime configuration

**Error: `import module 'env' not found`**
- Solution: Ensure runtime has registered native symbols (check `src/api/akira_export_api.c`)

### Runtime Errors

**Error: `Permission denied` or `-EPERM`**
- Solution: Check manifest capabilities - add required capability to `.json` file

**Error: `Out of memory`**
- Solution: Increase `memory_quota` in manifest or optimize memory usage

**Error: `App failed to start`**
- Solution: Check logs with `app info <name>` and verify WASM integrity

### Display Issues

**Problem: Nothing shows on display**
- Check: Ensure `display.write` capability is present
- Check: Display initialization in AkiraOS (see board config)
- Try: Run `display_test` app to verify display functionality

**Problem: Colors look wrong**
- Note: All colors use RGB565 format (16-bit)
- Use: Color constants from `akira_api.h` (e.g., `COLOR_RED`)

### GPIO Issues

**Problem: GPIO not responding**
- Check: Pin numbers are logical (mapped by runtime)
- Check: `gpio.read`/`gpio.write` capabilities in manifest
- Verify: Hardware connections and board configuration

---

## Additional Resources

- **Main Documentation:** [docs/](../docs/)
- **Native API Reference:** [docs/api-reference/native-api.md](../docs/api-reference/native-api.md)
- **Manifest Format:** [docs/api-reference/manifest-format.md](../docs/api-reference/manifest-format.md)
- **Building Apps Guide:** [docs/development/building-apps.md](../docs/development/building-apps.md)
- **Runtime Architecture:** [docs/architecture/runtime.md](../docs/architecture/runtime.md)
- **Security Model:** [docs/architecture/security.md](../docs/architecture/security.md)
- **AOT Compilation Guide:** [docs/architecture/aot-compilation.md](../docs/architecture/aot-compilation.md)

---

## Examples Gallery

### Hello World Output
```
=================================
  Hello from AkiraOS WASM!
=================================

[DEBUG] This is a debug message
[INFO]  This is an info message
[WARN]  This is a warning message
[ERROR] This is an error message

WASM app executed successfully!
```

### GPIO Control Pattern
```
Input LOW  -> Setting output LOW
Input HIGH -> Setting output HIGH
Input LOW  -> Setting output LOW
```

### Tetris Gameplay
- 10x20 game board
- Next piece preview
- Real-time score and level display
- Colorful tetromino pieces
- Smooth gameplay with level-based speed

---

## Contributing

We welcome contributions! To add new example apps:

1. Create `.c` source file and `.json` manifest
2. Follow existing code style and patterns
3. Test thoroughly on target hardware
4. Update this README with app description
5. Submit pull request

---

## License

Copyright (c) 2026 AkiraOS Contributors  
Licensed under Apache-2.0

---

**Happy WASM Development! 🚀**
 