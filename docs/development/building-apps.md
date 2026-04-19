# Building WASM Applications

Complete guide to developing WebAssembly applications for AkiraOS.

## Toolchain Setup

### WASI SDK (build toolchain only)

The WASI SDK provides the clang/lld toolchain used to compile `.wasm` binaries. AkiraOS apps target `wasm32-unknown-unknown` — **not** the WASI runtime. No WASI system calls are used at runtime.

```bash
cd ~
wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-24/wasi-sdk-24.0-x86_64-linux.tar.gz
tar xvf wasi-sdk-24.0-x86_64-linux.tar.gz
export WASI_SDK_PATH=~/wasi-sdk-24.0
```

## Build Process

### Using AkiraSDK (Recommended)

The `AkiraSDK/` submodule includes `include/akira_api.h` and ready-to-build sample apps in `AkiraSDK/wasm_apps/`.

```bash
# Build all sample apps
cd ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps
./build.sh

# Build a single app
./build.sh hello_world
```

### Simple Custom App

```c
// app.c
#include "akira_api.h"

int main(void) {
    printf("Hello from WASM!");
    return 0;
}
```

**Compile:**
```bash
$WASI_SDK_PATH/bin/clang \
  -target wasm32-unknown-unknown \
  -nostdlib \
  -fvisibility=hidden \
  -Wl,--no-entry \
  -Wl,--export=main \
  -Wl,--allow-undefined \
  -Wl,--strip-all \
  -z stack-size=4096 \
  -Wl,--initial-memory=65536 \
  -Wl,--max-memory=65536 \
  -I ~/akira-workspace/AkiraOS/AkiraSDK/include \
  -O2 \
  -o app.wasm \
  app.c
```

**Important:** Use `wasm32-unknown-unknown` (bare-metal), not `wasm32-wasi`. AkiraOS provides its own API layer without WASI system calls.

### Manifest File

Create `manifest.json` alongside your app:

```json
{
  "name": "my_app",
  "version": "1.0.0",
  "capabilities": ["display.write"],
  "memory_quota": 65536
}
```

**Embedding Manifest:**

The AkiraSDK build script automatically embeds the manifest as a custom section:

```bash
# Done automatically by build.sh if manifest.json exists
python3 AkiraSDK/scripts/embed_manifest.py app.wasm manifest.json app.wasm
```

This allows the runtime to extract capabilities without requiring a separate JSON file.

### Build Output

```
AkiraSDK/wasm_apps/bin/
├── app.wasm              # WASM bytecode (2-10 KB, universal)
├── app.json              # Standalone manifest
├── app-xtensa.aot        # AOT for ESP32-S3 (10-50 KB)
├── app-thumb.aot         # AOT for nRF54L15
├── app-riscv32.aot       # AOT for ESP32-C3
└── app-x86_64.aot        # AOT for native_sim
```

## Memory Configuration

WASM apps have two separate stack limits:

**1. WASM Linear Memory Stack:** 4KB (set by `-z stack-size=4096`)
- Used for WASM local variables and call frames
- Overflow causes WASM trap

**2. Zephyr Thread Stack:** 8KB (`CONFIG_AKIRA_WASM_APP_STACK_SIZE=8192`)
- Used for native API calls (C call chain)
- Handles host function execution

**Heap Memory:**
- Initial/max: 64KB (65536 bytes)
- Cannot grow beyond limit without rebuilding

**Custom Memory Sizes:**
```bash
clang \
  -z stack-size=8192 \
  -Wl,--initial-memory=131072 \
  -Wl,--max-memory=131072 \
  ...
```

## Optimization

### Optimization Levels

```bash
# Balanced (default in build.sh)
clang -O2 -Wl,--strip-all ...

# Size-optimized (smallest binary)
clang -Oz -Wl,--strip-all ...

# Speed-optimized (faster execution)
clang -O3 ...
```

**AOT Optimization:**
```bash
# Default (balanced)
wamrc --opt-level=3 --size-level=1 -o app.aot app.wasm

# Maximum speed (larger binary)
wamrc --opt-level=3 --size-level=0 -o app.aot app.wasm

# Maximum size reduction  
wamrc --opt-level=0 --size-level=3 -o app.aot app.wasm
```

