#include "akira_cfdp_service.h"

#include <errno.h>
#include <string.h>

#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "akira_cfdp_staging.h"
#include "ccsds_cfdp_config.h"

#if defined(CONFIG_NETWORKING) && !defined(CONFIG_AKIRA_CCSDS_FRAME_SUPPORT)
#include "ccsds_udp.h"
#endif

#ifdef CONFIG_FILE_SYSTEM
#include <zephyr/fs/fs.h>
#endif

LOG_MODULE_REGISTER(akira_cfdp_service, CONFIG_AKIRA_LOG_LEVEL);

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_LOCAL_ENTITY_ID
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_LOCAL_ENTITY_ID 1
#endif

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_REMOTE_ENTITY_ID
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_REMOTE_ENTITY_ID 2
#endif

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_ENTITY_ID_LEN
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_ENTITY_ID_LEN 1
#endif

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_TRANS_SEQ_LEN
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_TRANS_SEQ_LEN 1
#endif

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_INITIAL_TRANS_SEQ
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_INITIAL_TRANS_SEQ 1
#endif

#ifndef CONFIG_AKIRA_CCSDS_CFDP_SERVICE_APID
#define CONFIG_AKIRA_CCSDS_CFDP_SERVICE_APID 0x340
#endif

#define AKIRA_CFDP_SERVICE_PACKET_TYPE CCSDS_PACKET_TYPE_TC
#define AKIRA_CFDP_SERVICE_INITIAL_PACKET_SEQ 0u

struct akira_cfdp_service {
    ccsds_cfdp_entity_t entity;
    ccsds_cfdp_space_packet_adapter_t adapter;
    const ccsds_cfdp_filestore_ops_t *receive_filestore;
    ccsds_cfdp_event_cb_t event_cb;
    void *event_user;
#ifdef CONFIG_FILE_SYSTEM
    struct fs_file_t receive_file;
    struct fs_file_t source_file;
#endif
    bool receive_file_open;
    bool source_file_open;
};

static struct akira_cfdp_service service;
static K_MUTEX_DEFINE(service_status_lock);
static struct akira_cfdp_service_status service_status;

#ifdef CONFIG_FILE_SYSTEM
#define AKIRA_CFDP_STAGE_ROOT "/lfs/cfdp"
#define AKIRA_CFDP_STAGING_DIR AKIRA_CFDP_STAGE_ROOT "/staging"
#define AKIRA_CFDP_MAX_STAGED_PATH_LEN                                         \
    (sizeof(AKIRA_CFDP_STAGING_DIR) + CCSDS_CFDP_MAX_FILENAME_LEN)

static bool is_allowed_name_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static int make_staged_path(const char *dst_name, char *out, size_t out_len)
{
    const size_t dir_len = strlen(AKIRA_CFDP_STAGING_DIR);
    size_t name_len = 0u;
    char prev = '\0';

    if (dst_name == NULL || out == NULL || out_len == 0u ||
        dst_name[0] == '\0' || dst_name[0] == '/' || dst_name[0] == '\\') {
        return -EINVAL;
    }

    for (; dst_name[name_len] != '\0'; name_len++) {
        const char c = dst_name[name_len];

        if (name_len >= CCSDS_CFDP_MAX_FILENAME_LEN) {
            return -ENAMETOOLONG;
        }
        if (c == '/' || c == '\\' || !is_allowed_name_char(c)) {
            return -EINVAL;
        }
        if (prev == '.' && c == '.') {
            return -EINVAL;
        }
        prev = c;
    }

    if (name_len == 1u && dst_name[0] == '.') {
        return -EINVAL;
    }
    if (dir_len + 1u + name_len + 1u > out_len) {
        return -ENAMETOOLONG;
    }

    memcpy(out, AKIRA_CFDP_STAGING_DIR, dir_len);
    out[dir_len] = '/';
    memcpy(&out[dir_len + 1u], dst_name, name_len);
    out[dir_len + 1u + name_len] = '\0';
    return 0;
}
#endif /* CONFIG_FILE_SYSTEM */

