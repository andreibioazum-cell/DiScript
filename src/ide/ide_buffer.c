/* Text buffer for the DiScript editor: UTF-8 aware, with undo/redo,
 * selection and line indexing. Deliberately dependency-free so it builds on
 * both the Windows and the Android toolchain. */

#include "ide/ide_editor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IDE_UNDO_MAX 512
#define IDE_TAB_WIDTH 4

static void *xmalloc(size_t n) { return malloc(n); }

static void buffer_mark_dirty_internal(IDEBuffer *b);

static int is_utf8_cont(unsigned char c) { return (c & 0xC0) == 0x80; }

/* Byte length of the UTF-8 sequence starting at p (1..4). */
static size_t utf8_seq_len(const char *p, size_t remaining) {
    unsigned char c = (unsigned char)*p;
    size_t n = 1;
    if (c >= 0xF0)
        n = 4;
    else if (c >= 0xE0)
        n = 3;
    else if (c >= 0xC0)
        n = 2;
    if (n > remaining)
        n = 1;
    /* Trim to the actual continuation bytes present. */
    while (n > 1 && !is_utf8_cont((unsigned char)p[n - 1]))
        n--;
    return n;
}

static void buffer_mark_dirty_internal(IDEBuffer *b) {
    b->lines_dirty = 1;
    ide_buffer_mark_dirty(b);
}

void ide_buffer_mark_dirty(IDEBuffer *b) {
    if (b)
        b->lines_dirty = 1;
}

int ide_buffer_take_dirty(IDEBuffer *b) {
    (void)b;
    return 0;
}

void ide_buffer_init(IDEBuffer *b) {
    memset(b, 0, sizeof(*b));
    b->cap = 256;
    b->text = (char *)xmalloc(b->cap);
    if (b->text) {
        b->text[0] = '\0';
        b->len = 0;
    }
    b->lines_dirty = 1;
}

void ide_buffer_free(IDEBuffer *b) {
    if (!b)
        return;
    free(b->text);
    free(b->line_start);
    if (b->undo) {
        for (int i = 0; i < b->undo_count; i++)
            free(b->undo[i].old);
    }
    free(b->undo);
    memset(b, 0, sizeof(*b));
}

static int buffer_reserve(IDEBuffer *b, size_t need) {
    if (need + 1 <= b->cap)
        return 1;
    size_t cap = b->cap ? b->cap : 256;
    while (cap < need + 1)
        cap *= 2;
    char *p = (char *)realloc(b->text, cap);
    if (!p)
        return 0;
    b->text = p;
    b->cap = cap;
    return 1;
}

int ide_buffer_set_text(IDEBuffer *b, const char *text, size_t len) {
    if (!b || !text)
        return 0;
    if (!buffer_reserve(b, len))
        return 0;
    memcpy(b->text, text, len);
    b->text[len] = '\0';
    b->len = len;
    b->caret = b->anchor = 0;
    b->want_col = 0;
    b->undo_count = b->undo_pos = 0;
    b->lines_dirty = 1;
    return 1;
}

/* ---- undo ---------------------------------------------------------------- */

