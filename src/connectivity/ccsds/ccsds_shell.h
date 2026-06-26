/**
 * @file ccsds_shell.h
 * @brief Manual CCSDS TM shell activation helpers.
 */

#ifndef AKIRA_CCSDS_SHELL_H
#define AKIRA_CCSDS_SHELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

int ccsds_shell_tm_init(void);
int ccsds_shell_tm_start(void);
int ccsds_shell_tm_stop(void);
void ccsds_shell_tm_get_status(struct ccsds_shell_tm_status *status);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_SHELL_H */
