
/*
 * src/services/akira_runtime.c
 * Unified Akira Runtime - direct-to-WAMR implementation for Minimalist Arch.
 *
 * Implements: init, load, start, stop, capability guard, and native bridge.
 * Optimized for low SRAM usage with chunked WASM loading and PSRAM fallback.
 */

#include "akira_runtime.h"
#include "manifest_parser.h"
#include "runtime_cache.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdint.h>
#include <zephyr/fs/fs.h>
#include <lib/mem_helper.h>
#include "platform_hal.h"
#include "storage/fs_manager.h"
#include <lib/mem_helper.h>
#include <runtime/security.h>
#include <runtime/security/sandbox.h>
#include <runtime/security/app_signing.h>

#include "akira_api.h"

#ifdef CONFIG_AKIRA_WASM_IPC
#include <runtime/akira_ipc.h>
#endif

#ifdef CONFIG_AKIRA_WASM_RUNTIME
#include <wasm_export.h>
#endif

#include <lib/simple_json.h>
#include <stdio.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(akira_runtime, CONFIG_AKIRA_LOG_LEVEL);

#define FILE_DIR_MAX_LEN 128

/* Chunked loading configuration */
#define CHUNK_BUFFER_SIZE   (16 * 1024)  /* 16KB chunks for WASM loading */

akira_managed_app_t g_apps[AKIRA_MAX_WASM_INSTANCES];

/* Zephyr thread pool — one stack + control block per slot.
 * K_THREAD_STACK_ARRAY_DEFINE places stacks in .noinit so they only cost
 * RAM when an app is alive, not as zeroed BSS at boot. */
K_THREAD_STACK_ARRAY_DEFINE(g_app_stacks, AKIRA_MAX_WASM_INSTANCES,
                             CONFIG_AKIRA_WASM_APP_STACK_SIZE);
static struct k_thread g_app_threads[AKIRA_MAX_WASM_INSTANCES];

/* One mutex shared across all slots for cond_exit waits.
 * Per-slot mutexes add ~28B × MAX_CONTAINERS for zero benefit. */
static K_MUTEX_DEFINE(g_exit_mutex);

/* Optional exit callback registered by app_manager */
static akira_runtime_exit_cb_t g_exit_cb = NULL;

/* Use capability bits from security.h (AKIRA_CAP_*) */
static bool g_runtime_initialized = false;

#ifdef CONFIG_AKIRA_WASM_RUNTIME
/*
 * WAMR allocator trampolines — Alloc_With_Allocator mode.
 *
 * Each allocation carries a uint64_t header (8 bytes) storing the requested
 * size.  Using uint64_t (not size_t which is 4 bytes on 32-bit Xtensa) keeps
 * the returned user pointer 8-byte aligned when the underlying allocator
 * returns an 8-byte-aligned block.  WAMR asserts 8-byte alignment on the
 * instance heap buffer — misalignment produces the "heap init struct buf not
 * 8-byte aligned" error.
 */
static void *wamr_malloc(unsigned int size)
{
    uint64_t *hdr = akira_malloc_buffer(sizeof(uint64_t) + size);

    if (!hdr) {
        return NULL;
    }
    *hdr = (uint64_t)size;
    return hdr + 1;   /* +8 bytes — user ptr is 8-byte aligned */
}

static void wamr_free(void *ptr)
{
    if (!ptr) {
        return;
    }
    akira_free_buffer((uint64_t *)ptr - 1);
}

static void *wamr_realloc(void *ptr, unsigned int new_size)
{
    if (!ptr) {
        return wamr_malloc(new_size);
    }
    if (new_size == 0) {
        wamr_free(ptr);
        return NULL;
    }

    uint64_t old_size = *((uint64_t *)ptr - 1);
    void *new_ptr = wamr_malloc(new_size);

    if (!new_ptr) {
        return NULL;
    }

    size_t copy_size = (size_t)(old_size < new_size ? old_size : new_size);
    memcpy(new_ptr, ptr, copy_size);
    wamr_free(ptr);
    return new_ptr;
}
#endif /* CONFIG_AKIRA_WASM_RUNTIME */

/* Capability checking is implemented in src/runtime/security.c */

