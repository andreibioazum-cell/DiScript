#ifndef DISCRIPT_IDE_H
#define DISCRIPT_IDE_H

struct AAssetManager;
struct Buffer;

#ifdef __cplusplus
extern "C" {
#endif

/* Touch actions matching Android AMOTION_EVENT_ACTION_* */
#define DS_TOUCH_DOWN   0
#define DS_TOUCH_UP     1
#define DS_TOUCH_MOVE   2
#define DS_TOUCH_CANCEL 3

void ide_init(struct AAssetManager *assets);
void ide_update(void);
void ide_draw(struct Buffer *buffer);
void ide_touch(float x, float y, int action, int pointer_id);
int ide_back(void);
void ide_reset(void);
void ide_bye(void);

#ifdef __cplusplus
}
#endif

#endif
