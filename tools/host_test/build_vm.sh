#!/bin/bash
# Builds the DiScript VM host test binary twice: optimized and ASan/UBSan.
set -e
cd "$(dirname "$0")/../.."
CFILES="tools/host_test/ds_host_stubs.c tools/host_test/ds_soft_gfx.c src/engine/runtime.c \
src/discript/ds_gc.c src/discript/ds_lexer.c src/discript/ds_parser.c src/discript/ds_compiler.c \
src/discript/ds_vm.c src/discript/ds_stdlib.c src/discript/ds_love.c src/discript/ds_scene_api.c \
src/engine/scene/ds_scene.c tools/host_test/ds_run.c"
CPPFILES="src/cpp/native_jit.cpp src/cpp/love_noise_physics.cpp src/cpp/ui_fx.cpp"
mkdir -p build/vm build/vm-asan

build() {
  local out="$1"; shift
  local cflags="$*"
  rm -f "$out"/*.o
  for f in $CFILES; do
    gcc $cflags -Isrc -Itools/host_test/stub -c "$f" -o "$out/$(echo $f | tr '/' '_').o" &
  done
  wait
  for f in $CPPFILES; do
    g++ -std=c++17 -fno-exceptions -fno-rtti $cflags -Isrc -c "$f" -o "$out/$(echo $f | tr '/' '_').o" &
  done
  wait
  g++ $EXTRA_LINK "$out"/*.o -o "$out/ds_run" -lm
  # link the same objects again with the LÖVE2D loop harness as main
  objs=$(ls "$out"/*.o | grep -v ds_run)
  gcc $cflags -Isrc -Itools/host_test/stub -c tools/host_test/test_love_loop.c -o "$out/love_loop_main.o"
  g++ $EXTRA_LINK $objs "$out/love_loop_main.o" -o "$out/test_love_loop" -lm
  echo "built $out/ds_run + $out/test_love_loop"
}

# second binary: LÖVE2D loop harness
build_one() {
  local out="$1" main_src="$2"; shift 2
  :
}
build build/vm -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable
EXTRA_LINK="-fsanitize=address,undefined"
build build/vm-asan -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer
EXTRA_LINK=""
