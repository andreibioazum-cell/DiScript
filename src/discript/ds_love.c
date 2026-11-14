/* LÖVE2D-compatible game API: love.graphics / timer / touch / mouse /
 * keyboard / audio / math / image / filesystem / window, plus the
 * load / update(dt) / draw lifecycle bridge into the Vulkan renderer. */

#include "discript/ds_ast.h"
#include "discript/discript.h"
#include "cpp/ds_cpp.h"
#include "engine/runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- local helpers ------------------------------------------------------ */
static DSValue larg(int argc, DSValue *argv, int i) { return i < argc ? argv[i] : DS_VAL_NIL; }

static double lnum(DSVM *vm, int argc, DSValue *argv, int i, double def) {
    if (i < argc)
        return ds_to_number(vm, argv[i]);
    return def;
}

static const char *lstr(DSVM *vm, int argc, DSValue *argv, int i, const char *def) {
    if (i < argc && ds_obj_is(argv[i], DS_OBJ_STRING))
        return AS_CSTR(argv[i]);
    if (i < argc && !ds_is_nil(argv[i])) {
        DSString *s = ds_val_to_string(vm, argv[i]);
        return s ? s->chars : def;
    }
    return def;
}

static void lreg(DSVM *vm, DSDict *dict, const char *name, DSNativeFn fn) {
    DSCFunc *f = ds_cfunc_new(vm, name, fn, NULL);
    if (!f)
        return;
    ds_push_root(vm, ds_obj_val(f));
    ds_dict_set(vm, dict, ds_obj_val(ds_string_const(vm, name)), ds_obj_val(f));
    ds_pop_roots(vm, 1);
}

static DSDict *subtable(DSVM *vm, DSDict *parent, const char *name) {
    DSDict *d = ds_dict_new(vm, 24);
    if (!d)
        return NULL;
    ds_push_root(vm, ds_obj_val(d));
    ds_dict_set(vm, parent, ds_obj_val(ds_string_const(vm, name)), ds_obj_val(d));
    ds_pop_roots(vm, 1);
    return d;
}

/* ============================================================================
 * love.graphics
 * ============================================================================ */

static uint32_t to_argb(DSValue v) {
    if (ds_is_num(v))
        return (uint32_t)ds_as_num(v);
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(v);
        double a = 255, r = 255, g = 255, b = 255;
        if (l->len >= 3) {
            r = ds_as_num(l->items[0]);
            g = ds_as_num(l->items[1]);
            b = ds_as_num(l->items[2]);
            if (l->len >= 4)
                a = ds_as_num(l->items[3]);
        }
        if (r <= 1.0 && g <= 1.0 && b <= 1.0) {
            r *= 255;
            g *= 255;
            b *= 255;
            a = (l->len >= 4 && a <= 1.0) ? a * 255 : (l->len >= 4 ? a : 255);
        }
        uint32_t A = (uint32_t)(a < 0 ? 0 : a > 255 ? 255 : a);
        uint32_t R = (uint32_t)(r < 0 ? 0 : r > 255 ? 255 : r);
        uint32_t G = (uint32_t)(g < 0 ? 0 : g > 255 ? 255 : g);
        uint32_t B = (uint32_t)(b < 0 ? 0 : b > 255 ? 255 : b);
        return (A << 24) | (R << 16) | (G << 8) | B;
    }
    return 0xFFFFFFFFu;
}

/* Convert a colour given either as one packed number / {r,g,b[,a]} list, or as
 * separate r, g, b[, a] components. Components may be 0..1 (LOVE style) or
 * 0..255. */
static uint32_t argb_from_args(int argc, DSValue *argv) {
    if (argc <= 0)
        return 0xFFFFFFFFu;
    if (argc == 1 || ds_obj_is(argv[0], DS_OBJ_LIST))
        return to_argb(argv[0]);
    double r = ds_as_num(argv[0]), g = ds_as_num(argv[1]), b = ds_as_num(argv[2]);
    double a = (argc >= 4) ? ds_as_num(argv[3]) : 255.0;
    if (r <= 1.0 && g <= 1.0 && b <= 1.0) {
        r *= 255.0;
        g *= 255.0;
        b *= 255.0;
        a = (argc >= 4) ? (a <= 1.0 ? a * 255.0 : a) : 255.0;
    } else if (argc >= 4 && a <= 1.0) {
        a *= 255.0;
    }
    uint32_t A = (uint32_t)(a < 0 ? 0 : a > 255 ? 255 : a);
    uint32_t R = (uint32_t)(r < 0 ? 0 : r > 255 ? 255 : r);
    uint32_t G = (uint32_t)(g < 0 ? 0 : g > 255 ? 255 : g);
    uint32_t B = (uint32_t)(b < 0 ? 0 : b > 255 ? 255 : b);
    return (A << 24) | (R << 16) | (G << 8) | B;
}

