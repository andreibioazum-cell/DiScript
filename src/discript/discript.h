#ifndef DISCRIPT_PUBLIC_H
#define DISCRIPT_PUBLIC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct DSVM DSVM;

typedef void (*DSPrintCallback)(const char *line, int is_error, void *userdata);

typedef struct {
    size_t gc_bytes;
    size_t gc_peak_bytes;
    size_t gc_objects;
    uint64_t gc_young_runs;
    uint64_t gc_major_runs;
    double gc_last_pause_us;
    double gc_max_pause_us;
    double compile_ms;
    int jit_functions;
    int jit_available;
} DSVMStats;

DSVM *ds_vm_create(void);
void ds_vm_destroy(DSVM *vm);
void ds_vm_reset(DSVM *vm);

void ds_vm_set_print_callback(DSVM *vm, DSPrintCallback cb, void *userdata);
void ds_vm_set_project_dir(DSVM *vm, const char *dir);
void ds_vm_request_stop(DSVM *vm);

/* Compiles source directly to native Direct-Threaded closures (+ ARM64/x86_64 JIT)
 * with ZERO bytecode, and runs top-level module code. Returns 1 on success, 0 on error. */
int ds_vm_run_source(DSVM *vm, const char *filename, const char *source, size_t len);

/* Evaluates a single line/snippet in REPL mode and prints non-None results. */
int ds_vm_eval_repl(DSVM *vm, const char *line);

/* LÖVE2D Lifecycle & Event Hooks */
int ds_vm_has_love_loop(const DSVM *vm);
void ds_vm_love_refresh(DSVM *vm);
int ds_vm_love_load(DSVM *vm);
int ds_vm_love_update(DSVM *vm, double dt_sec);
int ds_vm_love_draw(DSVM *vm);
int ds_vm_love_touch(DSVM *vm, int action, int pointer_id, float x, float y);
int ds_vm_love_key(DSVM *vm, const char *key_name, int pressed);
void ds_vm_love_set_viewport(DSVM *vm, float x, float y, float w, float h);
uint32_t ds_vm_love_bg_color(const DSVM *vm);

/* Bounded Lua-style generational/incremental GC frame tick */
void ds_vm_gc_frame_tick(DSVM *vm, double max_budget_us);

/* Diagnostics & Telemetry */
void ds_vm_get_stats(const DSVM *vm, DSVMStats *out);
const char *ds_vm_last_error(const DSVM *vm);
int ds_vm_last_error_line(const DSVM *vm);
const char *ds_vm_last_error_file(const DSVM *vm);

#ifdef __cplusplus
}
#endif

#endif
