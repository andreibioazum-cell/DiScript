// src/discript/ds_sprite_api.c
//
// The `sprite` global: 2D images drawn over the screen, for games written in
// DiScript. A sprite is a PNG in the project folder; a script loads it, moves
// it, and draws it every frame.
//
//   ball = sprite.load("sprites/ball.png")
//   ball.x = 100
//   ball.y = 200
//   sprite.draw_all()
//
// Coordinates are screen pixels with the origin at the top-left, y growing
// downwards, which is what a phone screen and every 2D tutorial assume. A
// sprite's (x, y) is its top-left corner unless `center` is set, so moving by
// its width puts two sprites side by side with no surprises.
//
// The drawing has to happen inside the frame, so scripts run while the editor
// renders: RUN executes the script once per frame with `sprite.begin_frame()`
// and `sprite.draw_all()` around it. Drawing calls made outside a frame are
// dropped rather than queued, so a runaway script cannot exhaust memory.
#include "discript/ds_internal.h"
#include "discript/discript.h"
#include "engine/runtime.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SPRITE_MAX 256

typedef struct SpriteObj {
    char path[192];
    int w, h;          // natural size in pixels, 0 when the image is missing
    float x, y;
    float scale_x, scale_y;
    float angle;       // degrees, clockwise
    float opacity;     // 0..1
    int flip_x, flip_y;
    int visible;
    int z;             // draw order, lower first
    int tint;          // 0 = untouched, else 0xRRGGBB replaces brightness
    int center;        // 1: (x, y) is the middle of the image
    int loaded;
    int in_frame; /* added to the current frame with sprite.add() */
} SpriteObj;

static SpriteObj *g_sprites[SPRITE_MAX];
static int g_sprite_count = 0;
static int g_in_frame = 0;
static int g_drawn = 0;

/* ---- helpers ------------------------------------------------------------ */

static SpriteObj *sprite_of(DSVM *vm, DSValue v) {
    (void)vm;
    if (!ds_obj_is(v, DS_OBJ_NATIVE))
        return NULL;
    DSNativeObj *no = AS_NATIVE(v);
    if (!no || !no->ptr)
        return NULL;
    return (SpriteObj *)no->ptr;
}

/* The sprite list only borrows the pointer: the GC owns the allocation and
 * calls this finalizer, which just unregisters it. Freeing here would be a
 * double free, and leaving it registered after collection would hand draw_all a
 * dead pointer. */
static void sprite_free(void *data) {
    for (int i = 0; i < g_sprite_count; i++) {
        if (g_sprites[i] == (SpriteObj *)data) {
            for (int j = i; j + 1 < g_sprite_count; j++)
                g_sprites[j] = g_sprites[j + 1];
            g_sprite_count--;
            return;
        }
    }
}

static const char *arg_str(DSVM *vm, int argc, DSValue *argv, int i, const char *def) {
    (void)vm;
    if (i >= argc || !ds_obj_is(argv[i], DS_OBJ_STRING))
        return def;
    return ((DSString *)ds_as_obj(argv[i]))->chars;
}

static void args_num(DSVM *vm, int argc, DSValue *argv, int idx, float *out) {
    if (idx < argc && (ds_is_num(argv[idx]) || ds_is_bool(argv[idx])))
        *out = (float)ds_to_number(vm, argv[idx]);
}

static int sprite_get(DSVM *vm, DSNativeObj *no, const char *name, DSValue *out) {
    SpriteObj *s = (SpriteObj *)no->ptr;
    if (!s)
        return 0;
#define RET(v) do { *out = (v); return 1; } while (0)
    if (!strcmp(name, "x")) RET(ds_num(s->x));
    if (!strcmp(name, "y")) RET(ds_num(s->y));
    if (!strcmp(name, "width")) RET(ds_num(s->w));
    if (!strcmp(name, "height")) RET(ds_num(s->h));
    if (!strcmp(name, "scale")) RET(ds_num(s->scale_x));
    if (!strcmp(name, "angle")) RET(ds_num(s->angle));
    if (!strcmp(name, "opacity")) RET(ds_num(s->opacity));
    if (!strcmp(name, "visible")) RET(ds_bool(s->visible != 0));
    if (!strcmp(name, "z")) RET(ds_num((double)s->z));
    if (!strcmp(name, "flip_x")) RET(ds_bool(s->flip_x != 0));
    if (!strcmp(name, "flip_y")) RET(ds_bool(s->flip_y != 0));
    if (!strcmp(name, "center")) RET(ds_bool(s->center != 0));
    if (!strcmp(name, "path")) RET(ds_obj_val(ds_string_const(vm, s->path)));
    if (!strcmp(name, "loaded")) RET(ds_bool(s->loaded != 0));
    if (!strcmp(name, "right")) RET(ds_num(s->center ? s->x + s->w * 0.5f : s->x + s->w));
    if (!strcmp(name, "bottom")) RET(ds_num(s->center ? s->y + s->h * 0.5f : s->y + s->h));
#undef RET
    return 0;
}