static DSValue lg_setColor(DSVM *vm, int argc, DSValue *argv) {
    ds_vm_set_draw_color(vm, argb_from_args(argc, argv));
    return DS_VAL_NIL;
}

static DSValue lg_getColor(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    uint32_t c = vm->love.draw_color;
    DSList *l = ds_list_new(vm, 4);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num((double)((c >> 16) & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num((double)((c >> 8) & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num((double)(c & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num((double)((c >> 24) & 0xFF) / 255.0));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue lg_setBackgroundColor(DSVM *vm, int argc, DSValue *argv) {
    vm->love.bg_color = argb_from_args(argc, argv);
    return DS_VAL_NIL;
}



static DSValue lg_getBackgroundColor(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    uint32_t c = vm->love.bg_color;
    DSList *l = ds_list_new(vm, 4);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num((double)((c >> 16) & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num((double)((c >> 8) & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num((double)(c & 0xFF) / 255.0));
    ds_list_push(vm, l, ds_num(1.0));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue lg_rectangle(DSVM *vm, int argc, DSValue *argv) {
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0);
    double w = lnum(vm, argc, argv, 3, 0), h = lnum(vm, argc, argv, 4, 0);
    uint32_t color = vm->love.draw_color;
    if (strcmp(mode, "line") == 0)
        rect_line((float)x, (float)y, (float)w, (float)h, vm->love.line_width, color);
    else
        rect((float)x, (float)y, (float)w, (float)h, color);
    return DS_VAL_NIL;
}

static DSValue lg_rect(DSVM *vm, int argc, DSValue *argv) {
    /* love.graphics.rect(mode, x, y, w, h, r?) */
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0);
    double w = lnum(vm, argc, argv, 3, 0), h = lnum(vm, argc, argv, 4, 0);
    double r = lnum(vm, argc, argv, 5, 0);
    uint32_t color = vm->love.draw_color;
    if (r > 0)
        roundrect((float)x, (float)y, (float)w, (float)h, (float)r,
                  strcmp(mode, "line") == 0 ? (color & 0x00FFFFFFu) : color);
    else
        lg_rectangle(vm, argc, argv);
    return DS_VAL_NIL;
}

static DSValue lg_circle(DSVM *vm, int argc, DSValue *argv) {
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0), r = lnum(vm, argc, argv, 3, 0);
    uint32_t color = vm->love.draw_color;
    if (strcmp(mode, "line") == 0)
        ring((float)x, (float)y, (float)r, vm->love.line_width, color);
    else
        circle((float)x, (float)y, (float)r, color);
    return DS_VAL_NIL;
}

static DSValue lg_ellipse(DSVM *vm, int argc, DSValue *argv) {
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0);
    double rx = lnum(vm, argc, argv, 3, 0), ry = lnum(vm, argc, argv, 4, 0);
    uint32_t color = vm->love.draw_color;
    if (strcmp(mode, "line") == 0)
        ellipse_line((float)x, (float)y, (float)rx, (float)ry, vm->love.line_width, color);
    else
        ellipse_fill((float)x, (float)y, (float)rx, (float)ry, color);
    return DS_VAL_NIL;
}

static DSValue lg_arc(DSVM *vm, int argc, DSValue *argv) {
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0), r = lnum(vm, argc, argv, 3, 0);
    double a1 = lnum(vm, argc, argv, 4, 0) * 180.0 / 3.14159265358979323846;
    double a2 = lnum(vm, argc, argv, 5, 6.283185) * 180.0 / 3.14159265358979323846;
    uint32_t color = vm->love.draw_color;
    double sweep = a2 - a1;
    if (sweep < 0)
        sweep += 360.0;
    if (strcmp(mode, "line") == 0)
        arc_line((float)x, (float)y, (float)r, (float)a1, (float)sweep, vm->love.line_width, 0, color);
    else
        sector((float)x, (float)y, (float)r, (float)a1, (float)sweep, color);
    return DS_VAL_NIL;
}

static DSValue lg_line(DSVM *vm, int argc, DSValue *argv) {
    /* line(x1,y1,x2,y2,...) — draws a polyline */
    int pairs = (argc - 2) / 2;
    if (pairs <= 0) {
        /* two-point form */
        line((float)lnum(vm, argc, argv, 0, 0), (float)lnum(vm, argc, argv, 1, 0),
             (float)lnum(vm, argc, argv, 2, 0), (float)lnum(vm, argc, argv, 3, 0), vm->love.line_width,
             vm->love.draw_color);
        return DS_VAL_NIL;
    }
    if (argc == 4 || argc == 5) {
        line((float)lnum(vm, argc, argv, 0, 0), (float)lnum(vm, argc, argv, 1, 0),
             (float)lnum(vm, argc, argv, 2, 0), (float)lnum(vm, argc, argv, 3, 0), vm->love.line_width,
             vm->love.draw_color);
        return DS_VAL_NIL;
    }
    float pts[64];
    int n = argc / 2;
    if (n > 32)
        n = 32;
    for (int i = 0; i < n; i++) {
        pts[i * 2] = (float)lnum(vm, argc, argv, i * 2, 0);
        pts[i * 2 + 1] = (float)lnum(vm, argc, argv, i * 2 + 1, 0);
    }
    poly_line(pts, n, vm->love.line_width, 0, vm->love.draw_color);
    return DS_VAL_NIL;
}

static DSValue lg_polygon(DSVM *vm, int argc, DSValue *argv) {
    const char *mode = lstr(vm, argc, argv, 0, "fill");
    int n = (argc - 1) / 2;
    if (n > 64)
        n = 64;
    float pts[128];
    for (int i = 0; i < n; i++) {
        pts[i * 2] = (float)lnum(vm, argc, argv, 1 + i * 2, 0);
        pts[i * 2 + 1] = (float)lnum(vm, argc, argv, 2 + i * 2, 0);
    }
    if (strcmp(mode, "fill") == 0)
        poly_fill(pts, n, vm->love.draw_color);
    else
        poly_line(pts, n, vm->love.line_width, 1, vm->love.draw_color);
    return DS_VAL_NIL;
}

static DSValue lg_points(DSVM *vm, int argc, DSValue *argv) {
    for (int i = 0; i + 1 < argc; i += 2)
        rect((float)lnum(vm, argc, argv, i, 0) - 1, (float)lnum(vm, argc, argv, i + 1, 0) - 1, 2, 2,
             vm->love.draw_color);
    return DS_VAL_NIL;
}

static DSValue lg_print(DSVM *vm, int argc, DSValue *argv) {
    const char *text = lstr(vm, argc, argv, 0, "");
    float x = (float)lnum(vm, argc, argv, 1, 0), y = (float)lnum(vm, argc, argv, 2, 0);
    float scale = 1.0f;
    if (argc > 3 && ds_is_num(argv[3]))
        scale = (float)ds_as_num(argv[3]);
    else if (argc > 3 && !ds_is_nil(argv[3]))
        scale = 1.0f;
    text_scaled(text, x, y, vm->love.draw_color, scale);
    return DS_VAL_NIL;
}

static DSValue lg_printf(DSVM *vm, int argc, DSValue *argv) {
    const char *text = lstr(vm, argc, argv, 0, "");
    double x = lnum(vm, argc, argv, 1, 0), y = lnum(vm, argc, argv, 2, 0);
    double limit = lnum(vm, argc, argv, 3, 0);
    double align = lnum(vm, argc, argv, 4, 0); /* 0=left 1=center 2=right */
    /* simple word wrap */
    char linebuf[512];
    char outbuf[8192];
    outbuf[0] = '\0';
    size_t out_len = 0;
    const char *p = text;
    double cur_y = y;
    float lh = text_line_height(1.0f);
    while (*p) {
        const char *space = strchr(p, ' ');
        size_t chunk = space ? (size_t)(space - p) : strlen(p);
        if (chunk > sizeof(linebuf) - 1)
            chunk = sizeof(linebuf) - 1;
        memcpy(linebuf, p, chunk);
        linebuf[chunk] = '\0';
        float w = text_width_scaled(linebuf, 1.0f);
        if (limit > 0 && w > limit) {
            /* print accumulated buffer */
            if (out_len > 0) {
                float ox = (float)x;
                float ow = text_width_scaled(outbuf, 1.0f);
                if (align == 1)
                    ox = (float)(x + (limit - ow) / 2);
                else if (align == 2)
                    ox = (float)(x + limit - ow);
                text_scaled(outbuf, ox, (float)cur_y, vm->love.draw_color, 1.0f);
                cur_y += lh;
                out_len = 0;
                outbuf[0] = '\0';
            }
            memcpy(outbuf + out_len, linebuf, chunk);
            out_len += chunk;
        } else {
            if (out_len + chunk + 2 < sizeof(outbuf)) {
                memcpy(outbuf + out_len, linebuf, chunk);
                out_len += chunk;
                if (space) {
                    outbuf[out_len++] = ' ';
                }
                outbuf[out_len] = '\0';
            } else {
                if (out_len > 0) {
                    float ox = (float)x;
                    float ow = text_width_scaled(outbuf, 1.0f);
                    if (align == 1)
                        ox = (float)(x + (limit - ow) / 2);
                    else if (align == 2)
                        ox = (float)(x + limit - ow);
                    text_scaled(outbuf, ox, (float)cur_y, vm->love.draw_color, 1.0f);
                    cur_y += lh;
                }
                memcpy(outbuf, linebuf, chunk);
                out_len = chunk;
                outbuf[out_len] = '\0';
            }
        }
        if (!space)
            break;
        p = space + 1;
    }
    if (out_len > 0) {
        float ox = (float)x;
        float ow = text_width_scaled(outbuf, 1.0f);
        if (align == 1)
            ox = (float)(x + (limit - ow) / 2);
        else if (align == 2)
            ox = (float)(x + limit - ow);
        text_scaled(outbuf, ox, (float)cur_y, vm->love.draw_color, 1.0f);
    }
    return DS_VAL_NIL;
}

static DSValue lg_getWidth(DSVM *vm, int argc, DSValue *argv) {
    const char *t = lstr(vm, argc, argv, 0, "");
    return ds_num(text_width_scaled(t, 1.0f));
}
static DSValue lg_getHeight(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return ds_num(text_line_height(1.0f));
}

static DSValue lg_circleLine(DSVM *vm, int argc, DSValue *argv) {
    float x = (float)lnum(vm, argc, argv, 0, 0), y = (float)lnum(vm, argc, argv, 1, 0);
    float r = (float)lnum(vm, argc, argv, 2, 0);
    ring(x, y, r, vm->love.line_width, vm->love.draw_color);
    return DS_VAL_NIL;
}

static DSValue lg_setLineWidth(DSVM *vm, int argc, DSValue *argv) {
    vm->love.line_width = (float)lnum(vm, argc, argv, 0, 1.0);
    if (vm->love.line_width < 0.5f)
        vm->love.line_width = 0.5f;
    return DS_VAL_NIL;
}

static DSValue lg_getLineWidth(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.line_width);
}

static DSValue lg_clear(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    /* Clearing happens through the background colour each frame. */
    return DS_VAL_NIL;
}

static DSValue lg_getWidthScreen(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.vp_w);
}
static DSValue lg_getHeightScreen(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.vp_h);
}
static DSValue lg_getDimensions(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(vm->love.vp_w));
    ds_list_push(vm, l, ds_num(vm->love.vp_h));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue lg_push(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    if (vm->love.xf_sp < DS_LOVE_MAX_STACK - 1) {
        DSLoveTransform *cur = &vm->love.xf_stack[vm->love.xf_sp];
        DSLoveTransform *next = &vm->love.xf_stack[++vm->love.xf_sp];
        *next = *cur;
    }
    return DS_VAL_NIL;
}

static DSValue lg_pop(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    if (vm->love.xf_sp > 0)
        vm->love.xf_sp--;
    return DS_VAL_NIL;
}

static DSValue lg_translate(DSVM *vm, int argc, DSValue *argv) {
    DSLoveTransform *xf = &vm->love.xf_stack[vm->love.xf_sp];
    xf->tx += (float)lnum(vm, argc, argv, 0, 0);
    xf->ty += (float)lnum(vm, argc, argv, 1, 0);
    return DS_VAL_NIL;
}

static DSValue lg_rotate(DSVM *vm, int argc, DSValue *argv) {
    DSLoveTransform *xf = &vm->love.xf_stack[vm->love.xf_sp];
    xf->rot += (float)lnum(vm, argc, argv, 0, 0);
    return DS_VAL_NIL;
}

static DSValue lg_scale(DSVM *vm, int argc, DSValue *argv) {
    DSLoveTransform *xf = &vm->love.xf_stack[vm->love.xf_sp];
    double sx = lnum(vm, argc, argv, 0, 1), sy = lnum(vm, argc, argv, 1, sx);
    xf->sx *= (float)sx;
    xf->sy *= (float)sy;
    return DS_VAL_NIL;
}

static DSValue lg_origin(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return DS_VAL_NIL;
}

static DSValue lg_setScissor(DSVM *vm, int argc, DSValue *argv) {
    double x = lnum(vm, argc, argv, 0, 0), y = lnum(vm, argc, argv, 1, 0);
    double w = lnum(vm, argc, argv, 2, 0), h = lnum(vm, argc, argv, 3, 0);
    vm->love.scissor_on = 1;
    vm->love.sc_x = (float)x;
    vm->love.sc_y = (float)y;
    vm->love.sc_w = (float)w;
    vm->love.sc_h = (float)h;
    ds_gfx_scissor(x, y, w, h);
    return DS_VAL_NIL;
}

static DSValue lg_noScissor(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    vm->love.scissor_on = 0;
    ds_gfx_scissor(0, 0, 0, 0);
    return DS_VAL_NIL;
}

static DSValue lg_newImage(DSVM *vm, int argc, DSValue *argv) {
    const char *name = lstr(vm, argc, argv, 0, "");
    if (!png_load(name)) {
        ds_error(vm, 0, "love.graphics.newImage: не удалось загрузить '%s'", name);
        return DS_VAL_NIL;
    }
    int w = 0, h = 0;
    tex_size(name, &w, &h);
    DSNativeObj *no = ds_native_new(vm, "Image", (void *)ds_string_const(vm, name), sizeof(DSString), NULL, NULL);
    if (!no)
        return DS_VAL_NIL;
    DSDict *m = ds_dict_new(vm, 8);
    if (m) {
        ds_push_root(vm, ds_obj_val(no));
        ds_push_root(vm, ds_obj_val(m));
        no->methods = m;
        ds_dict_set_cstr(vm, m, "__image_w", ds_num(w));
        ds_dict_set_cstr(vm, m, "__image_h", ds_num(h));
        ds_dict_set_cstr(vm, m, "__image_name", ds_obj_val(ds_string_const(vm, name)));
        ds_pop_roots(vm, 2);
    }
    return ds_obj_val(no);
}

static const char *image_name(DSValue v) {
    if (!ds_obj_is(v, DS_OBJ_NATIVE))
        return NULL;
    DSNativeObj *no = AS_NATIVE(v);
    if (!no->type_tag || strcmp(no->type_tag, "Image") != 0)
        return NULL;
    return (const char *)no->ptr;
}

static DSValue lg_draw(DSVM *vm, int argc, DSValue *argv) {
    const char *name = image_name(larg(argc, argv, 0));
    if (!name) {
        ds_error(vm, 0, "love.graphics.draw: ожидалось изображение");
        return DS_VAL_NIL;
    }
    float x = (float)lnum(vm, argc, argv, 1, 0), y = (float)lnum(vm, argc, argv, 2, 0);
    float rot = (float)lnum(vm, argc, argv, 3, 0);
    float sx = (float)lnum(vm, argc, argv, 4, 1), sy = (float)lnum(vm, argc, argv, 5, sx);
    float ox = (float)lnum(vm, argc, argv, 6, 0), oy = (float)lnum(vm, argc, argv, 7, 0);
    tex_ex(name, x, y, rot, sx, sy, ox, oy, vm->love.draw_color);
    return DS_VAL_NIL;
}

static DSValue lg_drawTinted(DSVM *vm, int argc, DSValue *argv) {
    const char *name = image_name(larg(argc, argv, 0));
    if (!name)
        return DS_VAL_NIL;
    float x = (float)lnum(vm, argc, argv, 1, 0), y = (float)lnum(vm, argc, argv, 2, 0);
    tex_ex(name, x, y, 0, 1, 1, 0, 0, to_argb(larg(argc, argv, 3)));
    return DS_VAL_NIL;
}

/* ---- Canvas-style full-screen gradient (used by the IDE too) ------------ */
static DSValue lg_gradient(DSVM *vm, int argc, DSValue *argv) {
    float x = (float)lnum(vm, argc, argv, 0, 0), y = (float)lnum(vm, argc, argv, 1, 0);
    float w = (float)lnum(vm, argc, argv, 2, vm->love.vp_w), h = (float)lnum(vm, argc, argv, 3, vm->love.vp_h);
    uint32_t top = to_argb(larg(argc, argv, 4));
    uint32_t bot = to_argb(larg(argc, argv, 5));
    ds_panel_gradient(x, y, w, h, 0.0f, top, bot);
    return DS_VAL_NIL;
}

/* ============================================================================
 * love.timer
 * ============================================================================ */

static DSValue lt_getDelta(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.last_dt);
}
static DSValue lt_getTime(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(ds_time_sec() - vm->love.start_time);
}
static DSValue lt_getFPS(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.fps_smooth > 0.0 ? 1.0 / vm->love.fps_smooth : 0.0);
}
static DSValue lt_step(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.last_dt);
}

