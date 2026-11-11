#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "engine/runtime.h"
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#define DS_ERROR_MESSAGE_SIZE 1024
#define DS_CONSOLE_MAX 512
#define DS_CONSOLE_LINE_MAX 256

static char ds_console_buf[DS_CONSOLE_MAX][DS_CONSOLE_LINE_MAX];
static int ds_console_type_buf[DS_CONSOLE_MAX];
static int ds_console_head = 0;
static int ds_console_count = 0;
static pthread_mutex_t ds_console_lock = PTHREAD_MUTEX_INITIALIZER;

static void console_lock(void) { pthread_mutex_lock(&ds_console_lock); }
static void console_unlock(void) { pthread_mutex_unlock(&ds_console_lock); }

#define DS_CONSOLE_READ_SLOTS 8
static char ds_console_read[DS_CONSOLE_READ_SLOTS][DS_CONSOLE_LINE_MAX];
static int ds_console_read_pos = 0;

static void console_add_single(const char *line, size_t len, int is_error) {
    char tmp[DS_CONSOLE_LINE_MAX];
    size_t w = 0;
    for (size_t i = 0; i < len && w + 1 < sizeof(tmp); i++) {
        char c = line[i];
        if (c == '\r')
            continue;
        if (c == '\t') {
            for (int k = 0; k < 4 && w + 1 < sizeof(tmp); k++)
                tmp[w++] = ' ';
        } else {
            tmp[w++] = c;
        }
    }
    tmp[w] = '\0';
    console_lock();
    snprintf(ds_console_buf[ds_console_head], DS_CONSOLE_LINE_MAX, "%s", tmp);
    ds_console_type_buf[ds_console_head] = is_error ? 1 : 0;
    ds_console_head = (ds_console_head + 1) % DS_CONSOLE_MAX;
    if (ds_console_count < DS_CONSOLE_MAX)
        ds_console_count++;
    console_unlock();
}

static void console_add(const char *line, int is_error) {
    if (!line)
        return;
    const char *p = line;
    for (;;) {
        const char *nl = strchr(p, '\n');
        if (!nl) {
            console_add_single(p, strlen(p), is_error);
            break;
        }
        console_add_single(p, (size_t)(nl - p), is_error);
        p = nl + 1;
        if (*p == '\0')
            break;
    }
}

void ds_console_raw(int is_error, const char *line) {
    console_add(line ? line : "", is_error);
}

void console_clear(void) {
    console_lock();
    ds_console_head = 0;
    ds_console_count = 0;
    console_unlock();
}

int console_count(void) {
    int c;
    console_lock();
    c = ds_console_count;
    console_unlock();
    return c;
}

int console_type(int index) {
    int t = 0;
    console_lock();
    if (index >= 0 && index < ds_console_count) {
        int pos = (ds_console_head - ds_console_count + index) % DS_CONSOLE_MAX;
        if (pos < 0)
            pos += DS_CONSOLE_MAX;
        t = ds_console_type_buf[pos];
    }
    console_unlock();
    return t;
}

const char *console_line(int index) {
    console_lock();
    char *slot = ds_console_read[ds_console_read_pos];
    ds_console_read_pos = (ds_console_read_pos + 1) % DS_CONSOLE_READ_SLOTS;
    if (index >= 0 && index < ds_console_count) {
        int pos = (ds_console_head - ds_console_count + index) % DS_CONSOLE_MAX;
        if (pos < 0)
            pos += DS_CONSOLE_MAX;
        snprintf(slot, DS_CONSOLE_LINE_MAX, "%s", ds_console_buf[pos]);
    } else {
        slot[0] = '\0';
    }
    console_unlock();
    return slot;
}

Joy joy = {0};
int screen_w = 0;
int screen_h = 0;
double dt = 0.0;
int mouse_clicked = 0;
double ds_mouse_x = 0.0;
double ds_mouse_y = 0.0;

static jmp_buf ds_error_jump;
static int ds_error_handler_active = 0;
static int ds_has_error = 0;
static int ds_restart_requested = 0;
int ds_log_quiet = 0;
static char ds_last_error[DS_ERROR_MESSAGE_SIZE] = {0};

/* ---- Storage Directories ------------------------------------------------ */
static char g_internal_dir[512] = ".";
static char g_external_dir[512] = ".";

void ds_set_storage_dirs(const char *internal_dir, const char *external_dir) {
    if (internal_dir && *internal_dir)
        snprintf(g_internal_dir, sizeof(g_internal_dir), "%s", internal_dir);
    if (external_dir && *external_dir)
        snprintf(g_external_dir, sizeof(g_external_dir), "%s", external_dir);
    else if (internal_dir && *internal_dir)
        snprintf(g_external_dir, sizeof(g_external_dir), "%s", internal_dir);
}

const char *ds_data_dir(void) { return g_internal_dir; }
const char *ds_external_dir(void) { return g_external_dir; }

/* ---- Monotonic High-Resolution Clock ------------------------------------ */
static uint64_t g_boot_ns = 0;

