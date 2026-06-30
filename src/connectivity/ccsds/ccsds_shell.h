/**
 * @file ccsds_shell.h
 * @brief Manual CCSDS TM shell activation helpers.
 */

#ifndef AKIRA_CCSDS_SHELL_H
#define AKIRA_CCSDS_SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ccsds_tm_frame.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ccsds_shell_tm_status {
    bool initialized;
    bool running;
    uint8_t time_vcid;
    uint32_t log_route_calls;
    uint8_t last_vcid;
    size_t last_len;
    uint8_t last_mcfc;
    uint8_t last_vcfc;
    uint16_t last_fhp;
    bool last_cadu;
};

/**
 * @brief Initialize CCSDS TM services used by the shell command.
 *
 * @return 0 on success, or a negative errno from TM route registration.
 */
int ccsds_shell_tm_init(void);

/**
 * @brief Start shell-managed TM frame and time packet services.
 *
 * @return 0 on success, -EACCES if TM services have not been initialized, or
 *         a negative errno from the underlying TM service.
 */
int ccsds_shell_tm_start(void);

/**
 * @brief Stop shell-managed TM frame and time packet services.
 *
 * @return 0 on success.
 */
int ccsds_shell_tm_stop(void);

/**
 * @brief Copy the current shell-managed TM status.
 *
 * @param status Output status snapshot.
 */
void ccsds_shell_tm_get_status(struct ccsds_shell_tm_status *status);

/**
 * @brief Return whether one TM route bit is registered or built in.
 *
 * @param route_bit Exactly one CCSDS_TM_ROUTE_* bit.
 *
 * @return true if @p route_bit can currently be selected.
 */
bool ccsds_shell_tm_route_available(ccsds_tm_route_mask_t route_bit);

/**
 * @brief Parse a comma-separated TM route list into a route mask.
 *
 * @param routes Route list such as "log", "udp", or "log,udp".
 * @param route_mask Output route mask.
 *
 * @return 0 on success, or -EINVAL for an unknown route name.
 */
int ccsds_shell_tm_parse_route_mask(const char *routes,
                                    ccsds_tm_route_mask_t *route_mask);

/**
 * @brief Replace the selected routes for one virtual channel.
 *
 * @param vcid Virtual channel ID, 0 through 7.
 * @param route_mask Complete route mask to apply.
 *
 * @return 0 on success, -EACCES if TM services have not been initialized,
 *         -ENOTSUP for an unavailable route bit, or a negative errno from TM
 *         frame route validation.
 */
int ccsds_shell_tm_route_set(uint8_t vcid, ccsds_tm_route_mask_t route_mask);

/**
 * @brief Add routes to one virtual channel route mask.
 *
 * @param vcid Virtual channel ID, 0 through 7.
 * @param route_mask Route bits to add.
 *
 * @return 0 on success, -EACCES if TM services have not been initialized,
 *         -ENOTSUP for an unavailable route bit, or a negative errno from TM
 *         frame route validation.
 */
int ccsds_shell_tm_route_add(uint8_t vcid, ccsds_tm_route_mask_t route_mask);

/**
 * @brief Remove routes from one virtual channel route mask.
 *
 * @param vcid Virtual channel ID, 0 through 7.
 * @param route_mask Route bits to remove.
 *
 * @return 0 on success, -EACCES if TM services have not been initialized,
 *         -ENOTSUP for an unavailable route bit, or a negative errno from TM
 *         frame route validation.
 */
int ccsds_shell_tm_route_del(uint8_t vcid, ccsds_tm_route_mask_t route_mask);

/**
 * @brief Clear all routes from one virtual channel.
 *
 * @param vcid Virtual channel ID, 0 through 7.
 *
 * @return 0 on success, -EACCES if TM services have not been initialized, or
 *         -EINVAL for an invalid VCID.
 */
int ccsds_shell_tm_route_clear(uint8_t vcid);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_SHELL_H */
