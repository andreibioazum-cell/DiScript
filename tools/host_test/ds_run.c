/* Host test harness for the DiScript VM: compiles and runs .ds source files on
 * Linux with the Android stubs, so the language, GC, JIT and stdlib can be
 * exercised without a device. */

#include "discript/discript.h"
#include "discript/ds_ast.h"
#include "engine/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_cb(const char *line, int is_error, void *ud) {
    (void)ud;
    fprintf(is_error ? stderr : stdout, "%s\n", line);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.ds> [--gc-stress]\n", argv[0]);
        return 2;
    }
    int gc_stress = 0;
    for (int i = 2; i < argc; i++)
        if (strcmp(argv[i], "--gc-stress") == 0)
            gc_stress = 1;

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = (char *)malloc((size_t)sz + 1);
    size_t rd = fread(src, 1, (size_t)sz, f);
    src[rd] = '\0';
    fclose(f);

    uint64_t tc0 = ds_monotonic_ns();
    DSVM *vm = ds_vm_create();
    uint64_t tc1 = ds_monotonic_ns();
    ds_vm_set_print_callback(vm, print_cb, NULL);
    ds_vm_set_project_dir(vm, ".");

    uint64_t t0 = ds_monotonic_ns();
    int ok = ds_vm_run_source(vm, argv[1], src, rd);
    uint64_t t1 = ds_monotonic_ns();

    if (gc_stress) {
        for (int i = 0; i < 8; i++)
            ds_gc_collect_full(vm);
    }

    DSVMStats st;
    ds_vm_get_stats(vm, &st);
    printf("--- create=%.2fms ok=%d compile=%.2fms run=%.2fms gc=%zuKB peak=%zuKB objs=%zu minor=%llu major=%llu "
           "pause=%.1fus max=%.1fus jit=%d/%d\n",
           (double)(tc1 - tc0) * 1e-6, ok, st.compile_ms, (double)(t1 - t0) * 1e-6, st.gc_bytes / 1024, st.gc_peak_bytes / 1024, st.gc_objects,
           (unsigned long long)st.gc_young_runs, (unsigned long long)st.gc_major_runs, st.gc_last_pause_us,
           st.gc_max_pause_us, st.jit_functions, st.jit_available);

    ds_vm_destroy(vm);
    free(src);
    return ok ? 0 : 1;
}
