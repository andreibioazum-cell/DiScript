/* Headless LÖVE2D lifecycle test: runs a .ds game through load/update/draw
 * for N frames with synthetic touch events and a bounded GC budget. */
#include "discript/discript.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "tools/host_test/cases/love_api.ds";
    int frames = (argc > 2) ? atoi(argv[2]) : 180;
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 2;
    }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *src = (char *)malloc((size_t)n + 1);
    size_t rd = fread(src, 1, (size_t)n, f);
    src[rd] = '\0';
    fclose(f);

    DSVM *vm = ds_vm_create();
    ds_vm_set_project_dir(vm, ".");
    ds_vm_love_set_viewport(vm, 0, 0, 480, 320);
    if (!ds_vm_run_source(vm, path, src, rd)) {
        printf("RUN FAIL: %s\n", ds_vm_last_error(vm));
        return 1;
    }
    printf("has_love_loop=%d\n", ds_vm_has_love_loop(vm));
    if (!ds_vm_love_load(vm)) {
        printf("LOAD FAIL: %s\n", ds_vm_last_error(vm));
        return 1;
    }
    for (int i = 0; i < frames; i++) {
        if (!ds_vm_love_update(vm, 1.0 / 60.0)) {
            printf("UPDATE FAIL frame %d: %s\n", i, ds_vm_last_error(vm));
            return 1;
        }
        if (!ds_vm_love_draw(vm)) {
            printf("DRAW FAIL frame %d: %s\n", i, ds_vm_last_error(vm));
            return 1;
        }
        ds_vm_love_touch(vm, i % 3, 0, 100.0f + (float)(i % 50), 50.0f + (float)(i % 30));
        ds_vm_gc_frame_tick(vm, 200.0);
    }
    DSVMStats st;
    ds_vm_get_stats(vm, &st);
    printf("frames=%d bg=%08x gc=%zuKB peak=%zuKB minor=%llu major=%llu max_pause=%.1fus jit=%d\n", frames,
           ds_vm_love_bg_color(vm), st.gc_bytes / 1024, st.gc_peak_bytes / 1024,
           (unsigned long long)st.gc_young_runs, (unsigned long long)st.gc_major_runs, st.gc_max_pause_us,
           st.jit_functions);
    ds_vm_destroy(vm);
    free(src);
    return 0;
}