/* Helper: find free slot */
static int find_free_slot(void)
{
    for (int i = 0; i < AKIRA_MAX_WASM_INSTANCES; i++)
    {
        if (!g_apps[i].used)
            return i;
    }
    return -ENOMEM;
}

/* Helper: find by id validity */
static bool slot_valid(int id)
{
    return (id >= 0 && id < AKIRA_MAX_WASM_INSTANCES && g_apps[id].used);
}

/* O(1) cap lookup — slot pointer is stored as WAMR custom data at instantiation */
uint32_t akira_runtime_get_cap_mask_for_module_inst(wasm_module_inst_t inst)
{
    if (!inst) {
        return 0;
    }

    /* Fast path: custom data set in wasm_app_thread_fn */
    akira_managed_app_t *app =
        (akira_managed_app_t *)wasm_runtime_get_custom_data(inst);
    if (app) {
        return app->cap_mask;
    }

    /* Fallback: instance map (shouldn't normally reach here) */
    int slot = instance_map_get(inst);
    if (slot >= 0) {
        return g_apps[slot].cap_mask;
    }
    return 0;
}

int akira_runtime_get_name_for_module_inst(wasm_module_inst_t inst, char *buf, size_t buflen)
{
    if (!inst || !buf || buflen == 0) return -EINVAL;
    for (int i = 0; i < AKIRA_MAX_WASM_INSTANCES; i++) {
        if (g_apps[i].used && g_apps[i].instance == inst) {
            strncpy(buf, g_apps[i].name, buflen-1);
            buf[buflen-1] = '\0';
            return 0;
        }
    }
    return -ENOENT;
}

/* Get app slot from module instance - uses O(1) hash map with linear fallback */
int get_slot_for_module_inst(wasm_module_inst_t inst)
{
    if (!inst) return -1;

    /* Fast path: O(1) hash map lookup */
    int slot = instance_map_get(inst);
    if (slot >= 0) return slot;

    /* Slow path fallback: linear scan (shouldn't normally reach here) */
    for (int i = 0; i < AKIRA_MAX_WASM_INSTANCES; i++) {
        if (g_apps[i].used && g_apps[i].instance == inst) {
            /* Repair the map for next lookup */
            instance_map_put(inst, i);
            return i;
        }
    }
    return -1;
}

/*
 * WASM Memory Allocation with Per-App Quota Enforcement
 *
 * These functions provide quota-aware memory allocation for WASM apps.
 * They use PSRAM-preferred allocation and enforce per-app memory limits.
 *
 * Design:
 * - Atomic quota tracking for thread safety
 * - Graceful NULL return on quota violation (no crash)
 * - PSRAM-first allocation reduces SRAM pressure
 * - Header stores size for deallocation tracking
 */



/**
 * @brief Get current memory usage for an app
 *
 * @param instance_id  App instance ID
 * @return Current memory usage in bytes, or 0 if invalid
 */
uint32_t akira_runtime_get_memory_used(int instance_id)
{
    if (!slot_valid(instance_id)) {
        return 0;
    }
    return (uint32_t)atomic_get(&g_apps[instance_id].memory_used);
}

/**
 * @brief Get memory quota for an app
 *
 * @param instance_id  App instance ID
 * @return Memory quota in bytes (0 = unlimited), or 0 if invalid
 */
uint32_t akira_runtime_get_memory_quota(int instance_id)
{
    if (!slot_valid(instance_id)) {
        return 0;
    }
    return g_apps[instance_id].memory_quota;
}

