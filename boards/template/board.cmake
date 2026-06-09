# SPDX-License-Identifier: Apache-2.0
# FIXME: Rename this file — it must keep the name board.cmake regardless of board name.
#
# Uncomment EXACTLY ONE runner block below that matches your debug/flash probe.
# Delete the others once your choice is confirmed.
# Full runner list: https://docs.zephyrproject.org/latest/develop/west/build-flash-debug.html

# =============================================================================
# Option A: OpenOCD (STM32, generic Cortex-M, FTDI-based probes)
# =============================================================================
# FIXME: Verify openocd.cfg path and interface, then uncomment:
# include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)


# =============================================================================
# Option B: J-Link (Nordic nRF, NXP, STM32 with J-Link probe)
# =============================================================================
# FIXME: Set --device to the J-Link target string for your SoC.
# Find valid strings in J-Link Commander: "ShowDevices" command, or at:
#   https://www.segger.com/supported-devices.html
# Examples:
#   nRF54L15-QFAA-R7 app core:  nRF54L15_xxAA-CortexM33App
#   STM32L476RG:                 STM32L476RG
#   nRF52840:                    nRF52840_xxAA
#
# board_runner_args(jlink "--device=FIXME_JLINK_TARGET")
# board_runner_args(jlink "--speed=4000")
# include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)


# =============================================================================
# Option C: esptool (ESP32 family — requires Espressif toolchain on PATH)
# =============================================================================
# FIXME: Adjust app-address (= slot0_partition offset), flash-size, mode, freq.
# Common values:
#   --esp-flash-size  4MB | 8MB | 16MB
#   --esp-flash-mode  dio | qio | opi   (WROOM-1 modules: dio)
#   --esp-flash-freq  20m | 40m | 80m
#   --esp-app-address 0x20000           (default MCUboot slot0 offset)
#
# if(NOT "${OPENOCD}" MATCHES "^${ESPRESSIF_TOOLCHAIN_PATH}/.*")
#   set(OPENOCD OPENOCD-NOTFOUND)
# endif()
# find_program(OPENOCD openocd
#   PATHS ${ESPRESSIF_TOOLCHAIN_PATH}/openocd-esp32/bin
#   NO_DEFAULT_PATH)
# include(${ZEPHYR_BASE}/boards/common/esp32.board.cmake)
# include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)
#
# board_runner_args(esp32 "--esp-app-address=0x20000")
# board_runner_args(esp32 "--esp-flash-size=FIXME")   # e.g. 4MB
# board_runner_args(esp32 "--esp-flash-mode=dio")     # FIXME: dio | qio | opi
# board_runner_args(esp32 "--esp-flash-freq=40m")     # FIXME: 20m | 40m | 80m
#
# WARNING: Do NOT pass --erase to "west flash".
# Full chip erase destroys NVS settings and LittleFS user data.