### AOT Compilation

**For maximum performance**, compile your `.wasm` to native machine code using WAMR's AOT compiler.

#### Setup wamrc

Build `wamrc` from the WAMR submodule already included in AkiraOS (required — the AkiraOS fork includes Xtensa backend support):

```bash
cd ~/akira-workspace/AkiraOS/modules/wasm-micro-runtime/wamr-compiler
cmake . -DWAMR_BUILD_PLATFORM=linux
make
sudo cp wamrc /usr/local/bin/
# or: export WAMRC=$PWD/wamrc
```

#### Compile with the AkiraSDK build script (recommended)

```bash
cd ~/akira-workspace/AkiraOS/AkiraSDK/wasm_apps
./build.sh                    # build .wasm
./build.sh aot                # AOT for ESP32-S3 (xtensa, default)
./build.sh aot thumb          # AOT for nRF54L15 (Cortex-M33)
./build.sh aot thumbv7em      # AOT for STM32 (Cortex-M7)
./build.sh aot riscv32        # AOT for ESP32-C3 (RISC-V)
# Output: bin/<app>-<target>.aot
```

#### Or invoke wamrc directly

```bash
# ESP32-S3 (Xtensa LX7)
wamrc --target=xtensa --cpu=esp32s3 -o app-xtensa.aot app.wasm

# nRF54L15 (Cortex-M33)
wamrc --target=thumb --cpu=cortex-m33 -o app-thumb.aot app.wasm

# ESP32-C3 (RISC-V 32)
wamrc --target=riscv32 -o app-riscv32.aot app.wasm

# Native simulation (x86-64)
wamrc --target=x86_64 -o app-x86_64.aot app.wasm
```

**Performance Gain:** 10-50x faster execution compared to interpreter mode.

**Tradeoff:** Architecture-specific binaries (one `.aot` per platform).

See [AOT Compilation Architecture](../architecture/aot-compilation.md) for detailed guide.

## Deployment

### Upload via HTTP

The device provides two upload endpoints:

```bash
# Simple file upload (saved to /lfs/apps/ or /SD:/apps/)
curl -X POST -F "file=@app.wasm" http://192.168.1.100/upload

# Named app installation (recommended)
curl -X POST \
  -F "file=@app.wasm" \
  "http://192.168.1.100/api/apps/install?name=myapp"

# Hybrid deployment with AOT (best performance)
curl -X POST -F "file=@app.wasm" \
  "http://192.168.1.100/api/apps/install?name=myapp"
curl -X POST -F "file=@app-xtensa.aot" \
  "http://192.168.1.100/api/apps/install?name=myapp"
```

**Limits:** Max file size is `CONFIG_AKIRA_APP_MAX_SIZE_KB` (default: 1MB per app).

**Hybrid Mode:** Runtime automatically uses `.aot` if available for the current architecture, falls back to `.wasm` otherwise.

### SD Card Installation

```bash
# Copy apps to SD card
cp bin/*.wasm /media/SD_CARD/apps/
cp bin/*.json /media/SD_CARD/apps/
# Insert SD → apps auto-scanned on boot
```

### USB Storage Installation

```bash
# Copy to USB drive
cp bin/*.wasm /media/USB_DRIVE/apps/
# Connect USB → scan from shell: app scan usb
```

### Shell Commands

```bash
# View installed apps
AkiraOS:~$ app list

# Install from filesystem
AkiraOS:~$ app install /lfs/apps/myapp.wasm

# Start app
AkiraOS:~$ app start myapp

# View running apps
AkiraOS:~$ app status

# Stop app
AkiraOS:~$ app stop myapp
```

## Related Documentation

- [First App Tutorial](../getting-started/first-app.md)
- [SDK API Reference](../../AkiraSDK/docs/API_REFERENCE.md)
- [Manifest Format](../api-reference/manifest-format.md)
- [Best Practices](best-practices.md)
- [Advanced Sample Apps](advanced-apps.md)