/* Initialize WAMR runtime with PSRAM-backed heap when available */
int akira_runtime_init(void)
{
    if (g_runtime_initialized)
        return 0;

    LOG_INF("Initializing Akira unified runtime v2...");

    /* Initialize performance & security subsystems */
    module_cache_init();
    instance_map_init();
    sandbox_init();
    app_signing_init();

#ifdef CONFIG_AKIRA_WASM_RUNTIME
    /*
     * Initialize WAMR with Alloc_With_Allocator.
     *
     * This replaces the old static Alloc_With_Pool approach which reserved
     * CONFIG_WAMR_HEAP_SIZE (256 KB default) as BSS on RAM-only targets.
     * With Alloc_With_Allocator, WAMR calls our trampolines which route to
     * akira_malloc_buffer() — PSRAM-first on ESP32-S3, system heap on nRF54L15.
     * Memory is only consumed when WASM apps actually allocate it.
     */
    RuntimeInitArgs init_args = {0};
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func  = wamr_malloc;
    init_args.mem_alloc_option.allocator.free_func    = wamr_free;
    init_args.mem_alloc_option.allocator.realloc_func = wamr_realloc;

    if (!wasm_runtime_full_init(&init_args))
    {
        LOG_ERR("WAMR runtime initialization failed");
        return -ENODEV;
    }

    #ifdef CONFIG_AKIRA_WASM_API
    if(!akira_register_native_apis()){
        LOG_ERR("Failed to register native APIs");
        return -EIO;
    }
    #else
    LOG_WRN("Native API registration not included - no APIs enabled (CONFIG_AKIRA_WASM_API not set)");
    #endif /* CONFIG_AKIRA_WASM_API */

    /* Ensure WASM apps dir exists */
    if(fs_manager_exists("/lfs/apps") != 1){ // Not found
        fs_manager_mkdir("/lfs/apps");
    }

    g_runtime_initialized = true;
    LOG_INF("Akira runtime initialized (WAMR + native bridge)");
    return 0;
#else
    /* No runtime available when WAMR is disabled. To enable, set CONFIG_AKIRA_WASM_RUNTIME
     * and ensure WAMR integration is available for the target.
     */
    LOG_ERR("WASM support disabled - runtime not enabled (CONFIG_AKIRA_WASM_RUNTIME)");
    return -ENOTSUP;
#endif
}

/* Load WASM binary into runtime using chunked loading to reduce peak SRAM
 * The WASM binary is processed in 16KB chunks, with the chunk buffer
 * allocated from PSRAM when available to minimize SRAM pressure.
 */