uint64_t ds_monotonic_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0)
        return 0;
    uint64_t v = (uint64_t)now.tv_sec * 1000000000ull + (uint64_t)now.tv_nsec;
    if (g_boot_ns == 0)
        g_boot_ns = v;
    return v;
}

double ds_time_sec(void) {
    uint64_t now = ds_monotonic_ns();
    return (double)(now - g_boot_ns) * 1e-9;
}

/* ---- Editor Event Ring Buffer ------------------------------------------- */
#define DS_ED_QUEUE_CAP 256
static DSEditorEvent g_ed_queue[DS_ED_QUEUE_CAP];
static int g_ed_head = 0;
static int g_ed_tail = 0;
static pthread_mutex_t g_ed_lock = PTHREAD_MUTEX_INITIALIZER;

static void ed_push_raw(const DSEditorEvent *ev) {
    pthread_mutex_lock(&g_ed_lock);
    int next = (g_ed_tail + 1) % DS_ED_QUEUE_CAP;
    if (next == g_ed_head) {
        /* Coalesce or drop oldest only if saturated */
        g_ed_head = (g_ed_head + 1) % DS_ED_QUEUE_CAP;
    }
    g_ed_queue[g_ed_tail] = *ev;
    g_ed_tail = next;
    pthread_mutex_unlock(&g_ed_lock);
}

void ds_editor_push_text(const char *utf8) {
    if (!utf8 || !*utf8)
        return;
    size_t len = strlen(utf8);
    size_t pos = 0;
    while (pos < len) {
        DSEditorEvent ev;
        memset(&ev, 0, sizeof(ev));
        ev.kind = DS_ED_TEXT;
        size_t chunk = len - pos;
        if (chunk >= sizeof(ev.text)) {
            chunk = sizeof(ev.text) - 1;
            /* Do not split multi-byte UTF-8 sequence */
            while (chunk > 0 && ((unsigned char)utf8[pos + chunk] & 0xC0) == 0x80)
                chunk--;
            if (chunk == 0)
                chunk = 1;
        }
        memcpy(ev.text, utf8 + pos, chunk);
        ev.text[chunk] = '\0';
        ed_push_raw(&ev);
        pos += chunk;
    }
}

void ds_editor_push_backspace(int count) {
    if (count <= 0)
        return;
    DSEditorEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.kind = DS_ED_BACKSPACE;
    ev.count = count;
    ed_push_raw(&ev);
}

void ds_editor_push_enter(void) {
    DSEditorEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.kind = DS_ED_ENTER;
    ed_push_raw(&ev);
}

void ds_editor_push_key(int keycode, int meta) {
    DSEditorEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.kind = DS_ED_KEY;
    ev.keycode = keycode;
    ev.meta = meta;
    ed_push_raw(&ev);
}

int ds_editor_poll_event(DSEditorEvent *out) {
    int ok = 0;
    if (!out)
        return 0;
    pthread_mutex_lock(&g_ed_lock);
    if (g_ed_head != g_ed_tail) {
        *out = g_ed_queue[g_ed_head];
        g_ed_head = (g_ed_head + 1) % DS_ED_QUEUE_CAP;
        ok = 1;
    }
    pthread_mutex_unlock(&g_ed_lock);
    return ok;
}

void ds_editor_clear_events(void) {
    pthread_mutex_lock(&g_ed_lock);
    g_ed_head = 0;
    g_ed_tail = 0;
    pthread_mutex_unlock(&g_ed_lock);
}

/* ---- Platform Logging & Protected Hooks --------------------------------- */
typedef struct DSStringNode DSStringNode;
struct DSStringNode {
    DSStringNode *next;
    char *string;
    unsigned long gen;
};
static DSStringNode *ds_strings = NULL;
static unsigned long ds_string_gen = 0;

static void platform_log(int is_error, const char *format, va_list args) {
    __android_log_vprint(is_error ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO, "DiScript", format, args);
}

void ds_log(const char *format, ...) {
    char tmp[DS_CONSOLE_LINE_MAX];
    va_list args;
    if (ds_log_quiet != 0)
        return;
    va_start(args, format);
    platform_log(0, format, args);
    va_end(args);
    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);
    console_add(tmp, 0);
}

void ds_log_err(const char *format, ...) {
    char tmp[DS_CONSOLE_LINE_MAX];
    va_list args;
    va_start(args, format);
    platform_log(1, format, args);
    va_end(args);
    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);
    console_add(tmp, 1);
}

void ds_console_log(int is_error, const char *format, ...) {
    char tmp[DS_CONSOLE_LINE_MAX];
    va_list args;
    if (is_error == 0 && ds_log_quiet != 0)
        return;
    va_start(args, format);
    platform_log(is_error, format, args);
    va_end(args);
    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);
    console_add(tmp, is_error ? 1 : 0);
}

void ds_runtime_error(const char *format, ...) {
    char tmp[DS_CONSOLE_LINE_MAX];
    va_list args, copy;
    va_start(args, format);
    va_copy(copy, args);
    vsnprintf(ds_last_error, sizeof(ds_last_error), format, copy);
    va_end(copy);
    platform_log(1, format, args);
    va_end(args);
    va_start(args, format);
    vsnprintf(tmp, sizeof(tmp), format, args);
    va_end(args);
    console_add(tmp, 1);
    ds_has_error = 1;
    if (ds_error_handler_active)
        longjmp(ds_error_jump, 1);
}

