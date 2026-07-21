#include <errno.h>
#include <stdbool.h>
#include <string.h>

#include <zephyr/ztest.h>

#include "ccsds/akira_cfdp_service.h"
#include "ccsds/ccsds_cfdp_checksum.h"

#define CAPTURE_MAX_PACKETS 8u

struct packet_capture {
    uint8_t packet[CAPTURE_MAX_PACKETS][CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN +
                                        CCSDS_CFDP_MAX_PDU_SIZE];
    size_t packet_len[CAPTURE_MAX_PACKETS];
    uint32_t count;
};

struct event_capture {
    ccsds_cfdp_event_t event[8];
    uint32_t count;
};

struct memory_filestore {
    const uint8_t *data;
    uint32_t size;
    uint32_t read_count;
    uint32_t close_count;
};

struct receive_file {
    bool exists;
    bool open;
    char path[CCSDS_CFDP_MAX_FILENAME_LEN + 1u];
    uint8_t data[CCSDS_CFDP_MAX_SEGMENT_SIZE + 16u];
    uint32_t size;
};

struct receive_filestore {
    struct receive_file tmp;
    struct receive_file dst;
    uint32_t write_count;
    uint32_t read_count;
    uint32_t close_count;
    uint32_t commit_count;
    uint32_t discard_count;
};

struct link_endpoint {
    struct ccsds_router *peer_router;
    uint32_t sent_count;
    uint32_t dispatch_failures;
};

static int capture_send_packet(void *user, const uint8_t *packet,
                               size_t packet_len)
{
    struct packet_capture *capture = user;

    zassert_not_null(capture);
    zassert_not_null(packet);
    zassert_true(packet_len <= sizeof(capture->packet[0]));

    if (capture->count < CAPTURE_MAX_PACKETS) {
        memcpy(capture->packet[capture->count], packet, packet_len);
        capture->packet_len[capture->count] = packet_len;
    }
    capture->count++;
    return 0;
}

static int link_send_packet(void *user, const uint8_t *packet,
                            size_t packet_len)
{
    struct link_endpoint *endpoint = user;
    int ret;

    zassert_not_null(endpoint);
    zassert_not_null(endpoint->peer_router);

    endpoint->sent_count++;
    ret =
        ccsds_router_dispatch_bytes(endpoint->peer_router, packet, packet_len);
    if (ret != 0) {
        endpoint->dispatch_failures++;
    }
    return ret;
}

static void service_event_cb(void *user, const ccsds_cfdp_event_t *event)
{
    struct event_capture *capture = user;

    zassert_not_null(capture);
    zassert_not_null(event);

    if (capture->count < ARRAY_SIZE(capture->event)) {
        capture->event[capture->count] = *event;
    }
    capture->count++;
}

static int memory_open_read(void *user, const char *path, void **handle,
                            uint32_t *size)
{
    struct memory_filestore *store = user;

    zassert_not_null(store);
    zassert_equal(strcmp(path, "src.bin"), 0);

    *handle = store;
    *size = store->size;
    return 0;
}

static int memory_read(void *user, void *handle, uint32_t offset, uint8_t *buf,
                       size_t len, size_t *nread)
{
    struct memory_filestore *store = user;
    size_t remaining;

    ARG_UNUSED(user);
    zassert_equal(handle, store);
    zassert_true(offset < store->size);
    zassert_not_null(buf);
    zassert_not_null(nread);

    remaining = store->size - offset;
    *nread = remaining < len ? remaining : len;
    memcpy(buf, &store->data[offset], *nread);
    store->read_count++;
    return 0;
}

static int memory_close(void *user, void *handle)
{
    struct memory_filestore *store = user;

    ARG_UNUSED(user);
    zassert_equal(handle, store);
    store->close_count++;
    return 0;
}

