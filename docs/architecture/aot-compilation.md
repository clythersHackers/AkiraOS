# AOT Compilation Architecture

**Ahead-of-Time compilation for high-performance WebAssembly execution.**

## Overview

AkiraOS includes AOT (Ahead-of-Time) compilation support through WAMR, enabling WASM applications to be compiled to native machine code before deployment. This provides dramatically improved execution performance compared to bytecode interpretation.

### Current State

**✅ AOT Compiler Integration:** WAMR AOT support is compiled in for Xtensa/ESP32-S3  
**📦 Default Mode:** Applications currently run in interpreter mode (.wasm bytecode)  
**🚀 Available Capability:** AOT execution is ready to use when .aot files are provided

**Key Point:** AOT is a **compilation option**, not a runtime mode. The system can load both .wasm (interpreted) and .aot (native) files.

---

## Performance Comparison

### Interpreter vs AOT

| Execution Mode | Speed | Memory | Universality |
|----------------|-------|--------|--------------|
| **Interpreter (.wasm)** | 1x baseline | Lower overhead | Universal binary |
| **AOT (.aot)** | **10-50x faster** | Slightly higher | Architecture-specific |

### Real-World Impact

**Compute-Heavy Apps:**
- Image processing: ~40x faster
- Physics simulations: ~25x faster
- Cryptography: ~15x faster
- Game logic: ~10-15x faster

**Benefits:**
- ⚡ **Near-native CPU performance** - Direct machine code execution
- 🔋 **Lower power consumption** - Reduced per-instruction overhead
- 🎮 **Smooth UX** - 60 FPS display rendering, responsive games
- 💾 **Predictable performance** - No interpreter overhead variability

---

## Architecture-Specific Binaries

### The Tradeoff

AOT binaries are **native machine code** compiled for a specific CPU architecture. This means:

```
app.wasm (universal)
  ↓
wamrc --target=xtensa    →  app_esp32s3.aot   (ESP32-S3 only)
wamrc --target=thumbv7   →  app_stm32.aot     (STM32/ARM Cortex-M only)
wamrc --target=x86-64    →  app_native.aot    (Native sim only)
```

**Implications:**
- ✅ Maximum performance on target hardware
- ❌ Need one binary per architecture
- ❌ Larger storage if supporting multiple platforms
- ✅ Runtime can detect and use correct binary

---

## Deployment Strategies

AkiraOS supports three AOT deployment models:

### 1. Interpreter-Only (.wasm)

**Use Case:** Universal apps, prototyping, low-memory devices

```
app.wasm  →  Upload to device  →  WAMR interprets bytecode
```

**Pros:**
- ✅ Single binary works on all platforms
- ✅ Smaller file size (no native code bloat)
- ✅ Faster compile times during development
- ✅ Lower storage requirements

**Cons:**
- ❌ Slower execution (1x baseline)
- ❌ Higher CPU usage for same workload
- ❌ More battery drain on mobile devices

**Best For:**
- Simple UI apps
- Configuration scripts
- Low-frequency sensor polling
- Prototyping and development

---

### 2. AOT-Only (.aot)

**Use Case:** Performance-critical apps, single-platform deployment

```
app.c  →  clang → app.wasm  →  wamrc → app.aot  →  Upload to device
```

**Pros:**
- ✅ Maximum performance (10-50x faster)
- ✅ Lower power consumption
- ✅ Optimal for compute-heavy workloads

**Cons:**
- ❌ Architecture-specific binary
- ❌ Larger file size (~2-3x bigger than .wasm)
- ❌ Must recompile for each platform

**Best For:**
- Gaming engines
- Display rendering (60 FPS graphics)
- Real-time audio/video processing
- Machine learning inference
- Production apps on known hardware

---

### 3. Hybrid Deployment (.wasm + .aot)

**Use Case:** Production apps, multi-platform support, graceful degradation

```
Upload both app.wasm and app_esp32s3.aot
  ↓
Runtime checks available .aot for current arch
  ↓
Found?  → Load .aot (fast path)
Not found?  → Fall back to .wasm interpreter
```

