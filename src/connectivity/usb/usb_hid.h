/**
 * @file usb_hid.h
 * @brief USB HID Transport Header for HID Manager
 * @stability experimental
 * @since 1.4
 */

#ifndef USB_HID_H
#define USB_HID_H

#include <zephyr/kernel.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*===========================================================================*/
/* Raw HID (Report ID 3) constants                                          */
/*===========================================================================*/

/** Total bytes submitted to hid_device_submit_report (ID + payload) */
#define USB_HID_RAW_REPORT_SIZE 64
/** Usable payload bytes in each raw report */
#define USB_HID_RAW_PAYLOAD_SIZE 63
/** Report ID for vendor raw channel */
#define USB_HID_RAW_REPORT_ID 3

    /*===========================================================================*/
    /* Raw HID handler type                                                     */
    /*===========================================================================*/

    /**
     * @brief Callback invoked when a raw OUT report (ID 3) arrives from the host.
     *
     * Called from USB interrupt context — must be ISR-safe (no blocking).
     *
     * @param data   Pointer to the 63-byte payload (does NOT include Report ID byte).
     * @param len    Number of valid bytes in @p data (max USB_HID_RAW_PAYLOAD_SIZE).
     */
    typedef void (*usb_hid_raw_handler_t)(const uint8_t *data, uint8_t len);

    /*===========================================================================*/
    /* Public API                                                                */
    /*===========================================================================*/

    int usb_hid_transport_init(void);
    const struct device *usb_hid_get_device(void);
    bool usb_hid_is_ready(void);
    uint8_t usb_hid_get_protocol(void);

    /**
     * @brief Register (or unregister) a handler for raw OUT reports (Report ID 3).
     * @param handler Callback, or NULL to unregister.
     */
    void usb_hid_raw_set_handler(usb_hid_raw_handler_t handler);

    /**
     * @brief Send a raw IN report (Report ID 3) to the host.
     * @param payload  63-byte payload (USB_HID_RAW_PAYLOAD_SIZE bytes).
     * @return 0 on success, -ENOTCONN if USB not ready, -EBUSY if timeout.
     */
    int usb_hid_raw_send(const uint8_t *payload);

#ifdef __cplusplus
}
#endif

#endif /* USB_HID_H */