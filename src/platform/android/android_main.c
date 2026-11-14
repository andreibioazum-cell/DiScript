/* Main loop of the Android app. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <android_native_app_glue.h>
#include "engine/runtime.h"
#include "engine/network.h"
#include "ide/ide_app.h"
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
static struct android_app *ui_app = NULL;
static IDEApp g_ide;
static int ide_ready = 0;
/* Keyboard inset reported by the Java side, in physical pixels. */
static int kb_inset_px = 0;
/* Display density (pixels per dp). The editor surface is in physical pixels,
 * so the code font has to be scaled by this or it looks tiny on a phone. */
static float display_density = 1.0f;
static int kb_visible = 0;
/* Pending file-open/save results from the SAF dialogs. */
static volatile int open_result_ready = 0;
static char open_result_path[512];
static int open_result_fd = -1;
static char open_result_name[256];
static volatile int save_result_ready = 0;
static int save_result_fd = -1;
/* A picture chosen from the gallery, waiting for the editor thread to copy it
 * into the project. Kept apart from the script descriptor: the two pickers can
 * be open at different times and must not consume each other's result. */
static volatile int image_result_ready = 0;
static int image_result_fd = -1;
static char image_result_name[256];

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

static AAssetManager *app_assets = NULL;
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

/* The editor draws into the same logical surface the game used. */
/* The code font is sized in surface pixels. 18px is right for a desktop
 * surface; a phone reports physical pixels, where the same 18px is roughly a
 * third of the apparent size. Scale the base size by the display density and
 * keep it applied across rotation (init only runs once, the density call may
 * arrive later). */
static int applied_font_px = 0;
static void apply_editor_font(void) {
    if (!ide_ready)
        return;
    int px = (int)(18.0f * (display_density > 0.0f ? display_density : 1.0f) + 0.5f);
    if (px < 14)
        px = 14;
    if (px == applied_font_px)
        return;
    applied_font_px = px;
    ide_app_set_font_px(&g_ide, px);
}

static void ide_init_once(AAssetManager *assets) {
    if (!ide_ready) {
        ide_app_init(&g_ide, assets);
        ide_ready = 1;
    }
    apply_editor_font();
}

