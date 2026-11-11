/* Desktop and headless entry point for the DiScript code editor. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "engine/runtime.h"
#include "engine/network.h"
#include "ide/ide_app.h"
#include "platform/pc/pc_input.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>

static int pc_dialog_open(char *dst, int cap) {
    OPENFILENAMEA ofn;
    char file[MAX_PATH];
    file[0] = 0;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "DiScript (*.ds)\0*.ds\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Open script";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameA(&ofn))
        return 0;
    snprintf(dst, (size_t)cap, "%s", file);
    return 1;
}

static int pc_dialog_save(const char *name, char *dst, int cap) {
    OPENFILENAMEA ofn;
    char file[MAX_PATH];
    snprintf(file, sizeof(file), "%s", name && name[0] ? name : "script.ds");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "DiScript (*.ds)\0*.ds\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "ds";
    ofn.lpstrTitle = "Save script";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetSaveFileNameA(&ofn))
        return 0;
    snprintf(dst, (size_t)cap, "%s", file);
    return 1;
}
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int init_done = 0;
static uint64_t prev_loop_ns = 0;
static double frame_seconds = 0.0;
static int phys_w = 0, phys_h = 0;
static Buffer g_frame = {0};
static int pc_fixed_step_ms = 0; /* headless: deterministic 60 Hz */
#ifdef _WIN32
static int pc_quit = 0; /* the window loop owns the exit flag */
#endif

static uint64_t monotonic_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    if (freq.QuadPart <= 0)
        return 0;
    return (uint64_t)((double)now.QuadPart * 1e9 / (double)freq.QuadPart);
#else
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
#endif
}

double ds_frame_seconds(void) { return frame_seconds; }

static void apply_screen_size(void) {
    if (phys_w < 1 || phys_h < 1)
        return;
    screen_w = phys_w;
    screen_h = phys_h;
}

/* ---- editor hooks, protected exactly like on Android -------------------- */
static IDEApp g_ide;
static int ide_ready = 0;

/* Editor actions never touch VM state directly, but a bug in a script must not
 * take the application down: everything the script side runs goes through the
 * same protected-call wrapper the game used. */
static void protected_ide_run(void *userdata) {
    ide_app_run((IDEApp *)userdata);
}

void pc_ide_init(void) {
    if (ide_ready)
        return;
    ide_app_init(&g_ide, pc_asset_manager());
    ide_ready = 1;
}

IDEApp *pc_ide_app(void) { return ide_ready ? &g_ide : NULL; }

static void pc_touch(float x, float y, int action, int id) {
    (void)id;
    if (!ide_ready)
        return;
    if (screen_w > 0) {
        if (x < 0)
            x = 0;
        if (x > (float)(screen_w - 1))
            x = (float)(screen_w - 1);
    }
    if (screen_h > 0) {
        if (y < 0)
            y = 0;
        if (y > (float)(screen_h - 1))
            y = (float)(screen_h - 1);
    }
    /* The editor only distinguishes press from everything else. */
    ide_app_pointer_sized(&g_ide, x, y, action == 0 ? 0 : 1, screen_w, screen_h);
}

static int pc_back(void) {
    /* No back stack on the desktop build. */
    return 0;
}

/* ---- one iteration of the main loop (shared by window and harness) ------ */
static int pc_step(void) {
    uint64_t frame_start = monotonic_ns();
    double now;
    ds_string_pool_tick();
    if (prev_loop_ns && frame_start >= prev_loop_ns) {
        frame_seconds = (double)(frame_start - prev_loop_ns) / 1e9;
        ds_graphics_report_frame_interval(frame_seconds);
    }
    prev_loop_ns = frame_start;
    apply_screen_size();
    now = (double)frame_start / 1e9;
    if (!ide_ready)
        pc_ide_init();
    ide_app_update(&g_ide, now);

    g_frame.pixels = NULL;
    g_frame.width = screen_w;
    g_frame.height = screen_h;
    g_frame.stride = screen_w;
    if (g_frame.width > 0 && g_frame.height > 0 && ds_graphics_begin_frame(&g_frame)) {
        ide_app_draw(&g_ide, g_frame.width, g_frame.height);
        ds_graphics_end_frame();
    }
    return 1;
}

/* ---- shared setup ------------------------------------------------------- */
#include "platform/pc/pc_args.inc"

static void pc_exe_dir(char *out, size_t cap) {
#ifdef _WIN32
    char full[1024];
    DWORD n = GetModuleFileNameA(NULL, full, sizeof(full));
    if (n == 0 || n >= sizeof(full)) {
        snprintf(out, cap, ".");
        return;
    }
    {
        char *slash = strrchr(full, '\\');
        if (slash)
            *slash = '\0';
        else
            snprintf(full, sizeof(full), ".");
    }
    snprintf(out, cap, "%s", full);
#else
    char full[1024];
    ssize_t n = readlink("/proc/self/exe", full, sizeof(full) - 1);
    if (n <= 0) {
        snprintf(out, cap, ".");
        return;
    }
    full[n] = '\0';
    {
        char *slash = strrchr(full, '/');
        if (slash)
            *slash = '\0';
        else
            snprintf(full, sizeof(full), ".");
    }
    snprintf(out, cap, "%s", full);
#endif
}

