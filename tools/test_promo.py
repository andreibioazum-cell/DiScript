#!/usr/bin/env python3
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

ANDROID_LOG_H = """
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
int __android_log_print(int prio, const char *tag, const char *fmt, ...);
"""

JNI_H = """
#ifndef STUB_JNI_H
#define STUB_JNI_H
typedef int jint; typedef unsigned char jbyte; typedef int jsize; typedef int jboolean;
#define JNI_OK 0
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNI_VERSION_1_6 0x00010006
typedef void *jobject; typedef void *jclass; typedef void *jstring;
typedef void *jbyteArray; typedef void *jthrowable; typedef void *jmethodID;
struct JNINativeInterface {
    void *reserved0;
    jclass (*FindClass)(void *env, const char *name);
    void *(*NewGlobalRef)(void *env, void *obj);
    jobject (*NewObject)(void *env, jclass cls, jmethodID id, ...);
    jclass (*GetObjectClass)(void *env, jobject obj);
    jmethodID (*GetMethodID)(void *env, jclass cls, const char *name, const char *sig);
    jobject (*CallObjectMethod)(void *env, jobject obj, jmethodID id, ...);
    jint (*CallIntMethod)(void *env, jobject obj, jmethodID id, ...);
    void (*CallVoidMethod)(void *env, jobject obj, jmethodID id, ...);
    jstring (*NewStringUTF)(void *env, const char *bytes);
    const char *(*GetStringUTFChars)(void *env, jstring s, jboolean *is_copy);
    void (*ReleaseStringUTFChars)(void *env, jstring s, const char *bytes);
    jbyteArray (*NewByteArray)(void *env, jsize len);
    void (*SetByteArrayRegion)(void *env, jbyteArray a, jsize start, jsize len, const jbyte *buf);
    void (*GetByteArrayRegion)(void *env, jbyteArray a, jsize start, jsize len, jbyte *buf);
    void (*ExceptionClear)(void *env);
    jboolean (*ExceptionCheck)(void *env);
    jthrowable (*ExceptionOccurred)(void *env);
    int (*PushLocalFrame)(void *env, jint capacity);
    jobject (*PopLocalFrame)(void *env, jobject result);
};
struct JNIInvokeInterface {
    void *reserved0;
    int (*AttachCurrentThread)(void *vm, void *env, void *args);
    int (*DetachCurrentThread)(void *vm);
    int (*GetEnv)(void *vm, void *env, int version);
};
typedef const struct JNINativeInterface *JNIEnv;
typedef const struct JNIInvokeInterface *JavaVM;
#endif
"""

HARNESS = r'''#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "engine/network.c"

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt;
    return 0;
}
void ds_console_log(int is_error, const char *format, ...) { (void)is_error; (void)format; }

/* Host tests for fixed promo code 2027 persistence and legacy promo.dat migration. */

static int run_write(const char *dir) {
    net_set_data_path(dir);
    assert(net_promo_2027_used() == 0);
    net_promo_2027_mark_used();
    assert(net_promo_2027_used() == 1);
    puts("promo write: fresh device starts unused, mark sticks");
    return 0;
}

static int run_read(const char *dir) {
    net_set_data_path(dir);
    assert(net_promo_2027_used() == 1);
    puts("promo read: redeemed flag survives a restart");
    return 0;
}

static int run_legacy(const char *dir) {
    net_set_data_path(dir);
    assert(net_promo_2027_used() == 0);
    net_promo_2027_mark_used();
    assert(net_promo_2027_used() == 1);
    puts("promo legacy: card-era promo.dat ignored, 2027 still redeemable");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: test <write|read|legacy> <dir>\n"); return 2; }
    if (strcmp(argv[1], "write") == 0) return run_write(argv[2]);
    if (strcmp(argv[1], "read") == 0) return run_read(argv[2]);
    if (strcmp(argv[1], "legacy") == 0) return run_legacy(argv[2]);
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix="cubic-promo-") as directory:
        temp = Path(directory)
        android = temp / "android"
        android.mkdir()
        (android / "log.h").write_text(ANDROID_LOG_H)
        (android / "asset_manager.h").write_text(
            "typedef struct AAssetManager AAssetManager;\n")
        (android / "native_window.h").write_text("typedef struct ANativeWindow ANativeWindow;\n")
        (temp / "jni.h").write_text(JNI_H)
        (temp / "test.c").write_text(HARNESS)
        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O0",
            "-D_POSIX_C_SOURCE=200809L", "-D__ANDROID__",
            "-Werror=implicit-function-declaration",
            "-I", str(temp), "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(temp / "test"),
        ], check=True)

        data = temp / "data"
        data.mkdir()
        run = [str(temp / "test")]
        subprocess.run([*run, "write", str(data)], check=True)
        saved = (data / "promo.dat").read_text(encoding="utf-8")
        assert "used2027 1" in saved, saved
        subprocess.run([*run, "read", str(data)], check=True)

        old = temp / "old-version"
        old.mkdir()
        (old / "promo.dat").write_text(
            "used 1\nstreak 7\ncard_found 1\ncode K7QX2M9P4RTZ8H3N\n", encoding="utf-8")
        subprocess.run([*run, "legacy", str(old)], check=True)
        migrated = (old / "promo.dat").read_text(encoding="utf-8")
        assert "used2027 1" in migrated, migrated
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