static int receive_open_write_tmp(void *user, const char *dst_path,
                                  void **handle)
{
    struct receive_filestore *store = user;
    size_t path_len;

    zassert_not_null(store);
    zassert_not_null(dst_path);
    zassert_not_null(handle);

    path_len = strlen(dst_path);
    zassert_true(path_len > 0u);
    zassert_true(path_len <= CCSDS_CFDP_MAX_FILENAME_LEN);

    memset(&store->tmp, 0, sizeof(store->tmp));
    store->tmp.exists = true;
    store->tmp.open = true;
    memcpy(store->tmp.path, dst_path, path_len + 1u);
    *handle = &store->tmp;
    return 0;
}

static int receive_read(void *user, void *handle, uint32_t offset, uint8_t *buf,
                        size_t len, size_t *nread)
{
    struct receive_filestore *store = user;
    struct receive_file *file = handle;
    uint32_t available;

    zassert_not_null(store);
    zassert_not_null(file);
    zassert_not_null(buf);
    zassert_not_null(nread);

    if (offset >= file->size) {
        *nread = 0u;
        return 0;
    }

    available = file->size - offset;
    if (len > available) {
        len = available;
    }

    memcpy(buf, &file->data[offset], len);
    *nread = len;
    store->read_count++;
    return 0;
}

static int receive_write(void *user, void *handle, uint32_t offset,
                         const uint8_t *buf, size_t len)
{
    struct receive_filestore *store = user;
    struct receive_file *file = handle;
    size_t end;

    zassert_not_null(store);
    zassert_not_null(file);
    zassert_not_null(buf);
    zassert_true(file->exists);
    zassert_true(file->open);

    end = (size_t)offset + len;
    zassert_true(end <= sizeof(file->data));

    memcpy(&file->data[offset], buf, len);
    if (end > file->size) {
        file->size = end;
    }
    store->write_count++;
    return 0;
}

static int receive_close(void *user, void *handle)
{
    struct receive_filestore *store = user;
    struct receive_file *file = handle;

    zassert_not_null(store);
    zassert_not_null(file);
    zassert_true(file->open);

    file->open = false;
    store->close_count++;
    return 0;
}

static int receive_commit_tmp(void *user, const char *dst_path)
{
    struct receive_filestore *store = user;

    zassert_not_null(store);
    zassert_not_null(dst_path);
    zassert_true(store->tmp.exists);
    zassert_false(store->tmp.open);
    zassert_equal(strcmp(store->tmp.path, dst_path), 0);

    memset(&store->dst, 0, sizeof(store->dst));
    store->dst.exists = true;
    memcpy(store->dst.path, store->tmp.path, strlen(store->tmp.path) + 1u);
    memcpy(store->dst.data, store->tmp.data, store->tmp.size);
    store->dst.size = store->tmp.size;
    memset(&store->tmp, 0, sizeof(store->tmp));
    store->commit_count++;
    return 0;
}

static int receive_discard_tmp(void *user, const char *dst_path)
{
    struct receive_filestore *store = user;

    zassert_not_null(store);
    zassert_not_null(dst_path);
    zassert_equal(strcmp(store->tmp.path, dst_path), 0);

    memset(&store->tmp, 0, sizeof(store->tmp));
    store->discard_count++;
    return 0;
}

static ccsds_cfdp_filestore_ops_t
memory_filestore_ops(struct memory_filestore *store)
{
    return (ccsds_cfdp_filestore_ops_t){
        .user = store,
        .open_read = memory_open_read,
        .read = memory_read,
        .close = memory_close,
    };
}

static ccsds_cfdp_filestore_ops_t
receive_filestore_ops(struct receive_filestore *store)
{
    return (ccsds_cfdp_filestore_ops_t){
        .user = store,
        .open_write_tmp = receive_open_write_tmp,
        .read = receive_read,
        .write = receive_write,
        .close = receive_close,
        .commit_tmp = receive_commit_tmp,
        .discard_tmp = receive_discard_tmp,
    };
}

