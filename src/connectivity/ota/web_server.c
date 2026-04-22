/**
 * @file web_server.c
 * @brief Optimized Web Server Implementation for ESP32
 *
 */

/* Enable GNU extensions for memmem (used for multipart boundary search in HTTP uploads) */
#define _GNU_SOURCE
#include <string.h>

#include "web_server.h"
#include "ota_manager.h"
#include "../transport_interface.h"
#include "../buf_pool.h"

/* WebServer OTA transport implementation */
static int webserver_ota_start(void *user_data)
{
    /* Start OTA via HTTP upload */
    return OTA_OK;
}

static int webserver_ota_stop(void *user_data)
{
    /* Stop OTA upload */
    return OTA_OK;
}

static int webserver_ota_send_chunk(const uint8_t *data, size_t len, void *user_data)
{
    /* Pass chunk to OTA manager */
    // Example: ota_manager_write_chunk(data, len);
    return OTA_OK;
}

static int webserver_ota_report_progress(uint8_t percent, void *user_data)
{
    /* Report progress to HTTP client */
    return OTA_OK;
}

static ota_transport_t webserver_ota_transport = {
    .name = "webserver",
    .start = webserver_ota_start,
    .stop = webserver_ota_stop,
    .send_chunk = webserver_ota_send_chunk,
    .report_progress = webserver_ota_report_progress,
    .user_data = NULL};

static void register_webserver_ota_transport(void)
{
    ota_manager_register_transport(&webserver_ota_transport);
}

/* Call register_webserver_ota_transport() during web server init */
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/net_if.h>
#include <zephyr/kernel.h>
#include <zephyr/posix/fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "akira.h"
#ifdef CONFIG_AKIRA_APP_MANAGER
#include <runtime/app_manager/app_manager.h>
#endif

LOG_MODULE_REGISTER(web_server, CONFIG_LOG_DEFAULT_LEVEL);

/* TCP_NODELAY may not be defined on all platforms */
#ifndef TCP_NODELAY
#define TCP_NODELAY 1
#endif

/* Optimized constants - small stack buffers, large data uses akira_buf pool */
#define HTTP_BUFFER_SIZE 512
#define HTTP_RESPONSE_BUFFER_SIZE 256
#undef UPLOAD_CHUNK_SIZE
#define UPLOAD_CHUNK_SIZE 512
/* Backwards compatible fallback for MAX_CONNECTIONS: if CONFIG_AKIRA_HTTP_MAX_CONNECTIONS
 * isn't defined at build time, fall back to a sane default. This prevents
 * passing an invalid backlog to listen() which can lead to odd errno values
 * on some network stacks. */
#ifndef MAX_CONNECTIONS
#if defined(CONFIG_AKIRA_HTTP_MAX_CONNECTIONS)
#define MAX_CONNECTIONS CONFIG_AKIRA_HTTP_MAX_CONNECTIONS
#else
#define MAX_CONNECTIONS 5
#endif
#endif

/* Thread stack: 8 KB needed — ESP32-S3 Xtensa stores 6 nested IRQ frames
 * (~2 KB) plus zsock_poll/lwIP internals on this stack at runtime. */
static K_THREAD_STACK_DEFINE(web_server_stack, WEB_SERVER_STACK_SIZE);
static struct k_thread web_server_thread_data;
static k_tid_t web_server_thread_id;

/* Compact server state */
static struct
{
    enum web_server_state state;
    uint32_t requests_handled;
    uint32_t bytes_transferred;
    uint8_t active_connections;
    bool network_connected;
    char server_ip[16];
} server_state __aligned(4);

static struct web_server_callbacks callbacks = {0};
static K_MUTEX_DEFINE(server_mutex);

/* Message queue */
#define SERVER_MSG_QUEUE_SIZE 4

enum server_msg_type
{
    MSG_START_SERVER = 1,
    MSG_STOP_SERVER,
    MSG_NETWORK_STATUS_CHANGED
};

struct server_msg
{
    enum server_msg_type type : 8;
    uint8_t reserved;
    union
    {
        struct
        {
            bool connected;
            char ip_address[16];
        } network_status;
    } data;
} __packed;

K_MSGQ_DEFINE(server_msgq, sizeof(struct server_msg), SERVER_MSG_QUEUE_SIZE, 4);

