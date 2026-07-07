#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/ztest.h>

#include "ccsds/akira_cfdp_staging.h"
#include "ccsds/ccsds_cfdp_config.h"

#define FAKE_MAX_FILES 4u
#define FAKE_MAX_PATH 96u

struct fake_file {
    bool exists;
    char path[FAKE_MAX_PATH];
};

struct fake_staging_fs {
    struct fake_file files[FAKE_MAX_FILES];
    uint32_t mkdir_count;
    uint32_t create_count;
    uint32_t rename_count;
    uint32_t unlink_count;
};

static struct fake_staging_fs fake_fs;

static struct fake_file *fake_find(const char *path)
{
    for (size_t i = 0u; i < FAKE_MAX_FILES; i++) {
        if (fake_fs.files[i].exists &&
            strcmp(fake_fs.files[i].path, path) == 0) {
            return &fake_fs.files[i];
        }
    }

    return NULL;
}

static struct fake_file *fake_alloc(void)
{
    for (size_t i = 0u; i < FAKE_MAX_FILES; i++) {
        if (!fake_fs.files[i].exists) {
            return &fake_fs.files[i];
        }
    }

    return NULL;
}

static int fake_set_path(struct fake_file *file, const char *path)
{
    const size_t len = strlen(path);

    if (len == 0u || len >= FAKE_MAX_PATH) {
        return -ENAMETOOLONG;
    }

    memcpy(file->path, path, len + 1u);
    return 0;
}

static int fake_mkdir(void *user, const char *path)
{
    ARG_UNUSED(user);

    zassert_not_null(path);
    fake_fs.mkdir_count++;
    return 0;
}

static int fake_create(void *user, const char *path)
{
    struct fake_file *file;

    ARG_UNUSED(user);
    zassert_not_null(path);

    file = fake_find(path);
    if (file == NULL) {
        file = fake_alloc();
    }
    if (file == NULL) {
        return -ENOSPC;
    }

    memset(file, 0, sizeof(*file));
    zassert_equal(fake_set_path(file, path), 0);
    file->exists = true;
    fake_fs.create_count++;
    return 0;
}

static int fake_rename(void *user, const char *from, const char *to)
{
    struct fake_file *src;
    struct fake_file *dst;

    ARG_UNUSED(user);
    zassert_not_null(from);
    zassert_not_null(to);

    src = fake_find(from);
    if (src == NULL) {
        return -ENOENT;
    }

    dst = fake_find(to);
    if (dst == NULL) {
        dst = fake_alloc();
    }
    if (dst == NULL) {
        return -ENOSPC;
    }

    memset(dst, 0, sizeof(*dst));
    zassert_equal(fake_set_path(dst, to), 0);
    dst->exists = true;
    memset(src, 0, sizeof(*src));
    fake_fs.rename_count++;
    return 0;
}

static int fake_unlink(void *user, const char *path)
{
    struct fake_file *file;

    ARG_UNUSED(user);
    zassert_not_null(path);

    file = fake_find(path);
    if (file == NULL) {
        return -ENOENT;
    }

    memset(file, 0, sizeof(*file));
    fake_fs.unlink_count++;
    return 0;
}

static void setup_fake_backend(void)
{
    const struct akira_cfdp_staging_ops ops = {
        .user = NULL,
        .mkdir = fake_mkdir,
        .create = fake_create,
        .rename = fake_rename,
        .unlink = fake_unlink,
    };

    memset(&fake_fs, 0, sizeof(fake_fs));
    akira_cfdp_staging_set_ops_for_test(&ops);
}

static void teardown_fake_backend(void)
{
    akira_cfdp_staging_set_ops_for_test(NULL);
}

static void assert_rejects_all_ops(const char *name, int expected)
{
    zassert_equal(akira_cfdp_receive_to_staging(name), expected);
    zassert_equal(akira_cfdp_commit_staged(name), expected);
    zassert_equal(akira_cfdp_discard_staged(name), expected);
    zassert_equal(fake_fs.create_count, 0u);
    zassert_equal(fake_fs.rename_count, 0u);
    zassert_equal(fake_fs.unlink_count, 0u);
}

ZTEST(ccsds_cfdp_staging, test_valid_receive_commit_discard_flow)
{
    setup_fake_backend();

    zassert_equal(akira_cfdp_receive_to_staging("app-1.wasm"), 0);
    zassert_not_null(fake_find("/lfs/cfdp/staging/app-1.wasm"));
    zassert_equal(fake_fs.mkdir_count, 3u);
    zassert_equal(fake_fs.create_count, 1u);

    zassert_equal(akira_cfdp_commit_staged("app-1.wasm"), 0);
    zassert_is_null(fake_find("/lfs/cfdp/staging/app-1.wasm"));
    zassert_not_null(fake_find("/lfs/cfdp/received/app-1.wasm"));
    zassert_equal(fake_fs.rename_count, 1u);

    zassert_equal(akira_cfdp_receive_to_staging("drop.bin"), 0);
    zassert_not_null(fake_find("/lfs/cfdp/staging/drop.bin"));
    zassert_equal(akira_cfdp_discard_staged("drop.bin"), 0);
    zassert_is_null(fake_find("/lfs/cfdp/staging/drop.bin"));
    zassert_equal(fake_fs.unlink_count, 1u);

    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_empty_name)
{
    setup_fake_backend();
    assert_rejects_all_ops("", -EINVAL);
    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_path_traversal)
{
    setup_fake_backend();
    assert_rejects_all_ops("firmware..bin", -EINVAL);
    assert_rejects_all_ops("..", -EINVAL);
    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_absolute_paths)
{
    setup_fake_backend();
    assert_rejects_all_ops("/app.wasm", -EINVAL);
    assert_rejects_all_ops("\\app.wasm", -EINVAL);
    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_path_separators)
{
    setup_fake_backend();
    assert_rejects_all_ops("dir/app.wasm", -EINVAL);
    assert_rejects_all_ops("dir\\app.wasm", -EINVAL);
    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_overlong_names)
{
    char name[CCSDS_CFDP_MAX_FILENAME_LEN + 2u];

    setup_fake_backend();
    memset(name, 'a', sizeof(name));
    name[sizeof(name) - 1u] = '\0';

    assert_rejects_all_ops(name, -ENAMETOOLONG);
    teardown_fake_backend();
}

ZTEST(ccsds_cfdp_staging, test_rejects_illegal_characters)
{
    setup_fake_backend();
    assert_rejects_all_ops("bad:name.wasm", -EINVAL);
    assert_rejects_all_ops("bad name.wasm", -EINVAL);
    assert_rejects_all_ops("bad+name.wasm", -EINVAL);
    teardown_fake_backend();
}

ZTEST_SUITE(ccsds_cfdp_staging, NULL, NULL, NULL, NULL, NULL);
