/**
 * @file akira_time_module.h
 * @brief Modular WASM Time API for AkiraOS (WAMR)
 *
 * Declares the registration function for the time module.
 * @stability stable
 * @since 1.4
 */

#ifndef AKIRA_TIME_MODULE_H
#define AKIRA_TIME_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

int akira_register_time_module(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_TIME_MODULE_H */
