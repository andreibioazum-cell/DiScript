/* Android JNI keyboard, clipboard, and SAF document picker bridge. */
#include <pthread.h>
#include <unistd.h>

static char g_clip_fallback[65536] = {0};
static pthread_mutex_t g_clip_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int g_kb_visible = 0;
static volatile int g_kb_inset_px = 0;

static pthread_mutex_t g_saf_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_saf_fd = -1;
static int g_saf_kind = 0;
static char g_saf_name[256] = {0};

static void saf_store_fd(int kind, int fd, const char *name) {
    int stale = -1;
    pthread_mutex_lock(&g_saf_lock);
    stale = g_saf_fd;
    g_saf_fd = fd;
    g_saf_kind = kind;
    snprintf(g_saf_name, sizeof(g_saf_name), "%s", (name && *name) ? name : "script.ds");
    pthread_mutex_unlock(&g_saf_lock);
    if (stale >= 0 && stale != fd)
        close(stale);
}

int ds_saf_poll_fd(int *out_kind, char *out_name, size_t name_cap) {
    int fd;
    pthread_mutex_lock(&g_saf_lock);
    fd = g_saf_fd;
    if (fd >= 0) {
        if (out_kind)
            *out_kind = g_saf_kind;
        if (out_name && name_cap > 0)
            snprintf(out_name, name_cap, "%s", g_saf_name);
        g_saf_fd = -1;
        g_saf_kind = 0;
    }
    pthread_mutex_unlock(&g_saf_lock);
    return fd;
}

#ifdef __ANDROID__
#include <android/native_activity.h>
#include <android/keycodes.h>
#include <jni.h>

static ANativeActivity *kb_activity = NULL;
static BackspaceRepeat kb_del_repeat = {0, 0};
static pthread_mutex_t kb_repeat_mutex = PTHREAD_MUTEX_INITIALIZER;

void ds_set_activity(void *act) { kb_activity = (ANativeActivity *)act; }

static JNIEnv *kb_get_env(int *attached) {
    JNIEnv *env = NULL;
    JavaVM *vm;
    if (attached)
        *attached = 0;
    if (!kb_activity || !kb_activity->vm)
        return NULL;
    vm = kb_activity->vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
            return NULL;
        if (attached)
            *attached = 1;
    }
    return env;
}

static void kb_release_env(int attached) {
    if (attached && kb_activity && kb_activity->vm)
        (*kb_activity->vm)->DetachCurrentThread(kb_activity->vm);
}

static jstring jni_utf8_to_jstring(JNIEnv *env, const char *utf8) {
    const unsigned char *p = (const unsigned char *)(utf8 ? utf8 : "");
    size_t byte_len = strlen((const char *)p);
    jchar *chars = (jchar *)malloc((byte_len + 2) * sizeof(jchar));
    if (!chars)
        return (*env)->NewStringUTF(env, "");
    jsize n = 0;
    while (*p) {
        unsigned int cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
            p += 4;
        } else {
            cp = 0xFFFD;
            p++;
        }
        if (cp < 0x10000) {
            chars[n++] = (jchar)cp;
        } else if (cp <= 0x10FFFF) {
            cp -= 0x10000;
            chars[n++] = (jchar)(0xD800 + (cp >> 10));
            chars[n++] = (jchar)(0xDC00 + (cp & 0x3FF));
        }
    }
    jstring res = (*env)->NewString(env, chars, n);
    free(chars);
    return res;
}

static size_t jni_jstring_to_utf8(JNIEnv *env, jstring jstr, char *dst, size_t cap) {
    if (!dst || cap == 0)
        return 0;
    dst[0] = '\0';
    if (!jstr)
        return 0;
    jsize count = (*env)->GetStringLength(env, jstr);
    const jchar *chars = (*env)->GetStringChars(env, jstr, NULL);
    if (!chars)
        return 0;
    size_t w = 0;
    for (jsize i = 0; i < count; i++) {
        unsigned int cp = chars[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < count && chars[i + 1] >= 0xDC00 && chars[i + 1] <= 0xDFFF) {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (chars[++i] - 0xDC00);
        }
        char u[4];
        size_t un = 0;
        if (cp < 0x80) {
            u[0] = (char)cp;
            un = 1;
        } else if (cp < 0x800) {
            u[0] = (char)(0xC0 | (cp >> 6));
            u[1] = (char)(0x80 | (cp & 0x3F));
            un = 2;
        } else if (cp < 0x10000) {
            u[0] = (char)(0xE0 | (cp >> 12));
            u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
            u[2] = (char)(0x80 | (cp & 0x3F));
            un = 3;
        } else if (cp <= 0x10FFFF) {
            u[0] = (char)(0xF0 | (cp >> 18));
            u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
            u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
            u[3] = (char)(0x80 | (cp & 0x3F));
            un = 4;
        }
        if (w + un >= cap)
            break;
        memcpy(dst + w, u, un);
        w += un;
    }
    dst[w] = '\0';
    (*env)->ReleaseStringChars(env, jstr, chars);
    return w;
}

