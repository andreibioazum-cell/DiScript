#!/usr/bin/env python3
"""Decode the bundled disco MP3 and exercise the real C mixer loop."""
from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
MP3 = ROOT / "assets/audio/disco_medusae_kevin_macleod.mp3"
HARNESS = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/runtime.h"

struct AAssetManager { int unused; };
struct AAsset { FILE *file; long left; long size; };
static const char *test_path;
AAsset *AAssetManager_open(AAssetManager *mgr, const char *name, int mode) {
    (void)mgr; (void)mode;
    if (strcmp(name, "sounds/disco_medusae_kevin_macleod.mp3") != 0) return NULL;
    FILE *f = fopen(test_path, "rb");
    if (!f) return NULL;
    AAsset *a = (AAsset *)calloc(1, sizeof(*a));
    assert(a);
    a->file = f;
    assert(fseek(f, 0, SEEK_END) == 0);
    a->left = a->size = ftell(f);
    assert(a->size > 0);
    rewind(f);
    return a;
}
off_t AAsset_getLength(AAsset *a) { return (off_t)a->size; }
int AAsset_read(AAsset *a, void *buf, size_t count) {
    size_t take = count < (size_t)a->left ? count : (size_t)a->left;
    size_t got = fread(buf, 1, take, a->file);
    a->left -= (long)got;
    return (int)got;
}
int AAsset_close(AAsset *a) { fclose(a->file); free(a); return 0; }
void ds_log_err(const char *format, ...) {
    va_list ap; va_start(ap, format); vfprintf(stderr, format, ap); va_end(ap); fputc('\n', stderr);
}
#include "engine/audio/decode_mixer.inc"
#include "engine/audio/playback.inc"

int main(int argc, char **argv) {
    assert(argc == 2);
    test_path = argv[1];
    AAssetManager manager = {0};
    snd_amgr = &manager;
    assert(snd_load("disco_medusae_kevin_macleod.mp3") == 1);
    SndSound *s = snd_find("disco_medusae_kevin_macleod.mp3");
    assert(s && s->src_rate == 22050 && s->frames > 4800000 && s->frames < 5000000);
    assert(fabs((double)s->frames / 22050.0 - 221.283265) < 0.1);
    assert(snd_loop("disco_medusae_kevin_macleod.mp3") == 1);
    snd_volume("disco_medusae_kevin_macleod.mp3", 0.5);
    int16_t buffer[4096 * 2];
    long long produced = 0, peak = 0;
    int wraps = 0;
    double previous = 0;
    while (produced < 20000000) {
        snd_frame(buffer, 4096);
        produced += 4096;
        for (int i = 0; i < 4096 * 2; i++) {
            long long v = buffer[i] < 0 ? -(long long)buffer[i] : buffer[i];
            if (v > peak) peak = v;
        }
        int active = 0;
        for (int i = 0; i < SND_VOICES; i++) if (snd_voices[i].active) active++;
        assert(active == 1 && snd_voices[0].loop == 1);
        if (snd_voices[0].pos < previous) wraps++;
        previous = snd_voices[0].pos;
    }
    assert(wraps >= 2 && peak > 1000);
    assert(snd_playing("disco_medusae_kevin_macleod.mp3") == 1);
    snd_volume("disco_medusae_kevin_macleod.mp3", 0);
    snd_frame(buffer, 4096);
    for (int i = 0; i < 4096 * 2; i++) assert(buffer[i] == 0);
    snd_stop("disco_medusae_kevin_macleod.mp3");
    assert(snd_playing("disco_medusae_kevin_macleod.mp3") == 0);
    snd_release_sounds();
    printf("{\"wraps\":%d,\"output_frames\":%lld,\"peak\":%lld}\n", wraps, produced, peak);
    return 0;
}
'''


def main() -> int:
    cc = os.environ.get("CC") or shutil.which("gcc") or shutil.which("clang") or "cc"
    with tempfile.TemporaryDirectory(prefix="cb4-audio-") as directory:
        source = Path(directory) / "test_audio_loop.c"
        binary = Path(directory) / "test_audio_loop"
        source.write_text(HARNESS, encoding="utf-8")
        subprocess.run([
            cc, "-std=c99", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-O2",
            "-DCB_PC", "-I", str(ROOT), "-I", str(ROOT / "src"), "-I", str(ROOT / "src/platform/pc/shim"),
            str(source), "-o", str(binary), "-pthread", "-lm",
        ], check=True, cwd=ROOT)
        result = subprocess.run([str(binary), str(MP3)], capture_output=True, text=True, cwd=ROOT)
    if result.returncode != 0:
        raise RuntimeError(f"audio loop harness failed ({result.returncode}): {result.stderr}")
    report = json.loads(result.stdout)
    assert report["wraps"] >= 2 and report["peak"] > 1000
    print(f"Disco MP3 decoded and looped through the real mixer: {report}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
