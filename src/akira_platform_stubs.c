/**
 * @file akira_platform_stubs.c
 * @brief Weak no-op stubs for AkiraPlatform extension hooks.
 *
 * AkiraPlatform (or any other overlay module) replaces these with strong
 * implementations at link time. When building without a platform overlay
 * these no-ops are used so the rest of AkiraOS compiles and links cleanly.
 *
 * Declarations live in akira.h.
 *
 * @copyright Copyright (c) 2026 PenEngineering S.R.L
 */

#include <zephyr/kernel.h>
#include "akira_platform_stubs.h"

__weak void akira_on_app_installed(const char *name, int id, const char *version)
{
	ARG_UNUSED(name);
	ARG_UNUSED(id);
	ARG_UNUSED(version);
}

__weak void akira_on_app_uninstalled(const char *name)
{
	ARG_UNUSED(name);
}

__weak void akira_on_app_started(const char *name, int id)
{
	ARG_UNUSED(name);
	ARG_UNUSED(id);
}

__weak void akira_on_app_crashed(const char *name, int exit_code)
{
	ARG_UNUSED(name);
	ARG_UNUSED(exit_code);
}

__weak void akira_on_wifi_connected(void)
{
}
