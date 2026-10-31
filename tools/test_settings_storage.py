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

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "engine/network.c"

int __android_log_print(int prio, const char *tag, const char *fmt, ...) {
    (void)prio; (void)tag; (void)fmt;
    return 0;
}
void ds_console_log(int is_error, const char *format, ...) { (void)is_error; (void)format; }

static const char *CLOUD_PROFILE =
    "{\"nick\":\"tester\",\"cups\":10,\"candies\":5,\"cls\":3,\"azum\":1,\"santa\":0,"
    "\"ebuc\":1,\"level\":1,\"levels\":1,\"lang\":1,\"hitboxes\":0,\"musicvol\":40,\"mods_n\":2,"
    "\"mods\":\"winter.zip|my mod.zip\"}";

static int run_write(const char *dir) {
    net_set_data_path(dir);
    net_save_settings(1, 0);
    assert(net_load_language() == 1);
    assert(net_load_hitboxes() == 0);

    net_save_music_volume(35);
    assert(net_load_music_volume() == 35);
    net_save_music_volume(150);
    assert(net_load_music_volume() == 100);
    net_save_music_volume(35);
    assert(net_load_music_volume() == 35);

    assert(settings_sync_with_cloud(CLOUD_PROFILE) == 1);
    assert(net_load_language() == 1);
    assert(net_load_hitboxes() == 0);
    assert(net_load_music_volume() == 35);

    assert(net_load_new_theme() == 1);
    net_save_new_theme(0);
    assert(net_load_new_theme() == 0);
    assert(net_load_theme_mode() == 0);   /* ordinary theme by default */
    net_save_theme_mode(2);
    assert(net_load_theme_mode() == 2);   /* Halloween survives the save */
    assert(net_load_theme_chosen() == 0);
    net_save_theme_chosen(1);            /* the theme button was pressed */
    assert(net_load_theme_chosen() == 1);
    assert(net_load_language() == 1);
    puts("native write: settings.dat created, local wins over cloud");
    return 0;
}

static int run_read(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_language() == 1);
    assert(net_load_hitboxes() == 0);
    assert(net_load_music_volume() == 35);
    assert(net_load_new_theme() == 0);
    assert(net_load_theme_mode() == 2);
    assert(net_load_theme_chosen() == 1);
    puts("native read: a fresh process restores language, hitboxes, music volume and theme");
    return 0;
}

static int run_cloud(const char *dir) {
    net_set_data_path(dir);

    assert(settings_sync_with_cloud(CLOUD_PROFILE) == 0);
    assert(net_load_language() == 1);
    assert(net_load_hitboxes() == 0);
    assert(net_load_music_volume() == 40);

    puts("native cloud: profile settings adopted on a device without settings.dat");
    return 0;
}

static int run_legacy(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_language() == 1);
    assert(net_load_hitboxes() == 0);

    assert(net_load_music_volume() == 70);
    assert(net_load_new_theme() == 1); /* files from before the toggle: new theme */
    net_save_settings(0, 1);
    assert(net_load_language() == 0);
    assert(net_load_hitboxes() == 1);
    puts("native legacy: an old mod list is ignored and dropped on the next save");
    return 0;
}

/* A save from the previous version has a style line but no chosen flag:
 * it already counts as an explicit theme choice. */
static int run_old_style(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_theme_mode() == 2);
    assert(net_load_theme_chosen() == 1);
    puts("native old style: a saved theme counts as an explicit choice");
    return 0;
}

static int run_legal(const char *dir) {
    net_set_data_path(dir);
    assert(settings_legal_ts() == 0);
    settings_mark_legal();
    assert(settings_legal_ts() > 0);
    puts("native legal: consent stamp written to settings.dat");
    return 0;
}

static int run_legal_read(const char *dir) {
    net_set_data_path(dir);
    assert(settings_legal_ts() > 0);
    puts("native legal read: a fresh process still sees the consent stamp");
    return 0;
}

static int run_fresh(const char *dir) {
    net_set_data_path(dir);
    assert(net_load_language() == 1); /* phone language unknown: Russian (168-FZ) */
    assert(settings_pd_consent_version() == 0); /* no consent until the player ticks the box */
    puts("native fresh: Russian when the phone language is unknown, no data consent until it is given");
    return 0;
}

/* A new install follows the phone: Russian phone - Russian game, any other - English.
 * A language already saved in settings.dat wins over the phone. */
