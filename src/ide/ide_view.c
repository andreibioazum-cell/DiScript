/* Rendering for the DiScript editor: gutter, code with syntax colours, caret,
 * selection, toolbar strip and an output console. */

#include "ide/ide_view.h"

#include "engine/anim.h"
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

/* Portrait layout: a slim status strip on top, the code area in the middle and
 * a thumb-reachable toolbar at the bottom. */
#define TOP_H 46.0f
#define TOOLBAR_H 62.0f
#define BAR_H TOP_H /* the input layer's old name for the top strip */
#define GUTTER_W 66.0f
#define PAD_X 14.0f
#define CONS_H 170.0f

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

int ide_view_active(IDEView *v) { return (v && v->tab_count > 0 && v->active < v->tab_count) ? v->active : -1; }

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

/* The seven toolbar buttons: label and the action they map to. The RUN slot
 * turns into STOP while a script is running. Ids match ide_view_button_rect(). */
static const char *const k_btn_labels[IDE_BTN_COUNT] = {"NEW", "OPEN", "SAVE", "UNDO",
                                                        "REDO", "OUT", "RUN"};

void ide_view_button_rect(const IDEView *v, int id, int width, int height, float *x, float *y,
                          float *w, float *h) {
    float scale = (float)v->font_scale_px / 32.0f;
    float pad = 8.0f;
    float bar_y = (float)height - TOOLBAR_H;
    float weight[IDE_BTN_COUNT];
    float total = 0.0f;
    for (int i = 0; i < IDE_BTN_COUNT; i++) {
        const char *label = (i == IDE_BTN_RUN && v->running) ? "STOP" : k_btn_labels[i];
        float tw = text_width_scaled(label, scale * 0.8f);
        weight[i] = (i == IDE_BTN_RUN) ? tw + 34.0f : tw + 20.0f;
        total += weight[i];
    }
    float avail = (float)width - pad * 2.0f - 4.0f * (float)(IDE_BTN_COUNT - 1);
    float k = total > 0.0f && avail > 0.0f ? avail / total : 1.0f;
    float cx = pad;
    for (int i = 0; i < IDE_BTN_COUNT; i++) {
        float bw = weight[i] * k;
        if (i == id) {
            *x = cx;
            *y = bar_y + 9.0f;
            *w = bw;
            *h = TOOLBAR_H - 18.0f;
            return;
        }
        cx += bw + 4.0f;
    }
    *x = *y = *w = *h = 0.0f;
}

/* Bottom toolbar: thumb-reachable primary actions. */
static void draw_toolbar(IDEView *v, int width, int height, float scale) {
    float bar_y = (float)height - TOOLBAR_H;
    /* The bar eases up from the bottom edge once at start-up. */
    float slide = (1.0f - anim_ease_out_cubic(v->bar_anim)) * TOOLBAR_H;
    rect(0, bar_y + slide, (float)width, TOOLBAR_H, C_BAR);
    rect(0, bar_y + slide, (float)width, 1.0f, C_TAB_ON);
    if (slide > 1.0f)
        return; /* still mostly off-screen; buttons would be clipped anyway */

    for (int i = 0; i < IDE_BTN_COUNT; i++) {
        float x, y, w, h;
        ide_view_button_rect(v, i, width, height, &x, &y, &w, &h);
        if (w <= 0.0f)
            continue;
        int is_run = (i == IDE_BTN_RUN);
        uint32_t bg = is_run ? C_OK : C_TAB_ON;
        uint32_t fg = is_run ? C_BAR : C_TEXT;

        if (i == IDE_BTN_OUTPUT) {
            bg = v->toggle_anim > 0.5f ? C_ACCENT : C_TAB_ON;
            fg = v->toggle_anim > 0.5f ? C_BAR : C_TEXT;
        }
        /* While a script runs the RUN button breathes: the halo grows and the
         * body pulses, so the state is obvious even without motion. */
        if (is_run && v->running) {
            float pulse = 0.5f + 0.5f * v->run_anim;
            float grow = 2.0f + 5.0f * v->run_anim;
            rect(x - grow, y - grow, w + grow * 2.0f, h + grow * 2.0f,
                 ARGB((int)(70 + 90 * v->run_anim), 224, 108, 117));
            bg = ARGB(255, (int)(200.0f * pulse + 20.0f), (int)(90.0f * pulse + 20.0f),
                      (int)(80.0f * pulse + 20.0f));
            fg = C_BAR;
        }
        rect(x, y, w, h, bg);
        const char *label = (is_run && v->running) ? "STOP" : k_btn_labels[i];
        float tw = text_width_scaled(label, scale * 0.8f);
        text_scaled(label, x + (w - tw) * 0.5f, y + (h - scale * 20.0f) * 0.5f, fg, scale * 0.8f);
    }
}

