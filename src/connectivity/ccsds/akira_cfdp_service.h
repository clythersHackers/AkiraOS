/**
 * @file akira_cfdp_service.h
 * @brief Generic AkiraOS CFDP service instance.
 */

#ifndef AKIRA_CCSDS_CFDP_SERVICE_H
#define AKIRA_CCSDS_CFDP_SERVICE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "ccsds_cfdp_entity.h"
#include "ccsds_cfdp_space_packet.h"
#include "ccsds_router.h"
#include "ccsds_space_packet.h"

#ifdef __cplusplus
extern "C" {
#endif

struct akira_cfdp_service_config {
    uint64_t local_entity_id;
    uint64_t remote_entity_id;
    uint8_t entity_id_len;
    uint8_t transaction_sequence_number_len;
    uint64_t initial_transaction_sequence_number;
    uint16_t local_apid;
    uint16_t remote_apid;
    enum ccsds_packet_type packet_type;
    ccsds_cfdp_space_packet_send_cb_t send_packet;
    void *send_user;
    uint64_t (*now_ms)(void *user);
    const ccsds_cfdp_filestore_ops_t *receive_filestore;
    ccsds_cfdp_event_cb_t event_cb;
    void *event_user;
};

typedef struct akira_cfdp_service_config akira_cfdp_service_config_t;

struct akira_cfdp_service_status {
    bool valid;
    ccsds_cfdp_event_type_t event_type;
    enum ccsds_cfdp_status status;
    ccsds_cfdp_transaction_id_t transaction_id;
    char source_path[CCSDS_CFDP_MAX_FILENAME_LEN + 1u];
    char destination_path[CCSDS_CFDP_MAX_FILENAME_LEN + 1u];
    uint32_t file_size;
    uint32_t checksum;
    bool checksum_ok;
};

void akira_cfdp_service_config_defaults(akira_cfdp_service_config_t *config);

enum ccsds_cfdp_status
akira_cfdp_service_init(const akira_cfdp_service_config_t *config);

int akira_cfdp_service_register_rx(struct ccsds_router *router);

enum ccsds_cfdp_status
akira_cfdp_service_send_file(const ccsds_cfdp_filestore_ops_t *filestore,
                             const ccsds_cfdp_put_request_t *request,
                             ccsds_cfdp_transaction_id_t *transaction_id);

/** Send one arbitrary filesystem file using the generic CFDP filestore API. */
enum ccsds_cfdp_status
akira_cfdp_service_send_path(const char *source_path,
                             const char *destination_path,
                             ccsds_cfdp_transaction_id_t *transaction_id);

/** Copy the last persistent COMPLETE or FAILED service report. */
void akira_cfdp_service_get_status(struct akira_cfdp_service_status *status);

/** Return a stable operator-facing name for a CFDP status code. */
const char *
akira_cfdp_service_status_name(enum ccsds_cfdp_status status);

void akira_cfdp_service_poll(uint64_t now_ms);

ccsds_cfdp_entity_t *akira_cfdp_service_entity(void);

ccsds_cfdp_space_packet_adapter_t *
akira_cfdp_service_space_packet_adapter(void);

const ccsds_cfdp_filestore_ops_t *akira_cfdp_service_receive_filestore(void);

#ifdef __cplusplus
}
#endif

#endif /* AKIRA_CCSDS_CFDP_SERVICE_H */
