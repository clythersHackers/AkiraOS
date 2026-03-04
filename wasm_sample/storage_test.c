/*
 * Copyright (c) 2026 AkiraOS Contributors
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * storage_test.c — minimal WASM app exercising the sandboxed storage API.
 * Write a line to "test.txt", read it back, then list the sandbox root.
 *
 * Manifest (storage_test.json):
 *   "capabilities": ["storage.read", "storage.write"]
 */

#include "include/akira_api.h"
#include <string.h>

#define MSG "Hello from WASM storage!\n"

int main(void)
{
    /* ── Write ─────────────────────────────────────────────────────────── */
    int fd = storage_open("test.txt", STORAGE_O_WRITE);
    if (fd < 0) {
        printf("storage_open(write) failed: %d\n", fd);
        return 1;
    }

    int n = storage_write(fd, MSG, strlen(MSG));
    storage_close(fd);

    if (n < 0) {
        printf("storage_write failed: %d\n", n);
        return 1;
    }
    printf("Written %d bytes to test.txt\n", n);

    /* ── Read back ─────────────────────────────────────────────────────── */
    fd = storage_open("test.txt", STORAGE_O_READ);
    if (fd < 0) {
        printf("storage_open(read) failed: %d\n", fd);
        return 1;
    }

    char buf[128] = {0};
    n = storage_read(fd, buf, sizeof(buf) - 1);
    storage_close(fd);

    if (n < 0) {
        printf("storage_read failed: %d\n", n);
        return 1;
    }
    buf[n] = '\0';
    printf("Read back: %s", buf);

    /* ── List sandbox root ─────────────────────────────────────────────── */
    char list[512] = {0};
    int total = storage_list("", list, sizeof(list));
    if (total > 0) {
        printf("Sandbox contents:\n%s", list);
    } else {
        printf("storage_list failed: %d\n", total);
    }

    /* ── Clean up ──────────────────────────────────────────────────────── */
    storage_delete("test.txt");
    printf("storage_test done.\n");
    return 0;
}