static int staging_open_write_tmp(void *user, const char *dst_path,
                                  void **handle)
{
    struct akira_cfdp_service *svc = user;

    if (svc == NULL || dst_path == NULL || handle == NULL ||
        svc->receive_file_open) {
        return -EINVAL;
    }

#ifdef CONFIG_FILE_SYSTEM
    char staged_path[AKIRA_CFDP_MAX_STAGED_PATH_LEN + 1u];
    int rc;

    rc = akira_cfdp_receive_to_staging(dst_path);
    if (rc != 0) {
        return rc;
    }

    rc = make_staged_path(dst_path, staged_path, sizeof(staged_path));
    if (rc != 0) {
        return rc;
    }

    fs_file_t_init(&svc->receive_file);
    rc = fs_open(&svc->receive_file, staged_path,
                 FS_O_CREATE | FS_O_RDWR | FS_O_TRUNC);
    if (rc != 0) {
        return rc;
    }

    svc->receive_file_open = true;
    *handle = &svc->receive_file;
    return 0;
#else
    ARG_UNUSED(dst_path);
    ARG_UNUSED(handle);
    return -ENOSYS;
#endif
}

static int staging_read(void *user, void *handle, uint32_t offset, uint8_t *buf,
                        size_t len, size_t *nread)
{
    struct akira_cfdp_service *svc = user;

    if (svc == NULL || handle == NULL || buf == NULL || nread == NULL ||
        !svc->receive_file_open) {
        return -EINVAL;
    }

#ifdef CONFIG_FILE_SYSTEM
    ssize_t rc;

    if (fs_seek(handle, (off_t)offset, FS_SEEK_SET) != 0) {
        return -EIO;
    }

    rc = fs_read(handle, buf, len);
    if (rc < 0) {
        return (int)rc;
    }

    *nread = (size_t)rc;
    return 0;
#else
    ARG_UNUSED(offset);
    ARG_UNUSED(len);
    return -ENOSYS;
#endif
}

static int staging_write(void *user, void *handle, uint32_t offset,
                         const uint8_t *buf, size_t len)
{
    struct akira_cfdp_service *svc = user;

    if (svc == NULL || handle == NULL || buf == NULL ||
        !svc->receive_file_open) {
        return -EINVAL;
    }

#ifdef CONFIG_FILE_SYSTEM
    ssize_t rc;

    if (fs_seek(handle, (off_t)offset, FS_SEEK_SET) != 0) {
        return -EIO;
    }

    rc = fs_write(handle, buf, len);
    if (rc < 0) {
        return (int)rc;
    }
    return (size_t)rc == len ? 0 : -EIO;
#else
    ARG_UNUSED(offset);
    ARG_UNUSED(len);
    return -ENOSYS;
#endif
}

static int staging_close(void *user, void *handle)
{
    struct akira_cfdp_service *svc = user;

    if (svc == NULL || handle == NULL || !svc->receive_file_open) {
        return -EINVAL;
    }

#ifdef CONFIG_FILE_SYSTEM
    int rc = fs_close(handle);

    svc->receive_file_open = false;
    return rc;
#else
    return -ENOSYS;
#endif
}

static int staging_commit_tmp(void *user, const char *dst_path)
{
    ARG_UNUSED(user);

    return akira_cfdp_commit_staged(dst_path);
}

static int staging_discard_tmp(void *user, const char *dst_path)
{
    struct akira_cfdp_service *svc = user;

#ifdef CONFIG_FILE_SYSTEM
    if (svc != NULL && svc->receive_file_open) {
        (void)fs_close(&svc->receive_file);
        svc->receive_file_open = false;
    }
#else
    ARG_UNUSED(svc);
#endif

    return akira_cfdp_discard_staged(dst_path);
}