**Pros:**
- ✅ Best of both worlds
- ✅ AOT performance where available
- ✅ Universal fallback for unsupported platforms
- ✅ Graceful degradation

**Cons:**
- ❌ Requires storing multiple files
- ❌ More complex build pipeline
- ❌ Higher storage usage

**Best For:**
- Most serious WAMR deployments
- Production app stores
- Cross-platform SDK development
- Defense-in-depth (compile-time + runtime security)

**Implementation Notes:**
- Runtime auto-detects `.aot` files by naming convention: `app_<arch>.aot`
- Falls back to `app.wasm` if no matching AOT binary found
- No code changes needed—purely deployment-side decision

---

## Using WAMR AOT Compiler (wamrc)

### Installation

```bash
# Clone WAMR repository
git clone https://github.com/bytecodealliance/wasm-micro-runtime.git
cd wasm-micro-runtime/wamr-compiler

# Build wamrc
./build_llvm.sh
mkdir build && cd build
cmake ..
make
sudo cp wamrc /usr/local/bin/
```

### Compilation Examples

#### ESP32-S3 (Xtensa)

```bash
wamrc --target=xtensa \
      --cpu=esp32s3 \
      --size-level=3 \
      -o app_esp32s3.aot \
      app.wasm
```

#### STM32/nRF (ARM Cortex-M)

```bash
wamrc --target=thumbv7 \
      --cpu=cortex-m4 \
      --size-level=3 \
      -o app_stm32.aot \
      app.wasm
```

#### Native Simulation (x86-64)

```bash
wamrc --target=x86-64 \
      --size-level=3 \
      -o app_native.aot \
      app.wasm
```

### Optimization Flags

| Flag | Effect |
|------|--------|
| `--size-level=0` | No optimization (debug) |
| `--size-level=1` | Basic optimization |
| `--size-level=2` | Aggressive optimization |
| `--size-level=3` | Maximum size reduction |
| `--enable-simd` | Enable SIMD instructions |
| `--disable-aux-stack-check` | Remove stack overflow checks (faster, less safe) |

---

## Runtime Loading (Developer View)

### Current Implementation

```c
// src/runtime/akira_runtime.c (simplified)

int akira_load_app(const char *name, const char *file_path) {
    // Step 1: Check if .aot file exists
    char aot_path[128];
    snprintf(aot_path, sizeof(aot_path), "/lfs/%s_%s.aot", name, ARCH_NAME);
    
    bool is_aot = false;
    if (fs_exists(aot_path)) {
        file_path = aot_path;
        is_aot = true;
    }
    
    // Step 2: Load file into buffer
    uint8_t *buffer;
    uint32_t size;
    load_file_chunked(file_path, &buffer, &size);
    
    // Step 3: Load module (WAMR auto-detects .aot vs .wasm)
    wasm_module_t module = wasm_runtime_load(buffer, size, error_buf, sizeof(error_buf));
    
    // Step 4: Instantiate and run
    wasm_module_inst_t instance = wasm_runtime_instantiate(module, stack_size, heap_size, ...);
    
    LOG_INF("Loaded %s in %s mode", name, is_aot ? "AOT" : "interpreter");
    return 0;
}
```

**Key Points:**
- WAMR auto-detects file format (magic bytes differ)
- `.wasm` files: `0x00 0x61 0x73 0x6D` (WASM bytecode)
- `.aot` files: Custom WAMR AOT header
- No API changes needed—transparent to application code

---

## Build Pipeline Integration

### Makefile Example

```makefile
# Build universal .wasm
%.wasm: %.c
	$(WASI_SDK)/bin/clang \
		--target=wasm32-wasi \
		-O3 -Wl,--no-entry \
		-o $@ $<

# Compile AOT for ESP32-S3
%.aot: %.wasm
	wamrc --target=xtensa --cpu=esp32s3 --size-level=3 -o $@ $<

# Hybrid deployment
deploy: app.wasm app_esp32s3.aot
	curl -F "file=@app.wasm" http://$(DEVICE_IP)/upload
	curl -F "file=@app_esp32s3.aot" http://$(DEVICE_IP)/upload
```