/* ============================================================================
 * love.audio
 * ============================================================================ */

static DSValue la_play(DSVM *vm, int argc, DSValue *argv) {
    const char *name = lstr(vm, argc, argv, 0, "");
    snd_play(name);
    return DS_VAL_NIL;
}

static DSValue la_loop(DSVM *vm, int argc, DSValue *argv) {
    const char *name = lstr(vm, argc, argv, 0, "");
    snd_loop(name);
    return DS_VAL_NIL;
}

static DSValue la_stop(DSVM *vm, int argc, DSValue *argv) {
    const char *name = lstr(vm, argc, argv, 0, "");
    if (name[0])
        snd_stop(name);
    else
        snd_stop_all();
    return DS_VAL_NIL;
}

static DSValue la_stopAll(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    snd_stop_all();
    return DS_VAL_NIL;
}

static DSValue la_setVolume(DSVM *vm, int argc, DSValue *argv) {
    const char *name = lstr(vm, argc, argv, 0, "");
    double vol = lnum(vm, argc, argv, 1, 1.0);
    snd_volume(name, vol);
    return DS_VAL_NIL;
}

static DSValue la_tone(DSVM *vm, int argc, DSValue *argv) {
    double freq = lnum(vm, argc, argv, 0, 440), dur = lnum(vm, argc, argv, 1, 0.2);
    const char *wave = lstr(vm, argc, argv, 2, "sine");
    double vol = lnum(vm, argc, argv, 3, 0.5);
    return ds_num(snd_synth_tone(freq, dur, wave, vol));
}

