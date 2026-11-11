/* Editor application logic: documents, file dialogs, running scripts and
 * routing platform input into the buffer. The UI itself lives in ide_view.c. */

#include "ide/ide_app.h"

#include "discript/discript.h"
#include "engine/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IDE_MAX_OPEN_BYTES (4 * 1024 * 1024)

static IDEApp *g_app; /* the print callback needs to find the console */
static int g_app_width = 1280, g_app_height = 720;

/* ---- file helpers -------------------------------------------------------- */

static char *read_whole_file(const char *path, size_t *out_len) {
    if (!path || !*path)
        return NULL;
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long n = ftell(f);
    if (n < 0 || n > IDE_MAX_OPEN_BYTES) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *data = (char *)malloc((size_t)n + 1);
    if (!data) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(data, 1, (size_t)n, f);
    fclose(f);
    data[got] = '\0';
    /* Trim a UTF-8 BOM; it would otherwise show up as a stray glyph. */
    if (got >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF) {
        memmove(data, data + 3, got - 3);
        got -= 3;
        data[got] = '\0';
    }
    if (out_len)
        *out_len = got;
    return data;
}

static int write_whole_file(const char *path, const char *data, size_t len) {
    if (!path || !*path)
        return 0;
    FILE *f = fopen(path, "wb");
    if (!f)
        return 0;
    size_t put = len > 0 ? fwrite(data, 1, len, f) : 0;
    int ok = fflush(f) == 0 && put == len;
    if (fclose(f) != 0)
        ok = 0;
    return ok;
}

/* ---- script execution ---------------------------------------------------- */

static void print_callback(const char *line, int is_error, void *userdata) {
    (void)userdata;
    if (g_app && line)
        ide_console_add(&g_app->view, line, is_error);
}

static DSVM *app_vm(IDEApp *app) {
    if (!app->vm) {
        app->vm = ds_vm_create();
        if (app->vm)
            ds_vm_set_print_callback((DSVM *)app->vm, print_callback, app);
    }
    return (DSVM *)app->vm;
}

void ide_app_run(IDEApp *app) {
    if (!app || app->running)
        return;
    int idx = ide_view_active(&app->view);
    if (idx < 0) {
        ide_view_set_status(&app->view, "Nothing to run");
        return;
    }
    IDETab *t = &app->view.tabs[idx];
    DSVM *vm = app_vm(app);
    if (!vm) {
        ide_view_set_status(&app->view, "Out of memory");
        return;
    }

    app->view.show_console = 1;
    ide_console_add(&app->view, "--- run ---", 0);

    ds_vm_reset(vm);
    ds_vm_set_print_callback(vm, print_callback, app);
    const char *name = t->path[0] ? t->path : "untitled.ds";
    int ok = ds_vm_run_source(vm, name, t->buf.text, t->buf.len);
    if (!ok) {
        const char *err = ds_vm_last_error(vm);
        int line = ds_vm_last_error_line(vm);
        char msg[512];
        if (line > 0)
            snprintf(msg, sizeof(msg), "ERROR line %d: %s", line, err ? err : "unknown");
        else
            snprintf(msg, sizeof(msg), "ERROR: %s", err ? err : "unknown");
        ide_console_add(&app->view, msg, 1);
    }
    DSVMStats st;
    ds_vm_get_stats(vm, &st);
    char tail[160];
    snprintf(tail, sizeof(tail), "--- %s (%.1f ms) ---", ok ? "finished" : "failed", st.compile_ms);
    ide_console_add(&app->view, tail, ok ? 0 : 1);
    ide_view_set_status(&app->view, ok ? "Script finished" : "Script failed");
}

void ide_app_stop(IDEApp *app) {
    if (!app || !app->vm)
        return;
    ds_vm_request_stop((DSVM *)app->vm);
    app->running = 0;
    app->view.running = 0;
    ide_console_add(&app->view, "--- stopped ---", 1);
}

/* ---- documents ----------------------------------------------------------- */

