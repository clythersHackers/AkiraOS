---
layout: default
title: Hardware
nav_order: 7
has_children: true
permalink: /hardware
---

# Hardware

AkiraOS custom hardware designs and specifications.

## AkiraConsole

Handheld gaming console powered by AkiraOS.


- [SD Card Setup](sd-card.md)

### Specifications

- **Platform:** ESP32-S3
- **Display:** 2.8" TFT LCD 
- **Input:** D-pad + 4 buttons
- **Storage:** microSD card slot (FAT32, up to 32 GB)
- **Battery:** LiPo rechargeable with fuel gauge and bettery control accesible fromm software
- **Connectivity:** WiFi + Bluetooth + LoRa 2.4 and 868/915 + SubGHz + NFC 

---

## AkiraMicro

Compact wearable platform.

**Coming soon...:** `/docs/AkiraMicro/`

### Specifications

- **Platform:** ESP32-C6
- **Display:** 1.3" OLED
- **Sensors:** ???
- **Battery:** ???
- **Form Factor:** ???

---

## Supported Development Boards

See [Platform Guides](../platform) for supported development kits:

- ESP32 boards
- nRF boards
- STM32 boards

---

## Custom Hardware

AkiraOS can run on custom hardware with:
- Zephyr RTOS support
- Minimum 256KB RAM
- 1MB+ flash

Add board overlays and configuration files under the `boards/` directory to target new hardware.

---

## Schematics & Design Files

Aki hardware designs are open source.

**License:** [CERN Open Hardware License](https://ohwr.org/cern_ohl_s_v2.txt)

---

## Related Documentation

- [Platform Support](../platform) - Software support for boards
- [Getting Started](../getting-started) - Flashing firmware