static DSValue la_sfx(DSVM *vm, int argc, DSValue *argv) {
    const char *preset = lstr(vm, argc, argv, 0, "click");
    double vol = lnum(vm, argc, argv, 1, 0.6);
    return ds_num(snd_synth_sfx(preset, vol));
}

/* ============================================================================
 * love.math / love.system / love.window / love.filesystem
 * ============================================================================ */

static DSValue lm_random(DSVM *vm, int argc, DSValue *argv) {
    double lo = 0, hi = 1;
    if (argc >= 2) {
        lo = lnum(vm, argc, argv, 0, 0);
        hi = lnum(vm, argc, argv, 1, 1);
    } else if (argc == 1) {
        hi = lnum(vm, argc, argv, 0, 1);
    }
    uint64_t *s = vm->love.rng_state;
    uint64_t x = s[0] ^ (s[2] << 17);
    s[0] = s[1];
    s[1] = s[2];
    s[2] = s[3];
    s[3] ^= (s[3] << 43) ^ x ^ (x >> 11);
    double t = (double)(s[3] >> 11) * (1.0 / 9007199254740992.0);
    return ds_num(lo + (hi - lo) * t);
}

static DSValue lm_noise(DSVM *vm, int argc, DSValue *argv) {
    double x = lnum(vm, argc, argv, 0, 0), y = lnum(vm, argc, argv, 1, 0);
    if (argc >= 3)
        return ds_num(ds_simplex3d(x, y, lnum(vm, argc, argv, 2, 0)));
    return ds_num(ds_simplex2d(x, y));
}