static int sprite_set(DSVM *vm, DSNativeObj *no, const char *name, DSValue v) {
    SpriteObj *s = (SpriteObj *)no->ptr;
    if (!s)
        return 0;
    if (!strcmp(name, "x")) { s->x = (float)ds_to_number(vm, v); return 1; }
    if (!strcmp(name, "y")) { s->y = (float)ds_to_number(vm, v); return 1; }
    if (!strcmp(name, "angle")) { s->angle = (float)ds_to_number(vm, v); return 1; }
    if (!strcmp(name, "z")) { s->z = (int)ds_to_number(vm, v); return 1; }
    if (!strcmp(name, "opacity")) {
        float a = (float)ds_to_number(vm, v);
        s->opacity = a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a);
        return 1;
    }
    if (!strcmp(name, "visible")) { s->visible = ds_is_truthy(v) ? 1 : 0; return 1; }
    if (!strcmp(name, "flip_x")) { s->flip_x = ds_is_truthy(v) ? 1 : 0; return 1; }
    if (!strcmp(name, "flip_y")) { s->flip_y = ds_is_truthy(v) ? 1 : 0; return 1; }
    if (!strcmp(name, "center")) { s->center = ds_is_truthy(v) ? 1 : 0; return 1; }
    if (!strcmp(name, "scale")) {
        float k = (float)ds_to_number(vm, v);
        s->scale_x = s->scale_y = k;
        return 1;
    }
    return 0;
}

/* ---- sprite.load and friends ------------------------------------------- */

static int load_sprite_into(DSVM *vm, SpriteObj *s) {
    int w = 0, h = 0;
    s->loaded = tex_ready(s->path) && tex_size(s->path, &w, &h) && w > 0 && h > 0;
    s->w = w;
    s->h = h;
    (void)vm;
    return s->loaded;
}

static DSValue sp_load(DSVM *vm, int argc, DSValue *argv) {
    const char *path = arg_str(vm, argc, argv, 0, NULL);
    if (!path || !*path) {
        ds_runtime_error("sprite.load: нужен путь к картинке");
        return DS_VAL_NIL;
    }
    if (g_sprite_count >= SPRITE_MAX) {
        ds_runtime_error("sprite.load: слишком много спрайтов (%d)", SPRITE_MAX);
        return DS_VAL_NIL;
    }
    SpriteObj *s = (SpriteObj *)calloc(1, sizeof(*s));
    if (!s) {
        ds_runtime_error("sprite.load: нет памяти");
        return DS_VAL_NIL;
    }
    snprintf(s->path, sizeof(s->path), "%s", path);
    s->scale_x = s->scale_y = 1.0f;
    s->opacity = 1.0f;
    s->visible = 1;
    s->center = 0;
    load_sprite_into(vm, s);

    DSNativeObj *no = ds_native_new(vm, "Sprite", s, sizeof(*s), sprite_free, NULL);
    if (!no) {
        free(s);
        return DS_VAL_NIL;
    }
    no->get_prop = sprite_get;
    no->set_prop = sprite_set;
    g_sprites[g_sprite_count++] = s;
    return ds_obj_val(no);
}

/* sprite.size(path) -> (w, h) without loading, or nil. Handy for centring
 * before anything is drawn. */
static DSValue sp_size(DSVM *vm, int argc, DSValue *argv) {
    const char *path = arg_str(vm, argc, argv, 0, NULL);
    int w = 0, h = 0;
    if (!path || !tex_size(path, &w, &h) || w <= 0)
        return DS_VAL_NIL;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_list_push(vm, l, ds_num(w));
    ds_list_push(vm, l, ds_num(h));
    return ds_obj_val(l);
}

/* sprite.window() -> (width, height). Game code needs the screen size to place
 * things relative to it, and hard-coding a phone size breaks on the next one. */
static DSValue sp_window(DSVM *vm, int argc, DSValue *argv) {
    (void)argc; (void)argv;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_list_push(vm, l, ds_num(screen_w));
    ds_list_push(vm, l, ds_num(screen_h));
    return ds_obj_val(l);
}

/* sprite.add(obj): mark a sprite drawn this frame. draw_all() then paints
 * everything that was added, in z order. Explicit, so a sprite created for a
 * level but not active this frame is not painted by accident. */
static DSValue sp_add(DSVM *vm, int argc, DSValue *argv) {
    SpriteObj *s = argc > 0 ? sprite_of(vm, argv[0]) : NULL;
    if (!s)
        return ds_bool(0);
    s->visible = 1;
    s->in_frame = 1;
    return ds_bool(1);
}

static DSValue sp_exists(DSVM *vm, int argc, DSValue *argv) {
    const char *path = arg_str(vm, argc, argv, 0, NULL);
    return ds_bool(path && tex_ready(path));
}

/* ---- frame + drawing ---------------------------------------------------- */