static int kb_call_void(const char *name) {
    int attached = 0, ok = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 4) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID mid = cls ? (*env)->GetMethodID(env, cls, name, "()V") : NULL;
    if (mid) {
        (*env)->CallVoidMethod(env, kb_activity->clazz, mid);
        ok = !(*env)->ExceptionCheck(env);
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return ok;
}

static int kb_call_string(const char *name, const char *arg) {
    int attached = 0, ok = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 6) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID mid = cls ? (*env)->GetMethodID(env, cls, name, "(Ljava/lang/String;)V") : NULL;
    if (mid) {
        jstring jstr = jni_utf8_to_jstring(env, arg ? arg : "");
        if (jstr) {
            (*env)->CallVoidMethod(env, kb_activity->clazz, mid, jstr);
            ok = !(*env)->ExceptionCheck(env);
        }
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return ok;
}

long ds_wall_clock(void) {
    int attached = 0;
    long res = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return (long)time(NULL);
    if ((*env)->PushLocalFrame(env, 4) == 0) {
        jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
        jmethodID mid = cls ? (*env)->GetMethodID(env, cls, "wallClockMillis", "()J") : NULL;
        if (mid) {
            jlong v = (*env)->CallLongMethod(env, kb_activity->clazz, mid);
            if (!(*env)->ExceptionCheck(env) && v > 0)
                res = (long)(v / 1000L);
        }
        if ((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    kb_release_env(attached);
    return res > 0 ? res : (long)time(NULL);
}

void keyboard_show(void) {
    g_kb_visible = 1;
    if (!kb_call_void("showCodeKeyboard") && kb_activity)
        ANativeActivity_showSoftInput(kb_activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
}

void keyboard_hide(void) {
    g_kb_visible = 0;
    g_kb_inset_px = 0;
    keyboard_cancel_repeat();
    if (!kb_call_void("hideCodeKeyboard") && kb_activity)
        ANativeActivity_hideSoftInput(kb_activity, ANATIVEACTIVITY_HIDE_SOFT_INPUT_IMPLICIT_ONLY);
}

int keyboard_visible(void) { return g_kb_visible; }
int ds_keyboard_inset_px(void) { return g_kb_inset_px; }

void ds_clipboard_set(const char *utf8) {
    pthread_mutex_lock(&g_clip_lock);
    snprintf(g_clip_fallback, sizeof(g_clip_fallback), "%s", utf8 ? utf8 : "");
    pthread_mutex_unlock(&g_clip_lock);
    (void)kb_call_string("setClipboardText", utf8 ? utf8 : "");
}

void ds_clipboard_get(char *dst, size_t cap) {
    if (!dst || cap == 0)
        return;
    dst[0] = '\0';
    int attached = 0;
    int got_java = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (env && kb_activity && kb_activity->clazz && (*env)->PushLocalFrame(env, 6) == 0) {
        jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
        jmethodID mid = cls ? (*env)->GetMethodID(env, cls, "getClipboardText", "()Ljava/lang/String;") : NULL;
        if (mid) {
            jstring jstr = (jstring)(*env)->CallObjectMethod(env, kb_activity->clazz, mid);
            if (!(*env)->ExceptionCheck(env) && jstr) {
                if (jni_jstring_to_utf8(env, jstr, dst, cap) > 0)
                    got_java = 1;
            }
        }
        if ((*env)->ExceptionCheck(env))
            (*env)->ExceptionClear(env);
        (*env)->PopLocalFrame(env, NULL);
    }
    kb_release_env(attached);
    if (!got_java) {
        pthread_mutex_lock(&g_clip_lock);
        snprintf(dst, cap, "%s", g_clip_fallback);
        pthread_mutex_unlock(&g_clip_lock);
    }
}

void ds_saf_request_open(void) { (void)kb_call_void("pickScriptFile"); }
void ds_saf_request_save(const char *suggested_name) {
    (void)kb_call_string("saveScriptFile", suggested_name ? suggested_name : "script.ds");
}

static void kb_erase_one_cb(void *userdata) {
    (void)userdata;
    ds_editor_push_backspace(1);
}

void keyboard_cancel_repeat(void) {
    pthread_mutex_lock(&kb_repeat_mutex);
    backspace_repeat_up(&kb_del_repeat);
    pthread_mutex_unlock(&kb_repeat_mutex);
}

void keyboard_tick(uint64_t now_ms) {
    pthread_mutex_lock(&kb_repeat_mutex);
    if (!g_kb_visible)
        backspace_repeat_up(&kb_del_repeat);
    else
        backspace_repeat_tick(&kb_del_repeat, now_ms, kb_erase_one_cb, NULL);
    pthread_mutex_unlock(&kb_repeat_mutex);
}

int keyboard_handle_key(int keycode, int action, int meta) {
    if (keycode == AKEYCODE_DEL) {
        uint64_t now_ms = ds_monotonic_ns() / 1000000ull;
        pthread_mutex_lock(&kb_repeat_mutex);
        if (action == AKEY_EVENT_ACTION_DOWN)
            backspace_repeat_down(&kb_del_repeat, now_ms, kb_erase_one_cb, NULL);
        else if (action == AKEY_EVENT_ACTION_UP)
            backspace_repeat_up(&kb_del_repeat);
        pthread_mutex_unlock(&kb_repeat_mutex);
        return 1;
    }
    if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_MULTIPLE)
        return 0;
    if (keycode == AKEYCODE_ENTER || keycode == AKEYCODE_NUMPAD_ENTER) {
        ds_editor_push_enter();
        return 1;
    }
    int mapped_meta = 0;
    if (meta & AMETA_SHIFT_ON)
        mapped_meta |= DS_META_SHIFT;
    if (meta & AMETA_ALT_ON)
        mapped_meta |= DS_META_ALT;
    if (meta & AMETA_CTRL_ON)
        mapped_meta |= DS_META_CTRL;
    ds_editor_push_key(keycode, mapped_meta);
    return 1;
}

/* ---- JNI Entry Points from com.discript.ide.DiScriptActivity ------------ */
JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorText(
    JNIEnv *env, jobject self, jstring text) {
    (void)self;
    if (!text)
        return;
    jsize len = (*env)->GetStringLength(env, text);
    size_t cap = (size_t)len * 4 + 8;
    char *buf = (char *)malloc(cap);
    if (!buf)
        return;
    if (jni_jstring_to_utf8(env, text, buf, cap) > 0)
        ds_editor_push_text(buf);
    free(buf);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorBackspace(
    JNIEnv *env, jobject self, jint count) {
    (void)env;
    (void)self;
    if (count > 0)
        ds_editor_push_backspace((int)count);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorEnter(
    JNIEnv *env, jobject self) {
    (void)env;
    (void)self;
    ds_editor_push_enter();
}

JNIEXPORT jboolean JNICALL Java_com_discript_ide_DiScriptActivity_nativeEditorSpecialKey(
    JNIEnv *env, jobject self, jint keyCode, jint metaState) {
    (void)env;
    (void)self;
    int mapped_meta = 0;
    if (metaState & AMETA_SHIFT_ON)
        mapped_meta |= DS_META_SHIFT;
    if (metaState & AMETA_ALT_ON)
        mapped_meta |= DS_META_ALT;
    if (metaState & AMETA_CTRL_ON)
        mapped_meta |= DS_META_CTRL;
    ds_editor_push_key((int)keyCode, mapped_meta);
    return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeKeyboardInset(
    JNIEnv *env, jobject self, jint bottomInsetPx, jint visible) {
    (void)env;
    (void)self;
    g_kb_inset_px = bottomInsetPx > 0 ? (int)bottomInsetPx : 0;
    g_kb_visible = visible ? 1 : 0;
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativePickedFd(
    JNIEnv *env, jobject self, jint fd, jstring name) {
    (void)self;
    char fname[256] = "imported.ds";
    if (name)
        jni_jstring_to_utf8(env, name, fname, sizeof(fname));
    saf_store_fd(1, (int)fd, fname);
}

JNIEXPORT void JNICALL Java_com_discript_ide_DiScriptActivity_nativeSavedFd(
    JNIEnv *env, jobject self, jint fd) {
    (void)env;
    (void)self;
    saf_store_fd(2, (int)fd, "saved.ds");
}

#else
/* Host test stubs */
void ds_set_activity(void *act) { (void)act; }
long ds_wall_clock(void) { return (long)time(NULL); }
void keyboard_show(void) { g_kb_visible = 1; }
void keyboard_hide(void) { g_kb_visible = 0; g_kb_inset_px = 0; }
int keyboard_visible(void) { return g_kb_visible; }
int ds_keyboard_inset_px(void) { return g_kb_inset_px; }
void keyboard_cancel_repeat(void) {}
void keyboard_tick(uint64_t now_ms) { (void)now_ms; }
int keyboard_handle_key(int keycode, int action, int meta) {
    if (action == 0)
        ds_editor_push_key(keycode, meta);
    return 1;
}
void ds_clipboard_set(const char *utf8) {
    pthread_mutex_lock(&g_clip_lock);
    snprintf(g_clip_fallback, sizeof(g_clip_fallback), "%s", utf8 ? utf8 : "");
    pthread_mutex_unlock(&g_clip_lock);
}
void ds_clipboard_get(char *dst, size_t cap) {
    if (!dst || cap == 0)
        return;
    pthread_mutex_lock(&g_clip_lock);
    snprintf(dst, cap, "%s", g_clip_fallback);
    pthread_mutex_unlock(&g_clip_lock);
}
void ds_saf_request_open(void) {}
void ds_saf_request_save(const char *suggested_name) { (void)suggested_name; }
#endif
