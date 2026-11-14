#ifdef __ANDROID__
#include <android/native_activity.h>
#include <android/keycodes.h>
#include <jni.h>
#define KB_BUF 384
static ANativeActivity *kb_activity = NULL;
static char kb_text[KB_BUF] = {0};
static int kb_len = 0;
static int kb_show = 0;
static int kb_enter = 0;
static pthread_mutex_t kb_mutex = PTHREAD_MUTEX_INITIALIZER;
static BackspaceRepeat kb_del_repeat = {0, 0};
static pthread_mutex_t kb_repeat_mutex = PTHREAD_MUTEX_INITIALIZER;

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

static jstring kb_new_java_string(JNIEnv *env, const char *value) {
    const unsigned char *p = (const unsigned char *)(value ? value : "");
    jchar chars[KB_BUF];
    jsize n = 0;
    while (*p && n < KB_BUF - 1) {
        unsigned int cp;
        if (*p < 0x80)
            cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((p[0] & 7) << 18) | ((p[1] & 63) << 12) | ((p[2] & 63) << 6) | (p[3] & 63);
            p += 4;
        } else {
            cp = 0xFFFD;
            p++;
        }
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
            cp = 0xFFFD;
        if (cp < 0x10000)
            chars[n++] = (jchar)cp;
        else if (n + 2 <= KB_BUF - 1) {
            cp -= 0x10000;
            chars[n++] = (jchar)(0xD800 + (cp >> 10));
            chars[n++] = (jchar)(0xDC00 + (cp & 0x3FF));
        } else
            break;
    }
    return (*env)->NewString(env, chars, n);
}

static int kb_call_text_method(const char *name, const char *value) {
    int attached = 0, ok = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 8) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "(Ljava/lang/String;)V") : NULL;
    if (method) {
        jstring text = kb_new_java_string(env, value);
        if (text) {
            (*env)->CallVoidMethod(env, kb_activity->clazz, method, text);
            ok = !(*env)->ExceptionCheck(env);
        }
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return ok;
}

static int kb_call_bool_method(const char *name) {
    int attached = 0, result = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 4) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "()Z") : NULL;
    if (method) {
        jboolean value = (*env)->CallBooleanMethod(env, kb_activity->clazz, method);
        if (!(*env)->ExceptionCheck(env))
            result = value ? 1 : 0;
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return result;
}

static int kb_call_void_method(const char *name) {
    int attached = 0, ok = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 4) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "()V") : NULL;
    if (method) {
        (*env)->CallVoidMethod(env, kb_activity->clazz, method);
        ok = !(*env)->ExceptionCheck(env);
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return ok;
}

void ds_set_activity(void *act) { kb_activity = (ANativeActivity *)act; }

/* Wall clock for the New Year countdown: the Java bridge hands over the device
 * time in milliseconds, in seconds here. A missing bridge returns 0 and the
 * badge simply stays hidden. */
static long kb_call_long_method(const char *name) {
    int attached = 0;
    long result = 0;
    JNIEnv *env = kb_get_env(&attached);
    if (!env || !kb_activity || !kb_activity->clazz)
        return 0;
    if ((*env)->PushLocalFrame(env, 4) != 0) {
        kb_release_env(attached);
        return 0;
    }
    jclass cls = (*env)->GetObjectClass(env, kb_activity->clazz);
    jmethodID method = cls ? (*env)->GetMethodID(env, cls, name, "()J") : NULL;
    if (method) {
        jlong value = (*env)->CallLongMethod(env, kb_activity->clazz, method);
        if (!(*env)->ExceptionCheck(env))
            result = (long)value;
    }
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    kb_release_env(attached);
    return result;
}

long ds_wall_clock(void) {
    long ms = kb_call_long_method("gameWallClockMillis");
    if (ms <= 0) {
        return 0;
    }
    return ms / 1000L;
}

static void kb_sync_editor(void) {
    char copy[KB_BUF];
    pthread_mutex_lock(&kb_mutex);
    snprintf(copy, sizeof(copy), "%s", kb_text);
    pthread_mutex_unlock(&kb_mutex);
    (void)kb_call_text_method("setClipboardText", copy);
}

int keyboard_uses_editor(void) { return kb_call_bool_method("isCodeKeyboardVisible"); }

void keyboard_show(void) {
    char current[KB_BUF];
    pthread_mutex_lock(&kb_mutex);
    snprintf(current, sizeof(current), "%s", kb_text);
    kb_show = 1;
    pthread_mutex_unlock(&kb_mutex);
    (void)current;
    if (!kb_call_void_method("showCodeKeyboard") && kb_activity)
        ANativeActivity_showSoftInput(kb_activity, ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);
}