static size_t parse_content_length(const char *request_data)
{
    const char *content_length_header = strstr(request_data, "Content-Length:");
    if (!content_length_header)
    {
        return 0; // No Content-Length header
    }

    content_length_header += 15; // Skip "Content-Length:"

    /* Skip whitespace */
    while (*content_length_header == ' ' || *content_length_header == '\t')
    {
        content_length_header++;
    }

    /* Parse number with bounds checking */
    char *endptr;
    unsigned long length = strtoul(content_length_header, &endptr, 10);

    /* Validate parsing */
    if (endptr == content_length_header || length > SIZE_MAX)
    {
        LOG_ERR("Invalid Content-Length value");
        return 0;
    }

    /* Reasonable size limits for embedded device */
    if (length > (2 * 1024 * 1024))
    { // Max 2MB
        LOG_ERR("Content-Length too large: %lu", length);
        return 0;
    }

    return (size_t)length;
}

static int find_multipart_boundary(const char *request_data, char *boundary, size_t boundary_size)
{
    const char *content_type = strstr(request_data, "Content-Type:");
    if (!content_type)
    {
        return -1;
    }

    const char *boundary_start = strstr(content_type, "boundary=");
    if (!boundary_start)
    {
        return -1;
    }

    boundary_start += 9; // Skip "boundary="

    /* Find end of boundary (space, newline, or semicolon) */
    const char *boundary_end = boundary_start;
    while (*boundary_end && *boundary_end != ' ' &&
           *boundary_end != '\r' && *boundary_end != '\n' &&
           *boundary_end != ';')
    {
        boundary_end++;
    }

    size_t boundary_len = boundary_end - boundary_start;
    if (boundary_len == 0 || boundary_len >= boundary_size - 2)
    {
        return -1;
    }

    /* Add "--" prefix for multipart boundary */
    boundary[0] = '-';
    boundary[1] = '-';
    memcpy(boundary + 2, boundary_start, boundary_len);
    boundary[boundary_len + 2] = '\0';

    return 0;
}

/* HTTP response helpers - uses small stack buffer for headers only */
static int send_http_response(int client_fd, int status_code, const char *content_type,
                              const char *body, size_t body_len)
{
    char header[HTTP_RESPONSE_BUFFER_SIZE];
    int header_len;

    if (body_len == 0 && body)
    {
        body_len = strlen(body);
    }

    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %zu\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          status_code,
                          (status_code == 200) ? "OK" : "Error",
                          content_type,
                          body_len);

    if (header_len >= sizeof(header))
    {
        LOG_ERR("Header too large");
        return -1;
    }

    /* Send header */
    if (send(client_fd, header, header_len, 0) != header_len)
    {
        LOG_ERR("Header send failed: errno=%d", errno);
        return -1;
    }

    /* Send body - simple chunked send */
    if (body && body_len > 0)
    {
        const char *ptr = body;
        size_t remaining = body_len;

        while (remaining > 0)
        {
            size_t to_send = (remaining > 512) ? 512 : remaining;
            ssize_t sent = send(client_fd, ptr, to_send, 0);
            
            if (sent <= 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    k_msleep(50);
                    continue;
                }
                LOG_ERR("Body send failed: errno=%d", errno);
                return -1;
            }
            
            ptr += sent;
            remaining -= sent;
            k_yield();
        }
    }

    return 0;
}

/* HTTP response helpers */
static int parse_http_request(const char *buffer, char *method, char *path, size_t path_size)
{
    const char *space1 = strchr(buffer, ' ');
    const char *space2 = space1 ? strchr(space1 + 1, ' ') : NULL;

    if (!space1 || !space2)
    {
        return -1;
    }

    /* Extract method */
    size_t method_len = space1 - buffer;
    if (method_len >= 8)
        return -1; // Max method length

    memcpy(method, buffer, method_len);
    method[method_len] = '\0';

    /* Extract path */
    size_t path_len = space2 - space1 - 1;
    if (path_len >= path_size)
        return -1;

    memcpy(path, space1 + 1, path_len);
    path[path_len] = '\0';

    return 0;
}

/* Handle firmware upload with optimized multipart parsing and transport_notify()
 * initial_body: body data already read during HTTP header parsing
 * initial_body_len: length of initial_body data
 *
 * Uses in-place boundary checking and zero-copy dispatch via transport layer.
 */