static akira_cfdp_service_config_t
service_config(ccsds_cfdp_space_packet_send_cb_t send_packet, void *send_user,
               const ccsds_cfdp_filestore_ops_t *receive_filestore,
               ccsds_cfdp_event_cb_t event_cb, void *event_user)
{
    return (akira_cfdp_service_config_t){
        .local_entity_id = 0x12u,
        .remote_entity_id = 0x34u,
        .entity_id_len = 1u,
        .transaction_sequence_number_len = 1u,
        .initial_transaction_sequence_number = 7u,
        .local_apid = 0x251u,
        .remote_apid = 0u,
        .packet_type = CCSDS_PACKET_TYPE_TC,
        .send_packet = send_packet,
        .send_user = send_user,
        .receive_filestore = receive_filestore,
        .event_cb = event_cb,
        .event_user = event_user,
    };
}

static ccsds_cfdp_entity_config_t peer_entity_config(void)
{
    return (ccsds_cfdp_entity_config_t){
        .local_entity_id = 0x34u,
        .remote_entity_id = 0x12u,
        .entity_id_len = 1u,
        .transaction_sequence_number_len = 1u,
        .initial_transaction_sequence_number = 3u,
    };
}

static ccsds_cfdp_space_packet_adapter_config_t
peer_adapter_config(ccsds_cfdp_space_packet_send_cb_t send_packet,
                    void *send_user)
{
    return (ccsds_cfdp_space_packet_adapter_config_t){
        .remote_entity_id = 0x12u,
        .local_apid = 0x251u,
        .remote_apid = 0x251u,
        .packet_type = CCSDS_PACKET_TYPE_TC,
        .initial_sequence_count = 2u,
        .send_packet = send_packet,
        .send_user = send_user,
    };
}

static void encode_packet(uint16_t apid, const uint8_t *pdu, size_t pdu_len,
                          uint8_t *packet_buf, size_t packet_cap,
                          size_t *packet_len)
{
    const struct ccsds_space_packet packet = {
        .version = 0u,
        .type = CCSDS_PACKET_TYPE_TC,
        .secondary_header = false,
        .apid = apid,
        .sequence_flags = CCSDS_SEQUENCE_UNSEGMENTED,
        .sequence_count = 1u,
        .payload = pdu,
        .payload_len = pdu_len,
    };

    zassert_ok(
        ccsds_space_packet_encode(&packet, packet_buf, packet_cap, packet_len));
}

static ccsds_cfdp_pdu_header_t
incoming_header(enum ccsds_cfdp_pdu_type pdu_type, uint64_t sequence)
{
    return (ccsds_cfdp_pdu_header_t){
        .version = CCSDS_CFDP_VERSION_1,
        .pdu_type = pdu_type,
        .direction = CCSDS_CFDP_DIRECTION_TOWARD_RECEIVER,
        .transmission_mode = CCSDS_CFDP_TRANSMISSION_MODE_UNACKNOWLEDGED,
        .crc_flag = CCSDS_CFDP_CRC_NOT_PRESENT,
        .file_size_mode = CCSDS_CFDP_FILE_SIZE_SMALL,
        .segmentation_control =
            CCSDS_CFDP_SEGMENTATION_RECORD_BOUNDARIES_NOT_PRESERVED,
        .segment_metadata_present = false,
        .entity_id_len = 1u,
        .transaction_sequence_number_len = 1u,
        .source_entity_id = 0x34u,
        .transaction_sequence_number = sequence,
        .destination_entity_id = 0x12u,
    };
}

static void encode_incoming_metadata(uint64_t sequence, uint32_t file_size,
                                     uint8_t *buf, size_t cap, size_t *len)
{
    const char src[] = "src.bin";
    const char dst[] = "dst.bin";
    const ccsds_cfdp_metadata_pdu_t metadata = {
        .header = incoming_header(CCSDS_CFDP_PDU_TYPE_FILE_DIRECTIVE, sequence),
        .closure_requested = true,
        .checksum_type = CCSDS_CFDP_CHECKSUM_TYPE_MODULAR,
        .file_size = file_size,
        .source_filename =
            {
                .value = (const uint8_t *)src,
                .len = (uint8_t)strlen(src),
            },
        .destination_filename =
            {
                .value = (const uint8_t *)dst,
                .len = (uint8_t)strlen(dst),
            },
    };

    zassert_equal(ccsds_cfdp_encode_metadata(&metadata, buf, cap, len),
                  CCSDS_CFDP_STATUS_OK);
}