static ccsds_cfdp_filestore_ops_t default_receive_filestore = {
    .user = &service,
    .open_write_tmp = staging_open_write_tmp,
    .read = staging_read,
    .write = staging_write,
    .close = staging_close,
    .commit_tmp = staging_commit_tmp,
    .discard_tmp = staging_discard_tmp,
};

#ifdef CONFIG_FILE_SYSTEM
static int source_open_read(void *user, const char *path, void **handle,
                            uint32_t *size)
{
    struct akira_cfdp_service *svc = user;
    struct fs_dirent entry;
    int rc;

    if (svc == NULL || path == NULL || handle == NULL || size == NULL ||
        svc->source_file_open) {
        return -EINVAL;
    }

    rc = fs_stat(path, &entry);
    if (rc != 0) {
        return rc;
    }
    if (entry.type != FS_DIR_ENTRY_FILE || entry.size > UINT32_MAX) {
        return -EFBIG;
    }

    fs_file_t_init(&svc->source_file);
    rc = fs_open(&svc->source_file, path, FS_O_READ);
    if (rc != 0) {
        return rc;
    }

    svc->source_file_open = true;
    *handle = &svc->source_file;
    *size = (uint32_t)entry.size;
    return 0;
}

static int source_read(void *user, void *handle, uint32_t offset, uint8_t *buf,
                       size_t len, size_t *nread)
{
    struct akira_cfdp_service *svc = user;
    ssize_t rc;

    if (svc == NULL || handle == NULL || buf == NULL || nread == NULL ||
        !svc->source_file_open) {
        return -EINVAL;
    }
    if (fs_seek(handle, (off_t)offset, FS_SEEK_SET) != 0) {
        return -EIO;
    }

    rc = fs_read(handle, buf, len);
    if (rc < 0) {
        return (int)rc;
    }
    *nread = (size_t)rc;
    return 0;
}

static int source_close(void *user, void *handle)
{
    struct akira_cfdp_service *svc = user;
    int rc;

    if (svc == NULL || handle == NULL || !svc->source_file_open) {
        return -EINVAL;
    }

    rc = fs_close(handle);
    svc->source_file_open = false;
    return rc;
}
#endif /* CONFIG_FILE_SYSTEM */

static ccsds_cfdp_filestore_ops_t source_filestore = {
    .user = &service,
#ifdef CONFIG_FILE_SYSTEM
    .open_read = source_open_read,
    .read = source_read,
    .close = source_close,
#endif
};

static bool transaction_id_matches(const ccsds_cfdp_transaction_id_t *a,
                                   const ccsds_cfdp_transaction_id_t *b)
{
    return a->source_entity_id == b->source_entity_id &&
           a->transaction_sequence_number == b->transaction_sequence_number;
}

const char *akira_cfdp_service_status_name(enum ccsds_cfdp_status status)
{
    switch (status) {
    case CCSDS_CFDP_STATUS_OK:
        return "OK";
    case CCSDS_CFDP_STATUS_INVALID_ARGUMENT:
        return "INVALID_ARGUMENT";
    case CCSDS_CFDP_STATUS_BUFFER_TOO_SMALL:
        return "BUFFER_TOO_SMALL";
    case CCSDS_CFDP_STATUS_MALFORMED_PDU:
        return "MALFORMED_PDU";
    case CCSDS_CFDP_STATUS_UNSUPPORTED:
        return "UNSUPPORTED";
    case CCSDS_CFDP_STATUS_UNSUPPORTED_CHECKSUM:
        return "UNSUPPORTED_CHECKSUM";
    case CCSDS_CFDP_STATUS_INVALID_TRANSMISSION_MODE:
        return "INVALID_TRANSMISSION_MODE";
    case CCSDS_CFDP_STATUS_CHECKSUM_FAILURE:
        return "CHECKSUM_FAILURE";
    case CCSDS_CFDP_STATUS_FILE_SIZE_ERROR:
        return "FILE_SIZE_ERROR";
    case CCSDS_CFDP_STATUS_INACTIVITY_DETECTED:
        return "INACTIVITY_DETECTED";
    case CCSDS_CFDP_STATUS_NAK_LIMIT_REACHED:
        return "NAK_LIMIT_REACHED";
    case CCSDS_CFDP_STATUS_CANCEL_REQUEST:
        return "CANCEL_REQUEST";
    case CCSDS_CFDP_STATUS_FILESTORE_REJECTION:
        return "FILESTORE_REJECTION";
    case CCSDS_CFDP_STATUS_TRANSACTION_BUSY:
        return "TRANSACTION_BUSY";
    default:
        return "UNKNOWN";
    }
}

