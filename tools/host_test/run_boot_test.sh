#!/bin/bash
# Builds and runs the editor startup smoke test.
#
# This links the whole editor (not just the VM), so it catches the class of
# failure where the app builds but dies before showing a window: unresolved
# symbols in the shared library, or a crash on the first frames while Android
# reports a 0x0 surface.
set -e
cd "$(dirname "$0")/../.."
OUT=build/boot
mkdir -p "$OUT"
C_SOURCES="src/ide/ide_buffer.c src/ide/ide_highlight.c src/ide/ide_view.c \
src/ide/ide_app.c src/ide/ide_viewport.c src/ide/ide_project.c \
src/discript/ds_gc.c src/discript/ds_lexer.c src/discript/ds_parser.c \
src/discript/ds_compiler.c src/discript/ds_vm.c src/discript/ds_stdlib.c \
src/discript/ds_love.c src/discript/ds_scene_api.c src/discript/ds_sprite_api.c \
src/engine/runtime.c src/engine/anim.c \
src/engine/scene/ds_scene.c src/engine/scene/ds_render3d.c \
tools/host_test/ds_host_stubs.c tools/host_test/ds_soft_gfx.c"

rm -f "$OUT"/*.o
pids=()
for f in $C_SOURCES; do
    gcc -std=c99 -O1 -g -Isrc -Itools/host_test/stub -c "$f" -o "$OUT/$(echo "$f" | tr '/' '_').o" &
    pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

pids=()
for f in src/cpp/native_jit.cpp src/cpp/love_noise_physics.cpp src/cpp/ui_fx.cpp; do
    g++ -std=c++17 -O1 -g -fno-exceptions -fno-rtti -Isrc -c "$f" -o "$OUT/$(echo "$f" | tr '/' '_').o" &
    pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

gcc -std=c99 -O1 -g -Isrc -Itools/host_test/stub -c tools/host_test/test_boot.c -o "$OUT/test_boot_main.o"
g++ "$OUT"/*.o -o "$OUT/test_boot" -lm -lpthread
"$OUT/test_boot"
