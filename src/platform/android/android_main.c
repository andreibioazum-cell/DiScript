/* Main loop of the Android app. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <android_native_app_glue.h>
#include "engine/runtime.h"
#include "engine/network.h"
#include "game/game.h"
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
static int game_active = 0;
static struct android_app *ui_app = NULL;

static JavaVM *fs_vm_ptr;
static int fs_attached_now;

static JNIEnv *fs_java_env(void) {
    JavaVM *vm;
    JNIEnv *env = NULL;
    fs_attached_now = 0;
    if (!ui_app || !ui_app->activity || !ui_app->activity->vm)
        return NULL;
    vm = ui_app->activity->vm;
    fs_vm_ptr = vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
            return NULL;
        fs_attached_now = 1;
    }
    return env;
}

static void fs_env_release(void) {
    if (fs_attached_now && fs_vm_ptr)
        (*fs_vm_ptr)->DetachCurrentThread(fs_vm_ptr);
    fs_attached_now = 0;
}

static volatile int fs_fd_kind = 0;
static volatile int fs_fd_value = -1;
static pthread_mutex_t fs_fd_lock = PTHREAD_MUTEX_INITIALIZER;

static void fs_hand_fd(int kind, int fd) {
    int stale;
    pthread_mutex_lock(&fs_fd_lock);
    stale = fs_fd_value;
    fs_fd_value = fd;
    fs_fd_kind = kind;
    pthread_mutex_unlock(&fs_fd_lock);
    if (stale >= 0 && stale != fd)
        close(stale);
}

int net_fs_take_fd(int *kind) {
    int fd;
    pthread_mutex_lock(&fs_fd_lock);
    fd = fs_fd_value;
    if (kind)
        *kind = fs_fd_kind;
    fs_fd_value = -1;
    fs_fd_kind = 0;
    pthread_mutex_unlock(&fs_fd_lock);
    return fd;
}

/* Called from the Java side once the document picker returns a descriptor. */
JNIEXPORT void JNICALL Java_com_cb4_GameActivity_nativePickedFd(JNIEnv *env, jobject thiz, jint fd) {
    (void)env;
    (void)thiz;
    fs_hand_fd(1, (int)fd);
}

JNIEXPORT void JNICALL Java_com_cb4_GameActivity_nativeSavedFd(JNIEnv *env, jobject thiz, jint fd) {
    (void)env;
    (void)thiz;
    fs_hand_fd(2, (int)fd);
}

static int fs_call_void(const char *name, const char *sig, const char *arg) {
    JNIEnv *env = fs_java_env();
    jobject self;
    jclass cls;
    jmethodID id;
    jstring text = NULL;
    int ok = 0;
    if (!env || !ui_app->activity->clazz) {
        fs_env_release();
        ds_log("file picker: no JNI env");
        return 0;
    }
    self = ui_app->activity->clazz;
    if ((*env)->PushLocalFrame(env, 8) != 0) {
        fs_env_release();
        return 0;
    }
    cls = (*env)->GetObjectClass(env, self);
    id = cls ? (*env)->GetMethodID(env, cls, name, sig) : NULL;
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        id = NULL;
    }
    if (id) {
        if (arg) {
            text = (*env)->NewStringUTF(env, arg);
            (*env)->CallVoidMethod(env, self, id, text);
        } else {
            (*env)->CallVoidMethod(env, self, id);
        }
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
            ds_log("file picker: java exception in %s", name);
        } else {
            ok = 1;
            ds_log("file picker: %s called", name);
        }
    } else {
        ds_log("file picker: no method %s", name);
    }
    (*env)->PopLocalFrame(env, NULL);
    fs_env_release();
    return ok;
}

int net_fs_pick_import(void) { return fs_call_void("pickModZip", "()V", NULL); }

int net_fs_pick_export(const char *suggested) {
    return fs_call_void("saveModZip", "(Ljava/lang/String;)V", suggested ? suggested : "mod.zip");
}

static int game_started_once = 0;
static AAssetManager *game_assets = NULL;
static uint64_t restart_after_ns = 0;
static unsigned int restart_failures = 0;
static uint64_t prev_frame_ns = 0;
static uint64_t prev_loop_ns = 0;
static double frame_seconds = 0.0;
static uint64_t monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    return (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
}

/* Real frame length: dt is clamped to 0.1 s and is no good for the FPS counter. */
double ds_frame_seconds(void) { return frame_seconds; }

static int phys_w = 0, phys_h = 0;