static void record_terminal_status(struct akira_cfdp_service *svc,
                                   const ccsds_cfdp_event_t *event)
{
    const ccsds_cfdp_transaction_slot_t *slot = NULL;
    struct akira_cfdp_service_status report = {
        .valid = true,
        .event_type = event->type,
        .status = event->status,
        .transaction_id = event->transaction_id,
        .checksum_ok = event->status == CCSDS_CFDP_STATUS_OK,
    };

    if (svc->entity.sender.active &&
        transaction_id_matches(&svc->entity.sender.id,
                               &event->transaction_id)) {
        slot = &svc->entity.sender;
    } else if (svc->entity.receiver.active &&
               transaction_id_matches(&svc->entity.receiver.id,
                                      &event->transaction_id)) {
        slot = &svc->entity.receiver;
    }

    if (slot != NULL) {
        memcpy(report.source_path, slot->source_path,
               sizeof(report.source_path));
        memcpy(report.destination_path, slot->destination_path,
               sizeof(report.destination_path));
        report.file_size = slot->file_size;
        report.checksum = slot->eof_checksum;
    }

    k_mutex_lock(&service_status_lock, K_FOREVER);
    service_status = report;
    k_mutex_unlock(&service_status_lock);

    if (event->type == CCSDS_CFDP_EVENT_COMPLETE) {
        LOG_INF("transaction=%llu:%llu source=%s dest=%s size=%u checksum=0x%08x checksum=OK status=COMPLETE",
                (unsigned long long)report.transaction_id.source_entity_id,
                (unsigned long long)report.transaction_id.transaction_sequence_number,
                report.source_path, report.destination_path, report.file_size,
                report.checksum);
    } else {
        LOG_ERR("transaction=%llu:%llu source=%s dest=%s size=%u checksum=0x%08x checksum=NOK status=FAILED cfdp_status=%s",
                (unsigned long long)report.transaction_id.source_entity_id,
                (unsigned long long)report.transaction_id.transaction_sequence_number,
                report.source_path, report.destination_path, report.file_size,
                report.checksum,
                akira_cfdp_service_status_name(report.status));
    }
}

static void service_event_forward(void *user, const ccsds_cfdp_event_t *event)
{
    struct akira_cfdp_service *svc = user;

    if (svc != NULL && event != NULL &&
        (event->type == CCSDS_CFDP_EVENT_COMPLETE ||
         event->type == CCSDS_CFDP_EVENT_FAILED)) {
        record_terminal_status(svc, event);
    }

    if (svc != NULL && svc->event_cb != NULL) {
        svc->event_cb(svc->event_user, event);
    }
}