void ide_app_new(IDEApp *app) {
    if (!app)
        return;
    int idx = ide_view_open(&app->view, NULL, NULL, 0);
    if (idx < 0) {
        ide_view_set_status(&app->view, "Too many tabs");
        return;
    }
    app->current_path[0] = '\0';
    ide_view_set_status(&app->view, "New file");
}

int ide_app_open_path(IDEApp *app, const char *path) {
    if (!app || !path || !*path)
        return 0;
    size_t len = 0;
    char *data = read_whole_file(path, &len);
    if (!data) {
        ide_view_set_status(&app->view, "Could not open file");
        return 0;
    }
    int idx = ide_view_open(&app->view, path, data, len);
    free(data);
    if (idx < 0) {
        ide_view_set_status(&app->view, "Too many tabs");
        return 0;
    }
    snprintf(app->current_path, sizeof(app->current_path), "%s", path);
    ide_view_set_status(&app->view, "Opened");
    return 1;
}

int ide_app_save_to(IDEApp *app, const char *path) {
    if (!app || !path || !*path)
        return 0;
    int idx = ide_view_active(&app->view);
    if (idx < 0)
        return 0;
    IDETab *t = &app->view.tabs[idx];
    if (!write_whole_file(path, t->buf.text, t->buf.len)) {
        ide_view_set_status(&app->view, "Could not save file");
        return 0;
    }
    snprintf(t->path, sizeof(t->path), "%s", path);
    snprintf(app->current_path, sizeof(app->current_path), "%s", path);
    t->dirty = 0;
    ide_view_set_status(&app->view, "Saved");
    return 1;
}

void ide_app_open_dialog(IDEApp *app) {
    char path[IDE_PATH_MAX];
    path[0] = '\0';
    if (ds_pick_open_file(path, (int)sizeof(path)) && path[0])
        ide_app_open_path(app, path);
    else
        ide_view_set_status(&app->view, "Open cancelled");
}

void ide_app_save(IDEApp *app) {
    int idx = ide_view_active(&app->view);
    if (idx < 0)
        return;
    if (app->view.tabs[idx].path[0]) {
        ide_app_save_to(app, app->view.tabs[idx].path);
        return;
    }
    ide_app_save_as(app);
}

void ide_app_save_as(IDEApp *app) {
    int idx = ide_view_active(&app->view);
    if (idx < 0)
        return;
    char path[IDE_PATH_MAX];
    path[0] = '\0';
    const char *suggest = app->view.tabs[idx].path[0] ? app->view.tabs[idx].path : "script.ds";
    if (ds_pick_save_file(suggest, path, (int)sizeof(path)) && path[0])
        ide_app_save_to(app, path);
    else
        ide_view_set_status(&app->view, "Save cancelled");
}

void ide_app_toggle_console(IDEApp *app) {
    if (app)
        app->view.show_console = !app->view.show_console;
}

/* ---- input --------------------------------------------------------------- */

static IDETab *active_tab(IDEApp *app) {
    int idx = app ? ide_view_active(&app->view) : -1;
    return idx >= 0 ? &app->view.tabs[idx] : NULL;
}

void ide_app_text(IDEApp *app, const char *utf8) {
    IDETab *t = active_tab(app);
    if (!t || !utf8 || !*utf8 || app->running)
        return;
    ide_insert_cstr(&t->buf, utf8);
    t->dirty = 1;
}

void ide_app_backspace(IDEApp *app, int count) {
    IDETab *t = active_tab(app);
    if (!t || app->running)
        return;
    for (int i = 0; i < (count > 0 ? count : 1); i++)
        ide_backspace(&t->buf);
    t->dirty = 1;
}

void ide_app_enter(IDEApp *app) {
    IDETab *t = active_tab(app);
    if (!t || app->running)
        return;
    ide_newline(&t->buf);
    t->dirty = 1;
}

void ide_app_tab(IDEApp *app) {
    IDETab *t = active_tab(app);
    if (!t || app->running)
        return;
    ide_tab(&t->buf);
    t->dirty = 1;
}