void keyboard_hide(void) {
    pthread_mutex_lock(&kb_mutex);
    kb_show = 0;
    pthread_mutex_unlock(&kb_mutex);
    keyboard_cancel_repeat();
    if (!kb_call_void_method("hideCodeKeyboard") && kb_activity)
        ANativeActivity_hideSoftInput(kb_activity, ANATIVEACTIVITY_HIDE_SOFT_INPUT_IMPLICIT_ONLY);
}

/* Copy active IME text into caller-owned buffer. */
void keyboard_get_text_to(char *dst, size_t cap) {
    if (!dst || cap == 0) {
        return;
    }
    pthread_mutex_lock(&kb_mutex);
    snprintf(dst, cap, "%s", kb_text);
    pthread_mutex_unlock(&kb_mutex);
}

void keyboard_clear(void) {
    pthread_mutex_lock(&kb_mutex);
    kb_text[0] = '\0';
    kb_len = 0;
    kb_enter = 0;
    pthread_mutex_unlock(&kb_mutex);
    keyboard_cancel_repeat();
    (void)kb_call_text_method("setGameKeyboardText", "");
}

int keyboard_visible(void) {
    int visible;
    pthread_mutex_lock(&kb_mutex);
    visible = kb_show;
    pthread_mutex_unlock(&kb_mutex);
    return visible;
}

int keyboard_enter_pressed(void) {
    int enter;
    pthread_mutex_lock(&kb_mutex);
    enter = kb_enter;
    kb_enter = 0;
    pthread_mutex_unlock(&kb_mutex);
    return enter;
}

