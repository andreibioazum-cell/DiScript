/* Audio loading, mixing, and procedural synthesizer for DiScript LÖVE2D. */
#include "engine/audio/decode_mixer.inc"
#include "engine/audio/playback.inc"
#ifdef DS_HOST_SOFT
int ds_sound_init(AAssetManager *assets) { snd_amgr = assets; return 1; }
void ds_sound_shutdown(void) { snd_release_sounds(); snd_amgr = NULL; }
void ds_sound_suspend(void) {}
void ds_sound_pause(void) {}
void ds_sound_resume(void) {}
void ds_sound_set_java_vm(void *vm) { (void)vm; }
#else
#include "platform/android/audio_output.inc"
#endif