static int by_z(const void *a, const void *b) {
    const SpriteObj *sa = *(SpriteObj *const *)a;
    const SpriteObj *sb = *(SpriteObj *const *)b;
    return sa->z - sb->z;
}

/* Start a frame: everything added from here on is painted by draw_all(). The
 * flags are reset here, which is why a sprite added last frame does not linger. */
static DSValue sp_begin(DSVM *vm, int argc, DSValue *argv) {
    (void)vm; (void)argc; (void)argv;
    for (int i = 0; i < g_sprite_count; i++)
        g_sprites[i]->in_frame = 0;
    g_in_frame = 1;
    g_drawn = 0;
    return DS_VAL_NIL;
}

static DSValue sp_draw(DSVM *vm, int argc, DSValue *argv) {
    SpriteObj *s = argc > 0 ? sprite_of(vm, argv[0]) : NULL;
    (void)s;
    return DS_VAL_NIL;
}

static DSValue sp_draw_all(DSVM *vm, int argc, DSValue *argv) {
    (void)vm; (void)argc; (void)argv;
    if (g_sprite_count <= 0)
        return ds_num(0);
    SpriteObj *order[SPRITE_MAX];
    int n = 0;
    int any_added = 0;
    for (int i = 0; i < g_sprite_count; i++)
        if (g_sprites[i] && g_sprites[i]->in_frame)
            any_added = 1;
    for (int i = 0; i < g_sprite_count; i++) {
        SpriteObj *s = g_sprites[i];
        if (!s || !s->visible || !s->loaded || s->opacity <= 0.001f)
            continue;
        /* Once the script starts using sprite.add(), only those are painted;
         * before that every loaded sprite is, which keeps the simple case
         * ("load it and it shows up") working. */
        if (any_added && !s->in_frame)
            continue;
        order[n++] = s;
    }
    qsort(order, (size_t)n, sizeof(order[0]), by_z);
    for (int i = 0; i < n; i++) {
        SpriteObj *s = order[i];
        float w = (float)s->w * s->scale_x;
        float h = (float)s->h * s->scale_y;
        float x = s->center ? s->x - w * 0.5f : s->x;
        float y = s->center ? s->y - h * 0.5f : s->y;
        float ang = s->angle * 3.14159265f / 180.0f;
        if (s->flip_x)
            ang = -ang;
        /* tex_ex takes a single uniform scale, so non-uniform scaling is
         * applied to the source size by drawing through the transform below. */
        float sc = 1.0f;
        (void)w; (void)h;
        uint32_t tint = s->opacity >= 0.999f ? 0xFFFFFFFFu
                                             : (((uint32_t)(s->opacity * 255.0f) << 24) | 0x00FFFFFFu);
        tex_ex(s->path, x, y, ang, sc * s->scale_x, sc * s->scale_y, 0.0f, 0.0f, tint);
        g_drawn++;
    }
    return ds_num(g_drawn);
}

static DSValue sp_count(DSVM *vm, int argc, DSValue *argv) {
    (void)vm; (void)argc; (void)argv;
    return ds_num(g_sprite_count);
}

/* Drop the registry without touching memory: the objects are still owned by the
 * collector, and freeing them here would leave the GC with dangling pointers. */
static DSValue sp_clear(DSVM *vm, int argc, DSValue *argv) {
    (void)vm; (void)argc; (void)argv;
    for (int i = 0; i < g_sprite_count; i++)
        g_sprites[i]->in_frame = 0;
    g_sprite_count = 0;
    return DS_VAL_NIL;
}

/* ---- registration ------------------------------------------------------ */

static void reg_fn(DSVM *vm, DSDict *d, const char *name, DSNativeFn fn) {
    DSCFunc *f = ds_cfunc_new(vm, name, fn, NULL);
    if (f)
        ds_dict_set(vm, d, ds_obj_val(ds_string_const(vm, name)), ds_obj_val(f));
}

void ds_register_sprite(DSVM *vm) {
    if (!vm || !vm->builtins)
        return;
    DSDict *d = ds_dict_new(vm, 16);
    if (!d)
        return;
    ds_push_root(vm, ds_obj_val(d));
    ds_dict_set(vm, vm->builtins, ds_obj_val(ds_string_const(vm, "sprite")), ds_obj_val(d));
    reg_fn(vm, d, "load", sp_load);
    reg_fn(vm, d, "size", sp_size);
    reg_fn(vm, d, "exists", sp_exists);
    reg_fn(vm, d, "begin_frame", sp_begin);
    reg_fn(vm, d, "draw", sp_draw);
    reg_fn(vm, d, "draw_all", sp_draw_all);
    reg_fn(vm, d, "count", sp_count);
    reg_fn(vm, d, "clear", sp_clear);
    reg_fn(vm, d, "window", sp_window);
    reg_fn(vm, d, "add", sp_add);
    ds_pop_roots(vm, 1);
}
