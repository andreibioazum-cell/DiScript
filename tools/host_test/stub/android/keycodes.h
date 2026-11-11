#ifndef HOST_STUB_ANDROID_KEYCODES_H
#define HOST_STUB_ANDROID_KEYCODES_H
/* The subset of AKEYCODE_* the host build type-checks against. Values match
 * the Android platform so behaviour is identical when compiled for a device. */
enum {
    AKEYCODE_UNKNOWN = 0,
    AKEYCODE_HOME = 3,
    AKEYCODE_BACK = 4,
    AKEYCODE_DPAD_UP = 19,
    AKEYCODE_DPAD_DOWN = 20,
    AKEYCODE_DPAD_LEFT = 21,
    AKEYCODE_DPAD_RIGHT = 22,
    AKEYCODE_DEL = 67,
    AKEYCODE_TAB = 61,
    AKEYCODE_ENTER = 66,
    AKEYCODE_NUMPAD_ENTER = 160,
    AKEYCODE_FORWARD_DEL = 112,
    AKEYCODE_MOVE_HOME = 122,
    AKEYCODE_MOVE_END = 123,
    AKEYCODE_ESCAPE = 111,
    AKEYCODE_SPACE = 62,
    AKEYCODE_A = 29,
    AKEYCODE_R = 46,
    AKEYCODE_S = 47,
    AKEYCODE_Z = 54,
};
#endif
