# Building WASM Applications

Complete guide to developing WebAssembly applications for AkiraOS.

## Toolchain Setup

### WASI SDK (Recommended)

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
    log_info("Hello from WASM!");
}
```

**Compile:**
```bash
$WASI_SDK_PATH/bin/clang \
  --target=wasm32-wasi \
  --sysroot=$WASI_SDK_PATH/share/wasi-sysroot \
  -I ~/akira-workspace/AkiraOS/AkiraSDK/include \
  -O3 \
  -Wl,--no-entry \
  -Wl,--export=main \
  -Wl,--allow-undefined \
  -o app.wasm \
  app.c
```

## Optimization

### Size Optimization

```bash
# Use wasm-opt
wasm-opt -Oz app.wasm -o app_optimized.wasm

# Strip debug info
wasm-strip app.wasm
```

### AOT Compilation

**For maximum performance**, compile your `.wasm` to native machine code using WAMR's AOT compiler.

#### Setup wamrc

```bash
# Clone WAMR and build AOT compiler
git clone https://github.com/bytecodealliance/wasm-micro-runtime.git
cd wasm-micro-runtime/wamr-compiler
./build_llvm.sh
mkdir build && cd build
cmake ..
make
sudo cp wamrc /usr/local/bin/
```

#### Compile for ESP32-S3

```bash
wamrc --target=xtensa \
      --cpu=esp32s3 \
      --size-level=3 \
      -o app_esp32s3.aot \
      app.wasm
```

#### Compile for STM32/nRF (ARM Cortex-M)

```bash
wamrc --target=thumbv7 \
      --cpu=cortex-m4 \
      --size-level=3 \
      -o app_stm32.aot \
      app.wasm
```

#### Compile for Native Simulation

```bash
wamrc --target=x86-64 \
      --size-level=3 \
      -o app_native.aot \
      app.wasm
```

**Performance Gain:** 10-50x faster execution compared to interpreter mode.

**Tradeoff:** Architecture-specific binaries (one `.aot` per platform).

See [AOT Compilation Architecture](../architecture/aot-compilation.md) for detailed guide.

## Deployment

### Upload via HTTP

```bash
# Upload .wasm (interpreter mode)
curl -X POST -F "file=@app.wasm" http://192.168.1.100/upload

# Upload .aot (AOT mode - 10-50x faster)
curl -X POST -F "file=@app_esp32s3.aot" http://192.168.1.100/upload

# Hybrid deployment (best of both worlds)
curl -X POST -F "file=@app.wasm" http://192.168.1.100/upload
curl -X POST -F "file=@app_esp32s3.aot" http://192.168.1.100/upload
```

**Hybrid Mode:** Runtime automatically uses `.aot` if available for the current architecture, falls back to `.wasm` otherwise.

## Related Documentation

- [First App Tutorial](../getting-started/first-app.md)
- [Native API Reference](../api-reference/native-api.md)
- [Manifest Format](../api-reference/manifest-format.md)
