/* Rendering for the DiScript editor: gutter, code with syntax colours, caret,
 * selection, toolbar strip and an output console. */

#include "ide/ide_view.h"
#include "ide/ide_highlight.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The engine primitives. Including the real header means the compiler checks
 * every signature instead of trusting a hand-written copy. */
#include "engine/runtime.h"

#define ARGB(a, r, g, b) ((uint32_t)(a) << 24 | (uint32_t)(r) << 16 | (uint32_t)(g) << 8 | (uint32_t)(b))

/* Palette — tuned for long sessions, dark with a warm accent. */
#define C_BG        ARGB(255, 22, 26, 33)
#define C_GUTTER    ARGB(255, 17, 20, 26)
#define C_GUTTER_TX ARGB(255, 92, 103, 120)
#define C_TEXT      ARGB(255, 214, 222, 235)
#define C_CARET     ARGB(255, 255, 182, 84)
#define C_SEL       ARGB(90, 86, 156, 214)
#define C_LINE_HL   ARGB(255, 28, 33, 42)
#define C_KW        ARGB(255, 198, 120, 221)
#define C_BUILTIN   ARGB(255, 97, 175, 239)
#define C_NUM       ARGB(255, 209, 154, 102)
#define C_STR       ARGB(255, 152, 195, 121)
#define C_COMMENT   ARGB(255, 110, 118, 129)
#define C_FUNC      ARGB(255, 97, 175, 239)
#define C_CLASS     ARGB(255, 229, 192, 123)
#define C_OP        ARGB(255, 86, 182, 194)
#define C_BAR       ARGB(255, 15, 18, 23)
#define C_ACCENT    ARGB(255, 255, 182, 84)
#define C_TAB_ON    ARGB(255, 32, 38, 48)
#define C_TAB_OFF   ARGB(255, 24, 28, 35)
#define C_ERR       ARGB(255, 224, 108, 117)
#define C_OK        ARGB(255, 152, 195, 121)

#define GUTTER_W 58.0f
#define BAR_H 34.0f
#define PAD_X 10.0f
#define CONS_H 150.0f

static float line_h_of(const IDEView *v) {
    if (v->line_h > 0.0f)
        return v->line_h;
    float lh = text_line_height((float)v->font_scale_px / 32.0f);
    return lh > 0.0f ? lh : (float)v->font_scale_px + 6.0f;
}

/* Column width: measured once from the real font so the caret, selection bands
 * and mouse hit-testing all agree with the glyph advance. */
static float char_w_of(const IDEView *v) {
    if (v->char_w > 0.0f)
        return v->char_w;
    float w = text_advance_of("M", (float)v->font_scale_px / 32.0f);
    return w > 0.0f ? w : (float)v->font_scale_px * 0.6f;
}

float ide_view_line_height(const IDEView *v) { return line_h_of(v); }

int ide_view_visible_lines(const IDEView *v, int height) {
    int usable = height - (int)BAR_H - (v->show_console ? (int)CONS_H : 0);
    if (usable < 1)
        usable = 1;
    return (int)(usable / line_h_of(v));
}

void ide_console_add(IDEView *v, const char *text, int is_error) {
    if (!v || !text)
        return;
    /* Keep everything after the last '\n' as the newest line; longer chunks are
     * split so a multi-line error message stays readable. */
    const char *p = text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        if (v->cons_count == IDE_CONS_LINES) {
            free(v->console[0].text);
            memmove(&v->console[0], &v->console[1], sizeof(IDEConsoleLine) * (IDE_CONS_LINES - 1));
            v->cons_count--;
        }
        IDEConsoleLine *l = &v->console[v->cons_count];
        l->text = (char *)malloc(n + 1);
        if (l->text) {
            memcpy(l->text, p, n);
            l->text[n] = '\0';
            l->is_error = is_error;
            v->cons_count++;
        }
        if (!nl)
            break;
        p = nl + 1;
    }
    /* Autoscroll to the newest output. */
    v->cons_scroll = 0;
}