static DSValue lm_clamp(DSVM *vm, int argc, DSValue *argv) {
    double v = lnum(vm, argc, argv, 0, 0), lo = lnum(vm, argc, argv, 1, 0), hi = lnum(vm, argc, argv, 2, 1);
    return ds_num(v < lo ? lo : (v > hi ? hi : v));
}

static DSValue lm_lerp(DSVM *vm, int argc, DSValue *argv) {
    double a = lnum(vm, argc, argv, 0, 0), b = lnum(vm, argc, argv, 1, 0), t = lnum(vm, argc, argv, 2, 0);
    return ds_num(a + (b - a) * t);
}

static DSValue lw_width(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.vp_w);
}
static DSValue lw_height(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.vp_h);
}

static DSValue lfs_read(DSVM *vm, int argc, DSValue *argv) {
    /* Reuse fs.read semantics with project dir */
    extern DSValue ds_fs_read_for_love(DSVM *vm, const char *name);
    return ds_fs_read_for_love(vm, lstr(vm, argc, argv, 0, ""));
}
static DSValue lfs_write(DSVM *vm, int argc, DSValue *argv) {
    extern int ds_fs_write_for_love(DSVM *vm, const char *name, DSString *content);
    DSString *c = ds_val_to_string(vm, larg(argc, argv, 1));
    return ds_bool(ds_fs_write_for_love(vm, lstr(vm, argc, argv, 0, ""), c));
}