static void encode_incoming_filedata(uint64_t sequence, const uint8_t *data,
                                     size_t data_len, uint8_t *buf, size_t cap,
                                     size_t *len)
{
    const ccsds_cfdp_filedata_pdu_t filedata = {
        .header = incoming_header(CCSDS_CFDP_PDU_TYPE_FILE_DATA, sequence),
        .offset = 0u,
        .data = data,
        .data_len = data_len,
    };

    zassert_equal(ccsds_cfdp_encode_filedata(&filedata, buf, cap, len),
                  CCSDS_CFDP_STATUS_OK);
}

static void encode_incoming_eof(uint64_t sequence, uint32_t checksum,
                                uint32_t file_size, uint8_t *buf, size_t cap,
                                size_t *len)
{
    const ccsds_cfdp_eof_pdu_t eof = {
        .header = incoming_header(CCSDS_CFDP_PDU_TYPE_FILE_DIRECTIVE, sequence),
        .condition_code = CCSDS_CFDP_CONDITION_NO_ERROR,
        .file_checksum = checksum,
        .file_size = file_size,
    };

    zassert_equal(ccsds_cfdp_encode_eof(&eof, buf, cap, len),
                  CCSDS_CFDP_STATUS_OK);
}

static uint32_t modular_checksum(const uint8_t *data, size_t data_len)
{
    ccsds_cfdp_checksum_state_t state;
    uint32_t checksum;

    zassert_equal(
        ccsds_cfdp_checksum_init(&state, CCSDS_CFDP_CHECKSUM_TYPE_MODULAR),
        CCSDS_CFDP_STATUS_OK);
    zassert_equal(ccsds_cfdp_checksum_update(&state, 0u, data, data_len),
                  CCSDS_CFDP_STATUS_OK);
    zassert_equal(ccsds_cfdp_checksum_finish(&state, &checksum),
                  CCSDS_CFDP_STATUS_OK);
    return checksum;
}

ZTEST(akira_cfdp_service, test_init_static_config_wires_entity_and_adapter)
{
    struct packet_capture capture = {0};
    struct receive_filestore receive_store = {0};
    ccsds_cfdp_filestore_ops_t receive_ops =
        receive_filestore_ops(&receive_store);
    akira_cfdp_service_config_t cfg =
        service_config(capture_send_packet, &capture, &receive_ops, NULL, NULL);
    ccsds_cfdp_entity_t *entity;
    ccsds_cfdp_space_packet_adapter_t *adapter;

    zassert_equal(akira_cfdp_service_init(&cfg), CCSDS_CFDP_STATUS_OK);
    entity = akira_cfdp_service_entity();
    adapter = akira_cfdp_service_space_packet_adapter();

    zassert_equal(entity->local_entity_id, 0x12u);
    zassert_equal(entity->remote_entity_id, 0x34u);
    zassert_equal(entity->next_transaction_sequence_number, 7u);
    zassert_equal(adapter->local_apid, 0x251u);
    zassert_equal(adapter->remote_apid, 0x251u);
    zassert_equal(adapter->packet_type, CCSDS_PACKET_TYPE_TC);
    zassert_equal(adapter->sequence_count, 0u);
    zassert_equal(akira_cfdp_service_receive_filestore(), &receive_ops);
}