void ide_app_special_key(IDEApp *app, const char *name) {
    IDETab *t = active_tab(app);
    if (!t || !name || app->running)
        return;
    IDEBuffer *b = &t->buf;
    if (strcmp(name, "left") == 0) {
        if (ide_has_selection(b) && b->caret == ide_sel_end(b)) {
            b->caret = ide_sel_start(b);
            b->anchor = b->caret;
        } else {
            ide_move_left(b, 0);
        }
    } else if (strcmp(name, "right") == 0) {
        if (ide_has_selection(b) && b->caret == ide_sel_start(b)) {
            b->caret = ide_sel_end(b);
            b->anchor = b->caret;
        } else {
            ide_move_right(b, 0);
        }
    } else if (strcmp(name, "up") == 0) {
        ide_move_up(b, 0);
    } else if (strcmp(name, "down") == 0) {
        ide_move_down(b, 0);
    } else if (strcmp(name, "home") == 0) {
        ide_move_home(b, 0);
    } else if (strcmp(name, "end") == 0) {
        ide_move_end(b, 0);
    } else if (strcmp(name, "word_left") == 0 || strcmp(name, "ctrl_left") == 0) {
        ide_move_word_left(b, 0);
    } else if (strcmp(name, "word_right") == 0 || strcmp(name, "ctrl_right") == 0) {
        ide_move_word_right(b, 0);
    } else if (strcmp(name, "doc_start") == 0) {
        ide_move_doc_start(b, 0);
    } else if (strcmp(name, "doc_end") == 0) {
        ide_move_doc_end(b, 0);
    } else if (strcmp(name, "delete") == 0) {
        ide_delete(b);
        t->dirty = 1;
    } else if (strcmp(name, "undo") == 0) {
        ide_undo(b);
        t->dirty = 1;
    } else if (strcmp(name, "redo") == 0) {
        ide_redo(b);
        t->dirty = 1;
    } else if (strcmp(name, "select_all") == 0) {
        ide_select_all(b);
    } else if (strcmp(name, "run") == 0) {
        ide_app_run(app);
    } else if (strcmp(name, "save") == 0) {
        ide_app_save(app);
    } else if (strcmp(name, "new") == 0) {
        ide_app_new(app);
    } else if (strcmp(name, "open") == 0) {
        ide_app_open_dialog(app);
    }
}

/* Hit-boxes are recomputed here exactly as ide_view_draw() lays them out, so a
 * tap lands on the same button it looks like it is hitting. */
#define BAR_H 34.0f
#define GUTTER_W 58.0f
#define PAD_X 10.0f
#define CONS_H 150.0f

static float app_char_w(const IDEView *v, IDETab *t) {
    (void)t;
    if (v->char_w > 0.0f)
        return v->char_w;
    return text_advance_of("M", (float)v->font_scale_px / 32.0f);
}

void ide_app_pointer(IDEApp *app, float x, float y, int action) {
    ide_app_pointer_sized(app, x, y, action, g_app_width, g_app_height);
}