void akira_cfdp_service_config_defaults(akira_cfdp_service_config_t *config)
{
    __ASSERT(config != NULL, "Akira CFDP service config is NULL");

    memset(config, 0, sizeof(*config));
    config->local_entity_id = CONFIG_AKIRA_CCSDS_CFDP_SERVICE_LOCAL_ENTITY_ID;
    config->remote_entity_id =
        CONFIG_AKIRA_CCSDS_CFDP_SERVICE_REMOTE_ENTITY_ID;
    config->entity_id_len = CONFIG_AKIRA_CCSDS_CFDP_SERVICE_ENTITY_ID_LEN;
    config->transaction_sequence_number_len =
        CONFIG_AKIRA_CCSDS_CFDP_SERVICE_TRANS_SEQ_LEN;
    config->initial_transaction_sequence_number =
        CONFIG_AKIRA_CCSDS_CFDP_SERVICE_INITIAL_TRANS_SEQ;
    config->local_apid = CONFIG_AKIRA_CCSDS_CFDP_SERVICE_APID;
    config->remote_apid = CONFIG_AKIRA_CCSDS_CFDP_SERVICE_APID;
    config->packet_type = AKIRA_CFDP_SERVICE_PACKET_TYPE;
#if defined(CONFIG_NETWORKING) && !defined(CONFIG_AKIRA_CCSDS_FRAME_SUPPORT)
    config->send_packet = ccsds_udp_send;
#endif
}

enum ccsds_cfdp_status
akira_cfdp_service_init(const akira_cfdp_service_config_t *config)
{
    akira_cfdp_service_config_t cfg;
    ccsds_cfdp_space_packet_adapter_config_t adapter_config;
    ccsds_cfdp_entity_config_t entity_config;
    ccsds_cfdp_ut_ops_t ut;
    enum ccsds_cfdp_status status;

    __ASSERT(config != NULL, "Akira CFDP service config is NULL");

    akira_cfdp_service_config_defaults(&cfg);
    if (config->local_entity_id != 0u) {
        cfg.local_entity_id = config->local_entity_id;
    }
    if (config->remote_entity_id != 0u) {
        cfg.remote_entity_id = config->remote_entity_id;
    }
    if (config->entity_id_len != 0u) {
        cfg.entity_id_len = config->entity_id_len;
    }
    if (config->transaction_sequence_number_len != 0u) {
        cfg.transaction_sequence_number_len =
            config->transaction_sequence_number_len;
    }
    if (config->initial_transaction_sequence_number != 0u) {
        cfg.initial_transaction_sequence_number =
            config->initial_transaction_sequence_number;
    }
    if (config->local_apid != 0u) {
        cfg.local_apid = config->local_apid;
    }
    cfg.remote_apid =
        config->remote_apid != 0u ? config->remote_apid : cfg.local_apid;
    if (config->send_packet != NULL) {
        cfg.send_packet = config->send_packet;
    }
    if (cfg.send_packet == NULL) {
        return CCSDS_CFDP_STATUS_INVALID_ARGUMENT;
    }
    cfg.send_user = config->send_user;
    cfg.now_ms = config->now_ms;
    cfg.receive_filestore = config->receive_filestore;
    cfg.event_cb = config->event_cb;
    cfg.event_user = config->event_user;
    if (cfg.packet_type != CCSDS_PACKET_TYPE_TM &&
        cfg.packet_type != CCSDS_PACKET_TYPE_TC) {
        cfg.packet_type = AKIRA_CFDP_SERVICE_PACKET_TYPE;
    }
    if (config->packet_type == CCSDS_PACKET_TYPE_TM ||
        config->packet_type == CCSDS_PACKET_TYPE_TC) {
        cfg.packet_type = config->packet_type;
    }

    memset(&service, 0, sizeof(service));
    k_mutex_lock(&service_status_lock, K_FOREVER);
    memset(&service_status, 0, sizeof(service_status));
    k_mutex_unlock(&service_status_lock);
    default_receive_filestore.user = &service;
    source_filestore.user = &service;
    service.receive_filestore = cfg.receive_filestore != NULL
                                    ? cfg.receive_filestore
                                    : &default_receive_filestore;
    service.event_cb = cfg.event_cb;
    service.event_user = cfg.event_user;

    adapter_config = (ccsds_cfdp_space_packet_adapter_config_t){
        .remote_entity_id = cfg.remote_entity_id,
        .local_apid = cfg.local_apid,
        .remote_apid = cfg.remote_apid,
        .packet_type = cfg.packet_type,
        .initial_sequence_count = AKIRA_CFDP_SERVICE_INITIAL_PACKET_SEQ,
        .send_packet = cfg.send_packet,
        .send_user = cfg.send_user,
        .now_ms = cfg.now_ms,
    };
    status =
        ccsds_cfdp_space_packet_adapter_init(&service.adapter, &adapter_config);
    if (status != CCSDS_CFDP_STATUS_OK) {
        return status;
    }

    ut = ccsds_cfdp_space_packet_adapter_ut_ops(&service.adapter);
    entity_config = (ccsds_cfdp_entity_config_t){
        .local_entity_id = cfg.local_entity_id,
        .remote_entity_id = cfg.remote_entity_id,
        .entity_id_len = cfg.entity_id_len,
        .transaction_sequence_number_len = cfg.transaction_sequence_number_len,
        .initial_transaction_sequence_number =
            cfg.initial_transaction_sequence_number,
        .event_cb = service_event_forward,
        .event_user = &service,
    };

    return ccsds_cfdp_entity_init(&service.entity, &entity_config, &ut);
}