static int handle_firmware_upload(int client_fd, const char *request_headers, size_t content_length,
                                  const char *initial_body, size_t initial_body_len)
{
    if (content_length == 0 || content_length > (2 * 1024 * 1024))
    {
        send_http_response(client_fd, 400, "text/plain", "Invalid file size", 0);
        return -1;
    }

    /* Set longer receive timeout for upload */
    struct timeval upload_timeout = {.tv_sec = 60, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &upload_timeout, sizeof(upload_timeout));

    /* Find multipart boundary from HTTP headers - in-place check */
    char boundary[128];
    if (find_multipart_boundary(request_headers, boundary, sizeof(boundary)) != 0)
    {
        send_http_response(client_fd, 400, "text/plain", "Invalid multipart format", 0);
        return -1;
    }

    LOG_INF("Firmware upload: content-length=%zu, boundary=%s", content_length, boundary);

    /* Allocate buffer from shared pool for multipart header parsing */
    struct akira_buf *hdr_buf = akira_buf_alloc(K_MSEC(100));
    if (!hdr_buf)
    {
        LOG_ERR("Failed to allocate header buffer from pool");
        send_http_response(client_fd, 503, "text/plain", "Server busy", 0);
        return -1;
    }

    /* Copy initial body data to header buffer */
    if (initial_body_len > 0)
    {
        size_t copy_len = MIN(initial_body_len, akira_buf_tailroom(hdr_buf));
        akira_buf_add_mem(hdr_buf, initial_body, copy_len);
    }

    /* In-place boundary checking for multipart header end */
    char *data_start = strstr((char *)hdr_buf->data, "\r\n\r\n");

    while (!data_start && akira_buf_tailroom(hdr_buf) > 0)
    {
        ssize_t received = recv(client_fd, akira_buf_tail(hdr_buf),
                                akira_buf_tailroom(hdr_buf), 0);
        if (received <= 0)
        {
            LOG_ERR("Failed to receive multipart header");
            akira_buf_unref(hdr_buf);
            send_http_response(client_fd, 400, "text/plain", "Failed to receive header", 0);
            return -1;
        }
        akira_buf_add_len(hdr_buf, received);
        data_start = strstr((char *)hdr_buf->data, "\r\n\r\n");
    }

    if (!data_start)
    {
        LOG_ERR("Could not find multipart header end");
        akira_buf_unref(hdr_buf);
        send_http_response(client_fd, 400, "text/plain", "Invalid multipart format", 0);
        return -1;
    }
    data_start += 4; /* Skip \r\n\r\n */

    size_t header_size = data_start - (char *)hdr_buf->data;
    size_t first_data_len = hdr_buf->len - header_size;
    size_t expected_size = content_length;

    LOG_INF("Header=%zu, first_chunk=%zu, expected=%zu", header_size, first_data_len, expected_size);

    /* Signal transfer start via transport layer */
    int ret = transport_begin(TRANSPORT_DATA_FIRMWARE, expected_size, "firmware.bin");
    if (ret != 0)
    {
        LOG_ERR("transport_begin failed: %d", ret);
        akira_buf_unref(hdr_buf);
        send_http_response(client_fd, 500, "text/plain", "Failed to start OTA", 0);
        return -1;
    }

    size_t total_written = 0;
    size_t total_received = hdr_buf->len;
    size_t boundary_len = strlen(boundary);
    int retry_count = 0;
    uint8_t last_progress = 0;

    /* Chunk info for transport notifications */
    struct transport_chunk_info chunk_info = {
        .type = TRANSPORT_DATA_FIRMWARE,
        .total_size = expected_size,
        .offset = 0,
        .flags = 0,
        .name = "firmware.bin",
    };

    /* Write first chunk via transport_notify() */
    if (first_data_len > 0)
    {
        chunk_info.offset = 0;
        ret = transport_notify(TRANSPORT_DATA_FIRMWARE, (uint8_t *)data_start,
                               first_data_len, &chunk_info);
        if (ret != 0)
        {
            LOG_ERR("First chunk write failed: %d", ret);
            akira_buf_unref(hdr_buf);
            transport_abort(TRANSPORT_DATA_FIRMWARE);
            send_http_response(client_fd, 500, "text/plain", "OTA write failed", 0);
            return -1;
        }
        total_written = first_data_len;
        chunk_info.offset = total_written;
    }

    /* Release header buffer - no longer needed */
    akira_buf_unref(hdr_buf);
    hdr_buf = NULL;

    /* Allocate upload buffer from shared pool for zero-copy receive */
    struct akira_buf *upload_buf = akira_buf_alloc(K_MSEC(100));
    if (!upload_buf)
    {
        LOG_ERR("Failed to allocate upload buffer from pool");
        transport_abort(TRANSPORT_DATA_FIRMWARE);
        send_http_response(client_fd, 503, "text/plain", "Server busy", 0);
        return -1;
    }

    while (total_received < content_length)
    {
        /* Reset buffer for new chunk */
        akira_buf_reset(upload_buf);

        size_t chunk_size = MIN(AKIRA_BUF_SIZE, content_length - total_received);
        ssize_t received = recv(client_fd, upload_buf->data, chunk_size, 0);

        if (received < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                if (++retry_count > 300) /* 30 second timeout */
                {
                    LOG_ERR("Upload timeout at %zu bytes", total_written);
                    akira_buf_unref(upload_buf);
                    transport_abort(TRANSPORT_DATA_FIRMWARE);
                    send_http_response(client_fd, 408, "text/plain", "Upload timeout", 0);
                    return -1;
                }
                k_msleep(100);
                continue;
            }
            LOG_ERR("Receive failed: errno=%d", errno);
            akira_buf_unref(upload_buf);
            transport_abort(TRANSPORT_DATA_FIRMWARE);
            send_http_response(client_fd, 500, "text/plain", "Upload failed", 0);
            return -1;
        }
        else if (received == 0)
        {
            LOG_WRN("Connection closed at %zu/%zu bytes", total_received, content_length);
            break;
        }

        /* Update buffer length after recv */
        akira_buf_add_len(upload_buf, received);
        retry_count = 0;
        total_received += received;

        /* In-place boundary check for closing boundary */
        char *end_boundary = memmem(upload_buf->data, upload_buf->len, boundary, boundary_len);
        if (end_boundary)
        {
            size_t data_len = end_boundary - (char *)upload_buf->data;
            if (data_len >= 2) data_len -= 2; /* Remove preceding \r\n */

            if (data_len > 0)
            {
                ret = transport_notify(TRANSPORT_DATA_FIRMWARE, upload_buf->data,
                                       data_len, &chunk_info);
                if (ret != 0)
                {
                    LOG_ERR("Final chunk write failed: %d", ret);
                    akira_buf_unref(upload_buf);
                    transport_abort(TRANSPORT_DATA_FIRMWARE);
                    send_http_response(client_fd, 500, "text/plain", "OTA write failed", 0);
                    return -1;
                }
                total_written += data_len;
            }
            LOG_INF("Closing boundary found, upload complete");
            break;
        }

        /* Dispatch chunk via transport layer - zero-copy from akira_buf */
        ret = transport_notify(TRANSPORT_DATA_FIRMWARE, upload_buf->data,
                               upload_buf->len, &chunk_info);
        if (ret != 0)
        {
            LOG_ERR("Chunk write failed at %zu: %d", total_written, ret);
            akira_buf_unref(upload_buf);
            transport_abort(TRANSPORT_DATA_FIRMWARE);
            send_http_response(client_fd, 500, "text/plain", "OTA write failed", 0);
            return -1;
        }
        total_written += upload_buf->len;
        chunk_info.offset = total_written;

        /* Progress report every 10% */
        uint8_t progress = (total_written * 100) / expected_size;
        if (progress >= last_progress + 10)
        {
            LOG_INF("OTA: %u%% (%zu bytes)", progress, total_written);
            last_progress = progress;
        }

        k_yield();
    }

    /* Release upload buffer */
    akira_buf_unref(upload_buf);

    LOG_INF("Upload finished: wrote %zu bytes to flash", total_written);

    if (total_written == 0)
    {
        transport_abort(TRANSPORT_DATA_FIRMWARE);
        send_http_response(client_fd, 400, "text/plain", "No file data found", 0);
        return -1;
    }

    /* Signal transfer end - this triggers finalization */
    ret = transport_end(TRANSPORT_DATA_FIRMWARE, true);
    if (ret != 0)
    {
        LOG_ERR("transport_end failed: %d", ret);
        send_http_response(client_fd, 500, "text/plain", "OTA finalization failed", 0);
        return -1;
    }

    /* Send redirect to main page */
    const char *redirect_response =
        "HTTP/1.1 302 Found\r\n"
        "Location: /\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n"
        "\r\n";

    send(client_fd, redirect_response, strlen(redirect_response), 0);

    /* Schedule reboot */
    ota_reboot_to_apply_update(3000);
    return 0;
}