static void undo_push(IDEBuffer *b, size_t pos, const char *removed, size_t removed_len, const char *ins,
                      size_t added) {
    /* Drop any redo tail. */
    for (int i = b->undo_pos; i < b->undo_count; i++)
        free(b->undo[i].old);
    b->undo_count = b->undo_pos;

    if (b->undo_count == b->undo_cap) {
        int ncap = b->undo_cap ? b->undo_cap * 2 : 64;
        if (ncap > IDE_UNDO_MAX)
            ncap = IDE_UNDO_MAX;
        if (b->undo_count >= ncap) {
            /* Forget the oldest step. */
            free(b->undo[0].old);
            free(b->undo[0].ins);
            memmove(&b->undo[0], &b->undo[1], sizeof(IDEUndoStep) * (size_t)(b->undo_count - 1));
            b->undo_count--;
            b->undo_pos = b->undo_count;
        } else {
            IDEUndoStep *n = (IDEUndoStep *)realloc(b->undo, sizeof(IDEUndoStep) * (size_t)ncap);
            if (!n)
                return;
            b->undo = n;
            b->undo_cap = ncap;
        }
    }

    IDEUndoStep *s = &b->undo[b->undo_count++];
    b->undo_pos = b->undo_count;
    s->pos = pos;
    s->removed = removed_len;
    s->added = added;
    s->caret = b->caret;
    s->anchor = b->anchor;
    s->old = NULL;
    s->ins = NULL;
    if (removed_len > 0) {
        s->old = (char *)xmalloc(removed_len);
        if (s->old)
            memcpy(s->old, removed, removed_len);
        else
            s->removed = 0;
    }
    if (added > 0 && ins) {
        s->ins = (char *)xmalloc(added);
        if (s->ins)
            memcpy(s->ins, ins, added);
        else
            s->added = 0;
    }
}

/* Splice bytes: remove `removed` bytes at `pos`, then insert `ins`. */
static void buffer_splice(IDEBuffer *b, size_t pos, size_t removed, const char *ins, size_t ins_len,
                          int record_undo) {
    if (pos > b->len)
        pos = b->len;
    if (removed > b->len - pos)
        removed = b->len - pos;

    if (record_undo)
        undo_push(b, pos, b->text + pos, removed, ins, ins_len);

    if (ins_len > removed && !buffer_reserve(b, b->len - removed + ins_len))
        return;

    if (ins_len != removed && b->len > pos + removed)
        memmove(b->text + pos + ins_len, b->text + pos + removed, b->len - pos - removed);
    if (ins_len > 0 && ins)
        memcpy(b->text + pos, ins, ins_len);

    b->len = b->len - removed + ins_len;
    b->text[b->len] = '\0';
    buffer_mark_dirty_internal(b);
}

static void set_caret(IDEBuffer *b, size_t pos) {
    if (pos > b->len)
        pos = b->len;
    b->caret = pos;
    b->want_col = -1;
}

/* ---- selection helpers --------------------------------------------------- */

int ide_has_selection(const IDEBuffer *b) { return b && b->caret != b->anchor; }
size_t ide_sel_start(const IDEBuffer *b) { return b->caret < b->anchor ? b->caret : b->anchor; }
size_t ide_sel_end(const IDEBuffer *b) { return b->caret < b->anchor ? b->anchor : b->caret; }

void ide_delete_selection(IDEBuffer *b) {
    if (!b || !ide_has_selection(b))
        return;
    size_t s = ide_sel_start(b), e = ide_sel_end(b);
    buffer_splice(b, s, e - s, NULL, 0, 1);
    b->caret = b->anchor = s;
}

/* ---- line index ---------------------------------------------------------- */

void ide_lines_refresh(IDEBuffer *b) {
    if (!b || !b->lines_dirty)
        return;
    int count = 1;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n')
            count++;
    if (count + 1 > b->line_cap) {
        int ncap = b->line_cap ? b->line_cap : 64;
        while (ncap < count + 1)
            ncap *= 2;
        int *ns = (int *)realloc(b->line_start, sizeof(int) * (size_t)ncap);
        if (!ns)
            return;
        b->line_start = ns;
        b->line_cap = ncap;
    }
    b->line_start[0] = 0;
    int li = 1;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n')
            b->line_start[li++] = (int)(i + 1);
    b->line_count = count;
    b->lines_dirty = 0;
}