/* ============================================================================
 * PUBLIC LÖVE2D BRIDGE (called from the IDE / android_main)
 * ============================================================================ */

void ds_vm_set_draw_color(DSVM *vm, uint32_t argb) { vm->love.draw_color = argb; }

int ds_vm_has_love_loop(const DSVM *vm) {
    return vm && (!ds_is_nil(vm->love.cb_update) || !ds_is_nil(vm->love.cb_draw));
}

static DSValue lookup_cb(DSVM *vm, const char *name) {
    DSValue v = ds_lookup_name(vm, vm->main_module, ds_string_const(vm, name));
    if (vm->has_error) {
        vm->has_error = 0;
        vm->err_msg[0] = '\0';
        return DS_VAL_NIL;
    }
    return v;
}

void ds_vm_love_refresh(DSVM *vm) {
    if (!vm)
        return;
    DSLoveState *L = &vm->love;
    L->cb_load = lookup_cb(vm, "load");
    L->cb_update = lookup_cb(vm, "update");
    L->cb_draw = lookup_cb(vm, "draw");
    L->cb_touchpressed = lookup_cb(vm, "touchpressed");
    L->cb_touchmoved = lookup_cb(vm, "touchmoved");
    L->cb_touchreleased = lookup_cb(vm, "touchreleased");
    L->cb_mousepressed = lookup_cb(vm, "mousepressed");
    L->cb_mousereleased = lookup_cb(vm, "mousereleased");
    L->cb_mousemoved = lookup_cb(vm, "mousemoved");
    L->cb_keypressed = lookup_cb(vm, "keypressed");
    L->cb_keyreleased = lookup_cb(vm, "keyreleased");
}

