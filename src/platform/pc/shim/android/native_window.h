/* PC shim for opaque ANativeWindow pointer types. */
#ifndef PC_SHIM_ANDROID_NATIVE_WINDOW_H
#define PC_SHIM_ANDROID_NATIVE_WINDOW_H
#include <stdint.h>
typedef struct ANativeWindow ANativeWindow;
enum { WINDOW_FORMAT_RGBA_8888 = 1 };
#endif
