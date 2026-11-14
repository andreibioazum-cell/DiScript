#ifndef DISCRIPT_IDE_VIEW_H
#define DISCRIPT_IDE_VIEW_H

#include "ide/ide_editor.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDE_MAX_TABS 8
#define IDE_PATH_MAX 512
#define IDE_CONS_LINES 400

typedef struct IDETab {
    IDEBuffer buf;
    char path[IDE_PATH_MAX];
    int dirty;
    int scroll_x, scroll_y; /* first visible pixel offsets */
} IDETab;

typedef struct IDEConsoleLine {
    char *text;
    int is_error;
} IDEConsoleLine;

/* The "+" sheet, opened from the project bar. Adding things is the first
 * action in a new project, so it gets its own always-visible control rather
 * than hiding behind a menu. Entries are fixed so the hit boxes are stable. */
enum {
    IDE_ADD_SCRIPT = 0,
    IDE_ADD_PICTURE,
    IDE_ADD_SCRIPT_FOR_SPRITE,
    IDE_ADD_COUNT
};

typedef struct IDEAddSheet {
    int open;      /* 1 while the sheet is up */
    float anim;    /* 0..1 slide-in, driven by ide_view_tick */
    int hover;     /* -1 or the entry under the last pointer */
} IDEAddSheet;

typedef struct IDEView {
    IDETab tabs[IDE_MAX_TABS];
    int tab_count;
    int active;

    IDEConsoleLine console[IDE_CONS_LINES];
    int cons_count;
    int cons_scroll;

    int show_console;
    int running;
    int font_scale_px; /* text pixel height */
    float char_w, line_h;
    /* Height of the surface actually drawn into, when it differs from the size
     * the platform gives the frame. Android shrinks the drawing area while the
     * keyboard is up; without this the strip below it keeps the previous
     * frame's pixels (stale line numbers showed up as digits over the keys). */
    int occluded_h;

    /* ---- project -------------------------------------------------------- */
    IDEAddSheet add;      /* the "+" sheet state */
    char project_name[64]; /* empty when no project is open */
    int project_sprites;
    int project_scripts;
    /* Height of the project bar drawn above the code, 0 when no project. */
    float project_bar_h;
    int virgin; /* no file opened yet -> show welcome */

    char status[256];
    int status_frames;

    /* ---- animation state (see engine/anim.h) ------------------------------
     * Every one of these is advanced by ide_view_tick() from a frame delta, so
     * the motion is frame-rate independent and reproducible in tests. */
    double now;          /* accumulated UI time in seconds */
    float cons_anim;     /* 0..1 console slide-in */
    float cons_vel;      /* console spring velocity */
    float bar_anim;      /* 0..1 toolbar reveal */
    float welcome_anim;  /* 0..1 welcome screen fade */
    float run_anim;      /* 0..1 RUN button pulse */
    float toggle_anim;   /* 0..1 output-button state blend */
    float tab_anim;      /* 1..0 flash when the active tab changes */
    int last_active;     /* previous `active`, to detect a tab switch */
    int toast_seq;       /* incremented whenever a toast appears */
    float toast_anim;    /* 0..1 toast fade */
} IDEView;

/* Draw the whole editor into the current frame at the given logical size. */
void ide_view_draw(IDEView *v, int width, int height);

/* Advance every animation by `dt` seconds. Call once per frame before drawing. */
void ide_view_tick(IDEView *v, double dt);

/* Rect of the "+" button in the project bar. Returns a zero-width rect when
 * no project is open, so the input layer can test it unconditionally. */
void ide_view_add_button_rect(const IDEView *v, int width, float *x, float *y, float *w, float *h);
/* Rect of one entry in the open "+" sheet. */
void ide_view_add_entry_rect(const IDEView *v, int entry, int width, int height, float *x, float *y,
                             float *w, float *h);

/* Toolbar geometry. Ids are IDE_BTN_*; returns a zero-width rect for a bad id.
 * The input layer uses this so a tap hits exactly the button that was drawn. */
void ide_view_button_rect(const IDEView *v, int id, int width, int height, float *x, float *y,
                          float *w, float *h);

enum {
    IDE_BTN_NEW = 0,
    IDE_BTN_OPEN,
    IDE_BTN_SAVE,
    IDE_BTN_UNDO,
    IDE_BTN_REDO,
    IDE_BTN_OUTPUT,
    IDE_BTN_RUN,
    IDE_BTN_COUNT
};


/* Append a line to the output console (strips trailing newline). */
void ide_console_add(IDEView *v, const char *text, int is_error);
void ide_console_clear(IDEView *v);

/* Open / create a document; returns the tab index or -1. */
int ide_view_open(IDEView *v, const char *path, const char *text, size_t len);
int ide_view_active(IDEView *v);
void ide_view_set_status(IDEView *v, const char *text);

/* Layout metrics, shared with the input layer. */
float ide_view_line_height(const IDEView *v);
int ide_view_visible_lines(const IDEView *v, int height);
void ide_view_ensure_caret_visible(IDEView *v, int width, int height);

#ifdef __cplusplus
}
#endif

#endif /* DISCRIPT_IDE_VIEW_H */
