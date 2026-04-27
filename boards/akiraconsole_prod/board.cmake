# SPDX-License-Identifier: Apache-2.0

if(NOT "${OPENOCD}" MATCHES "^${ESPRESSIF_TOOLCHAIN_PATH}/.*")
  set(OPENOCD OPENOCD-NOTFOUND)
endif()
find_program(OPENOCD openocd PATHS ${ESPRESSIF_TOOLCHAIN_PATH}/openocd-esp32/bin NO_DEFAULT_PATH)
include(${ZEPHYR_BASE}/boards/common/esp32.board.cmake)
include(${ZEPHYR_BASE}/boards/common/openocd.board.cmake)

# MCUboot slot0_partition is at 0x20000.
board_runner_args(esp32 "--esp-app-address=0x20000")

# Flash geometry for ESP32-S3-WROOM-1-N16R8 (16MB flash, DIO, 40MHz).
board_runner_args(esp32 "--esp-flash-size=16MB")
board_runner_args(esp32 "--esp-flash-mode=dio")
board_runner_args(esp32 "--esp-flash-freq=40m")

# WARNING: Do NOT pass --erase to "west flash".
# Full chip erase destroys the NVS settings partition and LittleFS filesystem.