int akira_runtime_load_wasm(const uint8_t *buffer, uint32_t size)
{
    if (!g_runtime_initialized)
    {
        LOG_ERR("Runtime not initialized");
        return -ENODEV;
    }

    if (!buffer || size < 8 || memcmp(buffer, "\0asm", 4) != 0)
    {
        LOG_ERR("Invalid WASM binary");
        return -EINVAL;
    }

    int slot = find_free_slot();
    if (slot < 0)
    {
        LOG_ERR("No free slots for WASM modules");
        return slot;
    }

#ifdef CONFIG_AKIRA_WASM_RUNTIME
    /* ===== Step 1: Integrity verification ===== */
    uint8_t binary_hash[32];
    int integrity_ret = app_verify_wasm_integrity(buffer, size, binary_hash);
    if (integrity_ret != 0) {
        LOG_ERR("WASM binary integrity check failed: %d", integrity_ret);
        sandbox_audit_log(AUDIT_EVENT_INTEGRITY_FAIL, "load", (uint32_t)size);
        return integrity_ret;
    }

    /* ===== Step 2: Parse manifest from WASM custom section ===== */
    int64_t load_start_ms = k_uptime_get();

    akira_manifest_t manifest;
    manifest_init_defaults(&manifest);
    int manifest_ret = manifest_parse_wasm_section(buffer, size, &manifest);
    if (manifest_ret == 0) {
        LOG_INF("Found embedded manifest: cap_mask=0x%08x, memory_quota=%u",
                manifest.cap_mask, manifest.memory_quota);
    }

    /* ===== Step 4: Load WASM module ===== */
    /* Allocate chunk buffer from PSRAM if available, else SRAM */
    mem_source_t chunk_src;
    uint8_t *chunk_buffer = akira_malloc_buffer_ex(CHUNK_BUFFER_SIZE, &chunk_src);
    if (!chunk_buffer)
    {
        LOG_ERR("Failed to allocate chunk buffer (%d bytes)", CHUNK_BUFFER_SIZE);
        return -ENOMEM;
    }
    LOG_INF("Chunk buffer allocated from %s (%d bytes)",
            chunk_src == MEM_SOURCE_PSRAM ? "PSRAM" : "SRAM", CHUNK_BUFFER_SIZE);

    /* Use WAMR's streaming loader for chunked loading when available,
     * otherwise fall back to direct load with our buffer management.
     *
     * For WAMR interpreter mode, wasm_runtime_load() requires the full
     * binary, but we use our PSRAM-backed buffer to stage it.
     */
    wasm_module_t module = NULL;
    char error_buf[128] = {0};

    /* For large WASM files, copy to PSRAM-backed buffer if the source
     * is in constrained memory. This trades one-time copy for reduced
     * peak SRAM during the WAMR load/parse phase.
     */
    const uint8_t *load_buffer = buffer;
    uint8_t *staged_buffer = NULL;

    if (size > CHUNK_BUFFER_SIZE && chunk_src == MEM_SOURCE_PSRAM)
    {
        /* Stage the entire WASM to PSRAM in chunks to reduce SRAM pressure */
        staged_buffer = akira_malloc_buffer(size);
        if (staged_buffer)
        {
            LOG_INF("Staging %u bytes WASM to external memory", size);

            /* Copy in chunks to avoid large stack/heap spikes */
            uint32_t offset = 0;
            while (offset < size)
            {
                uint32_t chunk_len = MIN(CHUNK_BUFFER_SIZE, size - offset);
                memcpy(chunk_buffer, buffer + offset, chunk_len);
                memcpy(staged_buffer + offset, chunk_buffer, chunk_len);
                offset += chunk_len;
            }
            load_buffer = staged_buffer;
            LOG_INF("WASM staged to PSRAM successfully");
        }
        else
        {
            LOG_WRN("Could not stage to PSRAM, loading from original buffer");
        }
    }

    /* WAMR stores raw pointers into the binary buffer for export/import name
     * strings when called via wasm_runtime_load() (which sets
     * wasm_binary_freeable=false, causing reuse_const_strings=true in
     * load_from_sections). The buffer MUST outlive wasm_runtime_unload() or
     * every wasm_runtime_lookup_function() call will hit dangling pointers.
     *
     * Strategy: reuse staged_buffer if we already copied to PSRAM; otherwise
     * allocate a fresh owned copy. The caller's `buffer` is never borrowed. */
    uint8_t *owned_binary;
    if (staged_buffer) {
        /* Transfer ownership — staged_buffer is already a full PSRAM copy */
        owned_binary = staged_buffer;
        staged_buffer = NULL;
    } else {
        /* load_buffer == buffer (caller-owned) — make our own durable copy */
        owned_binary = akira_malloc_buffer(size);
        if (!owned_binary) {
            akira_free_buffer(chunk_buffer);
            LOG_ERR("OOM: cannot allocate owned WASM binary copy (%u bytes)", size);
            return -ENOMEM;
        }
        memcpy(owned_binary, buffer, size);
    }

    /* Load the WASM module — owned_binary is intentionally NOT freed here */
    module = wasm_runtime_load(owned_binary, size, error_buf, sizeof(error_buf));

    /* chunk_buffer is a scratch pad only — always free after load */
    akira_free_buffer(chunk_buffer);
    chunk_buffer = NULL;

    if (!module)
    {
        akira_free_buffer(owned_binary);
        LOG_ERR("wasm_runtime_load failed: %s", error_buf);
        return -EIO;
    }

    /* Measure load time for profiling */
    uint32_t load_time_ms = (uint32_t)(k_uptime_get() - load_start_ms);

    g_apps[slot].used = true;
    g_apps[slot].module = module;
    g_apps[slot].wasm_binary = owned_binary;  /* kept alive until wasm_runtime_unload */
    g_apps[slot].status = AKIRA_APP_STATUS_CREATED;
    g_apps[slot].exit_code = 0;
    g_apps[slot].tid = NULL;
    g_apps[slot].cap_mask = manifest.valid ? manifest.cap_mask : 0;
    g_apps[slot].memory_quota = manifest.valid ? manifest.memory_quota : 0;
    atomic_set(&g_apps[slot].memory_used, 0);
    memcpy(g_apps[slot].binary_hash, binary_hash, 32);
    g_apps[slot].hash_valid = true;
    g_apps[slot].trust_level = TRUST_LEVEL_USER;
    snprintf(g_apps[slot].name, sizeof(g_apps[slot].name),
             manifest.valid && manifest.name[0] ? manifest.name : "app%d", slot);

    /* Initialize sandbox and performance tracking */
    sandbox_ctx_init(&g_apps[slot].sandbox, TRUST_LEVEL_USER, g_apps[slot].cap_mask);
    memset(&g_apps[slot].perf, 0, sizeof(runtime_perf_stats_t));

    /* Store in module cache for future reuse */
    module_cache_store(binary_hash, module, size, load_time_ms);

    sandbox_audit_log(AUDIT_EVENT_APP_LOADED, g_apps[slot].name, (uint32_t)size);
    LOG_INF("WASM module loaded into slot %d (cap=0x%08x, quota=%u, load=%ums)",
            slot, g_apps[slot].cap_mask, g_apps[slot].memory_quota, load_time_ms);
    return slot;
#else
    (void)buffer; (void)size;
    return -ENOTSUP; /* WASM runtime not available */
#endif
}

