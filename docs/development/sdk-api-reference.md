# Akira SDK API Reference

> **The canonical API reference lives in the [AkiraSDK submodule](https://github.com/ArturR0k3r/AkiraSDK/blob/v1.4.x/docs/API_REFERENCE.md).**
>
> To avoid duplicating content that changes with every SDK release, the full API reference is maintained in AkiraSDK. Refer to it directly — the local copy in the submodule is always in sync.

## Where to Find the API Reference

| Location | Path |
|----------|------|
| **Online (canonical)** | [github.com/ArturR0k3r/AkiraSDK — API_REFERENCE.md](https://github.com/ArturR0k3r/AkiraSDK/blob/v1.4.x/docs/API_REFERENCE.md) |
| **Local submodule** | `AkiraSDK/docs/API_REFERENCE.md` |
| **Header file** | `AkiraSDK/include/akira_api.h` |

## Quick Start

Include the single SDK header in every app:

```c
#include "akira_api.h"
```

## API Groups

| Group | Capability | Key Functions |
|-------|-----------|---------------|
| **Display** | `display.write` | `display_clear`, `display_flush`, `display_text`, `display_rect`, `display_circle`, `display_image` |
| **GPIO** | `gpio.read` / `gpio.write` | `gpio_read`, `gpio_write`, `gpio_set_pull` |
| **Sensors** | `sensor.read` | `sensor_read_accel`, `sensor_read_gyro`, `sensor_read_mag`, `sensor_read_temp` |
| **Timer** | `timer` | `timer_create`, `timer_start`, `timer_elapsed`, `delay` |
| **BLE** | `ble` | `ble_init`, `ble_service_create`, `ble_char_create`, `ble_advertise`, `ble_event_pop` |
| **HID** | `hid` | `hid_keyboard_send`, `hid_mouse_move`, `hid_gamepad_send`, `hid_consumer_send` |
| **Storage** | `storage.read` / `storage.write` | `file_open`, `file_read`, `file_write`, `file_close`, `file_list` |
| **Networking** | `network` | `net_connect`, `net_send`, `net_recv`, `net_close` |
| **IPC** | `ipc` | `ipc_publish`, `ipc_subscribe`, `ipc_recv` |
| **UART** | `uart` | `uart_open`, `uart_write`, `uart_read`, `uart_close` |
| **I2C** | `i2c` | `i2c_write`, `i2c_read` |
| **PWM** | `pwm` | `pwm_set` |
| **System / Lifecycle** | `app.control` / `app.info` | `app_start`, `app_stop`, `app_list`, `sys_info` |
| **Memory** | `memory` | `mem_alloc`, `mem_free` |
| **Logging** | _(always available)_ | `log_info`, `log_debug`, `log_error` |

## Related Documentation

- [Best Practices](best-practices.md) — Patterns for efficient, maintainable apps
- [SDK Troubleshooting](sdk-troubleshooting.md) — Debug common app issues
- [Building WASM Apps](building-apps.md) — Build toolchain and workflow
- [Manifest Format](../api-reference/manifest-format.md) — Capability declarations
- [Native API Reference](../api-reference/native-api.md) — Low-level OS-side API
- [Sample Apps](https://github.com/ArturR0k3r/AkiraSDK/tree/v1.4.x/wasm_apps) — Reference implementations