---

## Performance Tuning

### When to Use AOT

**AOT is Beneficial:**
- ✅ Tight loops (physics, rendering)
- ✅ Math-heavy operations
- ✅ Frequent function calls
- ✅ Real-time constraints (audio, video)
- ✅ Battery-powered devices

**Interpreter is Fine:**
- ✅ I/O-bound apps (network, sensors)
- ✅ Infrequent execution (configuration)
- ✅ Simple state machines
- ✅ Prototyping/debugging

### Profiling Strategy

1. **Start with interpreter** (.wasm) during development
2. **Profile** with `akira_perf` shell command
3. **Identify bottlenecks** (CPU-bound functions)
4. **Compile AOT** for production
5. **Measure improvement** (should be 10-50x for compute)

---

## Limitations

### AOT-Specific Constraints

1. **Architecture Lock-In**
   - Each `.aot` file works on one CPU architecture only
   - Must maintain separate binaries for multi-platform apps

2. **Storage Overhead**
   - `.aot` files are ~2-3x larger than `.wasm`
   - Hybrid deployment requires both files (3-4x storage)

3. **Debugging Complexity**
   - Native crashes harder to debug than WASM traps
   - Disassembly shows machine code, not WASM opcodes

4. **Compilation Time**
   - AOT compilation adds ~5-30s per module
   - Not suitable for on-device JIT (requires offline compilation)

### WAMR-Specific Notes

- **No JIT:** WAMR does not support JIT (Just-In-Time) compilation—only AOT and interpreter
- **Offline Compilation:** All AOT compilation happens on host PC, not on device
- **Module Caching:** Compiled .aot files can be cached and reused

---

## Future Enhancements

### Planned Features

1. **Auto-Detection Script**
   ```bash
   # akira_compile.sh
   # Detects target platforms and compiles all variants
   ```

2. **Multi-Arch Bundles**
   ```
   app.akira (ZIP container)
   ├── manifest.json
   ├── app.wasm
   ├── esp32s3.aot
   ├── stm32.aot
   └── nrf54.aot
   ```

3. **OTA AOT Updates**
   - Download AOT binary for current architecture
   - Verify signature + hash
   - Hot-swap without reboot

4. **Runtime Performance Metrics**
   ```c
   // Expose via shell
   akira> perf app_name
   Execution mode: AOT (xtensa)
   Avg instruction time: 2.3ns
   Cache hit rate: 94%
   ```

---

## Best Practices

### Development Workflow

1. **Develop with .wasm** (fast compile, portable testing)
2. **Optimize code** (profiling tools, algorithms)
3. **Compile AOT** for production target
4. **Benchmark** (compare interpreter vs AOT performance)
5. **Deploy hybrid** (AOT + .wasm fallback)

### Deployment Checklist

- [ ] `.wasm` compiled with optimizations (`-O3`)
- [ ] `.wasm` stripped of debug info (`wasm-strip`)
- [ ] `.aot` compiled for target architecture
- [ ] `.aot` size-optimized (`--size-level=3`)
- [ ] Both files tested on actual hardware
- [ ] Fallback to .wasm verified
- [ ] Performance metrics captured

---

## Related Documentation

- [AkiraRuntime Architecture](runtime.md) - Runtime internals
- [Building WASM Apps](../development/building-apps.md) - Compilation workflow
- [Performance Benchmarks](../resources/performance.md) - Real-world metrics
- [Platform Guides](../platform/) - Architecture-specific notes

---

## External References

- [WAMR AOT Documentation](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/doc/build_wamr.md#aot-compiler)
- [wamrc User Guide](https://github.com/bytecodealliance/wasm-micro-runtime/blob/main/wamr-compiler/README.md)
- [WebAssembly Binary Format](https://webassembly.github.io/spec/core/binary/index.html)

---

<div align="center">

**Ready to compile AOT binaries?**

[Building Apps Guide →](../development/building-apps.md)

</div>