static void kb_append_cp_locked(unsigned int cp) {
    char u[5];
    int n = 0;
    if (cp < 0x80) {
        u[0] = (char)cp;
        n = 1;
    } else if (cp < 0x800) {
        u[0] = (char)(0xC0 | (cp >> 6));
        u[1] = (char)(0x80 | (cp & 0x3F));
        n = 2;
    } else if (cp < 0x10000) {
        u[0] = (char)(0xE0 | (cp >> 12));
        u[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[2] = (char)(0x80 | (cp & 0x3F));
        n = 3;
    } else if (cp <= 0x10FFFF) {
        u[0] = (char)(0xF0 | (cp >> 18));
        u[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[3] = (char)(0x80 | (cp & 0x3F));
        n = 4;
    }
    if (n > 0 && kb_len + n < KB_BUF) {
        memcpy(kb_text + kb_len, u, (size_t)n);
        kb_len += n;
        kb_text[kb_len] = '\0';
    }
}

void keyboard_type(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!p)
        return;
    pthread_mutex_lock(&kb_mutex);
    while (*p) {
        unsigned int cp;
        if (*p < 0x80)
            cp = *p++;
        else if ((*p & 0xE0) == 0xC0 && (p[1] & 0xC0) == 0x80) {
            cp = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80) {
            cp = ((p[0] & 15) << 12) | ((p[1] & 63) << 6) | (p[2] & 63);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && (p[1] & 0xC0) == 0x80 && (p[2] & 0xC0) == 0x80 && (p[3] & 0xC0) == 0x80) {
            cp = ((p[0] & 7) << 18) | ((p[1] & 63) << 12) | ((p[2] & 63) << 6) | (p[3] & 63);
            p += 4;
        } else {
            p++;
            continue;
        }
        if (cp == '\n' || cp == '\r')
            kb_enter = 1;
        else if (cp >= 0x20)
            kb_append_cp_locked(cp);
    }
    pthread_mutex_unlock(&kb_mutex);
    kb_sync_editor();
}

/* Delete exactly one trailing UTF-8 codepoint; never clear a whole streak. */
void keyboard_backspace(void) {
    pthread_mutex_lock(&kb_mutex);
    while (kb_len > 0) {
        unsigned char c = (unsigned char)kb_text[--kb_len];
        kb_text[kb_len] = '\0';
        if ((c & 0xC0) != 0x80)
            break;
    }
    pthread_mutex_unlock(&kb_mutex);
    kb_sync_editor();
}

static void kb_erase_one_cb(void *userdata) {
    (void)userdata;
    keyboard_backspace();
}

void keyboard_cancel_repeat(void) {
    pthread_mutex_lock(&kb_repeat_mutex);
    backspace_repeat_up(&kb_del_repeat);
    pthread_mutex_unlock(&kb_repeat_mutex);
}

/* Called once per Android game loop with CLOCK_MONOTONIC milliseconds. */
void keyboard_tick(uint64_t now_ms) {
    int show;
    pthread_mutex_lock(&kb_mutex);
    show = kb_show;
    pthread_mutex_unlock(&kb_mutex);
    pthread_mutex_lock(&kb_repeat_mutex);
    if (!show) {
        backspace_repeat_up(&kb_del_repeat);
    } else {
        backspace_repeat_tick(&kb_del_repeat, now_ms, kb_erase_one_cb, NULL);
    }
    pthread_mutex_unlock(&kb_repeat_mutex);
}

static unsigned int kb_unicode(int keycode, int meta) {
    JNIEnv *env = NULL;
    JavaVM *vm;
    jclass cls;
    jmethodID ctor, get_uni;
    jobject ev;
    jint uni = 0;
    int attached = 0;
    if (!kb_activity || !kb_activity->vm)
        return 0;
    vm = kb_activity->vm;
    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if ((*vm)->AttachCurrentThread(vm, &env, NULL) != JNI_OK)
            return 0;
        attached = 1;
    }
    if ((*env)->PushLocalFrame(env, 8) != 0) {
        if (attached)
            (*vm)->DetachCurrentThread(vm);
        return 0;
    }
    cls = (*env)->FindClass(env, "android/view/KeyEvent");
    if (!cls)
        goto done;
    ctor = (*env)->GetMethodID(env, cls, "<init>", "(II)V");
    get_uni = (*env)->GetMethodID(env, cls, "getUnicodeChar", "(I)I");
    if (!ctor || !get_uni)
        goto done;
    ev = (*env)->NewObject(env, cls, ctor, (jint)0, (jint)keycode);
    if (!ev || (*env)->ExceptionCheck(env))
        goto done;
    uni = (*env)->CallIntMethod(env, ev, get_uni, (jint)meta);
    if ((*env)->ExceptionCheck(env))
        uni = 0;
done:
    if ((*env)->ExceptionCheck(env))
        (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    if (attached)
        (*vm)->DetachCurrentThread(vm);
    return uni > 0 ? (unsigned int)uni : 0;
}

int keyboard_handle_key(int keycode, int action, int meta) {
    if (keycode == AKEYCODE_DEL || keycode == AKEYCODE_FORWARD_DEL) {
        struct timespec now;
        uint64_t now_ms = 0;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == 0)
            now_ms = (uint64_t)now.tv_sec * 1000ull + (uint64_t)now.tv_nsec / 1000000ull;
        pthread_mutex_lock(&kb_repeat_mutex);
        if (action == AKEY_EVENT_ACTION_DOWN) {
            backspace_repeat_down(&kb_del_repeat, now_ms, kb_erase_one_cb, NULL);
        } else if (action == AKEY_EVENT_ACTION_UP) {
            backspace_repeat_up(&kb_del_repeat);
        }
        pthread_mutex_unlock(&kb_repeat_mutex);
        return 1;
    }
    if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_MULTIPLE) {
        return 0;
    }
    if (keycode == AKEYCODE_ENTER || keycode == AKEYCODE_NUMPAD_ENTER || keycode == AKEYCODE_DPAD_CENTER) {
        pthread_mutex_lock(&kb_mutex);
        kb_enter = 1;
        pthread_mutex_unlock(&kb_mutex);
        return 1;
    }
    if (keycode == AKEYCODE_SPACE) {
        pthread_mutex_lock(&kb_mutex);
        kb_append_cp_locked(' ');
        pthread_mutex_unlock(&kb_mutex);
        kb_sync_editor();
        return 1;
    }
    {
        unsigned int cp = kb_unicode(keycode, meta);
        if (cp >= 0x20 && cp != 0x7F) {
            pthread_mutex_lock(&kb_mutex);
            kb_append_cp_locked(cp);
            pthread_mutex_unlock(&kb_mutex);
            kb_sync_editor();
            return 1;
        }
    }
    if (keycode >= AKEYCODE_A && keycode <= AKEYCODE_Z) {
        pthread_mutex_lock(&kb_mutex);
        kb_append_cp_locked((unsigned int)('a' + keycode - AKEYCODE_A));
        pthread_mutex_unlock(&kb_mutex);
        kb_sync_editor();
        return 1;
    }
    if (keycode >= AKEYCODE_0 && keycode <= AKEYCODE_9) {
        pthread_mutex_lock(&kb_mutex);
        kb_append_cp_locked((unsigned int)('0' + keycode - AKEYCODE_0));
        pthread_mutex_unlock(&kb_mutex);
        kb_sync_editor();
        return 1;
    }
    if (keycode == AKEYCODE_COMMA || keycode == AKEYCODE_PERIOD || keycode == AKEYCODE_MINUS) {
        unsigned int cp = keycode == AKEYCODE_COMMA ? ',' : keycode == AKEYCODE_PERIOD ? '.' : '-';
        pthread_mutex_lock(&kb_mutex);
        kb_append_cp_locked(cp);
        pthread_mutex_unlock(&kb_mutex);
        kb_sync_editor();
        return 1;
    }
    return 0;
}





#endif