static bool check_upload_token(const char *request_headers)
{
#if defined(CONFIG_AKIRA_HTTP_UPLOAD_TOKEN) && (sizeof(CONFIG_AKIRA_HTTP_UPLOAD_TOKEN) > 1)
    /* Locate Authorization header */
    const char *auth = strstr(request_headers, "Authorization: Bearer ");
    if (!auth) {
        return false;
    }
    auth += 22; /* skip "Authorization: Bearer " */

    const char *end = strpbrk(auth, "\r\n");
    size_t given_len = end ? (size_t)(end - auth) : strlen(auth);
    size_t expected_len = sizeof(CONFIG_AKIRA_HTTP_UPLOAD_TOKEN) - 1;

    /* Constant-time comparison to avoid trivial timing oracles */
    if (given_len != expected_len) {
        return false;
    }
    uint8_t diff = 0;
    for (size_t i = 0; i < expected_len; i++) {
        diff |= (uint8_t)auth[i] ^ (uint8_t)CONFIG_AKIRA_HTTP_UPLOAD_TOKEN[i];
    }
    return diff == 0;
#else
    /* No token configured — allow (warn is emitted at server start) */
    return true;
#endif
}

static int handle_http_request(int client_fd)
{
    char buffer[HTTP_BUFFER_SIZE];
    char method[16], path[128];
    ssize_t received;

    /* Set socket timeouts */
    struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
    setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    /* Disable Nagle for faster small packet sends */
    int flag = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

    /* Receive request with timeout */
    received = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (received <= 0)
    {
        LOG_WRN("Request receive failed or timeout: %d", errno);
        return -1;
    }

    buffer[received] = '\0';

    /* Parse request line */
    if (parse_http_request(buffer, method, path, sizeof(path)) != 0)
    {
        send_http_response(client_fd, 400, "text/plain", "Bad Request", 0);
        return -1;
    }

    LOG_DBG("HTTP %s %s", method, path);

    /* Only POST methods are served; the web UI is removed (Phase 2: AkiraHub). */
    if (strcmp(method, "POST") == 0)
    {
        if (strcmp(path, "/upload") == 0)
        {
            /* Authenticate before accepting any data */
            if (!check_upload_token(buffer)) {
                LOG_WRN("Upload rejected: missing or invalid Bearer token");
                return send_http_response(client_fd, 401, "text/plain",
                                          "Unauthorized", 0);
            }

            /* Parse Content-Length with validation */
            size_t content_length = parse_content_length(buffer);
            if (content_length == 0)
            {
                return send_http_response(client_fd, 400, "text/plain",
                                          "Missing or invalid Content-Length", 0);
            }

            /* Find end of HTTP headers - body data may already be in buffer */
            char *body_start = strstr(buffer, "\r\n\r\n");
            if (!body_start)
            {
                return send_http_response(client_fd, 400, "text/plain",
                                          "Invalid HTTP request", 0);
            }
            body_start += 4; /* Skip \r\n\r\n */

            /* Calculate how much body data was already received */
            size_t headers_len = body_start - buffer;
            size_t body_already_read = received - headers_len;

            return handle_firmware_upload(client_fd, buffer, content_length,
                                          body_start, body_already_read);
        }

#ifdef CONFIG_AKIRA_APP_MANAGER
        /* App upload endpoint - POST /api/apps/install */
        if (strncmp(path, "/api/apps/install", 17) == 0)
        {
            /* Authenticate before accepting any data */
            if (!check_upload_token(buffer)) {
                LOG_WRN("App install rejected: missing or invalid Bearer token");
                return send_http_response(client_fd, 401, "text/plain",
                                          "Unauthorized", 0);
            }

            size_t content_length = parse_content_length(buffer);
            if (content_length == 0 || content_length > CONFIG_AKIRA_APP_MAX_SIZE_KB * 1024)
            {
                return send_http_response(client_fd, 400, "text/plain",
                                          "Invalid Content-Length", 0);
            }

            char *body_start = strstr(buffer, "\r\n\r\n");
            if (!body_start)
            {
                return send_http_response(client_fd, 400, "text/plain",
                                          "Invalid HTTP request", 0);
            }
            body_start += 4;

            /* Get app name from query parameter */
            char app_name[32] = "uploaded_app";
            const char *name_param = strstr(buffer, "?name=");
            if (name_param)
            {
                name_param += 6;
                const char *end = strpbrk(name_param, " &\r\n");
                size_t len = end ? (size_t)(end - name_param) : strlen(name_param);
                if (len > 0 && len < sizeof(app_name))
                {
                    strncpy(app_name, name_param, len);
                    app_name[len] = '\0';
                }
            }

            /* Begin chunked install */
            int session = app_manager_install_begin(app_name, content_length, APP_SOURCE_HTTP);
            if (session < 0)
            {
                char resp[64];
                snprintf(resp, sizeof(resp), "{\"error\":\"Install begin failed: %d\"}", session);
                return send_http_response(client_fd, 500, "application/json", resp, 0);
            }

            /* Write first chunk */
            size_t headers_len = body_start - buffer;
            size_t body_already_read = received - headers_len;
            if (body_already_read > 0)
            {
                int ret = app_manager_install_chunk(session, body_start, body_already_read);
                if (ret < 0)
                {
                    app_manager_install_abort(session);
                    return send_http_response(client_fd, 500, "text/plain", "Chunk write failed", 0);
                }
            }

            /* Receive remaining data using shared buffer pool */
            size_t total_received = body_already_read;
            struct akira_buf *app_buf = akira_buf_alloc(K_MSEC(100));
            if (!app_buf)
            {
                app_manager_install_abort(session);
                return send_http_response(client_fd, 503, "text/plain", "Server busy", 0);
            }

            while (total_received < content_length)
            {
                akira_buf_reset(app_buf);
                size_t chunk_size = MIN(AKIRA_BUF_SIZE, content_length - total_received);
                ssize_t recvd = recv(client_fd, app_buf->data, chunk_size, 0);
                if (recvd <= 0)
                {
                    akira_buf_unref(app_buf);
                    app_manager_install_abort(session);
                    return send_http_response(client_fd, 500, "text/plain", "Upload failed", 0);
                }
                akira_buf_add_len(app_buf, recvd);
                int ret = app_manager_install_chunk(session, (const char *)app_buf->data, app_buf->len);
                if (ret < 0)
                {
                    akira_buf_unref(app_buf);
                    app_manager_install_abort(session);
                    return send_http_response(client_fd, 500, "text/plain", "Chunk write failed", 0);
                }
                total_received += recvd;
                k_yield();
            }

            akira_buf_unref(app_buf);

            /* Finalize install */
            int app_id = app_manager_install_end(session, NULL);
            if (app_id < 0)
            {
                char resp[64];
                snprintf(resp, sizeof(resp), "{\"error\":\"Install failed: %d\"}", app_id);
                return send_http_response(client_fd, 500, "application/json", resp, 0);
            }

            char resp[128];
            snprintf(resp, sizeof(resp), "{\"status\":\"installed\",\"name\":\"%s\",\"id\":%d}", app_name, app_id);
            return send_http_response(client_fd, 200, "application/json", resp, 0);
        }
#endif /* CONFIG_AKIRA_APP_MANAGER */
    }

    return send_http_response(client_fd, 404, "text/plain", "Not Found", 0);
}

