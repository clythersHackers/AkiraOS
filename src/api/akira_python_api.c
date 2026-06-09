#include "akira_python_api.h"

#include <stdint.h>
#include <string.h>
#include <zephyr/logging/log.h>
#include <wasm_export.h>

LOG_MODULE_REGISTER(akira_python, CONFIG_AKIRA_LOG_LEVEL);

/* ── setjmp / longjmp ────────────────────────────────────────────────────────
 * MicroPython is compiled with Emscripten SUPPORT_LONGJMP=none, which leaves
 * setjmp/longjmp as env-module imports.  setjmp always returns 0 (first-call
 * semantics); longjmp is a no-op.  Python exceptions that unwind through C
 * frames will trap rather than recover, but exception-free scripts run fine.
 */
static int py_setjmp(wasm_exec_env_t exec_env, void *jmp_buf)
{
    (void)exec_env;
    (void)jmp_buf;
    return 0;
}

static void py_longjmp(wasm_exec_env_t exec_env, void *jmp_buf, int val)
{
    (void)exec_env;
    (void)jmp_buf;
    (void)val;
}

/* ── WASI fd_* ───────────────────────────────────────────────────────────────
 * Emscripten's libc routes Python print() through fd_write on fd 1/2.
 * We forward those writes to the Zephyr logger; all other fds are discarded.
 * ciovec layout: [buf_app_offset: u32][buf_len: u32] × iovs_len
 */
typedef struct { uint32_t buf; uint32_t buf_len; } wasi_ciovec_t;

static int py_fd_write(wasm_exec_env_t exec_env, int fd,
                       const wasi_ciovec_t *iovs, int iovs_len,
                       int *nwritten)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    int total = 0;
    char tmp[256];

    for (int i = 0; i < iovs_len; i++) {
        uint32_t buf_len = iovs[i].buf_len;
        if (buf_len == 0) {
            continue;
        }
        char *buf = wasm_runtime_addr_app_to_native(inst, iovs[i].buf);
        if (!buf) {
            continue;
        }
        if (fd == 1 || fd == 2) {
            uint32_t pos = 0;
            while (pos < buf_len) {
                uint32_t chunk = buf_len - pos;
                if (chunk >= sizeof(tmp)) {
                    chunk = sizeof(tmp) - 1;
                }
                memcpy(tmp, buf + pos, chunk);
                tmp[chunk] = '\0';
                LOG_INF("%s", tmp);
                pos += chunk;
            }
        }
        total += (int)buf_len;
    }
    if (nwritten) {
        *nwritten = total;
    }
    return 0;
}

static int py_fd_close(wasm_exec_env_t exec_env, int fd)
{
    (void)exec_env; (void)fd;
    return 0;
}

static int py_fd_sync(wasm_exec_env_t exec_env, int fd)
{
    (void)exec_env; (void)fd;
    return 0;
}

static int py_fd_seek(wasm_exec_env_t exec_env, int fd,
                      int64_t offset, int whence, int64_t *newoffset)
{
    (void)exec_env; (void)fd; (void)offset; (void)whence;
    if (newoffset) {
        *newoffset = 0;
    }
    return 0;
}

static int py_fd_read(wasm_exec_env_t exec_env, int fd,
                      const wasi_ciovec_t *iovs, int iovs_len, int *nread)
{
    (void)exec_env; (void)fd; (void)iovs; (void)iovs_len;
    if (nread) {
        *nread = 0;
    }
    return 0;
}

/* ── Registration ────────────────────────────────────────────────────────── */

bool akira_python_register_natives(void)
{
    static NativeSymbol env_syms[] = {
        {"setjmp",  (void *)py_setjmp,  "(*)i", NULL},
        {"longjmp", (void *)py_longjmp, "(*i)",  NULL},
    };

    static NativeSymbol wasi_syms[] = {
        {"fd_write", (void *)py_fd_write, "(i*i*)i", NULL},
        {"fd_close", (void *)py_fd_close, "(i)i",    NULL},
        {"fd_sync",  (void *)py_fd_sync,  "(i)i",    NULL},
        {"fd_seek",  (void *)py_fd_seek,  "(iIi*)i", NULL},
        {"fd_read",  (void *)py_fd_read,  "(i*i*)i", NULL},
    };

    if (!wasm_runtime_register_natives("env", env_syms,
                                       sizeof(env_syms) / sizeof(NativeSymbol))) {
        LOG_ERR("Failed to register Python env natives");
        return false;
    }

    if (!wasm_runtime_register_natives("wasi_snapshot_preview1", wasi_syms,
                                       sizeof(wasi_syms) / sizeof(NativeSymbol))) {
        LOG_ERR("Failed to register Python WASI natives");
        return false;
    }

    return true;
}