/* ===== Thread-per-app implementation ===== */

#ifdef CONFIG_AKIRA_WASM_RUNTIME

/**
 * @brief WASM app thread entry point.
 *
 * Each WASM app runs in its own Zephyr thread so the shell and other
 * subsystems remain responsive while apps execute.  Flow:
 *   1. Instantiate WAMR module inside the thread (safe for all WAMR modes)
 *   2. Post sem_start so akira_runtime_start() can return to the caller
 *   3. Run wasm_runtime_call_wasm() — blocks until app exits or is terminated
 *   4. Clean up WAMR resources and signal cond_exit for any waiters
 */
static void wasm_app_thread_fn(void *p1, void *p2, void *p3)
{
    int slot = (int)(intptr_t)p1;
    (void)p2; (void)p3;

    akira_managed_app_t *app = &g_apps[slot];

#ifndef CONFIG_WAMR_INSTANCE_HEAP
#define CONFIG_WAMR_INSTANCE_HEAP 65536
#endif
#ifndef CONFIG_WAMR_STACK_SIZE
#define CONFIG_WAMR_STACK_SIZE 8192
#endif

    /* ------ Step 1: Instantiate ------ */
    char error_buf[128] = {0};
    wasm_module_inst_t inst = wasm_runtime_instantiate(
        app->module,
        CONFIG_WAMR_INSTANCE_HEAP,
        CONFIG_WAMR_STACK_SIZE,
        error_buf, sizeof(error_buf));

    if (!inst) {
        LOG_ERR("wasm_runtime_instantiate failed (slot %d): %s", slot, error_buf);
        app->exit_code = -EIO;
        app->status = AKIRA_APP_STATUS_ERROR;
        k_sem_give(&app->sem_start);
        goto thread_exit;
    }

    /* Use the singleton exec_env — one exec_env per instance is the correct
     * WAMR embedded model. Creating a second exec_env with
     * wasm_runtime_create_exec_env() corrupts the instance's export_functions
     * table in Alloc_With_Allocator mode, causing wasm_runtime_lookup_function
     * to return NULL even when exports are present in the binary. */
    wasm_exec_env_t exec_env = wasm_runtime_get_exec_env_singleton(inst);
    if (!exec_env) {
        LOG_ERR("Failed to get singleton exec_env for slot %d", slot);
        wasm_runtime_deinstantiate(inst);
        app->exit_code = -ENOMEM;
        app->status = AKIRA_APP_STATUS_ERROR;
        k_sem_give(&app->sem_start);
        goto thread_exit;
    }

    app->instance = inst;
    app->exec_env = exec_env;  /* singleton — do NOT call destroy_exec_env on it */

    /* O(1) cap lookup: store slot pointer as WAMR custom data */
    wasm_runtime_set_custom_data(inst, &g_apps[slot]);
    instance_map_put(inst, slot);

    wasm_runtime_clear_exception(inst);

    /* ------ Step 2: Signal caller that instance is ready ------ */
    k_sem_give(&app->sem_start);

    /* ------ Step 3: Execute ------ */
    sandbox_exec_begin(&app->sandbox);
    perf_exec_begin(&app->perf);

    /* wasm_application_execute_main() is WAMR's built-in entry-point runner.
     * It tries "main", "__main_argc_argv", "_main", and "_start" (WASI) in
     * order, handles argc/argv marshalling, and uses the singleton exec_env
     * so all native API calls receive the right context for cap checking. */
    int exit_code = 0;
    bool ran = wasm_application_execute_main(inst, 0, NULL);
    if (!ran) {
        const char *ex = wasm_runtime_get_exception(inst);
        if (ex && strstr(ex, "lookup the entry point")) {
            /* No main/start entry point — treat as a reactive (library) module */
            LOG_INF("Slot %d: no entry point — reactive module", slot);
            wasm_runtime_clear_exception(inst);
        } else if (ex) {
            LOG_ERR("WASM exception (slot %d): %s", slot, ex);
            app->perf.trap_count++;
            exit_code = -EIO;
        }
    }

    perf_exec_end(&app->perf);

    /* ------ Step 4: Clean up WAMR resources ------ */
    /* Singleton exec_env is automatically freed by wasm_runtime_deinstantiate;
     * never call wasm_runtime_destroy_exec_env on it. */
    app->exec_env = NULL;

    instance_map_remove(inst);
    wasm_runtime_deinstantiate(inst);
    app->instance = NULL;

    atomic_set(&app->memory_used, 0);
    sandbox_exec_end(&app->sandbox);
    sandbox_audit_log(AUDIT_EVENT_APP_STOPPED, app->name, (uint32_t)slot);

    /* Release IPC subscriptions before the name slot is cleared */
#ifdef CONFIG_AKIRA_WASM_IPC
    akira_ipc_cleanup_app(app->name);
#endif

    app->exit_code = (int8_t)exit_code;
    LOG_INF("App thread done (slot %d, exit=%d, calls=%u, traps=%u)",
            slot, exit_code, app->perf.call_count, app->perf.trap_count);

thread_exit:
    /* Notify any waiters (akira_runtime_stop or status query) */
    k_mutex_lock(&g_exit_mutex, K_FOREVER);
    if (app->status != AKIRA_APP_STATUS_ERROR) {
        app->status = AKIRA_APP_STATUS_EXITED;
    }
    k_condvar_broadcast(&app->cond_exit);
    k_mutex_unlock(&g_exit_mutex);

    /* -------- Release the WASM module and slot --------
     * Done here (in the app thread itself) to avoid a k_thread_join deadlock
     * if the exit callback tried to call akira_runtime_destroy().
     * exec_env and instance are already NULL (cleaned up in Step 4 above).
     * Only the module handle and the slot itself remain to free. */
    if (app->module) {
        wasm_runtime_unload(app->module);
        app->module = NULL;
    }
    /* Free the owned binary copy now that the module is unloaded.
     * Must come AFTER wasm_runtime_unload() — WAMR holds raw pointers
     * into this buffer for export/import name strings. */
    if (app->wasm_binary) {
        akira_free_buffer(app->wasm_binary);
        app->wasm_binary = NULL;
    }
    if (app->hash_valid) {
        module_cache_release(app->binary_hash);
        app->hash_valid = false;
    }
    app->name[0] = '\0';
    app->tid    = NULL;
    app->used   = false;   /* Slot now free — must be LAST write */

    /* Invoke registered exit callback (e.g. app_manager state transition).
     * Called AFTER slot is freed so a restart triggered by the callback
     * immediately gets a clean slot. */
    if (g_exit_cb) {
        g_exit_cb(slot, app->exit_code);
    }
}