/* Simplified web server */
static int run_web_server(void)
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        LOG_ERR("Socket creation failed: %d", errno);
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Set socket non-blocking so accept() returns EAGAIN immediately if no
     * client is queued.  We use poll() with a 5 s timeout to wait for
     * incoming connections.  poll() on Zephyr's native TCP stack delegates to
     * k_poll on the accept_q FIFO — it properly sleeps (zero CPU usage) until
     * a SYN-SYNACK-ACK handshake completes and a connection is queued, or the
     * timeout fires so we can check server_state every 5 seconds. */
    int sock_flags = fcntl(server_fd, F_GETFL, 0);
    if (sock_flags < 0)
    {
        sock_flags = 0;
    }
    fcntl(server_fd, F_SETFL, sock_flags | O_NONBLOCK);

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(HTTP_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        LOG_ERR("Bind failed: %d (%s)", errno, strerror(errno));
        close(server_fd);
        return -1;
    }
    /* Attempt to listen with a small retry loop since embedded socket
     * stacks may transiently fail (returning errno). Retry bind/listen
     * sequence a few times before giving up. */
    int attempts = 0;
    const int max_attempts = 3;
    for (;;)
    {
        if (listen(server_fd, MAX_CONNECTIONS) == 0)
        {
            /* Successfully listening - now we're truly running */
            server_state.state = WEB_SERVER_RUNNING;
            LOG_INF("HTTP server listening on port %d", HTTP_PORT);
            break;
        }

        int saved_errno = errno;
        LOG_ERR("Listen failed (attempt %d/%d): %d (%s) - fd=%d", ++attempts, max_attempts, saved_errno, strerror(saved_errno), server_fd);

        /* Additional diagnostics */
        struct sockaddr_in sa = {0};
        socklen_t sa_len = sizeof(sa);
        if (getsockname(server_fd, (struct sockaddr *)&sa, &sa_len) == 0)
        {
            LOG_INF("Socket bound to %s:%d", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port));
        }

        int so_err = 0;
        socklen_t so_len = sizeof(so_err);
        if (getsockopt(server_fd, SOL_SOCKET, SO_ERROR, &so_err, &so_len) == 0 && so_err != 0)
        {
            LOG_INF("SO_ERROR on socket: %d (%s)", so_err, strerror(so_err));
        }

        /* Try to dump fd flags if available */