/* Top strip: the active document name, one dot per open tab and the caret
 * position. Deliberately short so the code area keeps the vertical space. */
static void draw_top_strip(IDEView *v, int width, float scale) {
    rect(0, 0, (float)width, TOP_H, C_BAR);
    rect(0, TOP_H - 1.0f, (float)width, 1.0f, C_TAB_ON);

    IDETab *t = v->active >= 0 && v->active < v->tab_count ? &v->tabs[v->active] : NULL;
    const char *name = t && t->path[0] ? base_name(t->path) : "untitled.ds";
    char label[192];
    snprintf(label, sizeof(label), "%s%s", name, t && t->dirty ? " *" : "");
    text_scaled(label, 14, 14, C_TEXT, scale * 0.95f);

    /* Tab dots: a wide bar marks the active document. Switching tabs brightens
     * it for a moment (tab_anim) so the change is noticeable. */
    for (int i = 0; i < v->tab_count && i < IDE_MAX_TABS; i++) {
        float dx = 14.0f + (float)i * 12.0f;
        float dy = TOP_H - 12.0f;
        if (i == v->active) {
            uint32_t dot = v->tab_anim > 0.5f ? C_TEXT : C_ACCENT;
            rect(dx, dy, 16.0f, 3.0f, dot);
        } else {
            rect(dx, dy + 1.0f, 4.0f, 1.5f, C_GUTTER_TX);
        }
    }

    if (t) {
        ide_lines_refresh(&t->buf);
        char pos[48];
        snprintf(pos, sizeof(pos), "%d:%d", ide_line_of(&t->buf, t->buf.caret) + 1,
                 ide_visual_column_of(&t->buf, t->buf.caret) + 1);
        float pw = text_width_scaled(pos, scale * 0.8f);
        uint32_t col = v->running ? C_OK : C_GUTTER_TX;
        text_scaled(pos, (float)width - pw - 14.0f, 16, col, scale * 0.8f);
    }
}