static IDEApp *ide_app(void) { return ide_ready ? &g_ide : NULL; }

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
    app_assets = app->activity ? app->activity->assetManager : NULL;
    ds_set_activity(app->activity);

    if (!ds_graphics_init(app_assets, app->window)) {
        gfx_start_failed(app, ds_graphics_failure());
        return;
    }
    if (gfx_start_tries)
        ds_log("renderer started on try %d", gfx_start_tries + 1);
    gfx_start_tries = 0;
    gfx_retry_at_ns = 0;
    gfx_failure_shown = 0;

    ds_sound_init(app_assets);
    ds_sound_resume();
    init_done = 1;

    prev_loop_ns = 0;
    /* The frame before minimising says nothing about the new window. */
    frame_seconds = 0.0;
    ide_init_once(app_assets);
    ds_log("window ready (%dx%d)", phys_w, phys_h);
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
        break;
    default:
        break;
    }
}
static int32_t handle_input(struct android_app *app, AInputEvent *event) {
    (void)app;
    if (!event)
        return 0;
    IDEApp *ide = ide_app();
    int32_t type = AInputEvent_getType(event);

    if (type == AINPUT_EVENT_TYPE_MOTION) {
        if (!ide)
            return 0;
        size_t count = AMotionEvent_getPointerCount(event);
        if (count == 0)
            return 0;
        int raw = AMotionEvent_getAction(event);
        int action = raw & AMOTION_EVENT_ACTION_MASK;
        size_t index = (size_t)((raw & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
        if (index >= count)
            index = 0;
        /* The editor is single-pointer: a drag is just the newest position. */
        size_t i = (action == AMOTION_EVENT_ACTION_MOVE) ? 0 : index;
        for (; action == AMOTION_EVENT_ACTION_MOVE ? i < count : i <= index; i++) {
            float x = AMotionEvent_getX(event, i);
            float y = AMotionEvent_getY(event, i);
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
            if (action == AMOTION_EVENT_ACTION_DOWN || action == AMOTION_EVENT_ACTION_POINTER_DOWN) {
                ide_app_pointer_sized(ide, x, y, 0, screen_w, screen_h);
                /* Any tap in the code area asks the IME to show up. */
                keyboard_show();
            } else if (action == AMOTION_EVENT_ACTION_MOVE) {
                ide_app_pointer_sized(ide, x, y, 2, screen_w, screen_h);
            } else if (action == AMOTION_EVENT_ACTION_UP || action == AMOTION_EVENT_ACTION_POINTER_UP ||
                       action == AMOTION_EVENT_ACTION_CANCEL) {
                ide_app_pointer_sized(ide, x, y, 1, screen_w, screen_h);
            }
        }
        return 1;
    }

    if (type == AINPUT_EVENT_TYPE_KEY) {
        if (!ide)
            return 0;
        int32_t action = AKeyEvent_getAction(event);
        int32_t key = AKeyEvent_getKeyCode(event);
        int32_t meta = AKeyEvent_getMetaState(event);

        if (key == AKEYCODE_BACK && action == AKEY_EVENT_ACTION_DOWN) {
            if (keyboard_visible() || keyboard_uses_editor()) {
                keyboard_hide();
                return 1;
            }
            return 0;
        }
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_MULTIPLE)
            return 1;

        switch (key) {
        case AKEYCODE_DEL:
            ide_app_backspace(ide, 1);
            return 1;
        case AKEYCODE_FORWARD_DEL:
            ide_app_special_key(ide, "delete");
            return 1;
        case AKEYCODE_ENTER:
        case AKEYCODE_NUMPAD_ENTER:
            ide_app_enter(ide);
            return 1;
        case AKEYCODE_TAB:
            ide_app_tab(ide);
            return 1;
        case AKEYCODE_DPAD_LEFT:
            ide_app_special_key(ide, "left");
            return 1;
        case AKEYCODE_DPAD_RIGHT:
            ide_app_special_key(ide, "right");
            return 1;
        case AKEYCODE_DPAD_UP:
            ide_app_special_key(ide, "up");
            return 1;
        case AKEYCODE_DPAD_DOWN:
            ide_app_special_key(ide, "down");
            return 1;
        case AKEYCODE_MOVE_HOME:
            ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "doc_start" : "home");
            return 1;
        case AKEYCODE_MOVE_END:
            ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "doc_end" : "end");
            return 1;
        case AKEYCODE_Z:
            if (meta & AMETA_CTRL_ON) {
                ide_app_special_key(ide, (meta & AMETA_SHIFT_ON) ? "redo" : "undo");
                return 1;
            }
            return 0;
        case AKEYCODE_S:
            if (meta & AMETA_CTRL_ON) {
                ide_app_save(ide);
                return 1;
            }
            return 0;
        case AKEYCODE_R:
            if (meta & AMETA_CTRL_ON) {
                ide_app_run(ide);
                return 1;
            }
            return 0;
        case AKEYCODE_ESCAPE:
            keyboard_hide();
            return 1;
        default:
            return 0; /* let the IME turn it into text */
        }
    }
    return 0;
}

/* ---- native file dialogs -------------------------------------------------
 * The Java activity owns the Storage Access Framework UI, so the hook asks it
 * to show a picker and then blocks the calling frame until the descriptor
 * comes back. Reading and writing go through the same descriptor. */

/* Called once per frame to retire finished dialog descriptors. */
static void fs_drain_results(void) {
    pthread_mutex_lock(&fs_fd_lock);
    if (open_result_ready && open_result_fd >= 0) {
        close(open_result_fd);
        open_result_fd = -1;
    }
    if (save_result_ready && save_result_fd >= 0) {
        close(save_result_fd);
        save_result_fd = -1;
    }
    open_result_ready = 0;
    save_result_ready = 0;
    pthread_mutex_unlock(&fs_fd_lock);
}

static char *fs_read_all(int fd, size_t *out_len) {
    size_t cap = 4096, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
        return NULL;
    for (;;) {
        if (len + 2048 > cap) {
            char *nb = (char *)realloc(buf, cap * 2);
            if (!nb)
                break;
            buf = nb;
            cap *= 2;
        }
        ssize_t got = read(fd, buf + len, cap - len - 1);
        if (got < 0)
            break;
        if (got == 0)
            break;
        len += (size_t)got;
        if (len > 8u * 1024u * 1024u)
            break;
    }
    buf[len] = '\0';
    if (out_len)
        *out_len = len;
    return buf;
}

