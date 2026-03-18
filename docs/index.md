---
layout: default
title: Home
nav_order: 1
description: "AkiraOS - High-Performance WebAssembly OS for Embedded Systems"
permalink: /
---

# AkiraOS Documentation

<div align="center">

<img src="AKIRAOS-LOGO-removebg-preview.png" alt="AkiraOS Logo" width="300"/>

</div>

**High-Performance WebAssembly OS for Embedded Systems**

*Production-ready embedded OS combining Zephyr RTOS with WebAssembly sandboxed execution.*

---

## Quick Start

New to AkiraOS? Get up and running in under 15 minutes:

1. [Installation Guide](getting-started/installation.md) — Set up development environment
2. [Build Your First App](getting-started/first-app.md) — Hello World in WASM
3. [SDK Best Practices](development/best-practices.md) — Write better apps from day one
4. [Troubleshooting](getting-started/troubleshooting.md) — Common issues and fixes

Already familiar? Jump to:
- [Architecture Documentation](architecture) — System design deep dive
- [API Reference](api-reference) — Complete WASM API docs
- [SDK API Reference](development/sdk-api-reference.md) — Full Akira SDK function reference
- [Platform Guides](platform) — Board-specific setup

---

## What is AkiraOS?

AkiraOS is a production-ready embedded operating system that enables **secure, dynamic application execution** on resource-constrained devices through WebAssembly.

### Core Features

- **Zero-Trust Execution** — WASM sandboxing + capability-based access control
- **Real-Time Performance** — Zephyr RTOS with <60 ns native call overhead
- **AOT Compilation** — Optional native code execution (10–50x faster than interpreter)
- **OTA Updates** — MCUboot secure firmware updates with rollback
- **Multi-Platform** — ESP32, nRF54L15, STM32, native simulation
- **Modular Connectivity** — WiFi, Bluetooth, USB
- **Rich SDK** — Display, sensors, RF, storage, networking, BLE, HID, and system APIs

### Key Metrics

| Metric | Value |
|--------|-------|
| Native call overhead | ~60 ns |
| WASM load time (100 KB app) | ~80 ms |
| Max concurrent apps | 2 (configurable per board) |
| Memory per app | 64–256 KB configurable |
| Boot time | ~500 ms (ESP32-S3) |

[Learn more about the architecture](architecture)

---

## Documentation Sections

### [Getting Started](getting-started)

Install, build, and deploy your first WASM application.

- [Installation Guide](getting-started/installation.md) — Environment setup
- [First App Tutorial](getting-started/first-app.md) — Hello World
- [Troubleshooting](getting-started/troubleshooting.md) — Common issues

---

### [Architecture](architecture)

Deep dive into system design, components, and data flows.

- [System Overview](architecture/system-overview.md) — Complete architecture
- [Connectivity Layer](architecture/connectivity.md) — Network protocols
- [AkiraRuntime](architecture/runtime.md) — WASM execution environment
- [AOT Compilation](architecture/aot-compilation.md) — Native code compilation for 10–50x performance
- [Security Model](architecture/security.md) — Capability system
- [Data Flow](architecture/data-flow.md) — End-to-end diagrams

---

### [API Reference](api-reference)

Complete native API documentation for WASM applications.

- [API Overview](api-reference) — Quick reference
- [Native API](api-reference/native-api.md) — All functions
- [Manifest Format](api-reference/manifest-format.md) — App metadata
- [Error Codes](api-reference/error-codes.md) — Return values

---

### [SDK Developer Guide](development)

Complete Akira SDK documentation for WASM app development.

- [SDK API Reference](development/sdk-api-reference.md) — All SDK functions with examples
- [Best Practices](development/best-practices.md) — Patterns for efficient, maintainable code
- [Building WASM Apps](development/building-apps.md) — Complete build workflow
- [Debugging](development/debugging.md) — Debug techniques and tools
- [OTA Updates](development/ota-updates.md) — Over-the-air firmware deployment
- [SDK Troubleshooting](development/sdk-troubleshooting.md) — Debug common app issues

---

### [Platform Guides](platform)

Board-specific setup and configuration.

- [Platform Overview](platform) — Supported platforms
- [ESP32-S3 Guide](platform/esp32-s3.md) — Primary platform
- [ESP32-C3 Guide](platform/esp32-c3.md) — RISC-V variant
- [Native Simulation](platform/native-sim.md) — Host PC testing
- [nRF54L15 Guide](platform/nrf54l15.md) — Nordic BLE
- [STM32 Guide](platform/stm32.md) — Experimental

---

### [Hardware](hardware)

Custom hardware designs and schematics.

- [AkiraConsole & AkiraMicro](hardware) — Custom hardware platforms

---

### [Resources](resources)

Additional learning materials and references.

- [FAQ](resources/faq.md) — Frequently asked questions
- [Glossary](resources/glossary.md) — Technical terms
- [Performance](resources/performance.md) — Benchmarks

---

## Use Cases

**IoT Gateways**
- Sensor data collection and cloud forwarding
- OTA updates for remote devices

**Wearables and Handhelds**
- Gaming consoles (AkiraConsole)
- Fitness trackers (AkiraMicro)

**Industrial Automation**
- Programmable logic with WASM
- Secure remote updates
- Multi-protocol connectivity

**Education and Research**
- WASM on embedded systems
- RTOS development
- Security sandboxing research

---

## System Requirements

| Component | Minimum | Recommended |
|-----------|---------|-------------|
| RAM | 256 KB | 512 KB + PSRAM |
| Flash | 2 MB | 8 MB+ |
| CPU | ARM Cortex-M33 | Dual-core @ 240 MHz |
| Connectivity | None | WiFi + BLE |

---

## Contributing

AkiraOS is open source and welcomes contributions.

- **Report Issues:** [GitHub Issues](https://github.com/ArturR0k3r/AkiraOS/issues)
- **Discussions:** [GitHub Discussions](https://github.com/ArturR0k3r/AkiraOS/discussions)

---

## License

AkiraOS is licensed under the [GNU General Public License v3.0](https://github.com/ArturR0k3r/AkiraOS/blob/main/LICENSE).

Third-party components: Zephyr RTOS, WASM Micro Runtime (WAMR), MCUboot, ESP-IDF.

---

## External Links

- **Zephyr Project:** [docs.zephyrproject.org](https://docs.zephyrproject.org)
- **WAMR:** [github.com/bytecodealliance/wasm-micro-runtime](https://github.com/bytecodealliance/wasm-micro-runtime)
- **WebAssembly:** [webassembly.org](https://webassembly.org)