static int run_phone(const char *dir, const char *code, int expected) {
    net_set_system_language(code);
    net_set_data_path(dir);
    assert(net_load_language() == expected);
    printf("native phone: phone language '%s' -> game language %d\n", code, expected);
    return 0;
}

static int run_consent(const char *dir) {
    net_set_data_path(dir);
    settings_set_pd_consent(1);
    assert(settings_pd_consent_version() == 1);
    puts("native consent: personal data consent stored with its version and time");
    return 0;
}

static void fake_session(const char *nick, const char *pass) {
    lg_lock();
    snprintf(lg.session_nick, sizeof(lg.session_nick), "%s", nick);
    snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", pass);
    lg.status = NET_LOGIN_OK;
    lg_unlock();
    session_save(nick, pass);
}

static double wait_delete(void) {
    int spins = 0;
    double st;
    while ((st = net_delete_account_status()) == 1 && spins++ < 500)
        sleep_ms(10);
    return st;
}

static double wait_password(void) {
    int spins = 0;
    double st;
    while ((st = net_change_password_status()) == 1 && spins++ < 500)
        sleep_ms(10);
    return st;
}

static int file_exists(const char *dir, const char *name) {
    char path[512];
    FILE *f;
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    f = fopen(path, "r");
    if (f)
        fclose(f);
    return f != NULL;
}

static int run_delete(const char *dir) {
    double st;
    net_set_data_path(dir);
    assert(settings_pd_consent_version() == 1);
    fake_session("tester", "secret1");
    /* A wrong password deletes nothing. */
    net_delete_account("secret2");
    assert(wait_delete() == 4);
    net_delete_account("");
    assert(wait_delete() == 4);
    assert(file_exists(dir, "auth.dat") && file_exists(dir, "progress.dat"));
    assert(settings_pd_consent_version() == 1);
    net_delete_account("secret1");
    st = wait_delete();
    assert(st == 2);
    assert(settings_pd_consent_version() == 0);
    assert(net_load_language() == 0); /* the player's own settings stay */
    puts("native delete: wrong password refused; with the right one account files wiped, consent withdrawn, settings kept");
    return 0;
}