void ide_console_clear(IDEView *v) {
    if (!v)
        return;
    for (int i = 0; i < v->cons_count; i++)
        free(v->console[i].text);
    v->cons_count = 0;
    v->cons_scroll = 0;
}

int ide_view_open(IDEView *v, const char *path, const char *text, size_t len) {
    if (!v)
        return -1;
    /* Reuse an already open tab with that path. */
    for (int i = 0; i < v->tab_count; i++) {
        if (path && strcmp(v->tabs[i].path, path) == 0) {
            ide_buffer_set_text(&v->tabs[i].buf, text, len);
            v->tabs[i].dirty = 0;
            v->active = i;
            v->virgin = 0;
            return i;
        }
    }
    if (v->tab_count >= IDE_MAX_TABS)
        return -1;
    int i = v->tab_count++;
    IDETab *t = &v->tabs[i];
    memset(t, 0, sizeof(*t));
    ide_buffer_init(&t->buf);
    if (text && len > 0)
        ide_buffer_set_text(&t->buf, text, len);
    if (path)
        snprintf(t->path, sizeof(t->path), "%s", path);
    v->active = i;
    v->virgin = 0;
    return i;
}

int ide_view_active(IDEView *v) { return v ? v->active : -1; }

void ide_view_set_status(IDEView *v, const char *text) {
    if (!v || !text)
        return;
    snprintf(v->status, sizeof(v->status), "%s", text);
    v->status_frames = 180;
}

static const char *base_name(const char *path) {
    const char *s = strrchr(path, '/');
    if (!s)
        s = strrchr(path, '\\');
    return s ? s + 1 : path;
}

static uint32_t kind_color(int kind) {
    switch (kind) {
    case IDE_TK_KEYWORD: return C_KW;
    case IDE_TK_BUILTIN: return C_BUILTIN;
    case IDE_TK_NUMBER: return C_NUM;
    case IDE_TK_STRING: return C_STR;
    case IDE_TK_COMMENT: return C_COMMENT;
    case IDE_TK_FUNCTION: return C_FUNC;
    case IDE_TK_CLASS: return C_CLASS;
    case IDE_TK_OPERATOR: return C_OP;
    default: return C_TEXT;
    }
}

/* Find the token covering `pos`, or -1. */
static int token_at(const IDEToken *toks, int n, size_t pos) {
    for (int i = 0; i < n; i++)
        if (pos >= toks[i].start && pos < toks[i].end)
            return i;
    return -1;
}

/* Draw one line of code starting at `x`, stopping at `max_x`. Runs of the same
 * colour are coalesced so a line is a handful of draw calls, not one per char. */
static void draw_code_line(const char *seg, size_t seg_len, size_t seg_start, const IDEToken *toks, int ntoks,
                           float x, float y, float max_x, float scale, const IDEBuffer *b, int line_idx) {
    size_t i = 0;
    while (i < seg_len) {
        int tk = token_at(toks, ntoks, seg_start + i);
        size_t j = i + 1;
        while (j < seg_len) {
            int tk2 = token_at(toks, ntoks, seg_start + j);
            if (tk2 != tk)
                break;
            j++;
        }
        uint32_t col = (tk >= 0) ? kind_color(toks[tk].kind) : C_TEXT;
        if (x > max_x)
            break;
        char tmp[1024];
        size_t n = j - i;
        if (n >= sizeof(tmp))
            n = sizeof(tmp) - 1;
        memcpy(tmp, seg + i, n);
        tmp[n] = '\0';
        /* Expand tabs to spaces so columns line up. */
        char out[2048];
        size_t o = 0;
        for (size_t k = 0; k < n && o + 4 < sizeof(out); k++) {
            if (tmp[k] == '\t') {
                do {
                    out[o++] = ' ';
                } while (o % 4);
            } else {
                out[o++] = tmp[k];
            }
        }
        out[o] = '\0';
        text_scaled(out, x, y, col, scale);
        float adv = text_width_scaled(out, scale);
        x += adv;
        i = j;
    }
    (void)b;
    (void)line_idx;
}

