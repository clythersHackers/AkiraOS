# Building Your First WASM App

Create, build, and deploy a "Hello World" WebAssembly application on AkiraOS.

## Prerequisites

- AkiraOS firmware flashed and running
- WASM toolchain installed
- Basic C programming knowledge

## Install WASM Toolchain

### Option 1: WASI SDK (Recommended)

```bash
cd ~
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sdk-24.0-x86_64-linux.tar.gz
tar xvf wasi-sdk-24.0-x86_64-linux.tar.gz
export WASI_SDK_PATH=~/wasi-sdk-24.0
```

### Option 2: Emscripten

```bash
cd ~
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

## Hello World App

The **AkiraSDK submodule** (`AkiraSDK/`) already contains a ready-to-build `hello_world` example.
You can copy it as a starting point or browse the other sample apps in `AkiraSDK/wasm_apps/`.

### Step 1: Create Project

```bash
# Copy the example app (recommended starting point)
cp -r ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps/hello_world ~/my_hello_world
cd ~/my_hello_world
```

### Step 2: Write the Code

The SDK provides a single header — include it and all APIs are available:

**hello_world.c:**
```c
#include "akira_api.h"

int main(void) {
    log_info("Hello from WASM!");

    display_clear(COLOR_BLACK);
    display_text(10, 10, "Hello AkiraOS!", COLOR_WHITE, 1);
    display_flush();

    while (1) {
        delay(1000000);  // idle — 1 second
    }
    return 0;
}
```

### Step 3: Create Manifest

The AkiraSDK sample apps include a pre-made `manifest.json`. For a custom app, create:

**hello_world.json:**
```json
{
  "name": "hello_world",
  "version": "1.0.0",
  "author": "Your Name",
  "capabilities": ["display.write", "log"],
  "memory_quota": 65536,
  "description": "My first WASM app"
}
```

### Step 4: Build WASM Binary

**Using the AkiraSDK Makefile (recommended):**
```bash
# From your app directory (copy the Makefile from any AkiraSDK sample):
cp ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps/hello_world/Makefile .
make
```

**Or using the SDK build script (builds all apps in wasm_apps/):**
```bash
cd ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps
./build.sh hello_world       # build a single app
./build.sh                   # build all apps
```

**Or manually with WASI SDK:**
```bash
$WASI_SDK_PATH/bin/clang \
  --target=wasm32-wasi \
  --sysroot=$WASI_SDK_PATH/share/wasi-sysroot \
  -I ~/akira-workspace/AkiraOS/AkiraSDK/include \
  -O3 \
  -Wl,--no-entry \
  -Wl,--export=main \
  -Wl,--allow-undefined \
  -o hello_world.wasm \
  hello_world.c
```

**Verify the output:**
```bash
ls -lh hello_world.wasm
# Should be ~2-5KB

file hello_world.wasm
# Should say: WebAssembly (wasm) binary module
```

### Step 5: Optimize (Optional)

```bash
# Install wasm-opt
sudo apt install binaryen

# Optimize for size
wasm-opt -Oz hello_world.wasm -o hello_world_opt.wasm

# Compare sizes
ls -lh hello_world*.wasm
```

## Deploy to Hardware

### Method 1: HTTP Upload (Recommended)

```bash
# Find ESP32 IP address (check serial console or router)
export ESP32_IP=192.168.1.100

# Upload WASM app
curl -X POST \
  -F "file=@hello_world.wasm" \
  http://$ESP32_IP/upload

# Upload manifest
curl -X POST \
  -F "file=@hello_world.json" \
  http://$ESP32_IP/upload
```

### Method 2: File System Pre-load

```bash
# Copy to FS partition before flashing
cd ~/akira-workspace/AkiraOS
cp ~/my_hello_world/hello_world.wasm storage/apps/
./build.sh -b esp32s3_devkitm_esp32s3_procpu -r all
```

## Run Your App

### Via Shell Command

```bash
# Connect to serial console
west espressif monitor

