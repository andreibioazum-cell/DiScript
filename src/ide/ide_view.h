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
    int virgin; /* no file opened yet -> show welcome */

    char status[256];
    int status_frames;
} IDEView;

/* Draw the whole editor into the current frame at the given logical size. */
void ide_view_draw(IDEView *v, int width, int height);

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
