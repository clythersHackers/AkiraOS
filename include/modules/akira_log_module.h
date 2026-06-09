/**
 * @file akira_log_module.h
 * @brief Modular WASM Logging API for AkiraOS (WAMR)
 *
 * Declares the registration function for the log module.
 * @stability stable
 * @since 1.4
 */

#ifndef AKIRA_LOG_MODULE_H
#define AKIRA_LOG_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

int akira_register_log_module(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_LOG_MODULE_H */