#endif /* CONFIG_AKIRA_WASM_RUNTIME */

/* Instantiate and run a loaded WASM module in a dedicated thread */
int akira_runtime_start(int instance_id)
{
    if (!slot_valid(instance_id)) {
        return -EINVAL;
    }

    akira_managed_app_t *app = &g_apps[instance_id];

    if (app->status == AKIRA_APP_STATUS_RUNNING) {
        return 0; /* already running */
    }

#ifdef CONFIG_AKIRA_WASM_RUNTIME
    /* Initialize per-slot sync primitives for this run */
    k_sem_init(&app->sem_start, 0, 1);
    k_condvar_init(&app->cond_exit);
    app->exit_code = 0;

    app->tid = k_thread_create(
        &g_app_threads[instance_id],
        g_app_stacks[instance_id],
        K_THREAD_STACK_SIZEOF(g_app_stacks[instance_id]),
        wasm_app_thread_fn,
        (void *)(intptr_t)instance_id, NULL, NULL,
        CONFIG_AKIRA_WASM_APP_PRIORITY,
        0, K_NO_WAIT);

    if (!app->tid) {
        LOG_ERR("Failed to create app thread for slot %d", instance_id);
        return -ENOMEM;
    }

    /* Block until WAMR instantiation succeeds or fails */
    k_sem_take(&app->sem_start, K_FOREVER);

    if (app->status == AKIRA_APP_STATUS_ERROR) {
        LOG_ERR("App failed to start (slot %d)", instance_id);
        k_thread_join(&g_app_threads[instance_id], K_MSEC(500));
        app->tid = NULL;
        return -EIO;
    }

    app->status = AKIRA_APP_STATUS_RUNNING;
    sandbox_audit_log(AUDIT_EVENT_APP_STARTED, app->name, (uint32_t)instance_id);
    LOG_INF("WASM module loaded into slot %d (cap=0x%08x, quota=%u)",
            instance_id, app->cap_mask, app->memory_quota);
    return 0;
#else
    (void)instance_id;
    return -ENOTSUP;
#endif
}

