#ifndef DISCRIPT_IDE_APP_H
#define DISCRIPT_IDE_APP_H

#include "ide/ide_project.h"
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
    /* Game mode: the script defines update(dt)/draw() and those run every
     * frame. A plain script runs once and stops; a game has to keep going, so
     * the two are separate states. 0 = off, 1 = running, 2 = wants to stop. */
    volatile int game_mode;
    volatile int finished; /* the worker returned and awaits joining */
    volatile int stop_requested;
    double last_tick; /* previous ide_app_update timestamp, for the UI delta */

    /* The open project. `project.dir[0]` is empty until one is opened. */
    IDEProject project;

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

/* Set the code font height in surface pixels and re-measure. The surface is in
 * physical pixels on Android, so the platform layer scales this by the display
 * density: 18px on a 3x screen is a third of the size it is on a desktop. */
void ide_app_set_font_px(IDEApp *app, int px);

/* ---- project --------------------------------------------------------- */
/* Open or create the project the editor starts with. Uses the saved project
 * when one exists, otherwise creates a fresh one so a new user has something
 * to add pictures to immediately. Safe to call repeatedly. */
void ide_app_project_boot(IDEApp *app);
/* Create a new project folder and switch to it. */
int ide_app_project_new(IDEApp *app, const char *name);
/* Add an image to the open project and, when it is the first one, a script
 * that draws it. `source_path` is an image on disk. */
int ide_app_project_add_picture(IDEApp *app, const char *source_path, const char *display_name);
/* Ask the platform for a picture, then add it to the project. On Android this
 * is the photo library; elsewhere a file dialog. */
int ide_app_project_pick_picture(IDEApp *app);
/* Add another script file and open it. */
int ide_app_project_add_script(IDEApp *app, const char *name);
/* Status of the open project, for the bars. */
const char *ide_app_project_dir(IDEApp *app);

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