/* Open picker: the activity writes the script through nativeEditorText, which
 * is why this hook only has to wait for the descriptor to close it. */
static int fs_open_dialog(char *dst, int cap) {
    if (!ide_app() || cap <= 0)
        return 0;
    open_result_ready = 0;
    if (!fs_call_void("pickScriptFile", "()V", NULL))
        return 0;
    /* Wait up to ~10 s for the picker; frames keep rendering in the meantime. */
    for (int waited = 0; waited < 600; waited++) {
        if (open_result_ready)
            break;
        struct timespec ts = {0, 16 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    pthread_mutex_lock(&fs_fd_lock);
    int ready = open_result_ready;
    int fd = open_result_fd;
    char name[256];
    snprintf(name, sizeof(name), "%s", open_result_name);
    open_result_ready = 0;
    open_result_fd = -1;
    pthread_mutex_unlock(&fs_fd_lock);
    if (!ready || fd < 0)
        return 0;
    size_t len = 0;
    char *text = fs_read_all(fd, &len);
    close(fd);
    if (!text)
        return 0;
    IDEApp *ide = ide_app();
    int idx = ide_view_open(&ide->view, name, text, len);
    free(text);
    if (idx < 0)
        return 0;
    snprintf(ide->current_path, sizeof(ide->current_path), "%s", name);
    ide_view_set_status(&ide->view, "Opened");
    /* The SAF descriptor is already consumed, so the editor handles the load
     * itself and reports success by copying the display name back. */
    snprintf(dst, (size_t)cap, "%s", name);
    return 1;
}

/* Write the descriptor the gallery returned into a temp file and hand the path
 * back. The editor copies it into the project and owns the copy; the temp file
 * is removed right after, so a cancelled add leaves nothing behind. */
static int fs_image_dialog(char *dst, int cap, char *name_out, int name_cap) {
    if (!ide_app() || cap <= 0)
        return 0;
    pthread_mutex_lock(&fs_fd_lock);
    image_result_ready = 0;
    if (image_result_fd >= 0) {
        close(image_result_fd);
        image_result_fd = -1;
    }
    pthread_mutex_unlock(&fs_fd_lock);
    if (!fs_call_void("pickImageFile", "()V", NULL))
        return 0;
    for (int waited = 0; waited < 600; waited++) {
        if (image_result_ready)
            break;
        struct timespec ts = {0, 16 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    pthread_mutex_lock(&fs_fd_lock);
    int ready = image_result_ready;
    int fd = image_result_fd;
    char name[256];
    snprintf(name, sizeof(name), "%s", image_result_name);
    image_result_fd = -1;
    image_result_ready = 0;
    pthread_mutex_unlock(&fs_fd_lock);
    if (!ready || fd < 0)
        return 0;

    char *bytes = NULL;
    size_t len = 0;
    {
        size_t cap2 = 65536, used = 0;
        char *buf = (char *)malloc(cap2);
        if (!buf) {
            close(fd);
            return 0;
        }
        for (;;) {
            if (used + 16384 > cap2) {
                char *nb = (char *)realloc(buf, cap2 * 2);
                if (!nb)
                    break;
                buf = nb;
                cap2 *= 2;
            }
            ssize_t got = read(fd, buf + used, cap2 - used);
            if (got <= 0)
                break;
            used += (size_t)got;
        }
        close(fd);
        bytes = buf;
        len = used;
    }
    if (!bytes || len == 0) {
        free(bytes);
        return 0;
    }

    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s/picked_%u.img", g_ide.project.dir[0] ? g_ide.project.dir : ".",
             (unsigned)(ds_monotonic_ns() / 1000000ull));
    FILE *f = fopen(tmp, "wb");
    if (!f || fwrite(bytes, 1, len, f) != len) {
        if (f)
            fclose(f);
        free(bytes);
        remove(tmp);
        return 0;
    }
    fclose(f);
    free(bytes);
    snprintf(dst, (size_t)cap, "%s", tmp);
    /* The temp path is not the user's file name, so report the picked name
     * separately: the sprite is labelled with what they saw in the gallery. */
    if (name_out && name_cap > 0)
        snprintf(name_out, (size_t)name_cap, "%s", name);
    return 1;
}

static int fs_save_dialog(const char *name, char *dst, int cap) {
    if (!ide_app() || cap <= 0)
        return 0;
    save_result_ready = 0;
    fs_call_void("saveScriptFile", "(Ljava/lang/String;)V", name && name[0] ? name : "script.ds");
    for (int waited = 0; waited < 600; waited++) {
        if (save_result_ready)
            break;
        struct timespec ts = {0, 16 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }
    pthread_mutex_lock(&fs_fd_lock);
    int ready = save_result_ready;
    int fd = save_result_fd;
    save_result_ready = 0;
    save_result_fd = -1;
    pthread_mutex_unlock(&fs_fd_lock);
    if (!ready || fd < 0)
        return 0;
    IDEApp *ide = ide_app();
    int idx = ide_view_active(&ide->view);
    if (idx >= 0) {
        IDEBuffer *b = &ide->view.tabs[idx].buf;
        size_t off = 0;
        while (off < b->len) {
            ssize_t put = write(fd, b->text + off, b->len - off);
            if (put <= 0)
                break;
            off += (size_t)put;
        }
        ide->view.tabs[idx].dirty = 0;
        snprintf(ide->view.tabs[idx].path, sizeof(ide->view.tabs[idx].path), "%s",
                 name && name[0] ? name : "script.ds");
    }
    close(fd);
    ide_view_set_status(&ide->view, "Saved");
    snprintf(dst, (size_t)cap, "%s", name && name[0] ? name : "script.ds");
    return 1;
}

/* ---- JNI surface used by DiScriptActivity ------------------------------- */
/* The Java side keeps a sentinel-padded EditText and mirrors every change
 * here, because a native activity cannot host an editable view itself. */

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorText(JNIEnv *env, jobject thiz,
                                                                             jstring text) {
    (void)thiz;
    IDEApp *ide = ide_app();
    if (!ide || !text)
        return;
    const char *utf = (*env)->GetStringUTFChars(env, text, NULL);
    if (!utf)
        return;
    ide_app_text(ide, utf);
    (*env)->ReleaseStringUTFChars(env, text, utf);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorBackspace(JNIEnv *env, jobject thiz,
                                                                                   jint count) {
    (void)env;
    (void)thiz;
    IDEApp *ide = ide_app();
    if (ide)
        ide_app_backspace(ide, (int)count);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorEnter(JNIEnv *env, jobject thiz) {
    (void)env;
    (void)thiz;
    IDEApp *ide = ide_app();
    if (ide)
        ide_app_enter(ide);
}

JNIEXPORT jboolean JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorSpecialKey(JNIEnv *env, jobject thiz,
                                                                                        jint keycode,
                                                                                        jint meta) {
    (void)env;
    (void)thiz;
    IDEApp *ide = ide_app();
    if (!ide)
        return JNI_FALSE;
    switch (keycode) {
    case AKEYCODE_DEL:
        ide_app_backspace(ide, 1);
        break;
    case AKEYCODE_FORWARD_DEL:
        ide_app_special_key(ide, "delete");
        break;
    case AKEYCODE_ENTER:
    case AKEYCODE_NUMPAD_ENTER:
        ide_app_enter(ide);
        break;
    case AKEYCODE_TAB:
        ide_app_tab(ide);
        break;
    case AKEYCODE_DPAD_LEFT:
        ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "word_left" : "left");
        break;
    case AKEYCODE_DPAD_RIGHT:
        ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "word_right" : "right");
        break;
    case AKEYCODE_DPAD_UP:
        ide_app_special_key(ide, "up");
        break;
    case AKEYCODE_DPAD_DOWN:
        ide_app_special_key(ide, "down");
        break;
    case AKEYCODE_MOVE_HOME:
        ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "doc_start" : "home");
        break;
    case AKEYCODE_MOVE_END:
        ide_app_special_key(ide, (meta & AMETA_CTRL_ON) ? "doc_end" : "end");
        break;
    case AKEYCODE_Z:
        ide_app_special_key(ide, (meta & AMETA_SHIFT_ON) ? "redo" : "undo");
        break;
    default:
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeKeyboardInset(JNIEnv *env, jobject thiz,
                                                                                 jint bottom_inset_px,
                                                                                 jint visible) {
    (void)env;
    (void)thiz;
    kb_inset_px = (int)bottom_inset_px;
    kb_visible = (int)visible;
}

/* The Java side already read the stream into the editor, so it only has to
 * tell us the display name to show on the tab. */
JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeDensity(JNIEnv *env, jobject thiz,
                                                                         jfloat density) {
    (void)env;
    (void)thiz;
    if (density > 0.25f && density < 8.0f)
        display_density = (float)density;
}

/* Gallery pick finished: park the descriptor and the display name. The picker
 * hook below picks them up on the frame thread. */
JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativePickedImage(JNIEnv *env, jobject thiz,
                                                                              jint fd, jstring name) {
    (void)thiz;
    const char *nm = NULL;
    if (name)
        nm = (*env)->GetStringUTFChars(env, name, NULL);
    pthread_mutex_lock(&fs_fd_lock);
    if (image_result_fd >= 0)
        close(image_result_fd);
    image_result_fd = (int)fd;
    snprintf(image_result_name, sizeof(image_result_name), "%s", nm ? nm : "picture.png");
    image_result_ready = 1;
    pthread_mutex_unlock(&fs_fd_lock);
    if (nm)
        (*env)->ReleaseStringUTFChars(env, name, nm);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativePickedFd(JNIEnv *env, jobject thiz, jint fd,
                                                                            jstring name) {
    (void)thiz;
    const char *nm = NULL;
    if (name)
        nm = (*env)->GetStringUTFChars(env, name, NULL);
    pthread_mutex_lock(&fs_fd_lock);
    open_result_fd = (int)fd;
    snprintf(open_result_name, sizeof(open_result_name), "%s", nm ? nm : "script.ds");
    open_result_ready = 1;
    pthread_mutex_unlock(&fs_fd_lock);
    if (nm)
        (*env)->ReleaseStringUTFChars(env, name, nm);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeSavedFd(JNIEnv *env, jobject thiz, jint fd) {
    (void)env;
    (void)thiz;
    pthread_mutex_lock(&fs_fd_lock);
    save_result_fd = (int)fd;
    save_result_ready = 1;
    pthread_mutex_unlock(&fs_fd_lock);
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
    /* Let the language's file builtins reach the SAF pickers. */
    ds_pick_open_hook = fs_open_dialog;
    ds_pick_image_hook = fs_image_dialog;
    ds_pick_save_hook = fs_save_dialog;
    ds_log("DiScript editor: C + Vulkan");
    for (;;) {
        struct android_poll_source *source = NULL;
        int ident;
        int drawing = app->window && init_done;
        int timeout = drawing ? 0 : (gfx_retry_at_ns ? 50 : 250);
        while ((ident = ALooper_pollOnce(timeout, NULL, NULL, (void **)&source)) >= 0) {
            if (source && source->process)
                source->process(app, source);
            if (app->destroyRequested) {
                init_done = 0;
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

        uint64_t frame_start = monotonic_ns();
        /* Reclaim per-frame pooled strings before the editor runs. */
        ds_string_pool_tick();
        if (prev_loop_ns && frame_start >= prev_loop_ns) {
            frame_seconds = (double)(frame_start - prev_loop_ns) / 1e9;
            ds_graphics_report_frame_interval(frame_seconds);
        }
        prev_loop_ns = frame_start;
        apply_screen_size();

        ide_init_once(app_assets);
        if (!ide_ready)
            continue;
        fs_drain_results();
        ide_app_update(&g_ide, (double)frame_start / 1e9);
        /* The IME covers the bottom of the screen: shrink the drawing area so
         * the caret never hides behind it. */
        int view_h = screen_h;
        if (kb_visible && kb_inset_px > 0 && kb_inset_px < screen_h)
            view_h = screen_h - kb_inset_px;

        g_ide.view.occluded_h = view_h;

        frame.pixels = NULL;
        frame.width = screen_w;
        frame.height = screen_h;
        frame.stride = screen_w;
        if (frame.width > 0 && frame.height > 0 && ds_graphics_begin_frame(&frame)) {
            /* Always draw the whole frame: the editor paints the keyboard-covered
             * strip itself. Passing a short height with a tall buffer left that
             * strip holding the previous frame's pixels, which read as stray
             * digits sitting over the keyboard. */
            ide_app_draw(&g_ide, frame.width, screen_h);
            ds_graphics_end_frame();
        }
    }
}
#include "engine/graphics.c"
#include "engine/network.c"
#include "engine/audio.c"
