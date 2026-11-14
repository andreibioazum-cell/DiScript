/* Editor application logic: documents, file dialogs, running scripts and
 * routing platform input into the buffer. The UI itself lives in ide_view.c. */

#include "ide/ide_app.h"

#include "discript/discript.h"
/* The VM's own header: the editor drives the game loop directly, so it needs
 * the state behind the opaque DSVM handle. */
#include "discript/ds_internal.h"
#include "engine/runtime.h"

#include <pthread.h>
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

/* Running a script on a worker thread is what keeps the UI alive: the console
 * slides, the RUN button breathes and STOP works even for an infinite loop. The
 * VM is single-threaded, so the worker owns it exclusively between start and
 * join; the frame thread only ever touches the output queue, under `lock`. */

static void queue_line(const char *line, int is_error) {
    if (!g_app || !line || !g_app->lock)
        return;
    pthread_mutex_lock((pthread_mutex_t *)g_app->lock);
    int next = (g_app->out_head + 1) % IDE_OUT_QUEUE;
    if (next != g_app->out_tail) { /* drop rather than block the VM */
        snprintf(g_app->out_queue[g_app->out_head].text,
                 sizeof(g_app->out_queue[g_app->out_head].text), "%s", line);
        g_app->out_queue[g_app->out_head].is_error = is_error;
        g_app->out_head = next;
    }
    pthread_mutex_unlock((pthread_mutex_t *)g_app->lock);
}

static void print_callback(const char *line, int is_error, void *userdata) {
    (void)userdata;
    queue_line(line, is_error);
}

/* Moves queued console lines into the view. Called once per frame, on the frame
 * thread, so the view itself never needs locking. */
static void flush_output(IDEApp *app) {
    if (!app || !app->lock)
        return;
    for (;;) {
        IDEOutputLine line;
        pthread_mutex_lock((pthread_mutex_t *)app->lock);
        if (app->out_tail == app->out_head) {
            pthread_mutex_unlock((pthread_mutex_t *)app->lock);
            break;
        }
        line = app->out_queue[app->out_tail];
        app->out_tail = (app->out_tail + 1) % IDE_OUT_QUEUE;
        pthread_mutex_unlock((pthread_mutex_t *)app->lock);
        ide_console_add(&app->view, line.text, line.is_error);
    }
}

static DSVM *app_vm(IDEApp *app) {
    if (!app->vm) {
        app->vm = ds_vm_create();
        if (app->vm)
            ds_vm_set_print_callback((DSVM *)app->vm, print_callback, app);
    }
    return (DSVM *)app->vm;
}

typedef struct {
    IDEApp *app;
    DSVM *vm;
    char *source;
    char name[IDE_PATH_MAX];
} RunJob;

static void *run_thread(void *arg) {
    RunJob *job = (RunJob *)arg;
    IDEApp *app = job->app;
    DSVM *vm = job->vm;

    ds_vm_reset(vm);
    ds_vm_set_print_callback(vm, print_callback, app);
    int ok = ds_vm_run_source(vm, job->name, job->source, strlen(job->source));
    if (app->stop_requested)
        ok = 0;

    if (!ok && !app->stop_requested) {
        const char *err = ds_vm_last_error(vm);
        int line = ds_vm_last_error_line(vm);
        char msg[512];
        if (line > 0)
            snprintf(msg, sizeof(msg), "ERROR line %d: %s", line, err ? err : "unknown");
        else
            snprintf(msg, sizeof(msg), "ERROR: %s", err ? err : "unknown");
        queue_line(msg, 1);
    }
    if (app->stop_requested) {
        queue_line("--- stopped ---", 1);
    } else {
        DSVMStats st;
        ds_vm_get_stats(vm, &st);
        char tail[256];
        if (st.jit_functions > 0)
            snprintf(tail, sizeof(tail), "--- %s: %.1f ms (%d jit fn, %llu calls) ---",
                     ok ? "finished" : "failed", st.exec_ms, st.jit_functions,
                     (unsigned long long)st.jit_calls);
        else
            snprintf(tail, sizeof(tail), "--- %s: %.1f ms ---", ok ? "finished" : "failed",
                     st.exec_ms);
        queue_line(tail, ok ? 0 : 1);
    }

    /* A script that defines update(dt) or draw() is a game: keep it alive and
     * drive those from the frame loop. Everything else has finished. */
    if (ok && !app->stop_requested) {
        ds_vm_love_refresh(vm);
        if (ds_vm_has_love_loop(vm)) {
            ds_vm_love_load(vm);
            app->game_mode = 1;
            queue_line("--- игра запущена: update/draw каждый кадр ---", 0);
        }
    }

    free(job->source);
    free(job);
    app->finished = 1;
    return NULL;
}

