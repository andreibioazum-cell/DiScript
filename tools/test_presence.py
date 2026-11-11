#!/usr/bin/env python3
"""Host tests for background online presence classification and early exits."""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parent))
from test_promo import ANDROID_LOG_H, JNI_H  # noqa: E402  shared JNI and log stubs

ROOT = Path(__file__).resolve().parents[1]

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
static void check_classify(void) {
    assert(presence_classify(NULL, "Azum") == -1);
    assert(presence_classify("null", "") == -1);
    assert(presence_classify("null", "Azum") == 0);
    assert(presence_classify("", "Azum") == 0);
    assert(presence_classify("{\"0\":{\"uid\":\"f00d\",\"nick\":\"Bob\",\"x\":0.5}}", "Azum") == 1);
    assert(presence_classify("{\"0\":{\"uid\":\"f00d\",\"nick\":\"Azum\",\"x\":0.5}}", "Azum") == 2);
    assert(presence_classify("{\"0\":{\"nick\":\"AZUM\"}}", "azum") == 2);
    assert(presence_classify("{\"0\":{\"nick\":\"Bob\"},\"1\":{\"nick\":\"Azum\"}}", "Azum") == 2);
    assert(presence_classify("{\"0\":{\"nick\":\"Bob\"},\"2\":{\"nick\":\"Eve\"}}", "Azum") == 1);
    assert(presence_classify("{\"0\":{\"nick\":\"\"}}", "Azum") == 0);
    assert(presence_classify("{\"0\":{\"nick\":\"Azumx\"}}", "Azum") == 1);
    assert(presence_classify("{\"0\":{\"nick\":\"Azu\"}}", "Azum") == 1);
    puts("presence classify: ok");
}
int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "classify") == 0) {
        check_classify();
        return 0;
    }
    if (argc == 2) {
        printf("PRESENCE=%d\n", net_presence_check(argv[1]));
        return 0;
    }
    fprintf(stderr, "usage: test <classify|DIR>\n");
    return 2;
}
'''


def run(binary, *args):
    return subprocess.run([str(binary), *args], capture_output=True, text=True)


def main():
    with tempfile.TemporaryDirectory(prefix="cubic-presence-") as directory:
        temp = Path(directory)
        android = temp / "android"
        android.mkdir()
        (android / "log.h").write_text(ANDROID_LOG_H)
        (android / "asset_manager.h").write_text("typedef struct AAssetManager AAssetManager;\n")
        (android / "native_window.h").write_text("typedef struct ANativeWindow ANativeWindow;\n")
        (temp / "jni.h").write_text(JNI_H)
        (temp / "test.c").write_text(HARNESS)
        binary = temp / "test"
        subprocess.run([
            *shlex.split(os.environ.get("CC", "clang")), "-std=gnu99", "-O0",
            "-D_POSIX_C_SOURCE=200809L", "-D__ANDROID__",
            "-Werror=implicit-function-declaration",
            "-I", str(temp), "-I", str(ROOT / "src"), "-I", str(ROOT),
            str(temp / "test.c"), "-lm", "-lpthread", "-o", str(binary),
        ], check=True)

        check = run(binary, "classify")
        assert check.returncode == 0, check.stderr
        sys.stdout.write(check.stdout)

        # No saved login: nothing to check and no network.
        empty = temp / "no-session"
        empty.mkdir()
        result = run(binary, str(empty))
        assert result.returncode == 0 and result.stdout.strip() == "PRESENCE=-1", result.stdout + result.stderr

        # Nick only, no saved password: background sign-in is impossible.
        nick_only = temp / "nick-only"
        nick_only.mkdir()
        (nick_only / "auth.dat").write_text("Azum\n", encoding="utf-8")
        result = run(binary, str(nick_only))
        assert result.returncode == 0 and result.stdout.strip() == "PRESENCE=-1", result.stdout + result.stderr

        # Nick and password, but no JVM in this host process: sign-in cannot run,
        # the check fails cleanly, and the saved login must stay untouched.
        saved = temp / "saved"
        saved.mkdir()
        (saved / "auth.dat").write_text("Azum secret\n", encoding="utf-8")
        result = run(binary, str(saved))
        assert result.returncode == 0 and result.stdout.strip() == "PRESENCE=-2", result.stdout + result.stderr
        assert (saved / "auth.dat").read_text(encoding="utf-8") == "Azum secret\n"
        print("presence check: early exits ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