static int run_password(const char *dir) {
    char text[128] = "";
    FILE *f;
    net_set_data_path(dir);
    net_change_password("secret1", "newpass2");
    assert(wait_password() == 6); /* not signed in */
    fake_session("tester", "secret1");
    net_change_password("secret9", "newpass2");
    assert(wait_password() == 4); /* wrong current password */
    net_change_password("secret1", "new pass");
    assert(wait_password() == 5); /* spaces are not allowed */
    net_change_password("secret1", "");
    assert(wait_password() == 5);
    net_change_password("secret1", "newpass2");
    assert(wait_password() == 2);
    assert(strcmp(net_login_pass(), "newpass2") == 0);
    {
        char path[512];
        snprintf(path, sizeof(path), "%s/auth.dat", dir);
        f = fopen(path, "r");
        assert(f);
        assert(fgets(text, sizeof(text), f));
        fclose(f);
    }
    assert(strcmp(text, "tester newpass2\n") == 0); /* autologin keeps working */
    net_change_password("secret1", "other3");
    assert(wait_password() == 4); /* the old password no longer works */
    puts("native password: current password checked, new one validated, session and auth.dat moved over");
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: test <write|read|cloud> <dir>\n"); return 2; }
    if (strcmp(argv[1], "write") == 0) return run_write(argv[2]);
    if (strcmp(argv[1], "read") == 0) return run_read(argv[2]);
    if (strcmp(argv[1], "cloud") == 0) return run_cloud(argv[2]);
    if (strcmp(argv[1], "legacy") == 0) return run_legacy(argv[2]);
    if (strcmp(argv[1], "legal") == 0) return run_legal(argv[2]);
    if (strcmp(argv[1], "legal-read") == 0) return run_legal_read(argv[2]);
    if (strcmp(argv[1], "fresh") == 0) return run_fresh(argv[2]);
    if (strcmp(argv[1], "phone") == 0 && argc >= 5) return run_phone(argv[2], argv[3], atoi(argv[4]));
    if (strcmp(argv[1], "consent") == 0) return run_consent(argv[2]);
    if (strcmp(argv[1], "delete") == 0) return run_delete(argv[2]);
    if (strcmp(argv[1], "oldstyle") == 0) return run_old_style(argv[2]);
    if (strcmp(argv[1], "password") == 0) return run_password(argv[2]);
    fprintf(stderr, "unknown mode '%s'\n", argv[1]);
    return 2;
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix="cubic-settings-") as directory:
        temp = Path(directory)
        android = temp / "android"
        android.mkdir()
        (android / "log.h").write_text(ANDROID_LOG_H)
        (android / "asset_manager.h").write_text("typedef struct AAssetManager AAssetManager;\n")
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
        saved = (data / "settings.dat").read_text(encoding="utf-8")
        assert "lang 1" in saved and "hitboxes 0" in saved, saved
        assert "musicvol 35" in saved, saved
        assert "theme 0" in saved, saved
        assert "style 2" in saved, saved
        assert "chosen 1" in saved, saved
        assert "mod" not in saved, saved
        subprocess.run([*run, "read", str(data)], check=True)

        fresh = temp / "fresh-device"
        fresh.mkdir()
        subprocess.run([*run, "cloud", str(fresh)], check=True)
        assert (fresh / "settings.dat").exists()
        adopted = (fresh / "settings.dat").read_text(encoding="utf-8")
        assert "lang 1" in adopted and "hitboxes 0" in adopted, adopted
        assert "musicvol 40" in adopted, adopted

        legacy = temp / "legacy-device"
        legacy.mkdir()
        (legacy / "settings.dat").write_text(
            "lang 1\nhitboxes 0\nmods 2\nmod winter.zip\nmod my mod.zip\n",
            encoding="utf-8")
        subprocess.run([*run, "legacy", str(legacy)], check=True)
        rewritten = (legacy / "settings.dat").read_text(encoding="utf-8")
        assert "lang 0" in rewritten and "hitboxes 1" in rewritten, rewritten
        assert "musicvol 70" in rewritten, rewritten
        assert "chosen 0" in rewritten, rewritten
        assert "mod" not in rewritten, rewritten

        old_style = temp / "old-style-device"
        old_style.mkdir()
        (old_style / "settings.dat").write_text(
            "lang 1\nhitboxes 0\nmusicvol 60\ntheme 1\nstyle 2\n",
            encoding="utf-8")
        subprocess.run([*run, "oldstyle", str(old_style)], check=True)

        legal = temp / "legal-device"
        legal.mkdir()
        subprocess.run([*run, "legal", str(legal)], check=True)
        stamped = (legal / "settings.dat").read_text(encoding="utf-8")
        assert "legal " in stamped, stamped
        subprocess.run([*run, "legal-read", str(legal)], check=True)

        fresh_install = temp / "fresh-install"
        fresh_install.mkdir()
        subprocess.run([*run, "fresh", str(fresh_install)], check=True)
        for code, expected in (("en", 0), ("de", 0), ("ru", 1)):
            phone = temp / ("phone-" + code)
            phone.mkdir()
            subprocess.run([*run, "phone", str(phone), code, str(expected)], check=True)
        saved_choice = temp / "phone-saved"
        saved_choice.mkdir()
        (saved_choice / "settings.dat").write_text("lang 1\n", encoding="utf-8")
        subprocess.run([*run, "phone", str(saved_choice), "en", "1"], check=True)

        account = temp / "account-device"
        account.mkdir()
        (account / "settings.dat").write_text("lang 0\n", encoding="utf-8")
        subprocess.run([*run, "consent", str(account)], check=True)
        consented = (account / "settings.dat").read_text(encoding="utf-8")
        assert "pdv 1" in consented and "pdc 0" not in consented, consented
        for name in ("auth.dat", "progress.dat", "achievements.dat", "promo.dat", "progress.dirty"):
            (account / name).write_text("1\n", encoding="utf-8")
        subprocess.run([*run, "delete", str(account)], check=True)
        for name in ("auth.dat", "progress.dat", "achievements.dat", "promo.dat", "progress.dirty"):
            assert not (account / name).exists(), name
        withdrawn = (account / "settings.dat").read_text(encoding="utf-8")
        password_dir = temp / "password-device"
        password_dir.mkdir()
        subprocess.run([*run, "password", str(password_dir)], check=True)
        assert "pdv 0" in withdrawn and "pdc 0" in withdrawn and "lang 0" in withdrawn, withdrawn
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