#if defined(F_GETFL)
        int flags = fcntl(server_fd, F_GETFL, 0);
        if (flags >= 0)
        {
            LOG_INF("FD flags: 0x%08x", flags);
        }
#endif
        if (attempts >= max_attempts)
        {
            close(server_fd);
            return -1;
        }

        /* Try closing and recreating the socket (handle transient port/controller issues) */
        close(server_fd);
        k_sleep(K_MSEC(200));

        server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_fd < 0)
        {
            LOG_ERR("Socket recreation failed: %d (%s)", errno, strerror(errno));
            k_sleep(K_MSEC(200));
            continue;
        }
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sock_flags = fcntl(server_fd, F_GETFL, 0);
        fcntl(server_fd, F_SETFL, (sock_flags < 0 ? 0 : sock_flags) | O_NONBLOCK);
        if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            LOG_ERR("Bind retry failed: %d (%s)", errno, strerror(errno));
            close(server_fd);
            k_sleep(K_MSEC(200));
            continue;
        }
    }

    while (server_state.state == WEB_SERVER_RUNNING)
    {
        /* poll() sleeps (via k_poll on accept_q FIFO) until a completed TCP
         * connection is queued or the 5 s timeout fires.  Zero CPU overhead. */
        struct zsock_pollfd pfd = {.fd = server_fd, .events = ZSOCK_POLLIN};
        int pret = poll(&pfd, 1, 5000);

        if (pret < 0)
        {
            if (server_state.state == WEB_SERVER_RUNNING)
            {
                LOG_ERR("poll failed: errno=%d (%s)", errno, strerror(errno));
            }
            continue;
        }

        if (pret == 0)
        {
            /* Timeout — no connection in 5 s, just check state and retry */
            LOG_DBG("Web server waiting for connections on port %d...", HTTP_PORT);
            continue;
        }

        /* Connection ready — accept() should succeed immediately */
        client_len = sizeof(client_addr);
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /* Race: connection reset before accept() ran */
                continue;
            }
            if (server_state.state == WEB_SERVER_RUNNING)
            {
                LOG_ERR("accept failed: errno=%d (%s)", errno, strerror(errno));
                if (errno == ENOMEM)
                {
                    k_sleep(K_MSEC(100));
                }
            }
            continue;
        }

        LOG_DBG("Client connected from %d.%d.%d.%d",
                (client_addr.sin_addr.s_addr) & 0xFF,
                (client_addr.sin_addr.s_addr >> 8) & 0xFF,
                (client_addr.sin_addr.s_addr >> 16) & 0xFF,
                (client_addr.sin_addr.s_addr >> 24) & 0xFF);

        /* Handle request */
        if (handle_http_request(client_fd) == 0)
        {
            k_mutex_lock(&server_mutex, K_FOREVER);
            server_state.requests_handled++;
            k_mutex_unlock(&server_mutex);
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}