ZTEST(akira_cfdp_service, test_config_defaults_are_mutable_for_peer_instances)
{
    struct packet_capture capture = {0};
    akira_cfdp_service_config_t cfg;
    ccsds_cfdp_entity_t *entity;
    ccsds_cfdp_space_packet_adapter_t *adapter;

    akira_cfdp_service_config_defaults(&cfg);
    cfg.local_entity_id = 0x44u;
    cfg.remote_entity_id = 0x55u;
    cfg.local_apid = 0x321u;
    cfg.remote_apid = 0u;
    cfg.packet_type = CCSDS_PACKET_TYPE_TC;
    cfg.send_packet = capture_send_packet;
    cfg.send_user = &capture;

    zassert_equal(akira_cfdp_service_init(&cfg), CCSDS_CFDP_STATUS_OK);
    entity = akira_cfdp_service_entity();
    adapter = akira_cfdp_service_space_packet_adapter();

    zassert_equal(entity->local_entity_id, 0x44u);
    zassert_equal(entity->remote_entity_id, 0x55u);
    zassert_equal(adapter->local_apid, 0x321u);
    zassert_equal(adapter->remote_apid, 0x321u);
}

ZTEST(akira_cfdp_service, test_router_apid_dispatch_receives_file)
{
    struct ccsds_router router;
    struct packet_capture capture = {0};
    struct receive_filestore receive_store = {0};
    ccsds_cfdp_filestore_ops_t receive_ops =
        receive_filestore_ops(&receive_store);
    akira_cfdp_service_config_t cfg =
        service_config(capture_send_packet, &capture, &receive_ops, NULL, NULL);
    const uint8_t file[] = {0x10u, 0x20u, 0x30u};
    uint8_t pdu[CCSDS_CFDP_MAX_PDU_SIZE];
    uint8_t
        packet[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN + CCSDS_CFDP_MAX_PDU_SIZE];
    size_t pdu_len;
    size_t packet_len;
    uint32_t checksum;

    ccsds_router_init(&router);
    zassert_equal(akira_cfdp_service_init(&cfg), CCSDS_CFDP_STATUS_OK);
    zassert_ok(akira_cfdp_service_register_rx(&router));

    encode_incoming_metadata(0x55u, sizeof(file), pdu, sizeof(pdu), &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_ok(ccsds_router_dispatch_bytes(&router, packet, packet_len));

    encode_incoming_filedata(0x55u, file, sizeof(file), pdu, sizeof(pdu),
                             &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_ok(ccsds_router_dispatch_bytes(&router, packet, packet_len));

    checksum = modular_checksum(file, sizeof(file));
    encode_incoming_eof(0x55u, checksum, sizeof(file), pdu, sizeof(pdu),
                        &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_ok(ccsds_router_dispatch_bytes(&router, packet, packet_len));

    zassert_equal(receive_store.commit_count, 1u);
    zassert_true(receive_store.dst.exists);
    zassert_equal(receive_store.dst.size, sizeof(file));
    zassert_mem_equal(receive_store.dst.data, file, sizeof(file));
}

ZTEST(akira_cfdp_service, test_outbound_pdu_wraps_through_space_packet_adapter)
{
    struct packet_capture capture = {0};
    const uint8_t file[] = {0xa0u, 0xb1u, 0xc2u};
    struct memory_filestore source_store = {
        .data = file,
        .size = sizeof(file),
    };
    ccsds_cfdp_filestore_ops_t source_ops = memory_filestore_ops(&source_store);
    akira_cfdp_service_config_t cfg =
        service_config(capture_send_packet, &capture, NULL, NULL, NULL);
    ccsds_cfdp_transaction_id_t id;
    const ccsds_cfdp_put_request_t request = {
        .source_path = "src.bin",
        .destination_path = "dst.bin",
        .checksum_type = CCSDS_CFDP_CHECKSUM_TYPE_MODULAR,
        .closure_requested = false,
    };
    struct ccsds_space_packet packet;
    ccsds_cfdp_metadata_pdu_t metadata;
    size_t consumed;

    zassert_equal(akira_cfdp_service_init(&cfg), CCSDS_CFDP_STATUS_OK);
    zassert_equal(akira_cfdp_service_send_file(&source_ops, &request, &id),
                  CCSDS_CFDP_STATUS_OK);

    zassert_equal(id.source_entity_id, 0x12u);
    zassert_equal(id.transaction_sequence_number, 7u);
    zassert_true(capture.count >= 3u);
    zassert_ok(ccsds_space_packet_decode(capture.packet[0],
                                         capture.packet_len[0], &packet));
    zassert_equal(packet.apid, 0x251u);
    zassert_equal(packet.type, CCSDS_PACKET_TYPE_TC);
    zassert_equal(packet.sequence_count, 0u);
    zassert_equal(ccsds_cfdp_decode_metadata(packet.payload, packet.payload_len,
                                             &metadata, &consumed),
                  CCSDS_CFDP_STATUS_OK);
    zassert_equal(metadata.header.source_entity_id, 0x12u);
    zassert_equal(metadata.header.destination_entity_id, 0x34u);
}

ZTEST(akira_cfdp_service,
      test_loopback_receive_reaches_staging_fixture_and_complete_event)
{
    struct akira_cfdp_service_status report;
    struct ccsds_router service_router;
    struct ccsds_router peer_router;
    ccsds_cfdp_space_packet_adapter_t peer_adapter;
    ccsds_cfdp_entity_t peer_entity;
    struct link_endpoint service_link = {
        .peer_router = &peer_router,
    };
    struct link_endpoint peer_link = {
        .peer_router = &service_router,
    };
    struct event_capture events = {0};
    const uint8_t file[] = {0x01u, 0x23u, 0x45u, 0x67u};
    struct memory_filestore source_store = {
        .data = file,
        .size = sizeof(file),
    };
    struct receive_filestore receive_store = {0};
    ccsds_cfdp_filestore_ops_t source_ops = memory_filestore_ops(&source_store);
    ccsds_cfdp_filestore_ops_t receive_ops =
        receive_filestore_ops(&receive_store);
    akira_cfdp_service_config_t service_cfg =
        service_config(link_send_packet, &service_link, &receive_ops,
                       service_event_cb, &events);
    ccsds_cfdp_space_packet_adapter_config_t peer_adapter_cfg =
        peer_adapter_config(link_send_packet, &peer_link);
    ccsds_cfdp_ut_ops_t peer_ut;
    ccsds_cfdp_entity_config_t peer_cfg = peer_entity_config();
    ccsds_cfdp_transaction_id_t id;
    const ccsds_cfdp_put_request_t request = {
        .source_path = "src.bin",
        .destination_path = "dst.bin",
        .checksum_type = CCSDS_CFDP_CHECKSUM_TYPE_MODULAR,
        .closure_requested = true,
    };

    ccsds_router_init(&service_router);
    ccsds_router_init(&peer_router);
    zassert_equal(akira_cfdp_service_init(&service_cfg), CCSDS_CFDP_STATUS_OK);
    zassert_ok(akira_cfdp_service_register_rx(&service_router));
    zassert_equal(
        ccsds_cfdp_space_packet_adapter_init(&peer_adapter, &peer_adapter_cfg),
        CCSDS_CFDP_STATUS_OK);
    peer_ut = ccsds_cfdp_space_packet_adapter_ut_ops(&peer_adapter);
    zassert_equal(ccsds_cfdp_entity_init(&peer_entity, &peer_cfg, &peer_ut),
                  CCSDS_CFDP_STATUS_OK);
    zassert_ok(ccsds_cfdp_space_packet_adapter_register_rx(
        &peer_adapter, &peer_router, &peer_entity, NULL));

    zassert_equal(
        ccsds_cfdp_entity_send_file(&peer_entity, &source_ops, &request, &id),
        CCSDS_CFDP_STATUS_OK);

    zassert_equal(id.source_entity_id, 0x34u);
    zassert_equal(peer_link.dispatch_failures, 0u);
    zassert_equal(service_link.dispatch_failures, 0u);
    zassert_equal(receive_store.commit_count, 1u);
    zassert_true(receive_store.dst.exists);
    zassert_mem_equal(receive_store.dst.data, file, sizeof(file));
    zassert_true(events.count >= 1u);
    zassert_equal(events.event[events.count - 1u].type,
                  CCSDS_CFDP_EVENT_COMPLETE);
    zassert_equal(events.event[events.count - 1u].status, CCSDS_CFDP_STATUS_OK);

    akira_cfdp_service_get_status(&report);
    zassert_true(report.valid);
    zassert_equal(report.event_type, CCSDS_CFDP_EVENT_COMPLETE);
    zassert_equal(report.status, CCSDS_CFDP_STATUS_OK);
    zassert_equal(report.transaction_id.source_entity_id, 0x34u);
    zassert_equal(strcmp(report.source_path, "src.bin"), 0);
    zassert_equal(strcmp(report.destination_path, "dst.bin"), 0);
    zassert_equal(report.file_size, sizeof(file));
    zassert_equal(report.checksum, modular_checksum(file, sizeof(file)));
    zassert_true(report.checksum_ok);
}

ZTEST(akira_cfdp_service, test_failed_terminal_event_reaches_service_callback)
{
    struct akira_cfdp_service_status report;
    struct ccsds_router router;
    struct packet_capture capture = {0};
    struct event_capture events = {0};
    struct receive_filestore receive_store = {0};
    ccsds_cfdp_filestore_ops_t receive_ops =
        receive_filestore_ops(&receive_store);
    akira_cfdp_service_config_t cfg = service_config(
        capture_send_packet, &capture, &receive_ops, service_event_cb, &events);
    const uint8_t file[] = {0xdeu, 0xadu};
    uint8_t pdu[CCSDS_CFDP_MAX_PDU_SIZE];
    uint8_t
        packet[CCSDS_SPACE_PACKET_PRIMARY_HDR_LEN + CCSDS_CFDP_MAX_PDU_SIZE];
    size_t pdu_len;
    size_t packet_len;

    ccsds_router_init(&router);
    zassert_equal(akira_cfdp_service_init(&cfg), CCSDS_CFDP_STATUS_OK);
    zassert_ok(akira_cfdp_service_register_rx(&router));

    encode_incoming_metadata(0x56u, sizeof(file), pdu, sizeof(pdu), &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_ok(ccsds_router_dispatch_bytes(&router, packet, packet_len));

    encode_incoming_filedata(0x56u, file, sizeof(file), pdu, sizeof(pdu),
                             &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_ok(ccsds_router_dispatch_bytes(&router, packet, packet_len));

    encode_incoming_eof(0x56u, 0x12345678u, sizeof(file), pdu, sizeof(pdu),
                        &pdu_len);
    encode_packet(0x251u, pdu, pdu_len, packet, sizeof(packet), &packet_len);
    zassert_equal(ccsds_router_dispatch_bytes(&router, packet, packet_len),
                  -EINVAL);

    zassert_equal(receive_store.commit_count, 0u);
    zassert_equal(receive_store.discard_count, 1u);
    zassert_true(events.count >= 1u);
    zassert_equal(events.event[events.count - 1u].type,
                  CCSDS_CFDP_EVENT_FAILED);
    zassert_equal(events.event[events.count - 1u].status,
                  CCSDS_CFDP_STATUS_CHECKSUM_FAILURE);

    akira_cfdp_service_get_status(&report);
    zassert_true(report.valid);
    zassert_equal(report.event_type, CCSDS_CFDP_EVENT_FAILED);
    zassert_equal(report.status, CCSDS_CFDP_STATUS_CHECKSUM_FAILURE);
    zassert_equal(report.transaction_id.source_entity_id, 0x34u);
    zassert_equal(report.transaction_id.transaction_sequence_number, 0x56u);
    zassert_equal(strcmp(report.source_path, "src.bin"), 0);
    zassert_equal(strcmp(report.destination_path, "dst.bin"), 0);
    zassert_equal(report.file_size, sizeof(file));
    zassert_equal(report.checksum, 0x12345678u);
    zassert_false(report.checksum_ok);
    zassert_equal(strcmp(akira_cfdp_service_status_name(report.status),
                         "CHECKSUM_FAILURE"), 0);
}

ZTEST_SUITE(akira_cfdp_service, NULL, NULL, NULL, NULL, NULL);