int ds_vm_love_load(DSVM *vm) {
    if (!vm)
        return 0;
    ds_vm_love_refresh(vm);
    DSLoveState *L = &vm->love;
    L->start_time = ds_time_sec();
    if (!ds_is_nil(L->cb_load)) {
        ds_call_value(vm, L->cb_load, 0, NULL);
        if (vm->has_error)
            return 0;
    }
    return 1;
}

int ds_vm_love_update(DSVM *vm, double dt_sec) {
    if (!vm)
        return 0;
    DSLoveState *L = &vm->love;
    L->last_dt = dt_sec;
    if (dt_sec > 0.0)
        L->fps_smooth = L->fps_smooth <= 0.0 ? dt_sec : (L->fps_smooth * 0.9 + dt_sec * 0.1);
    if (!ds_is_nil(L->cb_update)) {
        DSValue arg = ds_num(dt_sec);
        ds_call_value(vm, L->cb_update, 1, &arg);
        if (vm->has_error)
            return 0;
    }
    return 1;
}

int ds_vm_love_draw(DSVM *vm) {
    if (!vm)
        return 0;
    DSLoveState *L = &vm->love;
    L->xf_sp = 0;
    memset(&L->xf_stack[0], 0, sizeof(DSLoveTransform));
    L->xf_stack[0].sx = 1.0f;
    L->xf_stack[0].sy = 1.0f;
    if (!ds_is_nil(L->cb_draw)) {
        ds_call_value(vm, L->cb_draw, 0, NULL);
        if (vm->has_error)
            return 0;
    }
    return 1;
}

int ds_vm_love_touch(DSVM *vm, int action, int pointer_id, float x, float y) {
    if (!vm)
        return 0;
    DSLoveState *L = &vm->love;
    for (int i = 0; i < DS_LOVE_MAX_TOUCHES; i++) {
        if (L->touches[i].id == pointer_id) {
            L->touches[i].x = x;
            L->touches[i].y = y;
            L->touches[i].active = (action != 2);
            break;
        }
        if (action == 0 && !L->touches[i].active) {
            L->touches[i].id = pointer_id;
            L->touches[i].x = x;
            L->touches[i].y = y;
            L->touches[i].active = 1;
            break;
        }
    }
    DSValue idv = ds_num(pointer_id), xv = ds_num(x), yv = ds_num(y);
    /* LÖVE passes (id, x, y). A game that only wants the position writes
     * touchpressed(x, y), and calling a 2-parameter function with 3 arguments
     * is an error, so the count follows the function's own arity. */
    DSValue argv[3] = {idv, xv, yv};
    DSValue cb = action == 0 ? L->cb_touchpressed : (action == 1 ? L->cb_touchmoved : L->cb_touchreleased);
    if (!ds_is_nil(cb)) {
        int n = 3;
        if (ds_obj_is(cb, DS_OBJ_CLOSURE)) {
            DSFuncProto *pr = AS_CLOSURE(cb)->proto;
            if (pr && pr->arity >= 0 && pr->arity < 3 && !pr->has_varargs)
                n = pr->arity;
        }
        /* With fewer than 3 arguments the id is dropped, so the values line up
         * with (x, y) as the script expects. */
        DSValue use[3];
        if (n == 3) {
            use[0] = argv[0]; use[1] = argv[1]; use[2] = argv[2];
        } else if (n == 2) {
            use[0] = argv[1]; use[1] = argv[2];
        } else if (n == 1) {
            use[0] = argv[1];
        }
        ds_call_value(vm, cb, n, use);
        if (vm->has_error)
            return 0;
    }
    DSValue mcb = action == 0 ? L->cb_mousepressed : (action == 1 ? L->cb_mousemoved : L->cb_mousereleased);
    if (!ds_is_nil(mcb)) {
        DSValue margv[3] = {xv, yv, ds_num(action == 0 ? 1 : 0)};
        ds_call_value(vm, mcb, 3, margv);
        if (vm->has_error)
            return 0;
    }
    return 1;
}

int ds_vm_love_key(DSVM *vm, const char *key_name, int pressed) {
    if (!vm)
        return 0;
    DSLoveState *L = &vm->love;
    DSValue cb = pressed ? L->cb_keypressed : L->cb_keyreleased;
    if (ds_is_nil(cb))
        return 1;
    DSValue arg = ds_obj_val(ds_string_const(vm, key_name ? key_name : "?"));
    ds_call_value(vm, cb, 1, &arg);
    return !vm->has_error;
}

void ds_vm_love_set_viewport(DSVM *vm, float x, float y, float w, float h) {
    if (!vm)
        return;
    vm->love.vp_x = x;
    vm->love.vp_y = y;
    vm->love.vp_w = w;
    vm->love.vp_h = h;
    if (vm->love.bg_color == 0)
        vm->love.bg_color = 0xFF101418u;
}