static void apply_screen_size(void) {
    if (phys_w < 1 || phys_h < 1)
        return;
    screen_w = phys_w;
    screen_h = phys_h;
}
static void protected_game_init(void *userdata) {
    game_init((AAssetManager *)userdata);
}
static void protected_game_reset(void *userdata) {
    (void)userdata;
    game_reset();
}
static void protected_game_update(void *userdata) {
    (void)userdata;
    game_update();
}
static void protected_game_draw(void *userdata) {
    game_draw((Buffer *)userdata);
}
typedef struct {
    float x;
    float y;
    int action;
    int id;
} TouchCall;
static void protected_game_touch(void *userdata) {
    TouchCall *call = (TouchCall *)userdata;
    game_touch(call->x, call->y, call->action, call->id);
}
static int back_consumed = 0;
typedef struct {
    int handled;
} BackCall;
static void protected_game_bye(void *userdata) {
    (void)userdata;
    game_bye();
}
/* Invoke game shutdown hook on activity stop. */
static void say_goodbye(void) {
    if (!game_active) {
        return;
    }
    (void)ds_call_protected(protected_game_bye, NULL, "goodbye");
}
static void protected_game_back(void *userdata) {
    BackCall *call = (BackCall *)userdata;
    call->handled = game_back();
}
static void mark_game_failed(const char *hook) {
    const char *message = ds_runtime_error_message();
    __android_log_print(ANDROID_LOG_ERROR, "CubicBattle", "game hook '%s' stopped: %s; scheduling a restart",
                        hook ? hook : "unknown", message);
    ds_console_log(1, "game error: hook '%s' stopped: %s; restarting", hook ? hook : "unknown", message);
    unsigned int shift = restart_failures < 5 ? restart_failures : 5;
    uint64_t delay = 1000000000ull << shift;
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

#define GFX_START_TRIES 4
static int gfx_start_tries = 0;
static uint64_t gfx_retry_at_ns = 0;
static int gfx_failure_shown = 0;
static void gfx_start_failed(struct android_app *app, const char *why) {
    init_done = 0;
    ++gfx_start_tries;
    if (gfx_start_tries < GFX_START_TRIES) {
        uint64_t delay = 500000000ull << (gfx_start_tries - 1);
        gfx_retry_at_ns = monotonic_ns() + delay;
        ds_log_err("renderer start %d/%d failed (%s), next try in %u ms", gfx_start_tries, GFX_START_TRIES, why,
                   (unsigned)(delay / 1000000ull));
        return;
    }
    gfx_retry_at_ns = 0;
    ds_log_err("renderer start failed %d times (%s): showing the reason on screen", gfx_start_tries, why);
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
    game_assets = app->activity ? app->activity->assetManager : NULL;
    ds_set_activity(app->activity);

    if (!ds_graphics_init(game_assets, app->window)) {
        gfx_start_failed(app, ds_graphics_failure());
        return;
    }
    if (gfx_start_tries)
        ds_log("renderer started on try %d", gfx_start_tries + 1);
    gfx_start_tries = 0;
    gfx_retry_at_ns = 0;
    gfx_failure_shown = 0;

    ds_sound_init(game_assets);
    ds_sound_resume();
    init_done = 1;

    prev_frame_ns = 0;
    prev_loop_ns = 0;
        /* The frame before minimising says nothing about the new window. */
    frame_seconds = 0.0;
    if (game_active) {

        ds_log("window is back (%dx%d): the game continues", phys_w, phys_h);
        return;
    }
    restart_failures = 0;
    ds_clear_game_restart();
    (void)start_game(game_started_once);
}
static void handle_cmd(struct android_app *app, int32_t command) {
    if (!app) {
        ds_runtime_error("no app");
        return;
    }
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
        say_goodbye();
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
        if (!game_active)
            return 0;
        TouchCall call;
        size_t count, index, i;
        int raw, action;
        count = AMotionEvent_getPointerCount(event);
        if (count == 0)
            return 0;
        raw = AMotionEvent_getAction(event);
        action = raw & AMOTION_EVENT_ACTION_MASK;
        if (action == AMOTION_EVENT_ACTION_POINTER_DOWN)
            action = AMOTION_EVENT_ACTION_DOWN;
        else if (action == AMOTION_EVENT_ACTION_POINTER_UP)
            action = AMOTION_EVENT_ACTION_UP;
        index = (size_t)((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (index >= count)
            index = 0;
        i = (action == AMOTION_EVENT_ACTION_MOVE) ? 0 : index;
        count = (action == AMOTION_EVENT_ACTION_MOVE) ? count : index + 1;
        for (; i < count; i++) {

            call.x = AMotionEvent_getX(event, i);
            call.y = AMotionEvent_getY(event, i);

            if (screen_w > 0) {
                if (call.x < 0)
                    call.x = 0;
                if (call.x > (float)(screen_w - 1))
                    call.x = (float)(screen_w - 1);
            }
            if (screen_h > 0) {
                if (call.y < 0)
                    call.y = 0;
                if (call.y > (float)(screen_h - 1))
                    call.y = (float)(screen_h - 1);
            }
            call.action = action;
            call.id = AMotionEvent_getPointerId(event, i);
            if (!ds_call_protected(protected_game_touch, &call, "touch")) {
                mark_game_failed("touch");
                break;
            }
        }
        return 1;
    } else if (type == AINPUT_EVENT_TYPE_KEY) {
        int32_t action = AKeyEvent_getAction(event);
        int32_t key = AKeyEvent_getKeyCode(event);
        int32_t meta = AKeyEvent_getMetaState(event);
        if (key == AKEYCODE_BACK && action == AKEY_EVENT_ACTION_DOWN &&
            (keyboard_visible() || keyboard_uses_editor())) {
            keyboard_hide();
            return 1;
        }
        /* Backspace UP must stop the native repeat even if the IME already
         * hid itself; DOWN starts one immediate delete plus the 500 ms timer. */
        if ((key == AKEYCODE_DEL || key == AKEYCODE_FORWARD_DEL) && action == AKEY_EVENT_ACTION_UP) {
            keyboard_handle_key(key, action, meta);
            return 1;
        }
        if (keyboard_visible() && (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_MULTIPLE)) {
            if (keyboard_handle_key(key, action, meta))
                return 1;
        }
        if (key == AKEYCODE_BACK) {

            if (action == AKEY_EVENT_ACTION_DOWN) {
                BackCall call = {0};
                back_consumed = 0;
                if (!game_active)
                    return 0;
                if (!ds_call_protected(protected_game_back, &call, "back_pressed")) {
                    mark_game_failed("back_pressed");
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

        if (keyboard_uses_editor())
            return 0;
        return 1;
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
    net_set_java_vm(app->activity->vm);
    ds_sound_set_java_vm((void *)app->activity->vm);
    ui_app = app;
    net_set_data_path(app->activity->internalDataPath);
    net_fs_set_dirs(app->activity->internalDataPath, app->activity->externalDataPath);
    ds_log_quiet = 1;
    {
        char lang[3] = {0};
        if (app->config)
            AConfiguration_getLanguage(app->config, lang);
        net_set_system_language(lang);
    }
    ds_set_activity(app->activity);
    ds_log("Cubic Battle 4: C + Vulkan");
    for (;;) {
        struct android_poll_source *source = NULL;
        int ident;

        int drawing = app->window && init_done;
        int timeout = drawing ? (game_active ? 0 : 10) : (gfx_retry_at_ns ? 50 : 250);
        while ((ident = ALooper_pollOnce(timeout, NULL, NULL, (void **)&source)) >= 0) {
            if (source && source->process)
                source->process(app, source);
            if (app->destroyRequested) {

                init_done = 0;
                game_active = 0;
                keyboard_hide();
                net_disconnect();
                ds_graphics_shutdown();
                ds_sound_shutdown();
                return;
            }
            timeout = 0;
        }
        if (app->window && !init_done && gfx_retry_at_ns && !app->destroyRequested &&
            monotonic_ns() >= gfx_retry_at_ns) {
            gfx_retry_at_ns = 0;
            start_window(app);
        }
        if (!app->window || !init_done || app->destroyRequested)
            continue;
        restart_game_if_due();
        uint64_t frame_start = monotonic_ns();
        /* Reclaim per-frame pooled strings before running game hooks. */
        ds_string_pool_tick();

        if (prev_loop_ns && frame_start >= prev_loop_ns) {
            frame_seconds = (double)(frame_start - prev_loop_ns) / 1e9;
            ds_graphics_report_frame_interval(frame_seconds);
        }
        prev_loop_ns = frame_start;
        apply_screen_size();
        if (game_active) {
            uint64_t now = frame_start;
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

        frame.pixels = NULL;
        frame.width = screen_w;
        frame.height = screen_h;
        frame.stride = screen_w;
        if (frame.width > 0 && frame.height > 0 && ds_graphics_begin_frame(&frame)) {
            int draw_failed = 0;
            if (game_active) {
                if (!ds_call_protected(protected_game_draw, &frame, "draw")) {
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
    }
}
#include "engine/graphics.c"
#include "engine/network.c"
#include "platform/android/presence_job.inc"
#include "engine/audio.c"