void ide_app_pointer_sized(IDEApp *app, float x, float y, int action, int width, int height) {
    g_app_width = width;
    g_app_height = height;
    if (!app)
        return;
    /* Buttons live in the top bar and react on press. */
    if (action == 0 && y < BAR_H) {
        IDEView *v = &app->view;
        float scale = (float)v->font_scale_px / 32.0f;
        float bw = text_width_scaled(v->running ? "STOP" : "RUN", scale * 0.9f) + 30.0f;
        float bx = (float)g_app_width - bw - 12.0f;
        if (x >= bx && x <= bx + bw) {
            if (v->running)
                ide_app_stop(app);
            else
                ide_app_run(app);
            return;
        }
        float cw2 = text_width_scaled(v->show_console ? "HIDE" : "OUT", scale * 0.85f) + 22.0f;
        float cbx = bx - cw2 - 8.0f;
        if (x >= cbx && x <= cbx + cw2) {
            ide_app_toggle_console(app);
            return;
        }
        /* Tab strip. */
        float tx = 6.0f;
        for (int i = 0; i < v->tab_count; i++) {
            const char *name = v->tabs[i].path[0] ? strrchr(v->tabs[i].path, '/') : NULL;
            name = name ? name + 1 : (v->tabs[i].path[0] ? v->tabs[i].path : "untitled.ds");
            char label[160];
            snprintf(label, sizeof(label), "%s%s", name, v->tabs[i].dirty ? " *" : "");
            float w = text_width_scaled(label, scale * 0.8f) + 26.0f;
            if (x >= tx && x <= tx + w) {
                v->active = i;
                return;
            }
            tx += w + 4.0f;
        }
        return;
    }

    /* Click in the code area places the caret. */
    IDETab *t = active_tab(app);
    if (!t || action != 0 || app->running)
        return;
    IDEBuffer *b = &t->buf;
    if (y < BAR_H || y > (float)g_app_height - (app->view.show_console ? CONS_H : 0.0f))
        return;
    if (x < GUTTER_W)
        return;

    ide_lines_refresh(b);
    float lh = ide_view_line_height(&app->view);
    float cwf = app_char_w(&app->view, t);
    if (cwf <= 0.0f)
        cwf = 8.0f;
    int first_line = (int)(t->scroll_y / lh);
    int line = first_line + (int)((y - BAR_H) / lh);
    if (line < 0)
        line = 0;
    if (line >= b->line_count)
        line = b->line_count > 0 ? b->line_count - 1 : 0;
    int col = (int)((x - GUTTER_W - PAD_X + (float)t->scroll_x) / cwf);
    if (col < 0)
        col = 0;
    size_t pos = ide_offset_at(b, line, (size_t)col);
    b->caret = pos;
    b->anchor = pos;
}

void ide_app_scroll(IDEApp *app, float dy, int width, int height) {
    (void)width;
    IDETab *t = active_tab(app);
    if (!t)
        return;
    float lh = ide_view_line_height(&app->view);
    t->scroll_y += (int)(dy * lh);
    if (t->scroll_y < 0)
        t->scroll_y = 0;
    /* Clamp to the document length so the view cannot scroll into empty space. */
    ide_lines_refresh(&t->buf);
    int visible = ide_view_visible_lines(&app->view, height);
    int max_y = (t->buf.line_count - visible + 1) * (int)lh;
    if (max_y < 0)
        max_y = 0;
    if (t->scroll_y > max_y)
        t->scroll_y = max_y;
}

void ide_app_init(IDEApp *app, void *asset_manager) {
    if (!app)
        return;
    memset(app, 0, sizeof(*app));
    g_app = app;
    app->view.font_scale_px = 18;
    app->view.virgin = 1;
    app->current_path[0] = '\0';
    (void)asset_manager;
}

void ide_app_update(IDEApp *app, double now_seconds) {
    (void)now_seconds;
    if (!app)
        return;
    app->view.running = app->running;
    /* Keep the dirty markers honest as the buffer changes. */
    for (int i = 0; i < app->view.tab_count; i++) {
        if (app->view.tabs[i].dirty)
            continue;
        if (app->view.tabs[i].buf.len > 0)
            app->view.tabs[i].dirty = 0;
    }
}

void ide_app_draw(IDEApp *app, int width, int height) {
    if (!app)
        return;
    g_app_width = width;
    g_app_height = height;
    IDEView *v = &app->view;
    /* Resolve the font metrics once; they are stable for a session. */
    if (v->char_w <= 0.0f || v->line_h <= 0.0f) {
        float scale = (float)v->font_scale_px / 32.0f;
        float lh = text_line_height(scale);
        float cw = text_advance_of("M", scale);
        v->line_h = lh > 0.0f ? lh : (float)v->font_scale_px + 6.0f;
        v->char_w = cw > 0.0f ? cw : (float)v->font_scale_px * 0.6f;
    }
    ide_view_draw(v, width, height);
}
