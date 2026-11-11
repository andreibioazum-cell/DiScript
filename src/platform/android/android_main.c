/* Android NativeActivity + Vulkan 1.0 entry point for DiScript IDE. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <android_native_app_glue.h>
#include "engine/runtime.h"
#include "ide/ide.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/native_activity.h>
#include <pthread.h>
#include <unistd.h>

static int init_done = 0;
static int ide_active = 0;
static int ide_started_once = 0;
static AAssetManager *ide_assets = NULL;
static uint64_t restart_after_ns = 0;
static unsigned int restart_failures = 0;
static uint64_t prev_frame_ns = 0;
static uint64_t prev_loop_ns = 0;
static double frame_seconds = 0.0;
static int phys_w = 0, phys_h = 0;
static int back_consumed = 0;

double ds_frame_seconds(void) { return frame_seconds; }

static void apply_screen_size(void) {
    if (phys_w < 1 || phys_h < 1)
        return;
    int s = ds_graphics_pixel_scale();
    if (s < 1)
        s = 1;
    screen_w = phys_w / s;
    screen_h = phys_h / s;
    if (screen_w < 1)
        screen_w = 1;
    if (screen_h < 1)
        screen_h = 1;
}

static void protected_ide_init(void *userdata) { ide_init((AAssetManager *)userdata); }
static void protected_ide_reset(void *userdata) { (void)userdata; ide_reset(); }
static void protected_ide_update(void *userdata) { (void)userdata; ide_update(); }
static void protected_ide_draw(void *userdata) { ide_draw((Buffer *)userdata); }
static void protected_ide_bye(void *userdata) { (void)userdata; ide_bye(); }

typedef struct {
    float x, y;
    int action, id;
} TouchCall;

static void protected_ide_touch(void *userdata) {
    TouchCall *call = (TouchCall *)userdata;
    ide_touch(call->x, call->y, call->action, call->id);
}

typedef struct {
    int handled;
} BackCall;

static void protected_ide_back(void *userdata) {
    BackCall *call = (BackCall *)userdata;
    call->handled = ide_back();
}

static void mark_ide_failed(const char *where) {
    ide_active = 0;
    restart_failures++;
    uint64_t backoff = 400000000ull << (restart_failures < 4 ? restart_failures : 4);
    restart_after_ns = ds_monotonic_ns() + backoff;
    ds_log_err("ide hook '%s' failed: %s", where, ds_runtime_error_message());
}

static int start_ide(int is_restart) {
    ds_clear_runtime_error();
    ds_clear_game_restart();
    if (is_restart) {
        if (!ds_call_protected(protected_ide_reset, NULL, "reset")) {
            mark_ide_failed("reset");
            return 0;
        }
    } else {
        if (!ds_call_protected(protected_ide_init, ide_assets, "init")) {
            mark_ide_failed("init");
            return 0;
        }
        ide_started_once = 1;
    }
    ide_active = 1;
    restart_failures = 0;
    restart_after_ns = 0;
    return 1;
}

static void restart_ide_if_due(void) {
    if (ide_active || !restart_after_ns)
        return;
    if (ds_monotonic_ns() < restart_after_ns)
        return;
    restart_after_ns = 0;
    (void)start_ide(ide_started_once);
}

#define GFX_START_TRIES 4
static int gfx_start_tries = 0;
static uint64_t gfx_retry_at_ns = 0;
static int gfx_failure_shown = 0;

static void gfx_start_failed(struct android_app *app, const char *why) {
    init_done = 0;
    ++gfx_start_tries;
    if (gfx_start_tries < GFX_START_TRIES) {
        uint64_t delay = 500000000ull << (gfx_start_tries - 1);
        gfx_retry_at_ns = ds_monotonic_ns() + delay;
        ds_log_err("renderer start %d/%d failed (%s)", gfx_start_tries, GFX_START_TRIES, why);
        return;
    }
    gfx_retry_at_ns = 0;
    gfx_failure_shown = ds_graphics_show_failure(app->window, gfx_start_tries);
}

static void start_window(struct android_app *app) {
    if (!app->window) {
        init_done = 0;
        return;
    }
    phys_w = ANativeWindow_getWidth(app->window);
    phys_h = ANativeWindow_getHeight(app->window);
    if (phys_w <= 0 || phys_h <= 0) {
        gfx_start_failed(app, "window size 0");
        return;
    }
    apply_screen_size();
    ide_assets = app->activity ? app->activity->assetManager : NULL;
    ds_set_activity(app->activity);

    if (!ds_graphics_init(ide_assets, app->window)) {
        gfx_start_failed(app, ds_graphics_failure());
        return;
    }
    gfx_start_tries = 0;
    gfx_retry_at_ns = 0;
    gfx_failure_shown = 0;

    ds_sound_init(ide_assets);
    ds_sound_resume();
    init_done = 1;
    prev_frame_ns = 0;
    prev_loop_ns = 0;
    frame_seconds = 0.0;
    if (ide_active)
        return;
    restart_failures = 0;
    ds_clear_game_restart();
    (void)start_ide(ide_started_once);
}

static void handle_cmd(struct android_app *app, int32_t command) {
    if (!app)
        return;
    switch (command) {
    case APP_CMD_INIT_WINDOW:
        gfx_start_tries = 0;
        gfx_retry_at_ns = 0;
        gfx_failure_shown = 0;
        start_window(app);
        break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONTENT_RECT_CHANGED:
    case APP_CMD_CONFIG_CHANGED:
        if (app->window) {
            int w = ANativeWindow_getWidth(app->window);
            int h = ANativeWindow_getHeight(app->window);
            if (w > 0 && h > 0) {
                phys_w = w;
                phys_h = h;
                apply_screen_size();
            }
        }
        break;
    case APP_CMD_TERM_WINDOW:
        init_done = 0;
        keyboard_hide();
        gfx_retry_at_ns = 0;
        gfx_failure_shown = 0;
        ds_graphics_window_lost();
        ds_sound_suspend();
        break;
    case APP_CMD_WINDOW_REDRAW_NEEDED:
        if (gfx_failure_shown && app->window)
            (void)ds_graphics_show_failure(app->window, gfx_start_tries);
        break;
    case APP_CMD_GAINED_FOCUS:
        ds_sound_resume();
        break;
    case APP_CMD_LOST_FOCUS:
        ds_sound_pause();
        break;
    case APP_CMD_STOP:
    case APP_CMD_SAVE_STATE:
        if (ide_active)
            (void)ds_call_protected(protected_ide_bye, NULL, "bye");
        break;
    default:
        break;
    }
}

static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    (void)app;
    if (!event)
        return 0;
    int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_MOTION) {
        if (!ide_active)
            return 0;
        TouchCall call;
        size_t count = AMotionEvent_getPointerCount(event);
        if (count == 0)
            return 0;
        int raw = AMotionEvent_getAction(event);
        int action = raw & AMOTION_EVENT_ACTION_MASK;
        if (action == AMOTION_EVENT_ACTION_POINTER_DOWN)
            action = AMOTION_EVENT_ACTION_DOWN;
        else if (action == AMOTION_EVENT_ACTION_POINTER_UP)
            action = AMOTION_EVENT_ACTION_UP;
        size_t index = (size_t)((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (index >= count)
            index = 0;
        size_t i = (action == AMOTION_EVENT_ACTION_MOVE) ? 0 : index;
        size_t end = (action == AMOTION_EVENT_ACTION_MOVE) ? count : index + 1;
        int scale = ds_graphics_pixel_scale();
        if (scale < 1)
            scale = 1;
        for (; i < end; i++) {
            call.x = AMotionEvent_getX(event, i) / (float)scale;
            call.y = AMotionEvent_getY(event, i) / (float)scale;
            if (screen_w > 0) {
                if (call.x < 0) call.x = 0;
                if (call.x > (float)(screen_w - 1)) call.x = (float)(screen_w - 1);
            }
            if (screen_h > 0) {
                if (call.y < 0) call.y = 0;
                if (call.y > (float)(screen_h - 1)) call.y = (float)(screen_h - 1);
            }
            call.action = action;
            call.id = AMotionEvent_getPointerId(event, i);
            if (!ds_call_protected(protected_ide_touch, &call, "touch")) {
                mark_ide_failed("touch");
                break;
            }
        }
        return 1;
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        int32_t key = AKeyEvent_getKeyCode(event);
        int32_t meta = AKeyEvent_getMetaState(event);
        if (key == AKEYCODE_BACK) {
            if (action == AKEY_EVENT_ACTION_DOWN) {
                if (keyboard_visible()) {
                    keyboard_hide();
                    back_consumed = 1;
                    return 1;
                }
                BackCall call = {0};
                back_consumed = 0;
                if (!ide_active)
                    return 0;
                if (!ds_call_protected(protected_ide_back, &call, "back")) {
                    mark_ide_failed("back");
                    return 1;
                }
                back_consumed = call.handled ? 1 : 0;
                return back_consumed;
            }
            if (action == AKEY_EVENT_ACTION_UP) {
                int c = back_consumed;
                back_consumed = 0;
                return c;
            }
            return back_consumed;
        }
        if (keyboard_handle_key(key, action, meta))
            return 1;
        return 0;
    }
    return 0;
}

void android_main(struct android_app *app) {
    Buffer frame = {0};
    if (!app)
        return;

    srand((unsigned)(time(NULL) * 2654435761u) ^ ((unsigned)getpid() * 0x9E3779B9u));
    app->onAppCmd = handle_cmd;
    app->onInputEvent = handle_input;
    ds_sound_set_java_vm((void *)app->activity->vm);
    ds_set_storage_dirs(app->activity->internalDataPath, app->activity->externalDataPath);
    ds_set_activity(app->activity);
    ds_log_quiet = 0;
    ds_log("DiScript IDE: C + C++17 + Vulkan 1.0 + Lua-GC + LÖVE2D");

    for (;;) {
        struct android_poll_source *source = NULL;
        int ident;
        int drawing = app->window && init_done;
        int timeout = drawing ? (ide_active ? 0 : 10) : (gfx_retry_at_ns ? 50 : 250);
        while ((ident = ALooper_pollOnce(timeout, NULL, NULL, (void **)&source)) >= 0) {
            if (source && source->process)
                source->process(app, source);
            if (app->destroyRequested) {
                if (ide_active)
                    (void)ds_call_protected(protected_ide_bye, NULL, "bye");
                init_done = 0;
                ide_active = 0;
                keyboard_hide();
                ds_graphics_shutdown();
                ds_sound_shutdown();
                return;
            }
            timeout = 0;
        }
        if (app->window && !init_done && gfx_retry_at_ns && !app->destroyRequested &&
            ds_monotonic_ns() >= gfx_retry_at_ns) {
            gfx_retry_at_ns = 0;
            start_window(app);
        }
        if (!app->window || !init_done || app->destroyRequested)
            continue;
        restart_ide_if_due();

        uint64_t frame_start = ds_monotonic_ns();
        ds_string_pool_tick();

        if (prev_loop_ns && frame_start >= prev_loop_ns) {
            frame_seconds = (double)(frame_start - prev_loop_ns) / 1e9;
            ds_graphics_report_frame_interval(frame_seconds);
        }
        prev_loop_ns = frame_start;
        apply_screen_size();

        if (ide_active) {
            uint64_t now = frame_start;
            dt = prev_frame_ns ? (double)(now - prev_frame_ns) / 1000000000.0 : 0.0;
            if (dt < 0.0)
                dt = 0.0;
            if (dt > 0.1)
                dt = 0.1;
            prev_frame_ns = now;
            keyboard_tick(ds_monotonic_ns() / 1000000ull);
            if (!ds_call_protected(protected_ide_update, NULL, "update"))
                mark_ide_failed("update");
            else if (ds_game_restart_requested()) {
                ide_active = 0;
                restart_after_ns = ds_monotonic_ns();
            }
        }

        frame.pixels = NULL;
        frame.width = screen_w;
        frame.height = screen_h;
        frame.stride = screen_w;
        if (frame.width > 0 && frame.height > 0 && ds_graphics_begin_frame(&frame)) {
            int draw_failed = 0;
            if (ide_active) {
                if (!ds_call_protected(protected_ide_draw, &frame, "draw")) {
                    mark_ide_failed("draw");
                    draw_failed = 1;
                } else if (ds_game_restart_requested()) {
                    ide_active = 0;
                    restart_after_ns = ds_monotonic_ns();
                }
            }
            if (!ide_active) {
                if (draw_failed || ds_game_has_error()) {
                    ds_graphics_error_screen(ds_runtime_error_message());
                    ds_graphics_end_frame();
                } else {
                    ds_graphics_cancel_frame();
                }
            } else {
                ds_graphics_end_frame();
            }
        }
    }
}

#include "engine/graphics.c"
#include "engine/audio.c"
