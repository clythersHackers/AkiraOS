/*
 * Copyright (c) 2025 AkiraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-only
 */

/**
 * @file boot_anim.h
 * @brief AkiraOS boot logo animation.
 *
 * Plays a three-phase entrance animation on the display:
 *   Phase 1 – the star logo grows from a point to full size (centre screen).
 *   Phase 2 – the star shrinks slightly then slides to the left quarter.
 *   Phase 3 – "AkiraOS" text slides in from the right edge to its resting
 *              position next to the star.
 *
 * The animation uses only the platform-agnostic display primitives so it
 * works on any board that has CONFIG_DISPLAY enabled.
 */

#ifndef AKIRA_BOOT_ANIM_H
#define AKIRA_BOOT_ANIM_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the full boot logo animation.
 *
 * Blocking call – returns once the animation is complete.
 * Should be called from main() after akira_hal_init() succeeds, inside the
 * CONFIG_AKIRA_BOOT_ANIMATION guard block.
 */
void boot_anim_run(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_BOOT_ANIM_H */