void ide_app_run(IDEApp *app) {
    if (!app || app->running || app->game_mode == 1)
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
    app->stop_requested = 0;
    app->finished = 0;
    ide_console_add(&app->view, "--- run ---", 0);

    RunJob *job = (RunJob *)calloc(1, sizeof(RunJob));
    if (!job) {
        ide_view_set_status(&app->view, "Out of memory");
        return;
    }
    job->app = app;
    job->vm = vm;
    job->source = (char *)malloc(t->buf.len + 1);
    if (!job->source) {
        free(job);
        ide_view_set_status(&app->view, "Out of memory");
        return;
    }
    memcpy(job->source, t->buf.text, t->buf.len);
    job->source[t->buf.len] = '\0';
    snprintf(job->name, sizeof(job->name), "%s", t->path[0] ? t->path : "untitled.ds");

    app->running = 1;
    app->view.running = 1;
    if (pthread_create(&app->worker, NULL, run_thread, job) != 0) {
        free(job->source);
        free(job);
        app->running = 0;
        app->view.running = 0;
        ide_view_set_status(&app->view, "Cannot start thread");
        return;
    }
    app->worker_live = 1;
}

void ide_app_stop(IDEApp *app) {
    if (!app)
        return;
    /* A game is stopped between frames, on the frame thread: the VM is being
     * used there, so it cannot be interrupted from here. */
    if (app->game_mode == 1) {
        app->game_mode = 3;
        app->view.running = 0;
        return;
    }
    if (!app->running)
        return;
    app->stop_requested = 1;
    if (app->vm)
        ds_vm_request_stop((DSVM *)app->vm);
    app->view.running = 0;
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

/* ---- project ---------------------------------------------------------- */

/* Mirror the open project into the view so the bars can draw it. The view does
 * not know about files; it only needs what to show. */
static void ide_project_sync_view(IDEApp *app) {
    if (!app)
        return;
    snprintf(app->view.project_name, sizeof(app->view.project_name), "%s",
             app->project.dir[0] ? app->project.name : "");
    app->view.project_sprites = app->project.sprite_count;
    app->view.project_scripts = app->project.script_count;
}

/* A new project's script. It is written to be run and seen working: load the
 * first picture the player adds and follow a finger. Better than an empty file,
 * which leaves no clue what the language looks like. */
static const char *k_starter_script =
    "# Лобби твоей игры. Нажми RUN — оно оживёт.\n"
    "#\n"
    "# Картинки добавляются кнопкой «+» наверху и лежат в sprites/.\n"
    "# Эта игра работает сама: update(dt) вызывается каждый кадр,\n"
    "# draw() рисует то, что нужно показать в этом кадре.\n"
    "\n"
    "player = sprite.load(\"sprites/player.png\")\n"
    "player.center = true\n"
    "\n"
    "w = sprite.window()\n"
    "px = 80\n"
    "py = w[1] * 0.5\n"
    "speed = 3\n"
    "\n"
    "def update(dt):\n"
    "    global px, py\n"
    "    px = px + speed\n"
    "    # у края — обратно, чтобы игрок не убегал за экран\n"
    "    if px > w[0] - 40:\n"
    "        speed = 0 - speed\n"
    "    if px < 40:\n"
    "        speed = 0 - speed\n"
    "    py = w[1] * 0.5\n"
    "\n"
    "def draw():\n"
    "    sprite.begin_frame()\n"
    "    player.x = px\n"
    "    player.y = py\n"
    "    sprite.add(player)\n"
    "    sprite.draw_all()\n";

void ide_app_project_boot(IDEApp *app) {
    if (!app)
        return;
    char root[400], dir[600];
    ide_project_default_root(root, sizeof(root));
    snprintf(dir, sizeof(dir), "%s/MyGame", root);
    if (ide_project_open(&app->project, dir)) {
        ide_project_sync_view(app);
        ide_app_open_path(app, NULL);
        return;
    }
    ide_app_project_new(app, "MyGame");
}

int ide_app_project_new(IDEApp *app, const char *name) {
    char root[400], dir[600];
    if (!app)
        return 0;
    ide_project_default_root(root, sizeof(root));
    ide_fs_mkdir(root);
    snprintf(dir, sizeof(dir), "%s/%s", root, (name && *name) ? name : "MyGame");
    if (!ide_project_create(&app->project, dir, name)) {
        ide_view_set_status(&app->view, "Не удалось создать проект");
        return 0;
    }
    ide_project_sync_view(app);
    ide_view_set_status(&app->view, "Проект создан");
    return 1;
}

/* Take the last path segment as the display name, minus the extension, so a
 * photo called IMG_1234.jpg becomes sprite "img_1234" and reads sensibly in the
 * project list. */
static void name_from_path(const char *path, char *out, size_t cap) {
    const char *base = path ? path : "";
    const char *slash = strrchr(base, '/');
    if (slash)
        base = slash + 1;
    snprintf(out, cap, "%s", *base ? base : "picture");
    char *dot = strrchr(out, '.');
    if (dot && dot != out)
        *dot = '\0';
}

int ide_app_project_pick_picture(IDEApp *app) {
    char path[IDE_PATH_MAX], picked[128];
    if (!app || !app->project.dir[0])
        return 0;
    path[0] = '\0';
    picked[0] = '\0';
    if (!ds_pick_image_file(path, (int)sizeof(path), picked, (int)sizeof(picked)) || !path[0]) {
        ide_view_set_status(&app->view, "Картинка не выбрана");
        return 0;
    }
    char name[64];
    /* Prefer the name from the picker: on Android the path is a temp copy. */
    if (picked[0]) {
        name_from_path(picked, name, sizeof(name));
    } else {
        name_from_path(path, name, sizeof(name));
    }
    return ide_app_project_add_picture(app, path, name);
}

const char *ide_app_project_dir(IDEApp *app) {
    return app ? app->project.dir : "";
}

int ide_app_project_add_script(IDEApp *app, const char *name) {
    if (!app || !app->project.dir[0])
        return 0;
    if (!ide_project_add_script(&app->project, name ? name : "script")) {
        ide_view_set_status(&app->view, "Не удалось добавить скрипт");
        return 0;
    }
    ide_project_sync_view(app);
    int idx = app->project.script_count - 1;
    char path[600];
    snprintf(path, sizeof(path), "%s/%s", app->project.dir, app->project.scripts[idx].file);
    ide_app_open_path(app, path);
    ide_view_set_status(&app->view, "Скрипт добавлен");
    return 1;
}

int ide_app_project_add_picture(IDEApp *app, const char *source_path, const char *display_name) {
    if (!app || !app->project.dir[0] || !source_path)
        return 0;
    int first = app->project.sprite_count == 0;
    if (!ide_project_add_sprite_file(&app->project, source_path, display_name)) {
        /* Say why. "Could not add" with no reason leaves the user with no idea
         * whether the file was missing, empty or unsupported. */
        char msg[256];
        snprintf(msg, sizeof(msg), "Не удалось прочитать картинку: %s",
                 strrchr(source_path, '/') ? strrchr(source_path, '/') + 1 : source_path);
        ide_view_set_status(&app->view, msg);
        return 0;
    }
    ide_project_sync_view(app);
    /* First picture: put a working script in front of the player instead of
     * leaving them with an empty file and no idea what to type. */
    if (first) {
        char path[600];
        ide_project_script_path(&app->project, path, sizeof(path));
        ide_fs_write_all(path, k_starter_script, strlen(k_starter_script));
        ide_app_open_path(app, path);
    }
    ide_view_set_status(&app->view, "Картинка добавлена");
    return 1;
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

/* The active tab, creating a scratch buffer on first keystroke so typing always
 * has somewhere to land. */
static IDETab *active_tab(IDEApp *app) {
    if (!app)
        return NULL;
    int idx = ide_view_active(&app->view);
    if (idx < 0 || app->view.tab_count == 0) {
        idx = ide_view_open(&app->view, NULL, NULL, 0);
        if (idx < 0)
            return NULL;
    }
    return &app->view.tabs[idx];
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
#define GUTTER_W 66.0f
#define PAD_X 14.0f
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
    IDEView *v = &app->view;

    /* ---- a running game gets the pointer instead of the editor -------- */
    if (app->game_mode == 1 && app->vm) {
        DSVM *vm = (DSVM *)app->vm;
        if (action == 0)
            ds_vm_love_touch(vm, 0, 0, x, y);
        else if (action == 2)
            ds_vm_love_touch(vm, 2, 0, x, y);
        return;
    }

    /* ---- the "+" sheet: it sits above everything, so it eats the tap first */
    if (v->add.open && v->project_name[0]) {
        if (action == 1 || action == 2) {
            /* Hover: light the row under the finger so the target is obvious. */
            v->add.hover = -1;
            for (int i = 0; i < IDE_ADD_COUNT; i++) {
                float ex, ey, ew, eh;
                ide_view_add_entry_rect(v, i, width, height, &ex, &ey, &ew, &eh);
                if (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh) {
                    v->add.hover = i;
                    break;
                }
            }
        }
        if (action == 0) {
            int hit = -1;
            for (int i = 0; i < IDE_ADD_COUNT; i++) {
                float ex, ey, ew, eh;
                ide_view_add_entry_rect(v, i, width, height, &ex, &ey, &ew, &eh);
                if (x >= ex && x <= ex + ew && y >= ey && y <= ey + eh) {
                    hit = i;
                    break;
                }
            }
            if (hit == IDE_ADD_SCRIPT)
                ide_app_project_add_script(app, "script");
            else if (hit == IDE_ADD_PICTURE)
                ide_app_project_pick_picture(app);
            else if (hit == IDE_ADD_SCRIPT_FOR_SPRITE)
                ide_app_project_add_script(app, "sprite_script");
            v->add.open = 0;
            v->add.hover = -1;
            return;
        }
    }

    /* ---- "+" button in the project bar --------------------------------- */
    if (action == 0) {
        float ax, ay, aw, ah;
        ide_view_add_button_rect(v, width, &ax, &ay, &aw, &ah);
        if (aw > 0.0f && x >= ax && x <= ax + aw && y >= ay && y <= ay + ah) {
            v->add.open = !v->add.open;
            v->add.hover = -1;
            return;
        }
        /* A tap anywhere else closes the sheet, the way a modal behaves. */
        if (v->add.open) {
            v->add.open = 0;
            v->add.hover = -1;
        }
    }

    /* ---- bottom toolbar: the primary controls, within thumb reach -------- */
    if (action == 0 && y >= (float)height - 62.0f) { /* TOOLBAR_H */
        for (int i = 0; i < IDE_BTN_COUNT; i++) {
            float bx, by, bw, bh;
            ide_view_button_rect(v, i, width, height, &bx, &by, &bw, &bh);
            if (bw <= 0.0f)
                continue;
            if (x < bx || x > bx + bw || y < by || y > by + bh)
                continue;
            switch (i) {
            case IDE_BTN_NEW: ide_app_new(app); break;
            case IDE_BTN_OPEN: ide_app_open_dialog(app); break;
            case IDE_BTN_SAVE: ide_app_save(app); break;
            case IDE_BTN_UNDO: ide_app_special_key(app, "undo"); break;
            case IDE_BTN_REDO: ide_app_special_key(app, "redo"); break;
            case IDE_BTN_OUTPUT: ide_app_toggle_console(app); break;
            case IDE_BTN_RUN:
                if (v->running)
                    ide_app_stop(app);
                else
                    ide_app_run(app);
                break;
            default: break;
            }
            return;
        }
        return;
    }

    /* ---- top strip: tap a document dot to switch tabs -------------------- */
    if (action == 0 && y < 46.0f) { /* TOP_H */
        for (int i = 0; i < v->tab_count && i < IDE_MAX_TABS; i++) {
            float dx = 14.0f + (float)i * 12.0f;
            if (x >= dx - 4.0f && x <= dx + 24.0f) {
                v->active = i;
                return;
            }
        }
        return;
    }

    /* ---- code area: place the caret -------------------------------------- */
    IDETab *t = active_tab(app);
    if (!t || action != 0 || app->running)
        return;
    IDEBuffer *b = &t->buf;

    /* The console pane covers the bottom; taps there must not move the caret. */
    float code_bottom = (float)height - 62.0f - 170.0f * v->cons_anim;
    if (y < 46.0f || y > code_bottom)
        return;
    if (x < 66.0f) /* GUTTER_W */
        return;

    ide_lines_refresh(b);
    float lh = ide_view_line_height(&app->view);
    float cwf = app_char_w(&app->view, t);
    if (cwf <= 0.0f)
        cwf = 8.0f;
    int first_line = (int)(t->scroll_y / lh);
    int line = first_line + (int)((y - 46.0f) / lh);
    if (line < 0)
        line = 0;
    if (line >= b->line_count)
        line = b->line_count > 0 ? b->line_count - 1 : 0;
    int col = (int)((x - 66.0f - 14.0f + (float)t->scroll_x) / cwf);
    if (col < 0)
        col = 0;
    /* The click gives a visual column; convert it to a byte offset by walking
     * the line the same way the renderer does. */
    size_t ls = ide_line_begin(b, line), le = ide_line_end(b, line);
    size_t pos = ls;
    int visual = 0;
    while (pos < le && visual < col) {
        visual = (b->text[pos] == '\t') ? (visual + 4) - (visual % 4) : visual + 1;
        pos++;
    }
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

    pthread_mutex_t *m = (pthread_mutex_t *)calloc(1, sizeof(pthread_mutex_t));
    if (m && pthread_mutex_init(m, NULL) == 0)
        app->lock = m;
    else if (m)
        free(m);
    app->out_head = app->out_tail = 0;
    app->running = 0;
    app->finished = 0;
    app->stop_requested = 0;
    app->worker_live = 0;
}

void ide_app_update(IDEApp *app, double now_seconds) {
    if (!app)
        return;
    /* Drive the animations from the real frame delta so the motion feels the
     * same at 60, 90 and 120 Hz. */
    double dt = app->last_tick > 0.0 ? now_seconds - app->last_tick : 0.0;
    app->last_tick = now_seconds;
    if (dt < 0.0 || dt > 0.25)
        dt = 0.0;
    ide_view_tick(&app->view, dt);

    /* Join a finished worker before the VM or the console is touched again. */
    if (app->worker_live && app->finished) {
        pthread_join(app->worker, NULL);
        app->worker_live = 0;
        app->running = 0;
        app->finished = 0;
        if (app->stop_requested) {
            app->stop_requested = 0;
            ide_view_set_status(&app->view, "Stopped");
        } else {
            ide_view_set_status(&app->view, "Done");
        }
    }
    /* Game mode: update and draw once per frame. The VM runs on this thread
     * while a game is active, which is what lets a script draw: the renderer
     * only accepts commands between begin and end of a frame. */
    if (app->game_mode == 3) {
        /* A stop was requested: close the loop before the next update. */
        app->game_mode = 0;
        app->view.running = 0;
        ide_view_set_status(&app->view, "Игра остановлена");
    } else if (app->game_mode == 1 && app->vm) {
        DSVM *vm = (DSVM *)app->vm;
        if (!ds_vm_love_update(vm, dt)) {
            const char *err = ds_vm_last_error(vm);
            char msg[512];
            snprintf(msg, sizeof(msg), "ERROR: %s", err ? err : "unknown");
            ide_console_add(&app->view, msg, 1);
            app->game_mode = 0;
            app->view.running = 0;
        }
    }

    /* Console lines produced on the worker thread reach the view here, on the
     * frame thread, so the view never needs locking. */
    flush_output(app);
    app->view.running = app->running || app->game_mode == 1;

    /* Keep the dirty markers honest as the buffer changes. */
    for (int i = 0; i < app->view.tab_count; i++) {
        if (app->view.tabs[i].dirty)
            continue;
        if (app->view.tabs[i].buf.len > 0)
            app->view.tabs[i].dirty = 0;
    }
}

void ide_app_set_font_px(IDEApp *app, int px) {
    if (!app)
        return;
    if (px < 10)
        px = 10;
    if (px > 72)
        px = 72;
    if (px == app->view.font_scale_px)
        return;
    app->view.font_scale_px = px;
    /* Force a re-measure on the next draw: the cached advance and line height
     * belong to the old size. */
    app->view.char_w = 0.0f;
    app->view.line_h = 0.0f;
}

/* Paint one game frame. Called from inside the render pass, so the drawing
 * commands the script issues land in this frame. */
static void draw_game_frame(IDEApp *app, int width, int height) {
    if (!app || !app->vm || app->game_mode != 1)
        return;
    DSVM *vm = (DSVM *)app->vm;
    sprite_clear_frame();
    ds_vm_love_set_viewport(vm, 0.0f, 0.0f, (float)width, (float)height);
    ds_vm_love_draw(vm);
    if (vm->has_error) {
        const char *err = ds_vm_last_error(vm);
        char msg[512];
        snprintf(msg, sizeof(msg), "ERROR: %s", err ? err : "unknown");
        ide_console_add(&app->view, msg, 1);
        app->game_mode = 0;
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
    /* The game paints over the editor while it runs. */
    if (app->game_mode == 1)
        draw_game_frame(app, width, height);
}