int akira_cfdp_service_register_rx(struct ccsds_router *router)
{
    __ASSERT(router != NULL, "CCSDS router is NULL");

    return ccsds_cfdp_space_packet_adapter_register_rx(
        &service.adapter, router, &service.entity, service.receive_filestore);
}

enum ccsds_cfdp_status
akira_cfdp_service_send_file(const ccsds_cfdp_filestore_ops_t *filestore,
                             const ccsds_cfdp_put_request_t *request,
                             ccsds_cfdp_transaction_id_t *transaction_id)
{
    return ccsds_cfdp_entity_send_file(&service.entity, filestore, request,
                                       transaction_id);
}

enum ccsds_cfdp_status
akira_cfdp_service_send_path(const char *source_path,
                             const char *destination_path,
                             ccsds_cfdp_transaction_id_t *transaction_id)
{
    return akira_cfdp_service_send_path_mode(
        source_path, destination_path, false, transaction_id);
}

enum ccsds_cfdp_status
akira_cfdp_service_send_path_mode(const char *source_path,
                                  const char *destination_path,
                                  bool acknowledged_mode,
                                  ccsds_cfdp_transaction_id_t *transaction_id)
{
#ifdef CONFIG_FILE_SYSTEM
    const ccsds_cfdp_put_request_t request = {
        .source_path = source_path,
        .destination_path = destination_path,
        .checksum_type = CCSDS_CFDP_CHECKSUM_TYPE_MODULAR,
        .closure_requested = acknowledged_mode,
        .acknowledged_mode = acknowledged_mode,
    };

    return akira_cfdp_service_send_file(&source_filestore, &request,
                                        transaction_id);
#else
    ARG_UNUSED(source_path);
    ARG_UNUSED(destination_path);
    ARG_UNUSED(transaction_id);
    return CCSDS_CFDP_STATUS_UNSUPPORTED;
#endif
}

void akira_cfdp_service_get_status(struct akira_cfdp_service_status *status)
{
    __ASSERT(status != NULL, "Akira CFDP service status output is NULL");

    k_mutex_lock(&service_status_lock, K_FOREVER);
    *status = service_status;
    k_mutex_unlock(&service_status_lock);
}

void akira_cfdp_service_poll(uint64_t now_ms)
{
    ccsds_cfdp_entity_poll(&service.entity, now_ms);
}

ccsds_cfdp_entity_t *akira_cfdp_service_entity(void) { return &service.entity; }

ccsds_cfdp_space_packet_adapter_t *akira_cfdp_service_space_packet_adapter(void)
{
    return &service.adapter;
}

const ccsds_cfdp_filestore_ops_t *akira_cfdp_service_receive_filestore(void)
{
    return service.receive_filestore;
}
