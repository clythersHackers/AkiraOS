/**
 * @file hid_manager.h
 * @brief HID Manager for AkiraOS
 *
 * Unified HID device management supporting keyboard and gamepad profiles
 * over multiple transports (BLE, USB, Simulation).
 */

#ifndef AKIRA_HID_MANAGER_H
#define AKIRA_HID_MANAGER_H

#include "hid_common.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /*===========================================================================*/
    /* Configuration                                                             */
    /*===========================================================================*/

    /** HID manager configuration */
    typedef struct
    {
        hid_device_type_t device_types;      /**< Which HID device types to enable */
        hid_transport_t preferred_transport; /**< Preferred transport */
        const char *device_name;             /**< Device name for discovery */
        uint16_t vendor_id;                  /**< USB/BLE vendor ID */
        uint16_t product_id;                 /**< USB/BLE product ID */
    } hid_config_t;

    /*===========================================================================*/
    /* HID Manager API                                                           */
    /*===========================================================================*/

    /**
     * @brief Convert transport enum to string
     * @param transport Transport type
     * @return String representation of transport
     */
    const char *transport_to_string(hid_transport_t transport);

    /**
     * @brief Initialize HID manager
     * @param config Configuration options
     * @return 0 on success, negative error code on failure
     */
    int hid_manager_init(const hid_config_t *config);

    /**
     * @brief Deinitialize HID manager
     * @return 0 on success
     */
    int hid_manager_deinit(void);

    /**
     * @brief Enable HID device
     *
     * Starts advertising (BLE) or enables USB endpoint
     * @return 0 on success
     */
    int hid_manager_enable(void);

    /**
     * @brief Disable HID device
     * @return 0 on success
     */
    int hid_manager_disable(void);

    /**
     * @brief Select active transport
     * @param transport Transport to use
     * @return 0 on success
     */
    int hid_manager_set_transport(hid_transport_t transport);

    /**
     * @brief Get current transport
     * @return Current transport type
     */
    hid_transport_t hid_manager_get_transport(void);

    /**
     * @brief Check if HID is connected to host
     * @return true if connected
     */
    bool hid_manager_is_connected(void);

    /**
     * @brief Get current HID state
     * @return Pointer to state structure
     */
    const hid_state_t *hid_manager_get_state(void);

    /*===========================================================================*/
    /* Keyboard API                                                              */
    /*===========================================================================*/

    /**
     * @brief Press a key
     * @param key HID key code
     * @return 0 on success
     */
    int hid_keyboard_press(hid_key_code_t key);

    /**
     * @brief Release a key
     * @param key HID key code
     * @return 0 on success
     */
    int hid_keyboard_release(hid_key_code_t key);

    /**
     * @brief Release all keys
     * @return 0 on success
     */
    int hid_keyboard_release_all(void);

    /**
     * @brief Set modifier keys
     * @param modifiers Modifier bitmask
     * @return 0 on success
     */
    int hid_keyboard_set_modifiers(uint8_t modifiers);

    /**
     * @brief Type a string (press and release each key)
     * @param str ASCII string to type
     * @return 0 on success
     */
    int hid_keyboard_type_string(const char *str);

    /**
     * @brief Send raw keyboard report
     * @param report Keyboard report
     * @return 0 on success
     */
    int hid_keyboard_send_report(const hid_keyboard_report_t *report);

    /*===========================================================================*/
    /* Gamepad API                                                               */
    /*===========================================================================*/

    /**
     * @brief Press a gamepad button
     * @param button Button bitmask
     * @return 0 on success
     */
    int hid_gamepad_press(hid_gamepad_btn_t button);

    /**
     * @brief Release a gamepad button
     * @param button Button bitmask
     * @return 0 on success
     */
    int hid_gamepad_release(hid_gamepad_btn_t button);

    /**
     * @brief Set gamepad axis value
     * @param axis Axis index
     * @param value Axis value (-32768 to 32767)
     * @return 0 on success
     */
    int hid_gamepad_set_axis(hid_gamepad_axis_t axis, int16_t value);

    /**
     * @brief Set D-pad/hat position
     * @param direction D-pad direction (0-8, 0=center)
     * @return 0 on success
     */
    int hid_gamepad_set_dpad(uint8_t direction);

    /**
     * @brief Send raw gamepad report
     * @param report Gamepad report
     * @return 0 on success
     */
    int hid_gamepad_send_report(const hid_gamepad_report_t *report);

    /**
     * @brief Reset all gamepad inputs to neutral
     * @return 0 on success
     */
    int hid_gamepad_reset(void);

    /*===========================================================================*/
    /* Mouse API                                                                 */
    /*===========================================================================*/

    /**
     * @brief Move the mouse cursor relatively
     * @param dx X delta (-127..127)
     * @param dy Y delta (-127..127)
     * @return 0 on success
     */
    int hid_mouse_move(int8_t dx, int8_t dy);

    /**
     * @brief Press a mouse button
     * @param button Button bitmask (hid_mouse_btn_t)
     * @return 0 on success
     */
    int hid_mouse_button_press(uint8_t button);

    /**
     * @brief Release a mouse button
     * @param button Button bitmask (hid_mouse_btn_t)
     * @return 0 on success
     */
    int hid_mouse_button_release(uint8_t button);

    /**
     * @brief Scroll the mouse wheel
     * @param delta Scroll delta (-127..127, positive = up)
     * @return 0 on success
     */
    int hid_mouse_scroll(int8_t delta);

    /**
     * @brief Release all mouse buttons and reset movement
     * @return 0 on success
     */
    int hid_mouse_reset(void);

    /*===========================================================================*/
    /* Consumer / Media Key API                                                  */
    /*===========================================================================*/

    /**
     * @brief Send a single HID Consumer key press + release
     *
     * Sends the usage code then immediately clears it so the host
     * sees a complete key press/release cycle.
     *
     * @param usage  HID Consumer Page usage code (hid_consumer_usage_t or raw uint16)
     * @return 0 on success
     */
    int hid_consumer_send(uint16_t usage);

    /*===========================================================================*/
    /* Raw Report API                                                            */
    /*===========================================================================*/

    /**
     * @brief Send a raw HID report via the active transport
     *
     * The caller is responsible for constructing a valid HID report
     * for the given report_id.  Returns -ENOTSUP if the active transport
     * does not implement a raw send handler.
     *
     * @param report_id  HID report ID
     * @param data       Report payload (excluding the report ID byte)
     * @param len        Payload length in bytes
     * @return 0 on success, -ENOTSUP if transport doesn\u2019t support raw sends
     */
    int hid_send_raw_report(uint8_t report_id, const uint8_t *data, size_t len);

    /*===========================================================================*/
    /* Named Action Registry                                                     */
    /*===========================================================================*/

    /**
     * @brief Register a named keyboard shortcut action
     *
     * Stores a (name, modifier, keycode) binding in a static table.
     * The table has CONFIG_AKIRA_HID_MAX_ACTIONS slots.
     *
     * @param name      Action name (null-terminated, max 31 chars)
     * @param modifier  Keyboard modifier bitmask (hid_keyboard_mod_t)
     * @param keycode   HID key code (hid_key_code_t)
     * @return 0 on success, -ENOMEM if table is full, -EINVAL on bad args
     */
    int hid_action_register(const char *name, uint8_t modifier, uint8_t keycode);

    /**
     * @brief Trigger a previously registered named action
     *
     * Looks up the name in the action table, presses the modifier + key,
     * then releases all keys.
     *
     * @param name  Action name
     * @return 0 on success, -ENOENT if name not found
     */
    int hid_action_trigger(const char *name);

    /*===========================================================================*/
    /* Callbacks                                                                 */
    /*===========================================================================*/

    /**
     * @brief Register HID event callback
     * @param callback Callback function
     * @param user_data User data
     * @return 0 on success
     */
    int hid_manager_register_event_callback(hid_event_callback_t callback, void *user_data);

    /**
     * @brief Register output report callback (LED state, rumble, etc.)
     * @param callback Callback function
     * @param user_data User data
     * @return 0 on success
     */
    int hid_manager_register_output_callback(hid_output_callback_t callback, void *user_data);

    /*===========================================================================*/
    /* Transport Registration                                                    */
    /*===========================================================================*/

    /**
     * @brief Register a HID transport
     * @param ops Transport operations
     * @return 0 on success
     */
    int hid_manager_register_transport(const hid_transport_ops_t *ops);

    /**
     * @brief Unregister a HID transport
     * @param name Transport name
     * @return 0 on success
     */
    int hid_manager_unregister_transport(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_HID_MANAGER_H */