uint32_t ds_vm_love_bg_color(const DSVM *vm) { return vm ? vm->love.bg_color : 0xFF101418u; }

/* ============================================================================
 * REGISTRATION
 * ============================================================================ */

void ds_register_love(DSVM *vm) {
    DSDict *love = ds_dict_new(vm, 32);
    if (!love)
        return;
    ds_push_root(vm, ds_obj_val(love));
    ds_dict_set(vm, vm->builtins, ds_obj_val(ds_string_const(vm, "love")), ds_obj_val(love));
    vm->love.love_mod_dict = love;

    /* ---- graphics ---- */
    DSDict *g = subtable(vm, love, "graphics");
    if (g) {
        lreg(vm, g, "setColor", lg_setColor);
        lreg(vm, g, "getColor", lg_getColor);
        lreg(vm, g, "setBackgroundColor", lg_setBackgroundColor);
        lreg(vm, g, "getBackgroundColor", lg_getBackgroundColor);
        lreg(vm, g, "rectangle", lg_rectangle);
        lreg(vm, g, "rect", lg_rect);
        lreg(vm, g, "circle", lg_circle);
        lreg(vm, g, "circleLine", lg_circleLine);
        lreg(vm, g, "ellipse", lg_ellipse);
        lreg(vm, g, "arc", lg_arc);
        lreg(vm, g, "line", lg_line);
        lreg(vm, g, "polygon", lg_polygon);
        lreg(vm, g, "points", lg_points);
        lreg(vm, g, "print", lg_print);
        lreg(vm, g, "printf", lg_printf);
        lreg(vm, g, "getWidth", lg_getWidth);
        lreg(vm, g, "getHeight", lg_getHeight);
        lreg(vm, g, "setLineWidth", lg_setLineWidth);
        lreg(vm, g, "getLineWidth", lg_getLineWidth);
        lreg(vm, g, "clear", lg_clear);
        lreg(vm, g, "push", lg_push);
        lreg(vm, g, "pop", lg_pop);
        lreg(vm, g, "translate", lg_translate);
        lreg(vm, g, "rotate", lg_rotate);
        lreg(vm, g, "scale", lg_scale);
        lreg(vm, g, "origin", lg_origin);
        lreg(vm, g, "setScissor", lg_setScissor);
        lreg(vm, g, "noScissor", lg_noScissor);
        lreg(vm, g, "newImage", lg_newImage);
        lreg(vm, g, "draw", lg_draw);
        lreg(vm, g, "drawTinted", lg_drawTinted);
        lreg(vm, g, "gradient", lg_gradient);
        lreg(vm, g, "getScreenWidth", lg_getWidthScreen);
        lreg(vm, g, "getScreenHeight", lg_getHeightScreen);
        lreg(vm, g, "getDimensions", lg_getDimensions);
    }

    /* ---- timer ---- */
    DSDict *t = subtable(vm, love, "timer");
    if (t) {
        lreg(vm, t, "getDelta", lt_getDelta);
        lreg(vm, t, "getTime", lt_getTime);
        lreg(vm, t, "getFPS", lt_getFPS);
        lreg(vm, t, "step", lt_step);
    }

    /* ---- audio ---- */
    DSDict *a = subtable(vm, love, "audio");
    if (a) {
        lreg(vm, a, "play", la_play);
        lreg(vm, a, "loop", la_loop);
        lreg(vm, a, "stop", la_stop);
        lreg(vm, a, "stopAll", la_stopAll);
        lreg(vm, a, "setVolume", la_setVolume);
        lreg(vm, a, "tone", la_tone);
        lreg(vm, a, "sfx", la_sfx);
    }

    /* ---- math ---- */
    DSDict *m = subtable(vm, love, "math");
    if (m) {
        lreg(vm, m, "random", lm_random);
        lreg(vm, m, "noise", lm_noise);
        lreg(vm, m, "clamp", lm_clamp);
        lreg(vm, m, "lerp", lm_lerp);
        ds_dict_set_cstr(vm, m, "pi", ds_num(3.14159265358979323846));
    }

    /* ---- window ---- */
    DSDict *w = subtable(vm, love, "window");
    if (w) {
        lreg(vm, w, "getWidth", lw_width);
        lreg(vm, w, "getHeight", lw_height);
    }

    /* ---- filesystem ---- */
    DSDict *f = subtable(vm, love, "filesystem");
    if (f) {
        lreg(vm, f, "read", lfs_read);
        lreg(vm, f, "write", lfs_write);
    }

    /* ---- touch / mouse / keyboard ---- */
    DSDict *touch = subtable(vm, love, "touch");
    if (touch)
        lreg(vm, touch, "getTouches", lg_getWidth);
    DSDict *mouse = subtable(vm, love, "mouse");
    if (mouse)
        lreg(vm, mouse, "getPosition", lg_getDimensions);

    ds_pop_roots(vm, 1);
}
