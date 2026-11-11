#ifndef DISCRIPT_IDE_EDITOR_H
#define DISCRIPT_IDE_EDITOR_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Text buffer ---------------------------------------------------------
 * A gap-free, growable UTF-8 buffer holding the whole document as one string.
 * All editing goes through these helpers so the undo stack and the per-line
 * index stay consistent. Line offsets are cached and rebuilt lazily.
 */
typedef struct IDEUndoStep {
    size_t pos;     /* where the change starts */
    size_t removed; /* bytes deleted at pos (kept in `old`) */
    char *old;      /* the deleted text, or NULL for a pure insert */
    size_t added;   /* bytes inserted at pos */
    char *ins;      /* the inserted text, or NULL when added == 0 */
    size_t caret;   /* caret before the change */
    size_t anchor;  /* selection anchor before the change */
} IDEUndoStep;

typedef struct IDEBuffer {
    char *text;
    size_t len;
    size_t cap;

    size_t caret;  /* byte offset */
    size_t anchor; /* selection anchor; == caret when there is no selection */
    size_t want_col; /* preferred visual column for up/down */

    int *line_start; /* byte offset of every line start */
    int line_count;
    int line_cap;
    int lines_dirty;

    IDEUndoStep *undo;
    int undo_count;
    int undo_cap;
    int undo_pos;
} IDEBuffer;

void ide_buffer_init(IDEBuffer *b);
void ide_buffer_free(IDEBuffer *b);
int ide_buffer_set_text(IDEBuffer *b, const char *text, size_t len);

/* Editing. All of these push an undo step (except undo/redo themselves). */
void ide_insert(IDEBuffer *b, const char *text, size_t len);
void ide_insert_cstr(IDEBuffer *b, const char *text);
void ide_backspace(IDEBuffer *b);
void ide_delete(IDEBuffer *b);
void ide_delete_selection(IDEBuffer *b);
void ide_newline(IDEBuffer *b);
void ide_tab(IDEBuffer *b);
void ide_undo(IDEBuffer *b);
void ide_redo(IDEBuffer *b);
void ide_select_all(IDEBuffer *b);

/* Caret movement. `extend` keeps the anchor (shift-selection). */
void ide_move_left(IDEBuffer *b, int extend);
void ide_move_right(IDEBuffer *b, int extend);
void ide_move_up(IDEBuffer *b, int extend);
void ide_move_down(IDEBuffer *b, int extend);
void ide_move_home(IDEBuffer *b, int extend);
void ide_move_end(IDEBuffer *b, int extend);
void ide_move_doc_start(IDEBuffer *b, int extend);
void ide_move_doc_end(IDEBuffer *b, int extend);
void ide_move_word_left(IDEBuffer *b, int extend);
void ide_move_word_right(IDEBuffer *b, int extend);

/* Geometry. */
void ide_lines_refresh(IDEBuffer *b);
int ide_line_of(IDEBuffer *b, size_t pos);
size_t ide_line_begin(IDEBuffer *b, int line);
size_t ide_line_end(IDEBuffer *b, int line); /* offset of '\n' or len */
int ide_column_of(IDEBuffer *b, size_t pos);  /* byte column within its line */
/* Byte offset for a (line, column) pair, clamped to the line and document. */
size_t ide_offset_at(IDEBuffer *b, int line, size_t column);
int ide_has_selection(const IDEBuffer *b);
size_t ide_sel_start(const IDEBuffer *b);
size_t ide_sel_end(const IDEBuffer *b);
size_t ide_indent_of_line(IDEBuffer *b, int line);

/* Returns 1 when the buffer changed since the last ide_buffer_clear_dirty(). */
int ide_buffer_take_dirty(IDEBuffer *b);
void ide_buffer_mark_dirty(IDEBuffer *b);

#ifdef __cplusplus
}
#endif

#endif /* DISCRIPT_IDE_EDITOR_H */
