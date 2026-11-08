/* Desktop and headless entry point for Cubic Battle 4. */
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
#include "game/game.h"
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
    ofn.lpstrFilter = "Cubic Battle 4 mod (*.zip)\0*.zip\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Cubic Battle 4: open mod";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;
    if (!GetOpenFileNameA(&ofn))
        return 0;
    snprintf(dst, (size_t)cap, "%s", file);
    return 1;
}

static int pc_dialog_save(const char *name, char *dst, int cap) {
    OPENFILENAMEA ofn;
    char file[MAX_PATH];
    snprintf(file, sizeof(file), "%s", name && name[0] ? name : "mod.zip");
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "Cubic Battle 4 mod (*.zip)\0*.zip\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = "zip";
    ofn.lpstrTitle = "Cubic Battle 4: save mod";
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
static int game_active = 0;
static int game_started_once = 0;
static AAssetManager *game_assets = NULL;
static uint64_t restart_after_ns = 0;
static unsigned int restart_failures = 0;
static uint64_t prev_frame_ns = 0;
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

/* ---- game hooks, protected exactly like on Android ---------------------- */
static void protected_game_init(void *userdata) { game_init((AAssetManager *)userdata); }
static void protected_game_reset(void *userdata) {
    (void)userdata;
    game_reset();
}
static void protected_game_update(void *userdata) {
    (void)userdata;
    game_update();
}
static void protected_game_draw(void *userdata) { game_draw((Buffer *)userdata); }
static void protected_game_bye(void *userdata) {
    (void)userdata;
    game_bye();
}

typedef struct {
    float x, y;
    int action, id;
} TouchCall;

static void protected_game_touch(void *userdata) {
    TouchCall *call = (TouchCall *)userdata;
    game_touch(call->x, call->y, call->action, call->id);
}

static void pc_touch(float x, float y, int action, int id) {
    TouchCall call;
    if (!game_active)
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
    call.x = x;
    call.y = y;
    call.action = action;
    call.id = id;
    if (!ds_call_protected(protected_game_touch, &call, "touch"))
        ds_log_err("touch hook failed: %s", ds_runtime_error_message());
}

static int pc_back(void) {
    if (!game_active)
        return 0;
    return game_back();
}

static void mark_game_failed(const char *hook) {
    const char *message = ds_runtime_error_message();
    unsigned int shift;
    uint64_t delay;
    ds_log_err("game hook '%s' stopped: %s; scheduling a restart", hook ? hook : "unknown", message);
    ds_console_log(1, "game error: hook '%s' stopped: %s; restarting", hook ? hook : "unknown", message);
    shift = restart_failures < 5 ? restart_failures : 5;
    delay = 1000000000ull << shift;
    game_active = 0;
    ds_request_game_restart();
    restart_after_ns = monotonic_ns() + delay;
    ++restart_failures;
}

static int start_game(int reset_state) {
    int ok;
    ds_clear_runtime_error();
    ds_clear_game_restart();
    ds_string_pool_reset();
    if (reset_state) {
        ok = ds_call_protected(protected_game_reset, NULL, "reset");
        if (!ok) {
            mark_game_failed("reset");
            return 0;
        }
    }
    ds_clear_runtime_error();
    ok = ds_call_protected(protected_game_init, game_assets, "init");
    if (!ok) {
        mark_game_failed("init");
        return 0;
    }
    ds_clear_runtime_error();
    restart_failures = 0;
    game_active = 1;
    game_started_once = 1;
    return 1;
}

static void restart_game_if_due(void) {
    uint64_t now;
    if (game_active || !ds_game_restart_requested())
        return;
    now = monotonic_ns();
    if (now < restart_after_ns)
        return;
    (void)start_game(1);
}

static void say_goodbye(void) {
    if (!game_active)
        return;
    (void)ds_call_protected(protected_game_bye, NULL, "goodbye");
}

/* ---- one iteration of the game loop (shared by window and harness) ------ */
static int pc_step(void) {
    uint64_t frame_start;
    restart_game_if_due();
    frame_start = monotonic_ns();
    ds_string_pool_tick();
    if (prev_loop_ns && frame_start >= prev_loop_ns) {
        frame_seconds = (double)(frame_start - prev_loop_ns) / 1e9;
        ds_graphics_report_frame_interval(frame_seconds);
    }
    prev_loop_ns = frame_start;
    apply_screen_size();
    if (game_active) {
        uint64_t now = frame_start;
        if (pc_fixed_step_ms > 0)
            dt = (double)pc_fixed_step_ms / 1000.0;
        else
            dt = prev_frame_ns ? (double)(now - prev_frame_ns) / 1000000000.0 : 0.0;
        if (dt < 0.0)
            dt = 0.0;
        if (dt > 0.1)
            dt = 0.1;
        prev_frame_ns = now;
        keyboard_tick(monotonic_ns() / 1000000ull);
        if (!ds_call_protected(protected_game_update, NULL, "update"))
            mark_game_failed("update");
        else if (ds_game_restart_requested()) {
            game_active = 0;
            restart_after_ns = monotonic_ns();
        }
    }

    g_frame.pixels = NULL;
    g_frame.width = screen_w;
    g_frame.height = screen_h;
    g_frame.stride = screen_w;
    if (g_frame.width > 0 && g_frame.height > 0 && ds_graphics_begin_frame(&g_frame)) {
        int draw_failed = 0;
        if (game_active) {
            if (!ds_call_protected(protected_game_draw, &g_frame, "draw")) {
                mark_game_failed("draw");
                draw_failed = 1;
            } else if (ds_game_restart_requested()) {
                game_active = 0;
                restart_after_ns = monotonic_ns();
            }
        }
        if (!game_active) {
            if (draw_failed || ds_game_has_error()) {
                ds_graphics_error_screen(ds_runtime_error_message());
                ds_graphics_end_frame();
            } else
                ds_graphics_cancel_frame();
        } else
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
    snprintf(marker, sizeof(marker), "%s/grass.png", given);
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
    game_assets = pc_asset_manager();
    ds_set_activity(NULL);
    ds_sound_set_java_vm(NULL);

    if (!ds_graphics_init(game_assets, NULL)) {
        ds_log_err("renderer failed: %s", ds_graphics_failure());
        return;
    }
    ds_sound_init(game_assets);
    ds_sound_resume();
    init_done = 1;
    prev_frame_ns = 0;
    prev_loop_ns = 0;
    frame_seconds = 0.0;
    (void)start_game(0);
}

static void pc_shutdown(void) {
    say_goodbye();
    init_done = 0;
    game_active = 0;
    keyboard_hide();
    net_disconnect();
    ds_graphics_shutdown();
    ds_sound_shutdown();
}

#include "platform/pc/pc_screenshot.inc"

#include "platform/pc/pc_assets.inc"
#include "platform/pc/pc_controls.inc"

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
