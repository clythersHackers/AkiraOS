#include "akira_cfdp_staging.h"

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/sys/util.h>

#include "ccsds_cfdp_config.h"

#ifdef CONFIG_FILE_SYSTEM
#include <zephyr/fs/fs.h>
#endif

#define AKIRA_CFDP_STAGE_ROOT "/lfs/cfdp"
#define AKIRA_CFDP_STAGING_DIR AKIRA_CFDP_STAGE_ROOT "/staging"
#define AKIRA_CFDP_RECEIVED_DIR AKIRA_CFDP_STAGE_ROOT "/received"

#define AKIRA_CFDP_MAX_STAGED_PATH_LEN \
    (sizeof(AKIRA_CFDP_RECEIVED_DIR) + CCSDS_CFDP_MAX_FILENAME_LEN)

struct akira_cfdp_staging_backend {
    void *user;
    int (*mkdir)(void *user, const char *path);
    int (*create)(void *user, const char *path);
    int (*rename)(void *user, const char *from, const char *to);
    int (*unlink)(void *user, const char *path);
};

static int default_mkdir(void *user, const char *path)
{
    ARG_UNUSED(user);

#ifdef CONFIG_FILE_SYSTEM
    return fs_mkdir(path);
#else
    ARG_UNUSED(path);
    return -ENOSYS;
#endif
}

static int default_create(void *user, const char *path)
{
    ARG_UNUSED(user);

#ifdef CONFIG_FILE_SYSTEM
    struct fs_file_t file;
    int rc;

    fs_file_t_init(&file);
    rc = fs_open(&file, path, FS_O_CREATE | FS_O_WRITE | FS_O_TRUNC);
    if (rc != 0) {
        return rc;
    }

    return fs_close(&file);
#else
    ARG_UNUSED(path);
    return -ENOSYS;
#endif
}

static int default_rename(void *user, const char *from, const char *to)
{
    ARG_UNUSED(user);

#ifdef CONFIG_FILE_SYSTEM
    return fs_rename(from, to);
#else
    ARG_UNUSED(from);
    ARG_UNUSED(to);
    return -ENOSYS;
#endif
}

static int default_unlink(void *user, const char *path)
{
    ARG_UNUSED(user);

#ifdef CONFIG_FILE_SYSTEM
    return fs_unlink(path);
#else
    ARG_UNUSED(path);
    return -ENOSYS;
#endif
}

static struct akira_cfdp_staging_backend backend = {
    .user = NULL,
    .mkdir = default_mkdir,
    .create = default_create,
    .rename = default_rename,
    .unlink = default_unlink,
};

static bool is_allowed_name_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
}

static int validate_logical_name(const char *dst_name, size_t *name_len)
{
    size_t len = 0u;
    char prev = '\0';

    if (dst_name == NULL || name_len == NULL) {
        return -EINVAL;
    }

    if (dst_name[0] == '\0') {
        return -EINVAL;
    }

    if (dst_name[0] == '/' || dst_name[0] == '\\') {
        return -EINVAL;
    }

    for (; dst_name[len] != '\0'; len++) {
        const char c = dst_name[len];

        if (len >= CCSDS_CFDP_MAX_FILENAME_LEN) {
            return -ENAMETOOLONG;
        }
        if (c == '/' || c == '\\') {
            return -EINVAL;
        }
        if (!is_allowed_name_char(c)) {
            return -EINVAL;
        }
        if (prev == '.' && c == '.') {
            return -EINVAL;
        }

        prev = c;
    }

    if (len == 1u && dst_name[0] == '.') {
        return -EINVAL;
    }

    *name_len = len;
    return 0;
}

static int make_path(const char *dir, const char *dst_name, char *out,
                     size_t out_len)
{
    const size_t dir_len = strlen(dir);
    size_t name_len = 0u;
    int rc;

    if (out == NULL || out_len == 0u) {
        return -EINVAL;
    }

    rc = validate_logical_name(dst_name, &name_len);
    if (rc != 0) {
        return rc;
    }

    if (dir_len + 1u + name_len + 1u > out_len) {
        return -ENAMETOOLONG;
    }

    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(&out[dir_len + 1u], dst_name, name_len);
    out[dir_len + 1u + name_len] = '\0';
    return 0;
}

static int mkdir_if_needed(const char *path)
{
    int rc;

    if (backend.mkdir == NULL) {
        return -ENOSYS;
    }

    rc = backend.mkdir(backend.user, path);
    if (rc == -EEXIST) {
        return 0;
    }

    return rc;
}

static int ensure_staging_dirs(void)
{
    int rc;

    rc = mkdir_if_needed(AKIRA_CFDP_STAGE_ROOT);
    if (rc != 0) {
        return rc;
    }

    rc = mkdir_if_needed(AKIRA_CFDP_STAGING_DIR);
    if (rc != 0) {
        return rc;
    }

    return mkdir_if_needed(AKIRA_CFDP_RECEIVED_DIR);
}

int akira_cfdp_receive_to_staging(const char *dst_name)
{
    char staged_path[AKIRA_CFDP_MAX_STAGED_PATH_LEN + 1u];
    int rc;

    rc = make_path(AKIRA_CFDP_STAGING_DIR, dst_name, staged_path,
                   sizeof(staged_path));
    if (rc != 0) {
        return rc;
    }
    if (backend.create == NULL) {
        return -ENOSYS;
    }

    rc = ensure_staging_dirs();
    if (rc != 0) {
        return rc;
    }

    return backend.create(backend.user, staged_path);
}

int akira_cfdp_commit_staged(const char *dst_name)
{
    char staged_path[AKIRA_CFDP_MAX_STAGED_PATH_LEN + 1u];
    char final_path[AKIRA_CFDP_MAX_STAGED_PATH_LEN + 1u];
    int rc;

    rc = make_path(AKIRA_CFDP_STAGING_DIR, dst_name, staged_path,
                   sizeof(staged_path));
    if (rc != 0) {
        return rc;
    }

    rc = make_path(AKIRA_CFDP_RECEIVED_DIR, dst_name, final_path,
                   sizeof(final_path));
    if (rc != 0) {
        return rc;
    }
    if (backend.rename == NULL) {
        return -ENOSYS;
    }

    rc = ensure_staging_dirs();
    if (rc != 0) {
        return rc;
    }

    return backend.rename(backend.user, staged_path, final_path);
}

int akira_cfdp_discard_staged(const char *dst_name)
{
    char staged_path[AKIRA_CFDP_MAX_STAGED_PATH_LEN + 1u];
    int rc;

    rc = make_path(AKIRA_CFDP_STAGING_DIR, dst_name, staged_path,
                   sizeof(staged_path));
    if (rc != 0) {
        return rc;
    }
    if (backend.unlink == NULL) {
        return -ENOSYS;
    }

    rc = ensure_staging_dirs();
    if (rc != 0) {
        return rc;
    }

    return backend.unlink(backend.user, staged_path);
}

#ifdef CONFIG_ZTEST
void akira_cfdp_staging_set_ops_for_test(
    const struct akira_cfdp_staging_ops *ops)
{
    if (ops == NULL) {
        backend.user = NULL;
        backend.mkdir = default_mkdir;
        backend.create = default_create;
        backend.rename = default_rename;
        backend.unlink = default_unlink;
        return;
    }

    backend.user = ops->user;
    backend.mkdir = ops->mkdir;
    backend.create = ops->create;
    backend.rename = ops->rename;
    backend.unlink = ops->unlink;
}
#endif
