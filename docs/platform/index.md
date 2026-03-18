---
layout: default
title: Platform Support
nav_order: 5
has_children: true
permalink: /platform
---

# Platform Support

AkiraOS supports multiple embedded platforms with varying feature sets.

## Supported Platforms

| Platform | CPU | RAM | PSRAM | WiFi | BLE | Status |
|----------|-----|-----|-------|------|-----|--------|
| [ESP32-S3](esp32-s3.md) | Dual Xtensa LX7 @ 240 MHz | 512 KB | 8 MB | Yes | Yes | Primary |
| [ESP32-C3](esp32-c3.md) | RISC-V @ 160 MHz | 400 KB | No | Yes | No (default) | Supported |
| [nRF54L15](nrf54l15.md) | Cortex-M33 @ 128 MHz | 256 KB | No | No | Yes | Supported |
| [STM32](stm32.md) | Cortex-M4/M7 | 256–512 KB | External | Partial | Partial | Experimental |
| [Native Sim](native-sim.md) | Host CPU | Host | N/A | No | No | Testing only |

## Quick Selection Guide

**For Production Deployment:**
- **ESP32-S3** — Best all-around choice. Large PSRAM for multiple WASM apps, WiFi + Bluetooth connectivity, USB support.

**For BLE-Only Applications:**
- **nRF54L15** — Power-efficient BLE 5.3, excellent sensor integration, Thread/Zigbee capable.

**For Development and Testing:**
- **Native Simulation** — No hardware needed. Quick debugging with GDB. Ideal for algorithm development.

**For STM32 Familiarity:**
- **STM32 Boards** — Experimental support. Limited without external PSRAM.

## Feature Comparison

| Feature | ESP32-S3 | Native Sim | nRF54L15 | STM32 |
|---------|----------|------------|----------|-------|
| Max concurrent WASM apps | 2 | 2 | 1–2 | 1 |
| Max app size | 200 KB | Unlimited | 100 KB | 50 KB |
| OTA updates | Yes (WiFi) | No | Yes (BLE) | Partial (UART) |
| Display support | Yes (SPI) | Yes (SDL2) | Yes (SPI) | Yes (SPI) |
| Sensor support | Yes (I2C/SPI) | Simulated | Yes (I2C/SPI) | Yes (I2C/SPI) |
| Power modes | Deep sleep | N/A | Ultra-low power | Low power |

## Platform-Specific Guides

- [ESP32-S3 Setup Guide](esp32-s3.md) — Detailed ESP32-S3 configuration
- [ESP32-C3 Setup Guide](esp32-c3.md) — RISC-V variant
- [Native Simulation Guide](native-sim.md) — Running on host PC
- [nRF54L15 Guide](nrf54l15.md) — Nordic platform setup
- [STM32 Guide](stm32.md) — STM32 experimental support

## Getting Started

1. Choose your platform from the comparison table above
2. Follow the platform-specific setup guide
3. Build and flash firmware
4. Deploy WASM applications

## Adding New Platforms

AkiraOS can be ported to any Zephyr-supported board. Requirements:
- Zephyr RTOS support
- Minimum 256 KB RAM
- 2 MB flash for firmware
- Optional: PSRAM for multiple concurrent apps

## Related Documentation

- [Installation Guide](../getting-started/installation.md) — Environment setup
- [Architecture Overview](../architecture) — System design
- [Hardware](../hardware) — Custom hardware designs
