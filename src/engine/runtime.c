/* Core runtime functions. */
#include "engine/runtime/core.inc"
#include "engine/runtime/values.inc"
#include "engine/input/backspace_repeat.h"
#ifdef CB_PC
#include "platform/pc/keyboard_pc.inc"
#else
#include "platform/android/keyboard.inc"
#endif
#include "platform/android/bye_notice.inc"