/* Server operations */
static void do_start_server(void)
{
    if (server_state.state == WEB_SERVER_RUNNING || 
        server_state.state == WEB_SERVER_STARTING)
    {
        return;
    }

    server_state.state = WEB_SERVER_STARTING;
    LOG_INF("Starting web server...");
    
    /* This is a blocking call - runs until server is stopped */
    int ret = run_web_server();
    
    if (ret < 0)
    {
        /* Server failed to start or encountered error */
        server_state.state = WEB_SERVER_ERROR;
        LOG_ERR("Web server failed with error: %d", ret);
    }
    else
    {
        /* Server stopped normally */
        server_state.state = WEB_SERVER_STOPPED;
        LOG_INF("Web server stopped");
    }
}

static void do_stop_server(void)
{
    server_state.state = WEB_SERVER_STOPPED;
    LOG_INF("Web server stopped");
}

static void do_network_status_changed(bool connected, const char *ip_address)
{
    server_state.network_connected = connected;

    if (connected && ip_address)
    {
        strncpy(server_state.server_ip, ip_address, sizeof(server_state.server_ip) - 1);
        server_state.server_ip[sizeof(server_state.server_ip) - 1] = '\0';
        LOG_INF("Network connected: http://%s:%d", server_state.server_ip, HTTP_PORT);

        LOG_INF("Network connected: %s, port %d", ip_address, HTTP_PORT);

        if (server_state.state == WEB_SERVER_STOPPED)
        {
            do_start_server();
        }
    }
    else
    {
        server_state.server_ip[0] = '\0';
        LOG_INF("Network disconnected");

    }
}