void ide_view_draw(IDEView *v, int width, int height) {
    if (!v)
        return;
    float lh = line_h_of(v);
    float scale = (float)v->font_scale_px / 32.0f;
    float cw = char_w_of(v);

    /* The console inset animates, so the code area grows and shrinks smoothly. */
    float cons_h = CONS_H * v->cons_anim;
    float code_top = TOP_H;
    float code_bottom = (float)height - TOOLBAR_H - cons_h;
    if (code_bottom < code_top + lh)
        code_bottom = code_top + lh;

    rect(0, 0, (float)width, (float)height, C_BG);
    /* Anything below occluded_h is covered by the keyboard. Paint it with the
     * background so no stale pixels survive there. */
    if (v->occluded_h > 0 && height > v->occluded_h)
        rect(0, (float)v->occluded_h, (float)width, (float)(height - v->occluded_h), C_BG);
    draw_top_strip(v, width, scale);

    IDETab *t = v->active >= 0 && v->active < v->tab_count ? &v->tabs[v->active] : NULL;
    IDEBuffer *b = t ? &t->buf : NULL;
    int welcome = v->virgin || !b;

    /* Gutter column, only over the code area. */
    rect(0, code_top, GUTTER_W, code_bottom - code_top, C_GUTTER);

    if (welcome) {
        /* Welcome screen fades in and its title drifts up slightly. */
        float ease = anim_ease_out_cubic(v->welcome_anim);
        float rise = (1.0f - ease) * 26.0f;
        const char *title = "DiScript";
        float tw = text_width_scaled(title, 2.6f);
        text_scaled(title, ((float)width - tw) * 0.5f, (float)height * 0.30f + rise, C_ACCENT, 2.6f);
        /* The 2.6x title grows well past its line box, so the subtitle starts
         * below the real glyph height rather than a fixed offset. */
        float title_h = text_line_height(2.6f);
        if (title_h < 40.0f)
            title_h = 40.0f;
        const char *sub = "A code editor for the DiScript language";
        float sub_scale = scale * 0.8f;
        float sw = text_width_scaled(sub, sub_scale);
        while (sw > (float)width - 24.0f && sub_scale > 0.35f) {
            sub_scale *= 0.94f;
            sw = text_width_scaled(sub, sub_scale);
        }
        float sub_y = (float)height * 0.30f + title_h + rise;
        text_scaled(sub, ((float)width - sw) * 0.5f, sub_y, C_GUTTER_TX, sub_scale);
        const char *hint = "Tap NEW to start, OPEN to load a script";
        float hint_scale = scale * 0.78f;
        float hw = text_width_scaled(hint, hint_scale);
        while (hw > (float)width - 24.0f && hint_scale > 0.35f) {
            hint_scale *= 0.94f;
            hw = text_width_scaled(hint, hint_scale);
        }
        text_scaled(hint, ((float)width - hw) * 0.5f, sub_y + text_line_height(hint_scale) + 14.0f,
                    C_GUTTER_TX, hint_scale);
        draw_toolbar(v, width, height, scale);
        return;
    }

    ide_lines_refresh(b);
    int first_line = (int)(t->scroll_y / lh);
    int visible = (int)((code_bottom - code_top) / lh);
    if (visible < 1)
        visible = 1;
    int last_line = first_line + visible + 1;
    if (last_line > b->line_count)
        last_line = b->line_count;
    int caret_line = ide_line_of(b, b->caret);
    if (caret_line < first_line)
        caret_line = first_line;

    /* Current-line band. */
    float hl_y = code_top + (float)(caret_line - first_line) * lh;
    if (!v->running && hl_y >= code_top && hl_y < code_bottom)
        rect(GUTTER_W, hl_y, (float)width - GUTTER_W, lh, C_LINE_HL);

    /* Line numbers. */
    char num[16];
    for (int line = first_line; line < last_line; line++) {
        float y = code_top + (float)(line - first_line) * lh;
        if (y + lh > code_bottom)
            break;
        snprintf(num, sizeof(num), "%d", line + 1);
        float nw = text_width_scaled(num, scale * 0.8f);
        uint32_t col = line == caret_line ? C_TEXT : C_GUTTER_TX;
        text_scaled(num, GUTTER_W - nw - 12.0f, y + 2.0f, col, scale * 0.8f);
    }

    IDEToken toks[512];
    int ntoks = ide_highlight(b->text, b->len, toks, 512);

    /* Clip the code area so a long line never spills over the gutter. */
    ds_gfx_scissor(GUTTER_W, code_top, width - GUTTER_W, code_bottom - code_top);

    float y = code_top + 2.0f;
    size_t sel_s = ide_sel_start(b), sel_e = ide_sel_end(b);
    int has_sel = ide_has_selection(b);

    for (int line = first_line; line < last_line; line++) {
        size_t ls = ide_line_begin(b, line), le = ide_line_end(b, line);
        float x = GUTTER_W + PAD_X - (float)t->scroll_x;

        if (has_sel && sel_e > ls && sel_s < le) {
            size_t a = sel_s > ls ? sel_s : ls;
            size_t e = sel_e < le ? sel_e : le;
            float x1 = GUTTER_W + PAD_X - (float)t->scroll_x +
                       (float)ide_visual_column_of(b, a) * cw;
            float x2 = GUTTER_W + PAD_X - (float)t->scroll_x +
                       (float)ide_visual_column_of(b, e) * cw;
            if (x2 > GUTTER_W)
                rect(x1 < GUTTER_W ? GUTTER_W : x1, y, x2 - x1, lh, C_SEL);
        }

        draw_code_line(b->text + ls, le - ls, ls, toks, ntoks, x, y + 2.0f, (float)width - 4.0f,
                       scale, b, line);

        y += lh;
        if (y > code_bottom)
            break;
    }

    /* Caret, blinking unless a script is running. */
    if (!v->running) {
        int cl = ide_visual_column_of(b, b->caret);
        float cx = GUTTER_W + PAD_X - (float)t->scroll_x + (float)cl * cw;
        float cy = code_top + (float)(caret_line - first_line) * lh + 2.0f;
        if (cx > GUTTER_W && cx < width && cy >= code_top && cy < code_bottom) {
            float blink = 0.78f + 0.22f * anim_pulse((float)v->now, 1.1f);
            rect(cx, cy, 2.0f, lh - 2.0f, ARGB((int)(255 * blink), 255, 182, 84));
        }
    }

    ds_gfx_scissor(0, 0, width, height);

    /* ---- console (slides in on a spring) --------------------------------- */
    if (cons_h > 1.0f) {
        float top = code_bottom;
        rect(0, top, (float)width, cons_h, C_BAR);
        rect(0, top, (float)width, 1.0f, C_TAB_ON);
        float row = text_line_height(scale * 0.85f);
        if (row <= 0.0f)
            row = lh * 0.85f;
        int lines = (int)((cons_h - 6) / row);
        if (lines < 1)
            lines = 1;
        int from = v->cons_count - lines - v->cons_scroll;
        if (from < 0)
            from = 0;
        float bottom = top + cons_h - 3.0f;
        ds_gfx_scissor(0, top, width, cons_h);
        float cy = bottom - row - (float)(v->cons_count - from - 1) * row;
        for (int i = from; i < v->cons_count; i++) {
            const IDEConsoleLine *l = &v->console[i];
            text_scaled(l->text, 10.0f, cy, l->is_error ? C_ERR : C_TEXT, scale * 0.85f);
            cy += row;
            if (cy > bottom + row)
                break;
        }
        ds_gfx_scissor(0, 0, width, height);
    }

    /* ---- toast ----------------------------------------------------------- */
    if (v->toast_anim > 0.01f && v->status[0]) {
        float sw = text_width_scaled(v->status, scale * 0.8f);
        float alpha = anim_ease_out_cubic(v->toast_anim);
        float ty = code_bottom - 40.0f * alpha;
        rect(((float)width - sw) * 0.5f - 12, ty, sw + 24, 24, C_TAB_ON);
        text_scaled(v->status, ((float)width - sw) * 0.5f, ty + 5.0f, C_ACCENT, scale * 0.8f);
    }

    draw_toolbar(v, width, height, scale);
}

