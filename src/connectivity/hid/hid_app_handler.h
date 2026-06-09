/**
 * @file hid_app_handler.h
 * @brief AkiraOS USB HID Raw Application Command Handler
 *
 * Implements the AkiraOS WebHID protocol over USB HID Report ID 3.
 *
 * Packet format (63 payload bytes = USB_HID_RAW_PAYLOAD_SIZE):
 *   Byte 0   CMD    — command / response code
 *   Byte 1   SEQ    — sequence number (echoed in responses)
 *   Byte 2   FLAGS  — 0x01 = more packets follow (multi-packet)
 *   Byte 3   LEN_LO — low byte of payload length
 *   Byte 4   LEN_HI — high byte of payload length
 *   Bytes 5..62 — data payload (58 bytes per packet)
 *
 * Command bytes (host → device, OUT reports):
 *   0x30  GET_INFO     — device info (name, fw version, chip)
 *   0x31  GET_APPS     — installed app list
 *   0x32  GET_STATUS   — storage + uptime
 *   0x33  GET_LOGS     — recent log lines (stub)
 *   0x40  INSTALL_BEGIN  — start chunked install (payload: name + total_size)
 *   0x41  INSTALL_CHUNK  — write data chunk
 *   0x42  INSTALL_END    — finalize install
 *   0x43  INSTALL_ABORT  — abort install
 *   0x50  APP_START    — start app (payload: null-terminated name)
 *   0x51  APP_STOP     — stop app
 *   0x52  APP_DELETE   — uninstall app
 *
 * Response bytes (device → host, IN reports):
 *   CMD | 0x80  — echo of the command with high bit set
 *   STATUS byte 2: 0=OK, non-zero=error code
 * @stability experimental
 * @since 1.5
 */

#ifndef HID_APP_HANDLER_H
#define HID_APP_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ---- Command byte definitions ---- */
#define HID_CMD_GET_INFO 0x30
#define HID_CMD_GET_APPS 0x31
#define HID_CMD_GET_STATUS 0x32
#define HID_CMD_GET_LOGS 0x33

#define HID_CMD_INSTALL_BEGIN 0x40
#define HID_CMD_INSTALL_CHUNK 0x41
#define HID_CMD_INSTALL_END 0x42
#define HID_CMD_INSTALL_ABORT 0x43

#define HID_CMD_APP_START 0x50
#define HID_CMD_APP_STOP 0x51
#define HID_CMD_APP_DELETE 0x52

/* ---- Packet layout offsets ---- */
#define HID_PKT_CMD 0
#define HID_PKT_SEQ 1
#define HID_PKT_FLAGS 2
#define HID_PKT_LEN_LO 3
#define HID_PKT_LEN_HI 4
#define HID_PKT_PAYLOAD 5
#define HID_PKT_PAYLOAD_SIZE 58 /* bytes 5..62 */

/* FLAGS bits */
#define HID_FLAG_MORE_DATA 0x01

    /**
     * @brief Initialize the HID app command handler.
     *
     * Registers the raw OUT report handler with the USB HID transport.
     * Must be called after usb_hid_transport_init().
     *
     * @return 0 on success, negative on error.
     */
    int hid_app_handler_init(void);

#ifdef __cplusplus
}
#endif

#endif /* HID_APP_HANDLER_H */
