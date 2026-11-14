cmake_minimum_required(VERSION 3.16)
project(DiScript LANGUAGES C CXX)

if(NOT CMAKE_C_COMPILER_ID MATCHES "^(Apple)?Clang$")
    message(FATAL_ERROR
        "DiScript supports Clang only. Reconfigure with CC=clang (or use "
        "the Android NDK toolchain, which provides Clang).")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT ANDROID)
    message(FATAL_ERROR
        "DiScript builds for Android only. Configure CMake with the Android "
        "NDK toolchain, for example: cmake -B build "
        "-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake "
        "-DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26")
endif()

set(CB_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(CB_ANDROID_GLUE "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(discript SHARED
    # Editor
    "${CB_SOURCE_ROOT}/ide/ide_buffer.c"
    "${CB_SOURCE_ROOT}/ide/ide_highlight.c"
    "${CB_SOURCE_ROOT}/ide/ide_view.c"
    "${CB_SOURCE_ROOT}/ide/ide_viewport.c"
    "${CB_SOURCE_ROOT}/ide/ide_app.c"
    # Language runtime
    "${CB_SOURCE_ROOT}/discript/ds_gc.c"
    "${CB_SOURCE_ROOT}/discript/ds_lexer.c"
    "${CB_SOURCE_ROOT}/discript/ds_parser.c"
    "${CB_SOURCE_ROOT}/discript/ds_compiler.c"
    "${CB_SOURCE_ROOT}/discript/ds_vm.c"
    "${CB_SOURCE_ROOT}/discript/ds_stdlib.c"
    "${CB_SOURCE_ROOT}/discript/ds_scene_api.c"
    "${CB_SOURCE_ROOT}/discript/ds_love.c"
    # Native support for the standard library and the JIT
    "${CB_SOURCE_ROOT}/cpp/native_jit.cpp"
    "${CB_SOURCE_ROOT}/cpp/love_noise_physics.cpp"
    "${CB_SOURCE_ROOT}/cpp/ui_fx.cpp"
    # Engine
    "${CB_SOURCE_ROOT}/engine/runtime.c"
    "${CB_SOURCE_ROOT}/engine/anim.c"
    "${CB_SOURCE_ROOT}/engine/scene/ds_scene.c"
    "${CB_SOURCE_ROOT}/engine/scene/ds_render3d.c"
    "${CB_SOURCE_ROOT}/platform/android/android_main.c"
    "${CB_ANDROID_GLUE}/android_native_app_glue.c"
)

target_include_directories(discript PRIVATE
    "${CB_SOURCE_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CB_ANDROID_GLUE}"
)
target_link_libraries(discript PRIVATE android log dl m vulkan)