void ide_view_tick(IDEView *v, double dt) {
    if (!v)
        return;
    float fdt = dt > 0.0 ? (float)dt : 0.0f;
    v->now += (double)fdt;

    /* The console slides in on a spring; the toolbar eases in once. Both are
     * driven by the frame delta, so the harness's fixed step reproduces the
     * same state every run. */
    float cons_target = v->show_console ? 1.0f : 0.0f;
    v->cons_anim = anim_spring(v->cons_anim, &v->cons_vel, cons_target, 180.0f, fdt);
    if (v->cons_anim < 0.0f)
        v->cons_anim = 0.0f;
    if (v->cons_anim > 1.0f)
        v->cons_anim = 1.0f;
    v->bar_anim = anim_step(v->bar_anim, 0.35f, fdt);
    v->welcome_anim = anim_step(v->welcome_anim, 0.5f, fdt);
    v->toggle_anim = anim_approach(v->toggle_anim, v->show_console ? 1.0f : 0.0f, 0.6f, fdt);
    v->toast_anim = v->status_frames > 0 ? anim_approach(v->toast_anim, 1.0f, 0.5f, fdt)
                                         : anim_approach(v->toast_anim, 0.0f, 0.35f, fdt);
    if (v->status_frames > 0)
        v->status_frames--;

    /* While a script runs the RUN button breathes. */
    v->run_anim = v->running ? anim_pulse((float)v->now, 0.7f)
                             : anim_approach(v->run_anim, 0.0f, 0.5f, fdt);

    /* Switching tabs flashes the strip so the change is noticeable. */
    if (v->last_active != v->active) {
        v->last_active = v->active;
        v->tab_anim = 1.0f;
    } else {
        v->tab_anim = anim_approach(v->tab_anim, 0.0f, 0.35f, fdt);
    }
}