int ds_call_protected(DSProtectedFunction function, void *userdata, const char *label) {
    int jumped;
    if (!function) {
        if (label && *label)
            ds_runtime_error("empty hook '%s'", label);
        else
            ds_runtime_error("empty hook");
        return 0;
    }
    if (ds_error_handler_active) {
        function(userdata);
        return !ds_has_error;
    }
    ds_error_handler_active = 1;
    jumped = setjmp(ds_error_jump);
    if (jumped == 0) {
        ds_clear_runtime_error();
        function(userdata);
        ds_error_handler_active = 0;
        return !ds_has_error;
    }
    ds_error_handler_active = 0;
    if (label && *label && ds_last_error[0] == '\0')
        snprintf(ds_last_error, sizeof(ds_last_error), "hook '%s' failed", label);
    return 0;
}

const char *ds_runtime_error_message(void) {
    return ds_last_error[0] ? ds_last_error : "unknown DiScript runtime error";
}

int ds_game_has_error(void) { return ds_has_error; }

void ds_clear_runtime_error(void) {
    ds_has_error = 0;
    ds_last_error[0] = '\0';
}

void ds_request_game_restart(void) { ds_restart_requested = 1; }
int ds_game_restart_requested(void) { return ds_restart_requested; }
void ds_clear_game_restart(void) { ds_restart_requested = 0; }

static char *ds_strdup(const char *s) {
    if (!s)
        s = "";
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c)
        memcpy(c, s, n);
    return c;
}

static char ds_empty_string[1] = {0};

static char *ds_track_string(char *s) {
    DSStringNode *node;
    if (!s)
        return ds_empty_string;
    node = (DSStringNode *)malloc(sizeof(*node));
    if (!node) {
        free(s);
        return ds_empty_string;
    }
    node->string = s;
    node->gen = ds_string_gen;
    node->next = ds_strings;
    ds_strings = node;
    return s;
}

void ds_string_pool_tick(void) {
    DSStringNode **link = &ds_strings;
    ds_string_gen = ds_string_gen + 1;
    while (*link) {
        DSStringNode *node = *link;
        if (node->gen + 2 <= ds_string_gen) {
            *link = node->next;
            free(node->string);
            free(node);
        } else {
            link = &node->next;
        }
    }
}

char *ds_num_to_string(double number) {
    char buf[96];
    if (snprintf(buf, sizeof(buf), "%g", number) < 0)
        return NULL;
    return ds_track_string(ds_strdup(buf));
}

void ds_string_pool_reset(void) {
    DSStringNode *node = ds_strings;
    while (node) {
        DSStringNode *next = node->next;
        free(node->string);
        free(node);
        node = next;
    }
    ds_strings = NULL;
}

char *ds_concat(const char *left, const char *right) {
    size_t la = left ? strlen(left) : 0, lb = right ? strlen(right) : 0;
    char *out = (char *)malloc(la + lb + 1);
    if (!out)
        return ds_track_string(ds_strdup(""));
    if (la)
        memcpy(out, left, la);
    if (lb)
        memcpy(out + la, right, lb);
    out[la + lb] = '\0';
    return ds_track_string(out);
}

struct DSArray {
    double *data;
    size_t len, cap;
};

DSArray *arr_new(void) {
    DSArray *a = (DSArray *)calloc(1, sizeof(*a));
    if (!a)
        return NULL;
    a->cap = 8;
    a->data = (double *)malloc(a->cap * sizeof(double));
    if (!a->data) {
        free(a);
        return NULL;
    }
    return a;
}

void arr_push(DSArray *a, double v) {
    if (!a)
        return;
    if (a->len >= a->cap) {
        if (a->cap > SIZE_MAX / (2 * sizeof(double)))
            return;
        size_t nc = a->cap * 2;
        if (nc < 8)
            nc = 8;
        double *nd = (double *)realloc(a->data, nc * sizeof(double));
        if (!nd)
            return;
        a->data = nd;
        a->cap = nc;
    }
    a->data[a->len++] = v;
}

double arr_get(DSArray *a, double idx) {
    if (!a)
        return 0;
    long i = (long)idx;
    if (i < 0 || (size_t)i >= a->len)
        return 0;
    return a->data[i];
}

void arr_set(DSArray *a, double idx, double v) {
    if (!a)
        return;
    long i = (long)idx;
    if (i < 0)
        return;
    if ((size_t)i >= a->len) {
        while (a->len <= (size_t)i) {
            size_t old_len = a->len;
            arr_push(a, 0);
            if (a->len == old_len)
                return;
        }
    }
    a->data[i] = v;
}

double arr_len(DSArray *a) { return a ? (double)a->len : 0; }
void arr_clear(DSArray *a) { if (a) a->len = 0; }
void arr_free(DSArray *a) {
    if (!a)
        return;
    free(a->data);
    free(a);
}
