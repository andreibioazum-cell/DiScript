#ifndef DISCRIPT_IDE_APP_H
#define DISCRIPT_IDE_APP_H

#include "ide/ide_view.h"
#include <pthread.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Console lines buffered between the worker thread and the frame thread. */
#define IDE_OUT_QUEUE 256

typedef struct {
    char text[1024];
    int is_error;
} IDEOutputLine;

typedef struct IDEApp {
    IDEView view;
    void *vm;              /* DSVM*, created lazily on the first Run */
    char current_path[IDE_PATH_MAX];
    volatile int running;  /* a script is executing right now */
    volatile int finished; /* the worker returned and awaits joining */
    volatile int stop_requested;
    double last_tick; /* previous ide_app_update timestamp, for the UI delta */

    /* Scripts run off the frame thread so the UI keeps animating and STOP stays
     * responsive even for a long loop. The worker owns the VM; the frame thread
     * only touches the queue below, under `lock`. */
    pthread_t worker;
    int worker_live;
    void *lock; /* pthread_mutex_t* guarding the queue */
    IDEOutputLine out_queue[IDE_OUT_QUEUE];
    int out_head;
    int out_tail;
} IDEApp;

/* One-time setup; safe to call every frame. `asset_manager` is the Android
 * AAssetManager (NULL on desktop). */
void ide_app_init(IDEApp *app, void *asset_manager);

/* Advance animations/state and close finished runs. */
void ide_app_update(IDEApp *app, double now_seconds);

/* Draw the editor for the current frame. */
void ide_app_draw(IDEApp *app, int width, int height);

/* ---- actions --------------------------------------------------------- */
void ide_app_new(IDEApp *app);
/* Open/save a specific path (used by the platform layers and the harness). */
int ide_app_open_path(IDEApp *app, const char *path);
int ide_app_save_to(IDEApp *app, const char *path);
void ide_app_open_dialog(IDEApp *app);  /* throws the platform file picker */
void ide_app_save(IDEApp *app);         /* save to the current path, or prompt */
void ide_app_save_as(IDEApp *app);
void ide_app_run(IDEApp *app);
void ide_app_stop(IDEApp *app);
void ide_app_toggle_console(IDEApp *app);

/* ---- input ------------------------------------------------------------ */
void ide_app_text(IDEApp *app, const char *utf8);   /* typed characters */
void ide_app_backspace(IDEApp *app, int count);
void ide_app_enter(IDEApp *app);
void ide_app_tab(IDEApp *app);
void ide_app_special_key(IDEApp *app, const char *name); /* "left", "up", ... */
void ide_app_pointer(IDEApp *app, float x, float y, int action); /* 0 down, 1 up */
/* Same, but with the current window size so hit-boxes match the last draw. */
void ide_app_pointer_sized(IDEApp *app, float x, float y, int action, int width, int height);
void ide_app_scroll(IDEApp *app, float dy, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* DISCRIPT_IDE_APP_H */