int ide_line_of(IDEBuffer *b, size_t pos) {
    ide_lines_refresh(b);
    int lo = 0, hi = b->line_count - 1, best = 0;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if ((size_t)b->line_start[mid] <= pos) {
            best = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return best;
}

size_t ide_line_begin(IDEBuffer *b, int line) {
    ide_lines_refresh(b);
    if (line < 0)
        line = 0;
    if (line >= b->line_count)
        return b->len;
    return (size_t)b->line_start[line];
}

size_t ide_line_end(IDEBuffer *b, int line) {
    ide_lines_refresh(b);
    if (line < 0)
        return 0;
    if (line >= b->line_count - 1)
        return b->len;
    size_t next = (size_t)b->line_start[line + 1];
    return next > 0 ? next - 1 : 0; /* drop the '\n' */
}

int ide_visual_column_of(IDEBuffer *b, size_t pos) {
    if (!b)
        return 0;
    ide_lines_refresh(b);
    int line = ide_line_of(b, pos);
    if (line < 0 || line >= b->line_count)
        return 0;
    size_t begin = ide_line_begin(b, line);
    if (pos > b->len)
        pos = b->len;
    if (pos < begin)
        return 0;
    int col = 0;
    for (size_t i = begin; i < pos; i++)
        col = (b->text[i] == '\t') ? (col + IDE_TAB_WIDTH) - (col % IDE_TAB_WIDTH) : col + 1;
    return col;
}

size_t ide_offset_at(IDEBuffer *b, int line, size_t column) {
    if (!b)
        return 0;
    ide_lines_refresh(b);
    if (b->line_count == 0)
        return 0;
    if (line < 0)
        line = 0;
    if (line >= b->line_count)
        line = b->line_count - 1;
    size_t begin = ide_line_begin(b, line), end = ide_line_end(b, line);
    if (column > end - begin)
        column = end - begin;
    return begin + column;
}

int ide_column_of(IDEBuffer *b, size_t pos) {
    int line = ide_line_of(b, pos);
    return (int)(pos - ide_line_begin(b, line));
}

size_t ide_indent_of_line(IDEBuffer *b, int line) {
    size_t p = ide_line_begin(b, line), e = ide_line_end(b, line), n = 0;
    while (p + n < e && (b->text[p + n] == ' ' || b->text[p + n] == '\t'))
        n++;
    return n;
}

/* ---- editing ------------------------------------------------------------- */

void ide_insert(IDEBuffer *b, const char *text, size_t len) {
    if (!b || !text || len == 0)
        return;
    ide_delete_selection(b);
    size_t at = b->caret;
    buffer_splice(b, at, 0, text, len, 1);
    set_caret(b, at + len);
    b->anchor = b->caret;
}

void ide_insert_cstr(IDEBuffer *b, const char *text) {
    if (text)
        ide_insert(b, text, strlen(text));
}

void ide_backspace(IDEBuffer *b) {
    if (!b)
        return;
    if (ide_has_selection(b)) {
        ide_delete_selection(b);
        return;
    }
    if (b->caret == 0)
        return;
    /* Work on whole UTF-8 sequences. */
    size_t n = 1;
    while (n < b->caret && is_utf8_cont((unsigned char)b->text[b->caret - n]))
        n++;
    /* Smart indent: backspace inside leading whitespace removes a full tab stop. */
    int line = ide_line_of(b, b->caret);
    size_t lb = ide_line_begin(b, line);
    int col = (int)(b->caret - lb);
    if (col > 0 && b->caret - n == b->caret - 1) {
        int leading = 1;
        for (int i = 0; i < col; i++)
            if (b->text[lb + (size_t)i] != ' ')
                leading = 0;
        if (leading && col % IDE_TAB_WIDTH == 0) {
            int back = IDE_TAB_WIDTH;
            if ((size_t)back > b->caret - lb)
                back = (int)(b->caret - lb);
            buffer_splice(b, b->caret - (size_t)back, (size_t)back, NULL, 0, 1);
            b->caret -= (size_t)back;
            b->anchor = b->caret;
            return;
        }
    }
    buffer_splice(b, b->caret - n, n, NULL, 0, 1);
    b->caret -= n;
    b->anchor = b->caret;
}

void ide_delete(IDEBuffer *b) {
    if (!b)
        return;
    if (ide_has_selection(b)) {
        ide_delete_selection(b);
        return;
    }
    if (b->caret >= b->len)
        return;
    size_t n = utf8_seq_len(b->text + b->caret, b->len - b->caret);
    buffer_splice(b, b->caret, n, NULL, 0, 1);
}

void ide_newline(IDEBuffer *b) {
    if (!b)
        return;
    ide_delete_selection(b);
    /* Keep the indentation of the current line, plus one level after ':' */
    int line = ide_line_of(b, b->caret);
    size_t lb = ide_line_begin(b, line);
    size_t indent = ide_indent_of_line(b, line);

    char tail = 0;
    size_t probe = b->caret;
    while (probe > lb) {
        char c = b->text[probe - 1];
        if (c == ' ' || c == '\t') {
            probe--;
            continue;
        }
        tail = c;
        break;
    }

    size_t extra = (tail == ':' ? IDE_TAB_WIDTH : 0);
    /* Pressing Enter on a blank line drops back to column 0 instead of
     * inheriting the previous block's indentation. */
    int blank_line = (tail == 0);
    if (blank_line)
        indent = 0;
    char buf[256];
    size_t n = 0;
    buf[n++] = '\n';
    size_t want = indent + extra;
    if (want > sizeof(buf) - 1)
        want = sizeof(buf) - 1;
    for (size_t i = 0; i < want; i++)
        buf[n++] = ' ';

    size_t at = b->caret;
    buffer_splice(b, at, 0, buf, n, 1);
    set_caret(b, at + n);
    b->anchor = b->caret;
}

void ide_tab(IDEBuffer *b) {
    if (!b)
        return;
    if (ide_has_selection(b)) {
        /* Indent every line the selection touches. */
        int first = ide_line_of(b, ide_sel_start(b));
        int last = ide_line_of(b, ide_sel_end(b));
        char pad[IDE_TAB_WIDTH] = {' ', ' ', ' ', ' '};
        for (int l = last; l >= first; l--) {
            size_t p = ide_line_begin(b, l);
            buffer_splice(b, p, 0, pad, IDE_TAB_WIDTH, 1);
        }
        b->caret += IDE_TAB_WIDTH;
        b->anchor += (size_t)(last - first + 1) * IDE_TAB_WIDTH;
        return;
    }
    ide_insert(b, "    ", 4);
}

void ide_undo(IDEBuffer *b) {
    if (!b || b->undo_pos <= 0)
        return;
    IDEUndoStep *s = &b->undo[--b->undo_pos];
    buffer_splice(b, s->pos, s->added, s->old, s->removed, 0);
    b->caret = s->caret;
    b->anchor = s->anchor;
}

void ide_redo(IDEBuffer *b) {
    if (!b || b->undo_pos >= b->undo_count)
        return;
    IDEUndoStep *s = &b->undo[b->undo_pos++];
    /* Put the inserted text back and drop whatever it replaced again. */
    buffer_splice(b, s->pos, s->removed, s->ins, s->added, 0);
    b->caret = s->pos + s->added;
    b->anchor = b->caret;
}

void ide_select_all(IDEBuffer *b) {
    if (!b)
        return;
    b->anchor = 0;
    b->caret = b->len;
}

/* ---- movement ------------------------------------------------------------ */

void ide_move_left(IDEBuffer *b, int extend) {
    if (!b)
        return;
    if (!extend && ide_has_selection(b)) {
        b->caret = b->anchor = ide_sel_start(b);
        return;
    }
    size_t n = 1;
    while (n < b->caret && is_utf8_cont((unsigned char)b->text[b->caret - n]))
        n++;
    if (b->caret >= n)
        b->caret -= n;
    if (!extend)
        b->anchor = b->caret;
    b->want_col = -1;
}

void ide_move_right(IDEBuffer *b, int extend) {
    if (!b)
        return;
    if (!extend && ide_has_selection(b)) {
        b->caret = b->anchor = ide_sel_end(b);
        return;
    }
    if (b->caret < b->len)
        b->caret += utf8_seq_len(b->text + b->caret, b->len - b->caret);
    if (!extend)
        b->anchor = b->caret;
    b->want_col = -1;
}

/* Visual column of a byte offset inside its line, expanding tabs. */
static int visual_col(IDEBuffer *b, int line, size_t pos) {
    size_t lb = ide_line_begin(b, line);
    int col = 0;
    for (size_t i = lb; i < pos && i < b->len; i++) {
        if (b->text[i] == '\t')
            col += IDE_TAB_WIDTH - (col % IDE_TAB_WIDTH);
        else if (!is_utf8_cont((unsigned char)b->text[i]))
            col++;
    }
    return col;
}

/* Byte offset in `line` closest to visual column `want`. */
static size_t offset_for_col(IDEBuffer *b, int line, int want) {
    size_t lb = ide_line_begin(b, line), le = ide_line_end(b, line);
    int col = 0;
    size_t i = lb;
    while (i < le) {
        int step = (b->text[i] == '\t') ? IDE_TAB_WIDTH - (col % IDE_TAB_WIDTH) : 1;
        if (col + step > want)
            break;
        col += step;
        i += (b->text[i] == '\t') ? 1 : utf8_seq_len(b->text + i, le - i);
    }
    return i;
}

void ide_move_up(IDEBuffer *b, int extend) {
    if (!b)
        return;
    int line = ide_line_of(b, b->caret);
    if (line == 0)
        return;
    int want = (b->want_col >= 0) ? b->want_col : visual_col(b, line, b->caret);
    b->caret = offset_for_col(b, line - 1, want);
    if (!extend)
        b->anchor = b->caret;
    b->want_col = want;
}

void ide_move_down(IDEBuffer *b, int extend) {
    if (!b)
        return;
    int line = ide_line_of(b, b->caret);
    ide_lines_refresh(b);
    if (line >= b->line_count - 1)
        return;
    int want = (b->want_col >= 0) ? b->want_col : visual_col(b, line, b->caret);
    b->caret = offset_for_col(b, line + 1, want);
    if (!extend)
        b->anchor = b->caret;
    b->want_col = want;
}

void ide_move_home(IDEBuffer *b, int extend) {
    if (!b)
        return;
    int line = ide_line_of(b, b->caret);
    size_t lb = ide_line_begin(b, line);
    size_t indent = ide_indent_of_line(b, line);
    /* First press goes to the first non-space, second to column 0. */
    b->caret = (b->caret != lb + indent) ? lb + indent : lb;
    if (!extend)
        b->anchor = b->caret;
    b->want_col = -1;
}

void ide_move_end(IDEBuffer *b, int extend) {
    if (!b)
        return;
    b->caret = ide_line_end(b, ide_line_of(b, b->caret));
    if (!extend)
        b->anchor = b->caret;
    b->want_col = -1;
}

void ide_move_doc_start(IDEBuffer *b, int extend) {
    b->caret = 0;
    if (!extend)
        b->anchor = 0;
    b->want_col = -1;
}

void ide_move_doc_end(IDEBuffer *b, int extend) {
    b->caret = b->len;
    if (!extend)
        b->anchor = b->len;
    b->want_col = -1;
}

static int is_word_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

void ide_move_word_left(IDEBuffer *b, int extend) {
    if (!b)
        return;
    size_t p = b->caret;
    while (p > 0 && !is_word_char(b->text[p - 1]))
        p--;
    while (p > 0 && is_word_char(b->text[p - 1]))
        p--;
    b->caret = p;
    if (!extend)
        b->anchor = p;
    b->want_col = -1;
}

void ide_move_word_right(IDEBuffer *b, int extend) {
    if (!b)
        return;
    size_t p = b->caret;
    while (p < b->len && is_word_char(b->text[p]))
        p++;
    while (p < b->len && !is_word_char(b->text[p]))
        p++;
    b->caret = p;
    if (!extend)
        b->anchor = p;
    b->want_col = -1;
}