/* Web server thread */
static void web_server_thread_main(void *p1, void *p2, void *p3)
{
    struct server_msg msg;

    LOG_INF("Web server thread started");

    while (1)
    {
        if (k_msgq_get(&server_msgq, &msg, K_MSEC(5000)) == 0)
        {
            switch (msg.type)
            {
            case MSG_START_SERVER:
                do_start_server();
                break;
            case MSG_STOP_SERVER:
                do_stop_server();
                break;
            case MSG_NETWORK_STATUS_CHANGED:
                do_network_status_changed(msg.data.network_status.connected,
                                          msg.data.network_status.ip_address);
                break;
            }
        }

        /* Periodic housekeeping */
        if (server_state.state == WEB_SERVER_RUNNING)
        {
            k_mutex_lock(&server_mutex, K_FOREVER);
            server_state.active_connections = 0; // Reset counter
            k_mutex_unlock(&server_mutex);
        }
    }
}

/* Public API */
int web_server_start(const struct web_server_callbacks *cb)
{
    if (cb)
    {
        memcpy(&callbacks, cb, sizeof(callbacks));
    }

    memset(&server_state, 0, sizeof(server_state));
    server_state.state = WEB_SERVER_STOPPED;
    strcpy(server_state.server_ip, "0.0.0.0");

    web_server_thread_id = k_thread_create(&web_server_thread_data,
                                           web_server_stack,
                                           K_THREAD_STACK_SIZEOF(web_server_stack),
                                           web_server_thread_main,
                                           NULL, NULL, NULL,
                                           WEB_SERVER_THREAD_PRIORITY,
                                           0, K_NO_WAIT);

    k_thread_name_set(web_server_thread_id, "web_server");

    LOG_INF("Web server initialized");
    return 0;
}

int web_server_stop(void)
{
    struct server_msg msg = {.type = MSG_STOP_SERVER};
    return (k_msgq_put(&server_msgq, &msg, K_NO_WAIT) == 0) ? 0 : -ENOMEM;
}

int web_server_get_stats(struct web_server_stats *stats)
{
    if (!stats)
        return -EINVAL;

    k_mutex_lock(&server_mutex, K_FOREVER);
    stats->state = server_state.state;
    stats->requests_handled = server_state.requests_handled;
    stats->bytes_transferred = server_state.bytes_transferred;
    stats->active_connections = server_state.active_connections;
    k_mutex_unlock(&server_mutex);

    return 0;
}

bool web_server_is_running(void)
{
    return (server_state.state == WEB_SERVER_RUNNING);
}

enum web_server_state web_server_get_state(void)
{
    return server_state.state;
}

void web_server_notify_network_status(bool connected, const char *ip_address)
{
    struct server_msg msg = {
        .type = MSG_NETWORK_STATUS_CHANGED,
        .data.network_status.connected = connected};

    if (connected && ip_address)
    {
        strncpy(msg.data.network_status.ip_address, ip_address,
                sizeof(msg.data.network_status.ip_address) - 1);
        msg.data.network_status.ip_address[sizeof(msg.data.network_status.ip_address) - 1] = '\0';
    }
    else
    {
        msg.data.network_status.ip_address[0] = '\0';
    }

    k_msgq_put(&server_msgq, &msg, K_NO_WAIT);
}