/* Stop a running WASM app: terminate its thread and wait for cleanup */
int akira_runtime_stop(int instance_id)
{
    if (!slot_valid(instance_id)) {
        return -EINVAL;
    }

    akira_managed_app_t *app = &g_apps[instance_id];

    if (app->status == AKIRA_APP_STATUS_STOPPED ||
        app->status == AKIRA_APP_STATUS_CREATED) {
        return 0; /* nothing to stop */
    }

#ifdef CONFIG_AKIRA_WASM_RUNTIME
    /* Ask WAMR to interrupt the running WASM execution.
     * This causes wasm_runtime_call_wasm() to return with an exception. */
    if (app->instance) {
        wasm_runtime_terminate(app->instance);
    }

    /* Wait for the app thread to finish cleanup and signal cond_exit */
    k_mutex_lock(&g_exit_mutex, K_FOREVER);
    if (app->status == AKIRA_APP_STATUS_RUNNING) {
        /* 5-second timeout; abort thread if it doesn't respond */
        int wait_ret = k_condvar_wait(&app->cond_exit, &g_exit_mutex,
                                      K_SECONDS(5));
        if (wait_ret != 0) {
            LOG_ERR("App thread (slot %d) timed out — aborting", instance_id);
            k_thread_abort(app->tid);
            app->tid = NULL;
            /* Manual cleanup since thread was aborted before our cleanup code ran.
             * exec_env is the singleton — do NOT call wasm_runtime_destroy_exec_env;
             * it will be freed by wasm_runtime_deinstantiate below. */
            app->exec_env = NULL;
            if (app->instance) {
                instance_map_remove(app->instance);
                wasm_runtime_deinstantiate(app->instance);
                app->instance = NULL;
            }
            if (app->module) {
                wasm_runtime_unload(app->module);
                app->module = NULL;
            }
            if (app->hash_valid) {
                module_cache_release(app->binary_hash);
                app->hash_valid = false;
            }
            atomic_set(&app->memory_used, 0);
        }
    }
    app->status = AKIRA_APP_STATUS_STOPPED;
    k_mutex_unlock(&g_exit_mutex);

    /* Join to release Zephyr thread resources */
    if (app->tid) {
        k_thread_join(&g_app_threads[instance_id], K_MSEC(200));
        app->tid = NULL;
    }

    sandbox_audit_log(AUDIT_EVENT_APP_STOPPED, app->name, (uint32_t)instance_id);
    LOG_INF("Stopped slot %d (calls=%u, traps=%u)",
            instance_id, app->perf.call_count, app->perf.trap_count);
    return 0;
#else
    (void)instance_id;
    return -ENOTSUP;
#endif
}