/* Resolve save directory next to executable unless overridden by --data or PC_DATA_PATH. */
static void pc_data_dir(const char *dir, char *out, size_t cap) {
    const char *fallback =
#ifdef PC_DATA_PATH
        PC_DATA_PATH;
#else
        "cb4_data";
#endif
    if (dir && *dir)
        snprintf(out, cap, "%s", dir);
    else if (fallback[0] == '/' || fallback[1] == ':')
        snprintf(out, cap, "%s", fallback);
    else {
        char exe_dir[1024];
        pc_exe_dir(exe_dir, sizeof(exe_dir));
        snprintf(out, cap, "%s/%s", exe_dir, fallback);
    }
#ifdef _WIN32
    CreateDirectoryA(out, NULL);
#else
    mkdir(out, 0755);
#endif
}

/* Resolve asset directory from working directory or executable folder. */
static void pc_resolve_assets(const char *given, char *out, size_t cap) {
    FILE *probe;
    char exe_dir[1024];
    char marker[1200];
    if (!given || !*given) {
        snprintf(out, cap, "assets");
        return;
    }
    if (given[0] == '/' || (given[0] && given[1] == ':')) {
        snprintf(out, cap, "%s", given);
        return;
    }
    /* Relative path: accept it as-is when it looks like an asset tree. Probe
     * for a file the editor actually ships rather than a game texture. */
    snprintf(marker, sizeof(marker), "%s/fonts/DejaVuSansMono.ttf", given);
    probe = fopen(marker, "rb");
    if (probe) {
        fclose(probe);
        snprintf(out, cap, "%s", given);
        return;
    }
    pc_exe_dir(exe_dir, sizeof(exe_dir));
    snprintf(out, cap, "%s/%s", exe_dir, given);
}

/* Seed PRNG deterministically in headless mode or from clock/PID in window mode. */
static void pc_seed_random(int fixed) {
    if (fixed) {
        srand(1);
        return;
    }
#ifdef _WIN32
    srand((unsigned)(time(NULL) * 2654435761u) ^ (unsigned)GetTickCount());
#else
    srand((unsigned)(time(NULL) * 2654435761u) ^ ((unsigned)getpid() * 0x9E3779B9u));
#endif
}

/* Headless runs have no system dialog. CB4_SAVE_TO points the save hook at a
 * file, which is how the texture pack export is tested end to end. */
static int pc_dialog_save_env(const char *name, char *dst, int cap) {
    const char *wanted = getenv("CB4_SAVE_TO");
    (void)name;
    if (!wanted || !*wanted)
        return 0;
    snprintf(dst, (size_t)cap, "%s", wanted);
    return 1;
}

static void pc_boot(const PcArgs *args) {
    char data_dir[512];
    char assets_dir[1200];
    if (args->verbose == 0 && getenv("CB4_LOGS") == NULL)
        ds_log_quiet = 1;
#ifdef _WIN32
    if (args->headless == 0) {
        ds_pick_open_hook = pc_dialog_open;
        ds_pick_save_hook = pc_dialog_save;
    }
#endif
    if (args->headless != 0 && getenv("CB4_SAVE_TO") != NULL)
        ds_pick_save_hook = pc_dialog_save_env;
    pc_seed_random(args->headless);
    pc_resolve_assets(args->assets, assets_dir, sizeof(assets_dir));
    pc_assets_set_root(assets_dir);
    pc_data_dir(args->data, data_dir, sizeof(data_dir));
    net_set_data_path(data_dir);
    net_set_system_language("ru");
    net_set_firebase_key(NET_FIREBASE_KEY);
    phys_w = args->width;
    phys_h = args->height;
    apply_screen_size();
    {
        AAssetManager *assets = pc_asset_manager();
        ds_set_activity(NULL);
        ds_sound_set_java_vm(NULL);

        if (!ds_graphics_init(assets, NULL)) {
            ds_log_err("renderer failed: %s", ds_graphics_failure());
            return;
        }
        ds_sound_init(assets);
        ds_sound_resume();
    }
    init_done = 1;
    prev_loop_ns = 0;
    frame_seconds = 0.0;
    pc_ide_init();
}

static void pc_shutdown(void) {
    init_done = 0;
    keyboard_hide();
    net_disconnect();
    ds_graphics_shutdown();
    ds_sound_shutdown();
}

#include "platform/pc/pc_screenshot.inc"

#include "platform/pc/pc_assets.inc"

#ifdef _WIN32
#include "platform/pc/pc_headless.inc"
#include "platform/pc/pc_window.inc"
#else
#include "platform/pc/pc_headless.inc"
#endif

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show) {
    PcArgs args;
    (void)inst;
    (void)prev;
    (void)cmdline;
    (void)show;
    pc_args_default(&args);
    pc_args_parse(GetCommandLineA(), &args);
    if (args.headless)
        return pc_run_headless(&args);
    return pc_run_window(&args);
}
#else
int main(int argc, char **argv) {
    PcArgs args;
    char joined[4096];
    char dump[260];
    pc_args_default(&args);
    pc_args_join(argc, argv, joined, sizeof(joined));
    pc_args_parse(joined, &args);
    if (strstr(joined, "--gfx-test")) {
        pc_args_dump_path(joined, dump, sizeof(dump));
        return pc_gfx_test(&args, dump);
    }
    return pc_run_headless(&args);
}
#endif

#include "engine/graphics.c"
#include "engine/network.c"
#include "engine/audio.c"