# In the shell:
uart:~$ wasm load /apps/hello_world.wasm
uart:~$ wasm start hello_world
```

**Expected output:**
```
[00:00:10.523] <inf> wasm: Loading app: /apps/hello_world.wasm
[00:00:10.687] <inf> wasm: App loaded: hello_world
[00:00:10.690] <inf> wasm: Starting app: hello_world
[00:00:10.692] <inf> app: Hello from WASM!
[00:00:10.698] <inf> display: Clear screen: 0x000000
[00:00:10.702] <inf> display: Draw pixel: (100, 50) = 0xFF0000
[00:00:10.705] <inf> wasm: App hello_world exited
```

### Via Autostart

Edit `prj.conf`:
```bash
CONFIG_AKIRA_AUTOSTART_APP="/apps/hello_world.wasm"
```

Rebuild and app will start on boot.

## Next Steps

### Add Input Handling

```c
#include "akira_api.h"

int main(void) {
    log_info("Waiting for button press...");
    
    while (1) {
        uint32_t buttons = input_read_buttons();
        if (buttons & AKIRA_BTN_UP) {
            log_info("Button pressed!");
            break;
        }
        delay(10000);  // 10 ms
    }
    return 0;
}
```

### Read Sensors

```c
#include "akira_api.h"

int main(void) {
    float ax, ay, az;
    sensor_read_accel(&ax, &ay, &az);

    char msg[64];
    snprintf(msg, sizeof(msg), "Accel: %.2f %.2f %.2f", ax, ay, az);
    log_info(msg);
    return 0;
}
```

### Persistent Storage

```c
#include "akira_api.h"

int main(void) {
    int fd = file_open("/data/hello/message.txt", FILE_WRITE | FILE_CREATE);
    if (fd >= 0) {
        const char *data = "Hello, filesystem!";
        file_write(fd, data, strlen(data));
        file_close(fd);
    }
    return 0;
}
```

## Debugging

### Check App Status

```bash
uart:~$ wasm status
```

### View Logs

```bash
uart:~$ log list   # List log sources
uart:~$ log enable akira 4   # Set debug level
```

### Common Issues

**"Failed to load WASM": **Check file exists
```bash
uart:~$ fs ls /apps
```

**"Permission denied":** Missing capability
```json
{
  "capabilities": ["display", "log", "input"]  # Add required caps
}
```

**"Out of memory":** Increase quota
```json
{
  "memory_quota": 131072  # 128KB instead of 64KB
}
```

## Build Automation

The AkiraSDK sample apps ship with ready-made Makefiles. Copy one as a starting point:

```bash
cp ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps/hello_world/Makefile .
```

Or write your own:

```makefile
WASI_SDK = ~/wasi-sdk-24.0
SDK_INCLUDE = ~/akira-workspace/AkiraOS/AkiraSDK/include
CC = $(WASI_SDK)/bin/clang
CFLAGS = --target=wasm32-wasi --sysroot=$(WASI_SDK)/share/wasi-sysroot \
         -I$(SDK_INCLUDE) -O3
LDFLAGS = -Wl,--no-entry -Wl,--export=main -Wl,--allow-undefined

hello_world.wasm: hello_world.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
	wasm-opt -Oz $@ -o $@

clean:
	rm -f hello_world.wasm

deploy:
	curl -X POST -F "file=@hello_world.wasm" http://$(ESP32_IP)/upload

.PHONY: clean deploy
```

**Usage:**
```bash
make                    # Build
make deploy ESP32_IP=192.168.1.100  # Build and upload
```

## API Reference

See [Native API Documentation](../api-reference/native-api.md) for complete list of available functions.

## Related Documentation

- [API Reference](../api-reference/) - Complete native API
- [Manifest Format](../api-reference/manifest-format.md) - Capability specification
- [Development Guide](../development/building-apps.md) - Advanced topics
- [Troubleshooting](troubleshooting.md) - Common issues