void ide_view_draw(IDEView *v, int width, int height) {
    if (!v)
        return;
    float lh = line_h_of(v);
    float scale = (float)v->font_scale_px / 32.0f;
    float cw = char_w_of(v);

    int cons_h = v->show_console ? (int)CONS_H : 0;
    float code_top = BAR_H;
    float code_bottom = (float)(height - cons_h);
    int visible = (int)((code_bottom - code_top) / lh);
    if (visible < 1)
        visible = 1;

    rect(0, 0, (float)width, (float)height, C_BG);

    /* ---- top bar with tabs and buttons ---------------------------------- */
    rect(0, 0, (float)width, BAR_H, C_BAR);
    float tx = 6.0f;
    for (int i = 0; i < v->tab_count && tx < width - 200; i++) {
        const char *name = v->tabs[i].path[0] ? base_name(v->tabs[i].path) : "untitled.ds";
        char label[128];
        snprintf(label, sizeof(label), "%s%s", name, v->tabs[i].dirty ? " *" : "");
        float w = text_width_scaled(label, scale * 0.8f) + 26.0f;
        rect(tx, 4, w, BAR_H - 8, i == v->active ? C_TAB_ON : C_TAB_OFF);
        if (i == v->active)
            rect(tx, BAR_H - 3, w, 3, C_ACCENT);
        text_scaled(label, tx + 13, 11, i == v->active ? C_TEXT : C_GUTTER_TX, scale * 0.8f);
        tx += w + 4.0f;
    }

    /* Run / stop button on the right. */
    const char *btn = v->running ? "STOP" : "RUN";
    float bw = text_width_scaled(btn, scale * 0.9f) + 30.0f;
    float bx = (float)width - bw - 12.0f;
    rect(bx, 5, bw, BAR_H - 10, v->running ? C_ERR : C_OK);
    text_scaled(btn, bx + 15, 11, C_BAR, scale * 0.9f);

    /* Console toggle. */
    const char *cbtn = v->show_console ? "HIDE" : "OUT";
    float cw2 = text_width_scaled(cbtn, scale * 0.85f) + 22.0f;
    float cbx = bx - cw2 - 8.0f;
    rect(cbx, 5, cw2, BAR_H - 10, C_TAB_ON);
    text_scaled(cbtn, cbx + 11, 11, C_TEXT, scale * 0.85f);

    if (v->virgin || v->tab_count == 0) {
        const char *title = "DiScript";
        float tw = text_width_scaled(title, 3.0f);
        text_scaled(title, ((float)width - tw) * 0.5f, (float)height * 0.34f, C_ACCENT, 3.0f);
        const char *sub = "Press New to start a script, or Open to load one";
        float sw = text_width_scaled(sub, scale);
        text_scaled(sub, ((float)width - sw) * 0.5f, (float)height * 0.34f + 70.0f, C_GUTTER_TX, scale);
        return;
    }

    IDETab *t = &v->tabs[v->active];
    IDEBuffer *b = &t->buf;
    ide_lines_refresh(b);

    int caret_line = ide_line_of(b, b->caret);

    /* Keep the caret in view. */
    int first_line = (int)(t->scroll_y / lh);
    if (caret_line < first_line)
        first_line = caret_line;
    if (caret_line >= first_line + visible)
        first_line = caret_line - visible + 1;
    if (first_line < 0)
        first_line = 0;
    t->scroll_y = (int)(first_line * lh);

    int last_line = first_line + visible;
    if (last_line > b->line_count)
        last_line = b->line_count;

    /* Highlight the caret line. */
    rect(0, code_top + (float)(caret_line - first_line) * lh, (float)width, lh, C_LINE_HL);

    /* Gutter. */
    rect(0, code_top, GUTTER_W, code_bottom - code_top, C_GUTTER);

    /* ---- tokens for the visible span ------------------------------------ */
    static IDEToken toks[20000];
    size_t span_start = ide_line_begin(b, first_line);
    size_t span_end = ide_line_end(b, last_line > 0 ? last_line - 1 : 0);
    if (span_end > b->len)
        span_end = b->len;
    int ntoks = ide_highlight(b->text + span_start, span_end - span_start, toks, 20000);

    ds_gfx_scissor(GUTTER_W, code_top, width - GUTTER_W, code_bottom - code_top);

    float y = code_top + 2.0f;
    size_t sel_s = ide_sel_start(b), sel_e = ide_sel_end(b);
    int has_sel = ide_has_selection(b);

    for (int line = first_line; line < last_line; line++) {
        size_t ls = ide_line_begin(b, line), le = ide_line_end(b, line);

        /* Line number. */
        char num[16];
        snprintf(num, sizeof(num), "%d", line + 1);
        float nw = text_width_scaled(num, scale * 0.8f);
        text_scaled(num, GUTTER_W - nw - 10.0f, y + 2.0f, line == caret_line ? C_ACCENT : C_GUTTER_TX,
                    scale * 0.8f);

        float x = GUTTER_W + PAD_X - (float)t->scroll_x;

        /* Selection band for this line. */
        if (has_sel && sel_e > ls && sel_s < le) {
            size_t a = sel_s > ls ? sel_s : ls;
            size_t e = sel_e < le ? sel_e : le;
            float x1 = GUTTER_W + PAD_X - (float)t->scroll_x + (float)(a - ls) * cw;
            float x2 = GUTTER_W + PAD_X - (float)t->scroll_x + (float)(e - ls) * cw;
            if (x2 > GUTTER_W)
                rect(x1 < GUTTER_W ? GUTTER_W : x1, y, x2 - x1, lh, C_SEL);
        }

        draw_code_line(b->text + ls, le - ls, ls, toks, ntoks, x, y + 2.0f, (float)width - 4.0f, scale, b, line);

        y += lh;
        if (y > code_bottom)
            break;
    }

    /* Caret. */
    if (v->active >= 0 && !v->running) {
        int cl = ide_column_of(b, b->caret);
        float cx = GUTTER_W + PAD_X - (float)t->scroll_x + (float)cl * cw;
        float cy = code_top + (float)(caret_line - first_line) * lh + 2.0f;
        if (cx > GUTTER_W && cx < width && cy >= code_top && cy < code_bottom)
            rect(cx, cy, 2.0f, lh - 2.0f, C_CARET);
    }

    ds_gfx_scissor(0, 0, width, height);

    /* ---- console --------------------------------------------------------- */
    if (v->show_console) {
        float top = code_bottom;
        rect(0, top, (float)width, (float)cons_h, C_BAR);
        rect(0, top, (float)width, 1.0f, C_TAB_ON);
        int lines = (int)((cons_h - 8) / (lh * 0.85f));
        if (lines < 1)
            lines = 1;
        int from = v->cons_count - lines - v->cons_scroll;
        if (from < 0)
            from = 0;
        float cy = top + 5.0f;
        for (int i = from; i < v->cons_count; i++) {
            const IDEConsoleLine *l = &v->console[i];
            text_scaled(l->text, 10.0f, cy, l->is_error ? C_ERR : C_TEXT, scale * 0.85f);
            cy += lh * 0.85f;
            if (cy > top + cons_h)
                break;
        }
    }

    /* ---- status line ----------------------------------------------------- */
    if (v->status_frames > 0) {
        v->status_frames--;
        float sw = text_width_scaled(v->status, scale * 0.8f);
        rect(((float)width - sw) * 0.5f - 12, code_bottom - 32, sw + 24, 24, C_TAB_ON);
        text_scaled(v->status, ((float)width - sw) * 0.5f, code_bottom - 27, C_ACCENT, scale * 0.8f);
    }
}

void ide_view_ensure_caret_visible(IDEView *v, int width, int height) {
    (void)v;
    (void)width;
    (void)height;
}
