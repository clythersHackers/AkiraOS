/*
 * stub_mcuboot.c — MCUboot stubs for AkiraOS OTA tests.
 *
 * Provides weak-linked replacements for boot_request_upgrade() and
 * boot_write_img_confirmed() so ota_manager.c compiles and links on
 * native_sim without CONFIG_MCUBOOT_IMG_MANAGER.
 *
 * Tests can override the return codes via the exported variables below.
 */

#include <zephyr/kernel.h>

/* Defined in <zephyr/dfu/mcuboot.h> when MCUBOOT_IMG_MANAGER is on.
 * Provide the constant here for the case when it is off. */
#ifndef BOOT_UPGRADE_TEST
#define BOOT_UPGRADE_TEST 0
#endif
#ifndef BOOT_UPGRADE_PERMANENT
#define BOOT_UPGRADE_PERMANENT 1
#endif

/* Test-controllable return values ----------------------------------------- */
int stub_boot_request_upgrade_rc;     /* 0 = succeed, non-zero = fail */
int stub_boot_write_img_confirmed_rc; /* 0 = succeed, non-zero = fail */

/* Weak stubs ---------------------------------------------------------------- */
int __attribute__((weak)) boot_request_upgrade(int permanent)
{
    (void)permanent;
    return stub_boot_request_upgrade_rc;
}

int __attribute__((weak)) boot_write_img_confirmed(void)
{
    return stub_boot_write_img_confirmed_rc;
}