int akira_runtime_install_with_manifest(const char *name, const void *binary, size_t size, const char *manifest_json, size_t manifest_size)
{
    if (!name || !binary || size == 0) return -EINVAL;

    /* Parse manifest with fallback: WASM section first, then JSON */
    akira_manifest_t manifest;
    manifest_parse_with_fallback((const uint8_t *)binary, size,
                                 manifest_json, manifest_size, &manifest);

    /* Save manifest if provided (for external tools/debugging) */
    if (manifest_json && manifest_size > 0) {
        char mpath[FILE_DIR_MAX_LEN];
        snprintf(mpath, sizeof(mpath), "/lfs/apps/%s.manifest.json", name);
        if (fs_manager_exists(mpath) ) {
            ssize_t mv = fs_manager_write_file(mpath, manifest_json, manifest_size);
            if (mv != (ssize_t)manifest_size) {
                LOG_WRN("Failed to write manifest fully for %s", name);
            } else {
                LOG_INF("Saved manifest to %s", mpath);
            }
        } else {
            LOG_WRN("Filesystem not available for manifest save");
        }
    }

    /* Load into runtime memory - this will also parse embedded manifest */
    int id = akira_runtime_load_wasm((const uint8_t *)binary, (uint32_t)size);
    if (id < 0) return id;

    /* Override with external manifest if it has more capabilities */
    if (manifest.valid && manifest.cap_mask != 0) {
        g_apps[id].cap_mask |= manifest.cap_mask;
        if (manifest.memory_quota > 0) {
            g_apps[id].memory_quota = manifest.memory_quota;
        }
        LOG_INF("App %s: merged manifest cap_mask=0x%08x, memory_quota=%u",
                name, g_apps[id].cap_mask, g_apps[id].memory_quota);
    }

    /* Store friendly name */
    strncpy(g_apps[id].name, name, sizeof(g_apps[id].name)-1);
    g_apps[id].name[sizeof(g_apps[id].name)-1] = '\0';

    return id;
}

int akira_runtime_install(const char *name, const void *binary, size_t size)
{
    return akira_runtime_install_with_manifest(name, binary, size, NULL, 0);
}

/* Destroy: stop if running, then fully unload the module and free the slot */
int akira_runtime_destroy(int instance_id)
{
    if (!slot_valid(instance_id)) {
        return -EINVAL;
    }

    akira_managed_app_t *app = &g_apps[instance_id];

    /* Stop the thread first if it's still running */
    if (app->status == AKIRA_APP_STATUS_RUNNING ||
        app->status == AKIRA_APP_STATUS_EXITED) {
        akira_runtime_stop(instance_id);
    }

#ifdef CONFIG_AKIRA_WASM_RUNTIME
    /* Safety: instance should already be NULL after stop,
     * but guard against the k_thread_abort path in stop().
     * exec_env is the singleton — freed automatically by deinstantiate, never
     * call wasm_runtime_destroy_exec_env on it. */
    app->exec_env = NULL;
    if (app->instance) {
        instance_map_remove(app->instance);
        wasm_runtime_deinstantiate(app->instance);
        app->instance = NULL;
    }

    /* Release module cache reference */
    if (app->hash_valid) {
        module_cache_release(app->binary_hash);
    }

    if (app->module) {
        wasm_runtime_unload(app->module);
        app->module = NULL;
    }
    /* Free owned binary AFTER unload — WAMR keeps raw string pointers into it */
    if (app->wasm_binary) {
        akira_free_buffer(app->wasm_binary);
        app->wasm_binary = NULL;
    }
#endif

    app->used = false;
    app->status = AKIRA_APP_STATUS_STOPPED;
    app->exit_code = 0;
    app->tid = NULL;
    app->cap_mask = 0;
    app->hash_valid = false;
    app->name[0] = '\0';
    memset(&app->sandbox, 0, sizeof(sandbox_ctx_t));
    memset(&app->perf, 0, sizeof(runtime_perf_stats_t));
    return 0;
}

/* Uninstall: remove persistent files and destroy runtime slot */
int akira_runtime_uninstall(const char *name, int instance_id)
{
    if (!name) return -EINVAL;

    /* Stop/destroy if running */
    if (instance_id >= 0)
    {
        akira_runtime_stop(instance_id);
        akira_runtime_destroy(instance_id);
    }

    return 0;
}

/* End of file */

/* ===== New API Functions (v2) ===== */

sandbox_ctx_t *akira_runtime_get_sandbox(int instance_id)
{
    if (!slot_valid(instance_id)) return NULL;
    return &g_apps[instance_id].sandbox;
}

runtime_perf_stats_t *akira_runtime_get_perf_stats(int instance_id)
{
    if (!slot_valid(instance_id)) return NULL;
    return &g_apps[instance_id].perf;
}

int akira_runtime_verify_binary(const uint8_t *binary, uint32_t size,
                                uint8_t *hash_out)
{
    return app_verify_wasm_integrity(binary, size, hash_out);
}

void akira_runtime_set_exit_callback(akira_runtime_exit_cb_t cb)
{
    g_exit_cb = cb;
}

void akira_runtime_get_cache_stats(module_cache_stats_t *stats)
{
    module_cache_get_stats(stats);
}
