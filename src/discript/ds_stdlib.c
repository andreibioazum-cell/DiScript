/* DiScript Standard Library: math, random, json, time, fs, re, color, vec,
 * physics, tween, gc, plus str/list/dict methods and core builtins. */

#include "discript/ds_ast.h"
#include "discript/discript.h"
#include "cpp/ds_cpp.h"
#include "engine/runtime.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * HELPERS
 * ============================================================================ */

static DSValue arg(DSVM *vm, int argc, DSValue *argv, int i) {
    if (i < argc)
        return argv[i];
    return DS_VAL_NIL;
}

static double arg_num(DSVM *vm, int argc, DSValue *argv, int i, double def) {
    if (i < argc && ds_is_num(argv[i]))
        return ds_as_num(argv[i]);
    if (i < argc && ds_is_bool(argv[i]))
        return ds_as_bool(argv[i]) ? 1.0 : 0.0;
    if (i < argc && ds_is_nil(argv[i]) == 0 && !ds_is_num(argv[i]))
        return ds_to_number(vm, argv[i]);
    return def;
}

static const char *arg_str(DSVM *vm, int argc, DSValue *argv, int i, const char *def) {
    if (i >= argc || ds_is_nil(argv[i]))
        return def;
    if (ds_obj_is(argv[i], DS_OBJ_STRING))
        return AS_CSTR(argv[i]);
    DSString *s = ds_val_to_string(vm, argv[i]);
    return s ? s->chars : def;
}

static int arg_bool(int argc, DSValue *argv, int i, int def) {
    if (i >= argc || ds_is_nil(argv[i]))
        return def;
    return ds_is_truthy(argv[i]);
}

static void reg(DSVM *vm, DSDict *dict, const char *name, DSNativeFn fn) {
    DSCFunc *f = ds_cfunc_new(vm, name, fn, NULL);
    if (!f)
        return;
    ds_push_root(vm, ds_obj_val(f));
    DSString *key = ds_string_const(vm, name);
    ds_dict_set(vm, dict, ds_obj_val(key), ds_obj_val(f));
    ds_pop_roots(vm, 1);
}

static void reg_method(DSVM *vm, DSDict *dict, const char *name, DSNativeFn fn) {
    reg(vm, dict, name, fn);
}

static void reg_const(DSVM *vm, DSDict *dict, const char *name, DSValue v) {
    DSString *key = ds_string_const(vm, name);
    ds_push_root(vm, v);
    ds_dict_set(vm, dict, ds_obj_val(key), v);
    ds_pop_roots(vm, 1);
}

static void reg_num(DSVM *vm, DSDict *dict, const char *name, double v) { reg_const(vm, dict, name, ds_num(v)); }

static DSDict *make_module(DSVM *vm, const char *name) {
    DSString *nm = ds_string_const(vm, name);
    DSDict *d = ds_dict_new(vm, 16);
    ds_push_root(vm, ds_obj_val(d));
    DSModule *m = ds_module_new(vm, nm);
    if (m) {
        m->dict = d;
        ds_push_root(vm, ds_obj_val(m));
        ds_dict_set(vm, vm->modules, ds_obj_val(nm), ds_obj_val(m));
        ds_dict_set(vm, vm->builtins, ds_obj_val(nm), ds_obj_val(m));
        ds_pop_roots(vm, 1);
    }
    ds_pop_roots(vm, 1);
    (void)nm;
    return d;
}

/* ============================================================================
 * CORE BUILTINS
 * ============================================================================ */

static DSValue bi_print(DSVM *vm, int argc, DSValue *argv) {
    size_t cap = 256, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
        return DS_VAL_NIL;
    for (int i = 0; i < argc; i++) {
        DSString *s = ds_val_to_string(vm, argv[i]);
        size_t need = len + s->len + 2;
        if (need > cap) {
            while (cap < need)
                cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb)
                break;
            buf = nb;
        }
        if (i > 0)
            buf[len++] = ' ';
        memcpy(buf + len, s->chars, s->len);
        len += s->len;
    }
    buf[len] = '\0';
    if (vm->print_cb)
        vm->print_cb(buf, 0, vm->print_ud);
    else
        ds_console_raw(0, buf);
    free(buf);
    return DS_VAL_NIL;
}

static DSValue bi_len(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(v);
        /* UTF-8 codepoint count */
        int n = 0;
        int i = 0;
        while (i < (int)s->len) {
            int k = i + 1;
            while (k < (int)s->len && ((uint8_t)s->chars[k] & 0xC0) == 0x80)
                k++;
            i = k;
            n++;
        }
        return ds_num(n);
    }
    if (ds_obj_is(v, DS_OBJ_LIST))
        return ds_num(AS_LIST(v)->len);
    if (ds_obj_is(v, DS_OBJ_DICT))
        return ds_num(AS_DICT(v)->count);
    if (ds_is_nil(v))
        return ds_num(0);
    ds_error(vm, 0, "len() не поддерживает '%s'", ds_type_name(v));
    return ds_num(0);
}

static DSValue bi_range(DSVM *vm, int argc, DSValue *argv) {
    double a = arg_num(vm, argc, argv, 0, 0), b = arg_num(vm, argc, argv, 1, 0), c = arg_num(vm, argc, argv, 2, 1);
    if (argc <= 1) {
        b = a;
        a = 0;
    }
    return ds_make_range(vm, a, b, c);
}

static DSValue bi_type(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    return ds_obj_val(ds_string_const(vm, ds_type_name(arg(vm, argc, argv, 0))));
}

static DSValue bi_str(DSVM *vm, int argc, DSValue *argv) {
    return ds_obj_val(ds_val_to_string(vm, arg(vm, argc, argv, 0)));
}

static DSValue bi_int(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(v);
        int base = 10;
        if (argc > 1)
            base = (int)arg_num(vm, argc, argv, 1, 10);
        return ds_num((double)strtoll(s->chars, NULL, base));
    }
    return ds_num((double)(long long)ds_to_number(vm, v));
}

static DSValue bi_float(DSVM *vm, int argc, DSValue *argv) { return ds_num(ds_to_number(vm, arg(vm, argc, argv, 0))); }

static DSValue bi_bool(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    return ds_bool(ds_is_truthy(arg(vm, argc, argv, 0)));
}

static DSValue bi_list(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    DSList *l = ds_list_new(vm, 8);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    if (!ds_is_nil(v)) {
        DSIter it;
        if (ds_iter_open(vm, v, &it)) {
            DSValue item;
            while (ds_iter_next(vm, &it, &item))
                ds_list_push(vm, l, item);
        }
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue bi_dict(DSVM *vm, int argc, DSValue *argv) {
    DSDict *d = ds_dict_new(vm, 8);
    if (!d)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(d));
    DSValue src = arg(vm, argc, argv, 0);
    if (ds_obj_is(src, DS_OBJ_DICT)) {
        DSDict *s = AS_DICT(src);
        for (int i = 0; i < s->cap; i++)
            if (s->entries[i].state == 1)
                ds_dict_set(vm, d, s->entries[i].key, s->entries[i].val);
    } else if (ds_obj_is(src, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(src);
        for (int i = 0; i < l->len; i++) {
            DSValue pair = l->items[i];
            if (ds_obj_is(pair, DS_OBJ_LIST) && AS_LIST(pair)->len >= 2)
                ds_dict_set(vm, d, AS_LIST(pair)->items[0], AS_LIST(pair)->items[1]);
        }
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(d);
}

static DSValue bi_abs(DSVM *vm, int argc, DSValue *argv) { return ds_num(fabs(arg_num(vm, argc, argv, 0, 0))); }
static DSValue bi_min(DSVM *vm, int argc, DSValue *argv) {
    if (argc == 1 && ds_obj_is(argv[0], DS_OBJ_LIST)) {
        DSList *l = AS_LIST(argv[0]);
        if (l->len == 0)
            return DS_VAL_NIL;
        DSValue best = l->items[0];
        for (int i = 1; i < l->len; i++)
            if (ds_is_num(l->items[i]) && ds_is_num(best) && ds_as_num(l->items[i]) < ds_as_num(best))
                best = l->items[i];
        return best;
    }
    if (argc == 0)
        return DS_VAL_NIL;
    DSValue best = argv[0];
    for (int i = 1; i < argc; i++)
        if (ds_is_num(argv[i]) && ds_is_num(best) && ds_as_num(argv[i]) < ds_as_num(best))
            best = argv[i];
    return best;
}
static DSValue bi_max(DSVM *vm, int argc, DSValue *argv) {
    if (argc == 1 && ds_obj_is(argv[0], DS_OBJ_LIST)) {
        DSList *l = AS_LIST(argv[0]);
        if (l->len == 0)
            return DS_VAL_NIL;
        DSValue best = l->items[0];
        for (int i = 1; i < l->len; i++)
            if (ds_is_num(l->items[i]) && ds_is_num(best) && ds_as_num(l->items[i]) > ds_as_num(best))
                best = l->items[i];
        return best;
    }
    if (argc == 0)
        return DS_VAL_NIL;
    DSValue best = argv[0];
    for (int i = 1; i < argc; i++)
        if (ds_is_num(argv[i]) && ds_is_num(best) && ds_as_num(argv[i]) > ds_as_num(best))
            best = argv[i];
    return best;
}
static DSValue bi_sum(DSVM *vm, int argc, DSValue *argv) {
    double acc = 0;
    DSValue v = arg(vm, argc, argv, 0);
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(v);
        for (int i = 0; i < l->len; i++)
            if (ds_is_num(l->items[i]))
                acc += ds_as_num(l->items[i]);
    }
    return ds_num(acc);
}
static DSValue bi_round(DSVM *vm, int argc, DSValue *argv) {
    double x = arg_num(vm, argc, argv, 0, 0);
    int digits = (int)arg_num(vm, argc, argv, 1, 0);
    double mul = pow(10.0, digits);
    return ds_num(round(x * mul) / mul);
}
static DSValue bi_floor(DSVM *vm, int argc, DSValue *argv) { return ds_num(floor(arg_num(vm, argc, argv, 0, 0))); }
static DSValue bi_ceil(DSVM *vm, int argc, DSValue *argv) { return ds_num(ceil(arg_num(vm, argc, argv, 0, 0))); }

static DSValue bi_sorted(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (!ds_obj_is(v, DS_OBJ_LIST)) {
        ds_error(vm, 0, "sorted() требует list");
        return DS_VAL_NIL;
    }
    DSList *src = AS_LIST(v);
    DSList *l = ds_list_new(vm, src->len);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, v);
    for (int i = 0; i < src->len; i++)
        ds_list_push(vm, l, src->items[i]);
    /* insertion sort: stable and allocation-free */
    for (int i = 1; i < l->len; i++) {
        DSValue key = l->items[i];
        int j = i - 1;
        while (j >= 0) {
            int c = 0;
            if (ds_is_num(l->items[j]) && ds_is_num(key)) {
                double a = ds_as_num(l->items[j]), b = ds_as_num(key);
                c = (a < b) ? -1 : (a > b ? 1 : 0);
            } else if (ds_obj_is(l->items[j], DS_OBJ_STRING) && ds_obj_is(key, DS_OBJ_STRING)) {
                c = strcmp(AS_CSTR(l->items[j]), AS_CSTR(key));
            }
            if (c <= 0)
                break;
            l->items[j + 1] = l->items[j];
            j--;
        }
        l->items[j + 1] = key;
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue bi_reversed(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    DSList *l = ds_list_new(vm, 8);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *s = AS_LIST(v);
        for (int i = s->len - 1; i >= 0; i--)
            ds_list_push(vm, l, s->items[i]);
    } else if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSIter it;
        if (ds_iter_open(vm, v, &it)) {
            DSValue item;
            while (ds_iter_next(vm, &it, &item))
                ds_list_push(vm, l, item);
            for (int i = 0, j = l->len - 1; i < j; i++, j--) {
                DSValue t = l->items[i];
                l->items[i] = l->items[j];
                l->items[j] = t;
            }
        }
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue bi_enumerate(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    double start = arg_num(vm, argc, argv, 1, 0);
    DSList *l = ds_list_new(vm, 8);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, v);
    DSIter it;
    if (ds_iter_open(vm, v, &it)) {
        DSValue item;
        double i = start;
        while (ds_iter_next(vm, &it, &item)) {
            DSList *pair = ds_list_new(vm, 2);
            if (!pair)
                break;
            ds_push_root(vm, ds_obj_val(pair));
            ds_list_push(vm, pair, ds_num(i));
            ds_list_push(vm, pair, item);
            ds_list_push(vm, l, ds_obj_val(pair));
            ds_pop_roots(vm, 1);
            i += 1.0;
        }
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue bi_zip(DSVM *vm, int argc, DSValue *argv) {
    DSList *out = ds_list_new(vm, argc);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    DSIter *iters = (DSIter *)calloc((size_t)(argc > 0 ? argc : 1), sizeof(DSIter));
    int n = 0;
    for (int i = 0; i < argc; i++) {
        if (ds_iter_open(vm, argv[i], &iters[i]))
            n++;
    }
    for (;;) {
        DSList *row = ds_list_new(vm, n);
        if (!row)
            break;
        ds_push_root(vm, ds_obj_val(row));
        int ok = 1;
        for (int i = 0; i < n; i++) {
            DSValue item;
            if (!ds_iter_next(vm, &iters[i], &item)) {
                ok = 0;
                break;
            }
            ds_list_push(vm, row, item);
        }
        if (ok)
            ds_list_push(vm, out, ds_obj_val(row));
        ds_pop_roots(vm, 1);
        if (!ok)
            break;
    }
    free(iters);
    ds_pop_roots(vm, 1);
    return ds_obj_val(out);
}

static DSValue bi_map(DSVM *vm, int argc, DSValue *argv) {
    DSValue fnv = arg(vm, argc, argv, 0);
    DSValue coll = arg(vm, argc, argv, 1);
    DSList *out = ds_list_new(vm, 8);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    ds_push_root(vm, fnv);
    ds_push_root(vm, coll);
    DSIter it;
    if (ds_iter_open(vm, coll, &it)) {
        DSValue item;
        while (ds_iter_next(vm, &it, &item)) {
            if (vm->has_error)
                break;
            DSValue r = ds_call_value(vm, fnv, 1, &item);
            ds_list_push(vm, out, r);
        }
    }
    ds_pop_roots(vm, 3);
    return ds_obj_val(out);
}

static DSValue bi_filter(DSVM *vm, int argc, DSValue *argv) {
    DSValue fnv = arg(vm, argc, argv, 0);
    DSValue coll = arg(vm, argc, argv, 1);
    DSList *out = ds_list_new(vm, 8);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    ds_push_root(vm, fnv);
    ds_push_root(vm, coll);
    DSIter it;
    if (ds_iter_open(vm, coll, &it)) {
        DSValue item;
        while (ds_iter_next(vm, &it, &item)) {
            if (vm->has_error)
                break;
            DSValue r = ds_call_value(vm, fnv, 1, &item);
            if (ds_is_truthy(r))
                ds_list_push(vm, out, item);
        }
    }
    ds_pop_roots(vm, 3);
    return ds_obj_val(out);
}

static DSValue bi_any(DSVM *vm, int argc, DSValue *argv) {
    DSValue coll = arg(vm, argc, argv, 0);
    DSIter it;
    DSValue item;
    if (!ds_iter_open(vm, coll, &it))
        return DS_VAL_FALSE;
    while (ds_iter_next(vm, &it, &item))
        if (ds_is_truthy(item))
            return DS_VAL_TRUE;
    return DS_VAL_FALSE;
}
static DSValue bi_all(DSVM *vm, int argc, DSValue *argv) {
    DSValue coll = arg(vm, argc, argv, 0);
    DSIter it;
    DSValue item;
    if (!ds_iter_open(vm, coll, &it))
        return DS_VAL_TRUE;
    while (ds_iter_next(vm, &it, &item))
        if (!ds_is_truthy(item))
            return DS_VAL_FALSE;
    return DS_VAL_TRUE;
}

static DSValue bi_assert(DSVM *vm, int argc, DSValue *argv) {
    if (!ds_is_truthy(arg(vm, argc, argv, 0))) {
        const char *msg = arg_str(vm, argc, argv, 1, "assert провален");
        ds_error(vm, 0, "%s", msg);
    }
    return DS_VAL_NIL;
}

static DSValue bi_input(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return DS_VAL_NIL;
}

static DSValue bi_clamp(DSVM *vm, int argc, DSValue *argv) {
    double v = arg_num(vm, argc, argv, 0, 0);
    double lo = arg_num(vm, argc, argv, 1, 0);
    double hi = arg_num(vm, argc, argv, 2, 1);
    return ds_num(v < lo ? lo : (v > hi ? hi : v));
}
static DSValue bi_lerp(DSVM *vm, int argc, DSValue *argv) {
    double a = arg_num(vm, argc, argv, 0, 0), b = arg_num(vm, argc, argv, 1, 0), t = arg_num(vm, argc, argv, 2, 0);
    return ds_num(a + (b - a) * t);
}
static DSValue bi_sign(DSVM *vm, int argc, DSValue *argv) {
    double v = arg_num(vm, argc, argv, 0, 0);
    return ds_num(v > 0 ? 1.0 : (v < 0 ? -1.0 : 0.0));
}
static DSValue bi_dist(DSVM *vm, int argc, DSValue *argv) {
    double x1 = arg_num(vm, argc, argv, 0, 0), y1 = arg_num(vm, argc, argv, 1, 0);
    double x2 = arg_num(vm, argc, argv, 2, 0), y2 = arg_num(vm, argc, argv, 3, 0);
    return ds_num(hypot(x2 - x1, y2 - y1));
}

static DSValue bi_hasattr(DSVM *vm, int argc, DSValue *argv) {
    DSValue obj = arg(vm, argc, argv, 0);
    DSString *name = ds_val_to_string(vm, arg(vm, argc, argv, 1));
    DSValue fn;
    if (ds_lookup_method(vm, obj, name, &fn))
        return DS_VAL_TRUE;
    if (ds_obj_is(obj, DS_OBJ_INSTANCE)) {
        DSValue tmp;
        if (ds_dict_get_str(AS_INSTANCE(obj)->fields, name, &tmp))
            return DS_VAL_TRUE;
    }
    return DS_VAL_FALSE;
}

static DSValue bi_getattr(DSVM *vm, int argc, DSValue *argv) {
    DSValue obj = arg(vm, argc, argv, 0);
    DSString *name = ds_val_to_string(vm, arg(vm, argc, argv, 1));
    DSValue r = ds_get_property(vm, obj, name, NULL);
    if (vm->has_error && argc > 2) {
        vm->has_error = 0;
        vm->err_msg[0] = '\0';
        return argv[2];
    }
    return r;
}

static DSValue bi_setattr(DSVM *vm, int argc, DSValue *argv) {
    DSString *name = ds_val_to_string(vm, arg(vm, argc, argv, 1));
    ds_set_property(vm, arg(vm, argc, argv, 0), name, arg(vm, argc, argv, 2));
    return DS_VAL_NIL;
}

static DSValue bi_callable(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    DSValue v = arg(vm, argc, argv, 0);
    return ds_bool(ds_obj_is(v, DS_OBJ_CLOSURE) || ds_obj_is(v, DS_OBJ_CFUNC) || ds_obj_is(v, DS_OBJ_CLASS) ||
                   ds_obj_is(v, DS_OBJ_BOUND));
}

static DSValue bi_iter(DSVM *vm, int argc, DSValue *argv) { return bi_list(vm, argc, argv); }

static DSValue bi_copy(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *s = AS_LIST(v);
        DSList *l = ds_list_new(vm, s->len);
        if (!l)
            return DS_VAL_NIL;
        ds_push_root(vm, ds_obj_val(l));
        ds_push_root(vm, v);
        for (int i = 0; i < s->len; i++)
            ds_list_push(vm, l, s->items[i]);
        ds_pop_roots(vm, 2);
        return ds_obj_val(l);
    }
    if (ds_obj_is(v, DS_OBJ_DICT)) {
        DSDict *s = AS_DICT(v);
        DSDict *d = ds_dict_new(vm, s->count + 1);
        if (!d)
            return DS_VAL_NIL;
        ds_push_root(vm, ds_obj_val(d));
        ds_push_root(vm, v);
        for (int i = 0; i < s->cap; i++)
            if (s->entries[i].state == 1)
                ds_dict_set(vm, d, s->entries[i].key, s->entries[i].val);
        ds_pop_roots(vm, 2);
        return ds_obj_val(d);
    }
    return v;
}

/* ============================================================================
 * STRING METHODS
 * ============================================================================ */

static DSValue sm_len(DSVM *vm, int argc, DSValue *argv) { return bi_len(vm, argc, argv); }

static DSValue sm_upper(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    char *b = (char *)malloc(s->len + 1);
    if (!b)
        return recv;
    for (uint32_t i = 0; i < s->len; i++)
        b[i] = (char)toupper((unsigned char)s->chars[i]);
    DSString *r = ds_string_new(vm, b, s->len);
    free(b);
    return ds_obj_val(r);
}

static DSValue sm_lower(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    char *b = (char *)malloc(s->len + 1);
    if (!b)
        return recv;
    for (uint32_t i = 0; i < s->len; i++)
        b[i] = (char)tolower((unsigned char)s->chars[i]);
    DSString *r = ds_string_new(vm, b, s->len);
    free(b);
    return ds_obj_val(r);
}

static DSValue sm_strip(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    uint32_t a = 0, b = s->len;
    while (a < b && isspace((unsigned char)s->chars[a]))
        a++;
    while (b > a && isspace((unsigned char)s->chars[b - 1]))
        b--;
    return ds_obj_val(ds_string_new(vm, s->chars + a, b - a));
}

static DSValue sm_split(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    const char *sep = (argc > 1 && !ds_is_nil(argv[1])) ? arg_str(vm, argc, argv, 1, " ") : NULL;
    DSList *out = ds_list_new(vm, 8);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    ds_push_root(vm, recv);
    if (!sep) {
        uint32_t i = 0;
        while (i < s->len) {
            while (i < s->len && isspace((unsigned char)s->chars[i]))
                i++;
            uint32_t start = i;
            while (i < s->len && !isspace((unsigned char)s->chars[i]))
                i++;
            if (i > start)
                ds_list_push(vm, out, ds_obj_val(ds_string_new(vm, s->chars + start, i - start)));
        }
    } else {
        size_t sl = strlen(sep);
        if (sl == 0) {
            uint32_t i = 0;
            while (i < s->len) {
                uint32_t k = i + 1;
                while (k < s->len && ((uint8_t)s->chars[k] & 0xC0) == 0x80)
                    k++;
                ds_list_push(vm, out, ds_obj_val(ds_string_new(vm, s->chars + i, k - i)));
                i = k;
            }
        } else {
            uint32_t start = 0;
            for (uint32_t i = 0; i + sl <= s->len;) {
                if (memcmp(s->chars + i, sep, sl) == 0) {
                    ds_list_push(vm, out, ds_obj_val(ds_string_new(vm, s->chars + start, i - start)));
                    i += sl;
                    start = i;
                } else {
                    i++;
                }
            }
            ds_list_push(vm, out, ds_obj_val(ds_string_new(vm, s->chars + start, s->len - start)));
        }
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(out);
}

static DSValue sm_join(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue coll = arg(vm, argc, argv, 1);
    DSString *sep = AS_STRING(recv);
    ds_push_root(vm, recv);
    ds_push_root(vm, coll);
    DSList *parts = ds_list_new(vm, 8);
    if (!parts) {
        ds_pop_roots(vm, 2);
        return DS_VAL_NIL;
    }
    ds_push_root(vm, ds_obj_val(parts));
    DSIter it;
    if (ds_iter_open(vm, coll, &it)) {
        DSValue item;
        while (ds_iter_next(vm, &it, &item)) {
            DSString *s = ds_val_to_string(vm, item);
            ds_list_push(vm, parts, ds_obj_val(s));
        }
    }
    size_t total = 0;
    for (int i = 0; i < parts->len; i++) {
        total += AS_STRING(parts->items[i])->len;
        if (i > 0)
            total += sep->len;
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf) {
        ds_pop_roots(vm, 3);
        return DS_VAL_NIL;
    }
    size_t off = 0;
    for (int i = 0; i < parts->len; i++) {
        DSString *s = AS_STRING(parts->items[i]);
        if (i > 0) {
            memcpy(buf + off, sep->chars, sep->len);
            off += sep->len;
        }
        memcpy(buf + off, s->chars, s->len);
        off += s->len;
    }
    DSString *r = ds_string_new(vm, buf, off);
    free(buf);
    ds_pop_roots(vm, 3);
    return ds_obj_val(r);
}

static DSValue sm_replace(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    const char *from = arg_str(vm, argc, argv, 1, "");
    const char *to = arg_str(vm, argc, argv, 2, "");
    size_t fl = strlen(from), tl = strlen(to);
    if (fl == 0)
        return recv;
    size_t cap = s->len + 16, len = 0;
    char *b = (char *)malloc(cap);
    if (!b)
        return recv;
    for (uint32_t i = 0; i < s->len;) {
        if (i + fl <= s->len && memcmp(s->chars + i, from, fl) == 0) {
            if (len + tl + 1 > cap) {
                cap = (len + tl + 1) * 2;
                char *nb = (char *)realloc(b, cap);
                if (!nb)
                    break;
                b = nb;
            }
            memcpy(b + len, to, tl);
            len += tl;
            i += (uint32_t)fl;
        } else {
            if (len + 2 > cap) {
                cap *= 2;
                char *nb = (char *)realloc(b, cap);
                if (!nb)
                    break;
                b = nb;
            }
            b[len++] = s->chars[i++];
        }
    }
    DSString *r = ds_string_new(vm, b, len);
    free(b);
    return ds_obj_val(r);
}

static DSValue sm_find(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSString *s = AS_STRING(recv);
    const char *needle = arg_str(vm, argc, argv, 1, "");
    const char *p = strstr(s->chars, needle);
    if (!p)
        return ds_num(-1);
    return ds_num((double)(p - s->chars));
}

static DSValue sm_startswith(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    const char *prefix = arg_str(vm, argc, argv, 1, "");
    size_t pl = strlen(prefix);
    return ds_bool(pl <= s->len && memcmp(s->chars, prefix, pl) == 0);
}

static DSValue sm_endswith(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    const char *suffix = arg_str(vm, argc, argv, 1, "");
    size_t sl = strlen(suffix);
    return ds_bool(sl <= s->len && memcmp(s->chars + (s->len - sl), suffix, sl) == 0);
}

static DSValue sm_substr(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    int a = (int)arg_num(vm, argc, argv, 1, 0);
    int n = (int)arg_num(vm, argc, argv, 2, (double)s->len);
    if (a < 0)
        a += (int)s->len;
    if (a < 0)
        a = 0;
    if (a > (int)s->len)
        a = (int)s->len;
    if (n < 0)
        n = (int)s->len - a;
    if (a + n > (int)s->len)
        n = (int)s->len - a;
    if (n < 0)
        n = 0;
    return ds_obj_val(ds_string_new(vm, s->chars + a, (size_t)n));
}

static DSValue sm_char_at(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    int i = (int)arg_num(vm, argc, argv, 1, 0);
    if (i < 0 || i >= (int)s->len)
        return ds_obj_val(ds_string_new(vm, "", 0));
    return ds_obj_val(ds_string_new(vm, s->chars + i, 1));
}

static DSValue sm_repeat(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    int n = (int)arg_num(vm, argc, argv, 1, 1);
    if (n <= 0)
        return ds_obj_val(ds_string_new(vm, "", 0));
    size_t total = s->len * (size_t)n;
    char *b = (char *)malloc(total + 1);
    if (!b)
        return DS_VAL_NIL;
    for (int i = 0; i < n; i++)
        memcpy(b + (size_t)i * s->len, s->chars, s->len);
    DSString *r = ds_string_new(vm, b, total);
    free(b);
    return ds_obj_val(r);
}

static DSValue sm_chars(DSVM *vm, int argc, DSValue *argv) {
    DSString *s = AS_STRING(arg(vm, argc, argv, 0));
    DSList *l = ds_list_new(vm, (int)s->len);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, ds_obj_val(s));
    uint32_t i = 0;
    while (i < s->len) {
        uint32_t k = i + 1;
        while (k < s->len && ((uint8_t)s->chars[k] & 0xC0) == 0x80)
            k++;
        ds_list_push(vm, l, ds_obj_val(ds_string_new(vm, s->chars + i, k - i)));
        i = k;
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

/* ============================================================================
 * LIST METHODS
 * ============================================================================ */

static DSValue lm_append(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    ds_list_push(vm, AS_LIST(recv), arg(vm, argc, argv, 1));
    return DS_VAL_NIL;
}

static DSValue lm_extend(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue src = arg(vm, argc, argv, 1);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    ds_push_root(vm, recv);
    ds_push_root(vm, src);
    DSIter it;
    if (ds_iter_open(vm, src, &it)) {
        DSValue item;
        while (ds_iter_next(vm, &it, &item))
            ds_list_push(vm, AS_LIST(recv), item);
    }
    ds_pop_roots(vm, 2);
    return DS_VAL_NIL;
}

static DSValue lm_pop(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *l = AS_LIST(recv);
    if (argc > 1) {
        int i = (int)arg_num(vm, argc, argv, 1, -1);
        if (i < 0)
            i += l->len;
        if (i < 0 || i >= l->len) {
            ds_error(vm, 0, "pop: индекс вне диапазона");
            return DS_VAL_NIL;
        }
        DSValue v = l->items[i];
        memmove(&l->items[i], &l->items[i + 1], sizeof(DSValue) * (size_t)(l->len - i - 1));
        l->len--;
        return v;
    }
    if (l->len == 0) {
        ds_error(vm, 0, "pop из пустого списка");
        return DS_VAL_NIL;
    }
    return l->items[--l->len];
}

static DSValue lm_insert(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *l = AS_LIST(recv);
    int i = (int)arg_num(vm, argc, argv, 1, 0);
    DSValue v = arg(vm, argc, argv, 2);
    if (i < 0)
        i += l->len;
    if (i < 0)
        i = 0;
    if (i > l->len)
        i = l->len;
    ds_list_push(vm, l, DS_VAL_NIL);
    memmove(&l->items[i + 1], &l->items[i], sizeof(DSValue) * (size_t)(l->len - i - 1));
    l->items[i] = v;
    ds_gc_write_barrier_back(vm, &l->hdr, v);
    return DS_VAL_NIL;
}

static DSValue lm_remove(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue v = arg(vm, argc, argv, 1);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *l = AS_LIST(recv);
    for (int i = 0; i < l->len; i++) {
        if (ds_values_equal(l->items[i], v)) {
            memmove(&l->items[i], &l->items[i + 1], sizeof(DSValue) * (size_t)(l->len - i - 1));
            l->len--;
            return DS_VAL_NIL;
        }
    }
    ds_error(vm, 0, "remove: элемент не найден");
    return DS_VAL_NIL;
}

static DSValue lm_index(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue v = arg(vm, argc, argv, 1);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return ds_num(-1);
    DSList *l = AS_LIST(recv);
    for (int i = 0; i < l->len; i++)
        if (ds_values_equal(l->items[i], v))
            return ds_num(i);
    return ds_num(-1);
}

static DSValue lm_clear(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (ds_obj_is(recv, DS_OBJ_LIST))
        AS_LIST(recv)->len = 0;
    return DS_VAL_NIL;
}

static DSValue lm_copy(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *s = AS_LIST(recv);
    DSList *l = ds_list_new(vm, s->len);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, recv);
    for (int i = 0; i < s->len; i++)
        ds_list_push(vm, l, s->items[i]);
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue lm_reverse(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *l = AS_LIST(recv);
    for (int i = 0, j = l->len - 1; i < j; i++, j--) {
        DSValue t = l->items[i];
        l->items[i] = l->items[j];
        l->items[j] = t;
    }
    return DS_VAL_NIL;
}

static DSValue lm_sort(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return DS_VAL_NIL;
    DSList *l = AS_LIST(recv);
    for (int i = 1; i < l->len; i++) {
        DSValue key = l->items[i];
        int j = i - 1;
        while (j >= 0) {
            int c = 0;
            if (ds_is_num(l->items[j]) && ds_is_num(key)) {
                double a = ds_as_num(l->items[j]), b = ds_as_num(key);
                c = (a < b) ? -1 : (a > b ? 1 : 0);
            } else if (ds_obj_is(l->items[j], DS_OBJ_STRING) && ds_obj_is(key, DS_OBJ_STRING)) {
                c = strcmp(AS_CSTR(l->items[j]), AS_CSTR(key));
            }
            if (c <= 0)
                break;
            l->items[j + 1] = l->items[j];
            j--;
        }
        l->items[j + 1] = key;
    }
    return DS_VAL_NIL;
}

static DSValue lm_count(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue v = arg(vm, argc, argv, 1);
    if (!ds_obj_is(recv, DS_OBJ_LIST))
        return ds_num(0);
    DSList *l = AS_LIST(recv);
    int n = 0;
    for (int i = 0; i < l->len; i++)
        if (ds_values_equal(l->items[i], v))
            n++;
    return ds_num(n);
}

/* ============================================================================
 * DICT METHODS
 * ============================================================================ */

static DSValue dm_keys(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_DICT))
        return DS_VAL_NIL;
    DSDict *d = AS_DICT(recv);
    DSList *l = ds_list_new(vm, d->count);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, recv);
    for (int i = 0; i < d->cap; i++)
        if (d->entries[i].state == 1)
            ds_list_push(vm, l, d->entries[i].key);
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue dm_values(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_DICT))
        return DS_VAL_NIL;
    DSDict *d = AS_DICT(recv);
    DSList *l = ds_list_new(vm, d->count);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, recv);
    for (int i = 0; i < d->cap; i++)
        if (d->entries[i].state == 1)
            ds_list_push(vm, l, d->entries[i].val);
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue dm_items(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (!ds_obj_is(recv, DS_OBJ_DICT))
        return DS_VAL_NIL;
    DSDict *d = AS_DICT(recv);
    DSList *l = ds_list_new(vm, d->count);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_push_root(vm, recv);
    for (int i = 0; i < d->cap; i++) {
        if (d->entries[i].state != 1)
            continue;
        DSList *pair = ds_list_new(vm, 2);
        if (!pair)
            break;
        ds_push_root(vm, ds_obj_val(pair));
        ds_list_push(vm, pair, d->entries[i].key);
        ds_list_push(vm, pair, d->entries[i].val);
        ds_list_push(vm, l, ds_obj_val(pair));
        ds_pop_roots(vm, 1);
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(l);
}

static DSValue dm_get(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue key = arg(vm, argc, argv, 1);
    DSValue def = arg(vm, argc, argv, 2);
    if (ds_obj_is(recv, DS_OBJ_DICT)) {
        DSValue out;
        if (ds_dict_get(AS_DICT(recv), key, &out))
            return out;
    }
    return def;
}

static DSValue dm_set(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (ds_obj_is(recv, DS_OBJ_DICT))
        ds_dict_set(vm, AS_DICT(recv), arg(vm, argc, argv, 1), arg(vm, argc, argv, 2));
    return DS_VAL_NIL;
}

static DSValue dm_has(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue key = arg(vm, argc, argv, 1);
    if (ds_obj_is(recv, DS_OBJ_DICT)) {
        DSValue out;
        return ds_bool(ds_dict_get(AS_DICT(recv), key, &out));
    }
    return DS_VAL_FALSE;
}

static DSValue dm_remove(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    DSValue key = arg(vm, argc, argv, 1);
    DSValue out = arg(vm, argc, argv, 2);
    if (ds_obj_is(recv, DS_OBJ_DICT)) {
        DSDict *d = AS_DICT(recv);
        DSValue v;
        if (ds_dict_get(d, key, &v)) {
            ds_dict_del(d, key);
            return v;
        }
    }
    return out;
}

static DSValue dm_clear(DSVM *vm, int argc, DSValue *argv) {
    DSValue recv = arg(vm, argc, argv, 0);
    if (ds_obj_is(recv, DS_OBJ_DICT)) {
        DSDict *d = AS_DICT(recv);
        if (d->entries) {
            memset(d->entries, 0, sizeof(DSDictEntry) * (size_t)d->cap);
        }
        d->count = 0;
        d->used = 0;
    }
    return DS_VAL_NIL;
}

static DSValue dm_copy(DSVM *vm, int argc, DSValue *argv) { return bi_copy(vm, argc, argv); }

/* ============================================================================
 * math MODULE
 * ============================================================================ */

static DSValue m_sin(DSVM *vm, int argc, DSValue *argv) { return ds_num(sin(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_cos(DSVM *vm, int argc, DSValue *argv) { return ds_num(cos(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_tan(DSVM *vm, int argc, DSValue *argv) { return ds_num(tan(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_asin(DSVM *vm, int argc, DSValue *argv) { return ds_num(asin(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_acos(DSVM *vm, int argc, DSValue *argv) { return ds_num(acos(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_atan(DSVM *vm, int argc, DSValue *argv) { return ds_num(atan(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_atan2(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(atan2(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 1)));
}
static DSValue m_sqrt(DSVM *vm, int argc, DSValue *argv) { return ds_num(sqrt(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_pow(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(pow(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 1)));
}
static DSValue m_log(DSVM *vm, int argc, DSValue *argv) {
    double x = arg_num(vm, argc, argv, 0, 1);
    if (argc > 1)
        return ds_num(log(x) / log(arg_num(vm, argc, argv, 1, 10)));
    return ds_num(log(x));
}
static DSValue m_exp(DSVM *vm, int argc, DSValue *argv) { return ds_num(exp(arg_num(vm, argc, argv, 0, 0))); }
static DSValue m_abs(DSVM *vm, int argc, DSValue *argv) { return bi_abs(vm, argc, argv); }
static DSValue m_floor(DSVM *vm, int argc, DSValue *argv) { return bi_floor(vm, argc, argv); }
static DSValue m_ceil(DSVM *vm, int argc, DSValue *argv) { return bi_ceil(vm, argc, argv); }
static DSValue m_fmod(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(fmod(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 1)));
}
static DSValue m_hypot(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(hypot(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 0)));
}
static DSValue m_deg(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(arg_num(vm, argc, argv, 0, 0) * 180.0 / 3.14159265358979323846);
}
static DSValue m_rad(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(arg_num(vm, argc, argv, 0, 0) * 3.14159265358979323846 / 180.0);
}
static DSValue m_clamp(DSVM *vm, int argc, DSValue *argv) { return bi_clamp(vm, argc, argv); }
static DSValue m_lerp(DSVM *vm, int argc, DSValue *argv) { return bi_lerp(vm, argc, argv); }
static DSValue m_sign(DSVM *vm, int argc, DSValue *argv) { return bi_sign(vm, argc, argv); }
static DSValue m_min(DSVM *vm, int argc, DSValue *argv) { return bi_min(vm, argc, argv); }
static DSValue m_max(DSVM *vm, int argc, DSValue *argv) { return bi_max(vm, argc, argv); }
static DSValue m_smoothstep(DSVM *vm, int argc, DSValue *argv) {
    double a = arg_num(vm, argc, argv, 0, 0), b = arg_num(vm, argc, argv, 1, 1), t = arg_num(vm, argc, argv, 2, 0);
    double x = (b == a) ? 0.0 : (t - a) / (b - a);
    if (x < 0) x = 0;
    if (x > 1) x = 1;
    return ds_num(x * x * (3.0 - 2.0 * x));
}
static DSValue m_noise2(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(ds_simplex2d(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 0)));
}
static DSValue m_noise3(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(ds_simplex3d(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 0),
                               arg_num(vm, argc, argv, 2, 0)));
}
static DSValue m_fbm(DSVM *vm, int argc, DSValue *argv) {
    return ds_num(ds_fbm2d(arg_num(vm, argc, argv, 0, 0), arg_num(vm, argc, argv, 1, 0),
                           (int)arg_num(vm, argc, argv, 2, 4)));
}

/* ============================================================================
 * random MODULE (xoshiro256** seeded from clock)
 * ============================================================================ */

static uint64_t rng_next(DSVM *vm) {
    uint64_t *s = vm->love.rng_state;
    if (s[0] == 0 && s[1] == 0 && s[2] == 0 && s[3] == 0) {
        uint64_t seed = (uint64_t)(ds_time_sec() * 1e6) ^ 0x9E3779B97F4A7C15ULL;
        for (int i = 0; i < 4; i++) {
            seed ^= seed << 13;
            seed ^= seed >> 7;
            seed ^= seed << 17;
            s[i] = seed + (uint64_t)i * 0x9E3779B97F4A7C15ULL;
        }
    }
    uint64_t result = s[1] * 5;
    result = ((result << 7) | (result >> 57)) * 9;
    uint64_t t = s[1] << 17;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = (s[3] << 45) | (s[3] >> 19);
    return result;
}

static DSValue rnd_random(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num((double)(rng_next(vm) >> 11) * (1.0 / 9007199254740992.0));
}

static DSValue rnd_int(DSVM *vm, int argc, DSValue *argv) {
    long long lo = 0, hi = 0;
    if (argc >= 2) {
        lo = (long long)arg_num(vm, argc, argv, 0, 0);
        hi = (long long)arg_num(vm, argc, argv, 1, 0);
    } else {
        hi = (long long)arg_num(vm, argc, argv, 0, 1);
    }
    if (hi < lo) {
        long long t = lo;
        lo = hi;
        hi = t;
    }
    unsigned long long span = (unsigned long long)(hi - lo) + 1ULL;
    return ds_num((double)(lo + (long long)(rng_next(vm) % span)));
}

static DSValue rnd_float(DSVM *vm, int argc, DSValue *argv) {
    double lo = arg_num(vm, argc, argv, 0, 0), hi = arg_num(vm, argc, argv, 1, 1);
    double t = (double)(rng_next(vm) >> 11) * (1.0 / 9007199254740992.0);
    return ds_num(lo + (hi - lo) * t);
}

static DSValue rnd_choice(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(v);
        if (l->len == 0)
            return DS_VAL_NIL;
        return l->items[rng_next(vm) % (uint64_t)l->len];
    }
    if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(v);
        if (s->len == 0)
            return ds_obj_val(s);
        uint32_t i = (uint32_t)(rng_next(vm) % s->len);
        return ds_obj_val(ds_string_new(vm, s->chars + i, 1));
    }
    return DS_VAL_NIL;
}

static DSValue rnd_shuffle(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (!ds_obj_is(v, DS_OBJ_LIST))
        return v;
    DSList *l = AS_LIST(v);
    for (int i = l->len - 1; i > 0; i--) {
        int j = (int)(rng_next(vm) % (uint64_t)(i + 1));
        DSValue t = l->items[i];
        l->items[i] = l->items[j];
        l->items[j] = t;
    }
    return v;
}

static DSValue rnd_seed(DSVM *vm, int argc, DSValue *argv) {
    uint64_t seed = (uint64_t)arg_num(vm, argc, argv, 0, 0);
    for (int i = 0; i < 4; i++) {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        vm->love.rng_state[i] = seed + (uint64_t)i * 0x9E3779B97F4A7C15ULL;
    }
    return DS_VAL_NIL;
}

/* ============================================================================
 * json MODULE
 * ============================================================================ */

typedef struct JsonOut {
    char *buf;
    size_t len;
    size_t cap;
} JsonOut;

static void jo_push(JsonOut *o, const char *s, size_t n) {
    if (o->len + n + 1 > o->cap) {
        size_t ncap = o->cap ? o->cap * 2 : 256;
        while (ncap < o->len + n + 1)
            ncap *= 2;
        char *nb = (char *)realloc(o->buf, ncap);
        if (!nb)
            return;
        o->buf = nb;
        o->cap = ncap;
    }
    memcpy(o->buf + o->len, s, n);
    o->len += n;
    o->buf[o->len] = '\0';
}

static void jo_puts(JsonOut *o, const char *s) { jo_push(o, s, strlen(s)); }

static void jo_escape(JsonOut *o, const char *s, size_t n) {
    jo_push(o, "\"", 1);
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        char tmp[8];
        switch (c) {
        case '"': jo_push(o, "\\\"", 2); break;
        case '\\': jo_push(o, "\\\\", 2); break;
        case '\n': jo_push(o, "\\n", 2); break;
        case '\r': jo_push(o, "\\r", 2); break;
        case '\t': jo_push(o, "\\t", 2); break;
        default:
            if (c < 0x20) {
                snprintf(tmp, sizeof(tmp), "\\u%04x", c);
                jo_puts(o, tmp);
            } else {
                jo_push(o, (const char *)&s[i], 1);
            }
        }
    }
    jo_push(o, "\"", 1);
}

static void json_write(DSVM *vm, JsonOut *o, DSValue v, int indent, int depth) {
    char tmp[64];
    if (ds_is_num(v)) {
        double d = ds_as_num(v);
        if (d == floor(d) && fabs(d) < 1e15)
            snprintf(tmp, sizeof(tmp), "%lld", (long long)d);
        else
            snprintf(tmp, sizeof(tmp), "%.17g", d);
        jo_puts(o, tmp);
        return;
    }
    if (v == DS_VAL_NIL) {
        jo_puts(o, "null");
        return;
    }
    if (ds_is_bool(v)) {
        jo_puts(o, ds_as_bool(v) ? "true" : "false");
        return;
    }
    (void)vm;
    (void)depth;
    (void)indent;
    if (ds_obj_is(v, DS_OBJ_STRING)) {
        jo_escape(o, AS_STRING(v)->chars, AS_STRING(v)->len);
        return;
    }
    if (ds_obj_is(v, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(v);
        jo_push(o, "[", 1);
        for (int i = 0; i < l->len; i++) {
            if (i > 0)
                jo_push(o, ",", 1);
            json_write(vm, o, l->items[i], indent, depth + 1);
        }
        jo_push(o, "]", 1);
        return;
    }
    if (ds_obj_is(v, DS_OBJ_DICT)) {
        DSDict *d = AS_DICT(v);
        jo_push(o, "{", 1);
        int first = 1;
        for (int i = 0; i < d->cap; i++) {
            if (d->entries[i].state != 1)
                continue;
            if (!first)
                jo_push(o, ",", 1);
            first = 0;
            DSValue k = d->entries[i].key;
            if (ds_obj_is(k, DS_OBJ_STRING))
                jo_escape(o, AS_STRING(k)->chars, AS_STRING(k)->len);
            else {
                DSString *ks = ds_val_to_string(vm, k);
                jo_escape(o, ks->chars, ks->len);
            }
            jo_push(o, ":", 1);
            json_write(vm, o, d->entries[i].val, indent, depth + 1);
        }
        jo_push(o, "}", 1);
        return;
    }
    jo_puts(o, "null");
}

static DSValue json_encode(DSVM *vm, int argc, DSValue *argv) {
    JsonOut o;
    memset(&o, 0, sizeof(o));
    json_write(vm, &o, arg(vm, argc, argv, 0), 0, 0);
    DSString *r = ds_string_new(vm, o.buf ? o.buf : "null", o.len);
    free(o.buf);
    return ds_obj_val(r);
}

typedef struct JsonIn {
    const char *s;
    size_t pos;
    size_t len;
    int err;
} JsonIn;

static void json_ws(JsonIn *j) {
    while (j->pos < j->len && isspace((unsigned char)j->s[j->pos]))
        j->pos++;
}

static DSValue json_parse_value(DSVM *vm, JsonIn *j, int depth);

static DSValue json_parse_string(DSVM *vm, JsonIn *j) {
    char *buf = (char *)malloc(j->len - j->pos + 1);
    size_t n = 0;
    j->pos++; /* opening quote */
    while (j->pos < j->len && j->s[j->pos] != '"') {
        char c = j->s[j->pos++];
        if (c == '\\' && j->pos < j->len) {
            char e = j->s[j->pos++];
            switch (e) {
            case 'n': buf[n++] = '\n'; break;
            case 't': buf[n++] = '\t'; break;
            case 'r': buf[n++] = '\r'; break;
            case 'b': buf[n++] = '\b'; break;
            case 'f': buf[n++] = '\f'; break;
            case 'u': {
                unsigned cp = 0;
                for (int i = 0; i < 4 && j->pos < j->len; i++) {
                    char h = j->s[j->pos++];
                    int d = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? 10 + h - 'a' : (h >= 'A' && h <= 'F') ? 10 + h - 'A' : 0;
                    cp = cp * 16 + (unsigned)d;
                }
                if (cp < 0x80) {
                    buf[n++] = (char)cp;
                } else if (cp < 0x800) {
                    buf[n++] = (char)(0xC0 | (cp >> 6));
                    buf[n++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    buf[n++] = (char)(0xE0 | (cp >> 12));
                    buf[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    buf[n++] = (char)(0x80 | (cp & 0x3F));
                }
                break;
            }
            default: buf[n++] = e; break;
            }
        } else {
            buf[n++] = c;
        }
    }
    if (j->pos < j->len)
        j->pos++; /* closing quote */
    DSString *r = ds_string_new(vm, buf ? buf : "", n);
    free(buf);
    return ds_obj_val(r);
}

static DSValue json_parse_value(DSVM *vm, JsonIn *j, int depth) {
    if (depth > 64) {
        j->err = 1;
        return DS_VAL_NIL;
    }
    json_ws(j);
    if (j->pos >= j->len) {
        j->err = 1;
        return DS_VAL_NIL;
    }
    char c = j->s[j->pos];
    if (c == '{') {
        j->pos++;
        DSDict *d = ds_dict_new(vm, 8);
        if (!d)
            return DS_VAL_NIL;
        ds_push_root(vm, ds_obj_val(d));
        json_ws(j);
        if (j->pos < j->len && j->s[j->pos] == '}') {
            j->pos++;
            ds_pop_roots(vm, 1);
            return ds_obj_val(d);
        }
        for (;;) {
            json_ws(j);
            if (j->pos >= j->len || j->s[j->pos] != '"') {
                j->err = 1;
                break;
            }
            DSValue k = json_parse_string(vm, j);
            json_ws(j);
            if (j->pos >= j->len || j->s[j->pos] != ':') {
                j->err = 1;
                break;
            }
            j->pos++;
            DSValue v = json_parse_value(vm, j, depth + 1);
            ds_dict_set(vm, d, k, v);
            json_ws(j);
            if (j->pos < j->len && j->s[j->pos] == ',') {
                j->pos++;
                continue;
            }
            if (j->pos < j->len && j->s[j->pos] == '}') {
                j->pos++;
                break;
            }
            j->err = 1;
            break;
        }
        ds_pop_roots(vm, 1);
        return ds_obj_val(d);
    }
    if (c == '[') {
        j->pos++;
        DSList *l = ds_list_new(vm, 8);
        if (!l)
            return DS_VAL_NIL;
        ds_push_root(vm, ds_obj_val(l));
        json_ws(j);
        if (j->pos < j->len && j->s[j->pos] == ']') {
            j->pos++;
            ds_pop_roots(vm, 1);
            return ds_obj_val(l);
        }
        for (;;) {
            DSValue v = json_parse_value(vm, j, depth + 1);
            ds_list_push(vm, l, v);
            json_ws(j);
            if (j->pos < j->len && j->s[j->pos] == ',') {
                j->pos++;
                continue;
            }
            if (j->pos < j->len && j->s[j->pos] == ']') {
                j->pos++;
                break;
            }
            j->err = 1;
            break;
        }
        ds_pop_roots(vm, 1);
        return ds_obj_val(l);
    }
    if (c == '"')
        return json_parse_string(vm, j);
    if (!strncmp(j->s + j->pos, "true", 4)) {
        j->pos += 4;
        return DS_VAL_TRUE;
    }
    if (!strncmp(j->s + j->pos, "false", 5)) {
        j->pos += 5;
        return DS_VAL_FALSE;
    }
    if (!strncmp(j->s + j->pos, "null", 4)) {
        j->pos += 4;
        return DS_VAL_NIL;
    }
    /* number */
    char *endp = NULL;
    double d = strtod(j->s + j->pos, &endp);
    if (endp && endp > j->s + j->pos) {
        j->pos = (size_t)(endp - j->s);
        return ds_num(d);
    }
    j->err = 1;
    return DS_VAL_NIL;
}

static DSValue json_decode(DSVM *vm, int argc, DSValue *argv) {
    DSValue v = arg(vm, argc, argv, 0);
    if (!ds_obj_is(v, DS_OBJ_STRING)) {
        ds_error(vm, 0, "json.decode() требует строку");
        return DS_VAL_NIL;
    }
    DSString *s = AS_STRING(v);
    JsonIn j;
    j.s = s->chars;
    j.pos = 0;
    j.len = s->len;
    j.err = 0;
    DSValue r = json_parse_value(vm, &j, 0);
    if (j.err) {
        if (argc > 1)
            return argv[1];
        ds_error(vm, 0, "ошибка разбора JSON около позиции %d", (int)j.pos);
        return DS_VAL_NIL;
    }
    return r;
}

/* ============================================================================
 * time MODULE
 * ============================================================================ */

static DSValue t_now(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return ds_num(ds_time_sec());
}

static DSValue t_clock(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return ds_num((double)ds_monotonic_ns() * 1e-9);
}

static DSValue t_wall(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    (void)argc;
    (void)argv;
    return ds_num((double)ds_wall_clock());
}

static DSValue t_sleep(DSVM *vm, int argc, DSValue *argv) {
    (void)vm;
    double sec = arg_num(vm, argc, argv, 0, 0);
    if (sec > 0 && sec < 5.0) {
        /* Busy-wait guard so the REPL never blocks the render thread long */
        uint64_t until = ds_monotonic_ns() + (uint64_t)(sec * 1e9);
        while (ds_monotonic_ns() < until) {
        }
    }
    return DS_VAL_NIL;
}

static DSValue t_dt(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.last_dt);
}

static DSValue t_fps(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    return ds_num(vm->love.fps_smooth > 0.0 ? 1.0 / vm->love.fps_smooth : 0.0);
}

/* ============================================================================
 * fs MODULE
 * ============================================================================ */

static int path_join(char *out, size_t cap, const char *dir, const char *name) {
    if (!name || !*name)
        return 0;
    if (name[0] == '/')
        return snprintf(out, cap, "%s", name) < (int)cap;
    const char *base = (dir && *dir) ? dir : ".";
    return snprintf(out, cap, "%s/%s", base, name) < (int)cap;
}

static DSValue fs_read(DSVM *vm, int argc, DSValue *argv) {
    const char *name = arg_str(vm, argc, argv, 0, "");
    char path[640];
    if (!path_join(path, sizeof(path), vm->project_dir, name)) {
        ds_error(vm, 0, "путь слишком длинный");
        return DS_VAL_NIL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        ds_error(vm, 0, "не удалось открыть '%s'", path);
        return DS_VAL_NIL;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0 || sz > 32 * 1024 * 1024) {
        fclose(f);
        ds_error(vm, 0, "файл '%s' слишком большой", path);
        return DS_VAL_NIL;
    }
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        ds_error(vm, 0, "нехватка памяти");
        return DS_VAL_NIL;
    }
    size_t rd = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    DSString *r = ds_string_new(vm, buf, rd);
    free(buf);
    return ds_obj_val(r);
}

static DSValue fs_write(DSVM *vm, int argc, DSValue *argv) {
    const char *name = arg_str(vm, argc, argv, 0, "");
    DSString *content = ds_val_to_string(vm, arg(vm, argc, argv, 1));
    char path[640];
    if (!path_join(path, sizeof(path), vm->project_dir, name)) {
        ds_error(vm, 0, "путь слишком длинный");
        return DS_VAL_NIL;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        ds_error(vm, 0, "не удалось записать '%s'", path);
        return DS_VAL_NIL;
    }
    fwrite(content->chars, 1, content->len, f);
    fclose(f);
    return DS_VAL_NIL;
}

static DSValue fs_exists(DSVM *vm, int argc, DSValue *argv) {
    const char *name = arg_str(vm, argc, argv, 0, "");
    char path[640];
    if (!path_join(path, sizeof(path), vm->project_dir, name))
        return DS_VAL_FALSE;
    FILE *f = fopen(path, "rb");
    if (f) {
        fclose(f);
        return DS_VAL_TRUE;
    }
    return DS_VAL_FALSE;
}

static DSValue fs_remove(DSVM *vm, int argc, DSValue *argv) {
    const char *name = arg_str(vm, argc, argv, 0, "");
    char path[640];
    if (!path_join(path, sizeof(path), vm->project_dir, name))
        return DS_VAL_FALSE;
    return ds_bool(remove(path) == 0);
}

/* ============================================================================
 * re MODULE (small regex engine: literal, '.', '*', '+', '?', classes, anchors)
 * ============================================================================ */

typedef struct Regex {
    const char *pat;
    size_t len;
} Regex;

static int re_match_here(const char *pat, size_t plen, const char *s, size_t slen, int *out_len) {
    size_t pi = 0, si = 0;
    while (pi < plen) {
        char pc = pat[pi];
        /* Parse an atom */
        char atom = pc;
        int cls = 0;
        if (pc == '\\' && pi + 1 < plen) {
            atom = pat[pi + 1];
            pi += 2;
        } else if (pc == '.') {
            cls = 1;
            pi++;
        } else if (pc == '[') {
            size_t j = pi + 1;
            int negate = 0;
            if (j < plen && pat[j] == '^') {
                negate = 1;
                j++;
            }
            const char *set = pat + j;
            size_t set_len = 0;
            while (j < plen && pat[j] != ']') {
                j++;
                set_len++;
            }
            if (j < plen)
                j++;
            if (si >= slen)
                return 0;
            int hit = 0;
            for (size_t k = 0; k < set_len; k++)
                if (set[k] == s[si])
                    hit = 1;
            if (hit == negate)
                return 0;
            si++;
            pi = j;
            continue;
        } else {
            pi++;
        }

        char quant = 0;
        if (pi < plen && (pat[pi] == '*' || pat[pi] == '+' || pat[pi] == '?')) {
            quant = pat[pi];
            pi++;
        }

        size_t match_start = si;
        if (quant == '*') {
            if (cls) {
                while (si < slen)
                    si++;
            } else {
                while (si < slen && s[si] == atom)
                    si++;
            }
            /* backtracking: try to satisfy the rest */
            for (size_t k = si + 1; k-- > match_start;) {
                int sub = 0;
                if (re_match_here(pat + pi, plen - pi, s + k, slen - k, &sub)) {
                    if (out_len)
                        *out_len = (int)(k + (size_t)sub);
                    return 1;
                }
                if (k == 0)
                    break;
            }
            continue;
        }
        if (quant == '+') {
            size_t count = 0;
            while (si < slen && (cls || s[si] == atom)) {
                si++;
                count++;
            }
            if (count == 0)
                return 0;
            for (size_t k = si + 1; k-- > match_start + 1;) {
                int sub = 0;
                if (re_match_here(pat + pi, plen - pi, s + k, slen - k, &sub)) {
                    if (out_len)
                        *out_len = (int)(k + (size_t)sub);
                    return 1;
                }
                if (k == 0)
                    break;
            }
            continue;
        }
        if (quant == '?') {
            int sub = 0;
            if (si < slen && (cls || s[si] == atom)) {
                if (re_match_here(pat + pi, plen - pi, s + si + 1, slen - si - 1, &sub)) {
                    if (out_len)
                        *out_len = 1 + sub;
                    return 1;
                }
            }
            continue;
        }
        if (si >= slen)
            return 0;
        if (!cls && s[si] != atom)
            return 0;
        si++;
        (void)quant;
    }
    if (out_len)
        *out_len = (int)si;
    return 1;
}

static int re_search(Regex *re, const char *s, size_t slen, size_t *out_start, size_t *out_len) {
    size_t plen = re->len;
    const char *pat = re->pat;
    int anchor_start = 0;
    if (plen > 0 && pat[0] == '^') {
        anchor_start = 1;
        pat++;
        plen--;
    }
    int anchor_end = 0;
    if (plen > 0 && pat[plen - 1] == '$') {
        anchor_end = 1;
        plen--;
    }
    for (size_t i = 0; i <= slen; i++) {
        int mlen = 0;
        if (re_match_here(pat, plen, s + i, slen - i, &mlen)) {
            if (anchor_end && i + (size_t)mlen != slen)
                continue;
            *out_start = i;
            *out_len = (size_t)mlen;
            return 1;
        }
        if (anchor_start)
            break;
    }
    return 0;
}

static DSValue re_find(DSVM *vm, int argc, DSValue *argv) {
    Regex re;
    const char *p = arg_str(vm, argc, argv, 0, "");
    const char *s = arg_str(vm, argc, argv, 1, "");
    re.pat = p;
    re.len = strlen(p);
    size_t start = 0, len = 0;
    if (!re_search(&re, s, strlen(s), &start, &len))
        return DS_VAL_NIL;
    return ds_obj_val(ds_string_new(vm, s + start, len));
}

static DSValue re_find_all(DSVM *vm, int argc, DSValue *argv) {
    Regex re;
    const char *p = arg_str(vm, argc, argv, 0, "");
    const char *s = arg_str(vm, argc, argv, 1, "");
    size_t slen = strlen(s);
    re.pat = p;
    re.len = strlen(p);
    DSList *out = ds_list_new(vm, 8);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    size_t pos = 0;
    while (pos <= slen) {
        size_t start = 0, len = 0;
        if (!re_search(&re, s + pos, slen - pos, &start, &len))
            break;
        ds_list_push(vm, out, ds_obj_val(ds_string_new(vm, s + pos + start, len)));
        pos += start + (len > 0 ? len : 1);
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(out);
}

static DSValue re_replace(DSVM *vm, int argc, DSValue *argv) {
    Regex re;
    const char *p = arg_str(vm, argc, argv, 0, "");
    const char *s = arg_str(vm, argc, argv, 1, "");
    const char *rep = arg_str(vm, argc, argv, 2, "");
    size_t slen = strlen(s);
    re.pat = p;
    re.len = strlen(p);
    size_t rl = strlen(rep);
    size_t cap = slen + 32, len = 0;
    char *buf = (char *)malloc(cap);
    if (!buf)
        return DS_VAL_NIL;
    size_t pos = 0;
    while (pos < slen) {
        size_t start = 0, mlen = 0;
        if (!re_search(&re, s + pos, slen - pos, &start, &mlen))
            break;
        size_t need = len + start + rl + 2;
        if (need > cap) {
            while (cap < need)
                cap *= 2;
            char *nb = (char *)realloc(buf, cap);
            if (!nb)
                break;
            buf = nb;
        }
        memcpy(buf + len, s + pos, start);
        len += start;
        memcpy(buf + len, rep, rl);
        len += rl;
        pos += start + (mlen > 0 ? mlen : 1);
    }
    if (pos < slen) {
        size_t need = len + (slen - pos) + 2;
        if (need > cap) {
            cap = need;
            char *nb = (char *)realloc(buf, cap);
            if (nb)
                buf = nb;
        }
        memcpy(buf + len, s + pos, slen - pos);
        len += slen - pos;
    }
    DSString *r = ds_string_new(vm, buf, len);
    free(buf);
    return ds_obj_val(r);
}

static DSValue re_match(DSVM *vm, int argc, DSValue *argv) {
    Regex re;
    const char *p = arg_str(vm, argc, argv, 0, "");
    const char *s = arg_str(vm, argc, argv, 1, "");
    re.pat = p;
    re.len = strlen(p);
    int mlen = 0;
    return ds_bool(re_match_here(re.pat, re.len, s, strlen(s), &mlen));
}

/* ============================================================================
 * color MODULE
 * ============================================================================ */

static uint32_t pack_argb(double a, double r, double g, double b) {
    if (r <= 1.0 && g <= 1.0 && b <= 1.0 && a <= 1.0) {
        r *= 255.0;
        g *= 255.0;
        b *= 255.0;
        a *= 255.0;
    }
    uint32_t A = (uint32_t)(a < 0 ? 0 : (a > 255 ? 255 : a));
    uint32_t R = (uint32_t)(r < 0 ? 0 : (r > 255 ? 255 : r));
    uint32_t G = (uint32_t)(g < 0 ? 0 : (g > 255 ? 255 : g));
    uint32_t B = (uint32_t)(b < 0 ? 0 : (b > 255 ? 255 : b));
    return (A << 24) | (R << 16) | (G << 8) | B;
}

static DSValue c_rgb(DSVM *vm, int argc, DSValue *argv) {
    double r = arg_num(vm, argc, argv, 0, 1), g = arg_num(vm, argc, argv, 1, 1), b = arg_num(vm, argc, argv, 2, 1);
    return ds_num((double)pack_argb(255, r, g, b));
}
static DSValue c_rgba(DSVM *vm, int argc, DSValue *argv) {
    double r = arg_num(vm, argc, argv, 0, 1), g = arg_num(vm, argc, argv, 1, 1);
    double b = arg_num(vm, argc, argv, 2, 1), a = arg_num(vm, argc, argv, 3, 1);
    return ds_num((double)pack_argb(a, r, g, b));
}
static DSValue c_hsv(DSVM *vm, int argc, DSValue *argv) {
    float h = (float)arg_num(vm, argc, argv, 0, 0), s = (float)arg_num(vm, argc, argv, 1, 1);
    float v = (float)arg_num(vm, argc, argv, 2, 1), a = (float)arg_num(vm, argc, argv, 3, 1);
    return ds_num((double)ds_hsv_to_argb(h, s, v, a));
}
static DSValue c_lerp(DSVM *vm, int argc, DSValue *argv) {
    uint32_t a = (uint32_t)arg_num(vm, argc, argv, 0, 0);
    uint32_t b = (uint32_t)arg_num(vm, argc, argv, 1, 0);
    float t = (float)arg_num(vm, argc, argv, 2, 0);
    return ds_num((double)ds_lerp_argb(a, b, t));
}
static DSValue c_unpack(DSVM *vm, int argc, DSValue *argv) {
    uint32_t c = (uint32_t)arg_num(vm, argc, argv, 0, 0);
    DSList *l = ds_list_new(vm, 4);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num((c >> 16) & 0xFF));
    ds_list_push(vm, l, ds_num((c >> 8) & 0xFF));
    ds_list_push(vm, l, ds_num(c & 0xFF));
    ds_list_push(vm, l, ds_num((c >> 24) & 0xFF));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}
static DSValue c_alpha(DSVM *vm, int argc, DSValue *argv) {
    uint32_t c = (uint32_t)arg_num(vm, argc, argv, 0, 0);
    double a = arg_num(vm, argc, argv, 1, 1);
    if (a <= 1.0)
        a *= 255.0;
    c = (c & 0x00FFFFFFu) | ((uint32_t)a << 24);
    return ds_num((double)c);
}

/* ============================================================================
 * vec2 MODULE
 * ============================================================================ */

static DSValue v2_make(DSVM *vm, int argc, DSValue *argv) {
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(arg_num(vm, argc, argv, 0, 0)));
    ds_list_push(vm, l, ds_num(arg_num(vm, argc, argv, 1, 0)));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue v2_add(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0)), *b = AS_LIST(arg(vm, argc, argv, 1));
    if (!a || !b || a->len < 2 || b->len < 2)
        return DS_VAL_NIL;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[0]) + ds_as_num(b->items[0])));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[1]) + ds_as_num(b->items[1])));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}
static DSValue v2_sub(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0)), *b = AS_LIST(arg(vm, argc, argv, 1));
    if (!a || !b || a->len < 2 || b->len < 2)
        return DS_VAL_NIL;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[0]) - ds_as_num(b->items[0])));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[1]) - ds_as_num(b->items[1])));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}
static DSValue v2_scale(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0));
    double k = arg_num(vm, argc, argv, 1, 1);
    if (!a || a->len < 2)
        return DS_VAL_NIL;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[0]) * k));
    ds_list_push(vm, l, ds_num(ds_as_num(a->items[1]) * k));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}
static DSValue v2_len(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0));
    if (!a || a->len < 2)
        return ds_num(0);
    return ds_num(hypot(ds_as_num(a->items[0]), ds_as_num(a->items[1])));
}
static DSValue v2_norm(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0));
    if (!a || a->len < 2)
        return DS_VAL_NIL;
    double x = ds_as_num(a->items[0]), y = ds_as_num(a->items[1]);
    double n = hypot(x, y);
    if (n == 0)
        n = 1;
    DSList *l = ds_list_new(vm, 2);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    ds_list_push(vm, l, ds_num(x / n));
    ds_list_push(vm, l, ds_num(y / n));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}
static DSValue v2_dot(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0)), *b = AS_LIST(arg(vm, argc, argv, 1));
    if (!a || !b || a->len < 2 || b->len < 2)
        return ds_num(0);
    return ds_num(ds_as_num(a->items[0]) * ds_as_num(b->items[0]) + ds_as_num(a->items[1]) * ds_as_num(b->items[1]));
}
static DSValue v2_dist(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0)), *b = AS_LIST(arg(vm, argc, argv, 1));
    if (!a || !b || a->len < 2 || b->len < 2)
        return ds_num(0);
    return ds_num(hypot(ds_as_num(a->items[0]) - ds_as_num(b->items[0]), ds_as_num(a->items[1]) - ds_as_num(b->items[1])));
}
static DSValue v2_angle(DSVM *vm, int argc, DSValue *argv) {
    DSList *a = AS_LIST(arg(vm, argc, argv, 0));
    if (!a || a->len < 2)
        return ds_num(0);
    return ds_num(atan2(ds_as_num(a->items[1]), ds_as_num(a->items[0])));
}

/* ============================================================================
 * gc / debug MODULE
 * ============================================================================ */

static DSValue gc_collect(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    ds_gc_collect_full(vm);
    return DS_VAL_NIL;
}

static DSValue gc_stats(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    DSDict *d = ds_dict_new(vm, 8);
    if (!d)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(d));
    ds_dict_set_cstr(vm, d, "bytes", ds_num((double)vm->gc.bytes_allocated));
    ds_dict_set_cstr(vm, d, "peak", ds_num((double)vm->gc.peak_bytes));
    ds_dict_set_cstr(vm, d, "objects", ds_num((double)vm->gc.object_count));
    ds_dict_set_cstr(vm, d, "minor", ds_num((double)vm->gc.young_runs));
    ds_dict_set_cstr(vm, d, "major", ds_num((double)vm->gc.major_runs));
    ds_dict_set_cstr(vm, d, "last_pause_us", ds_num(vm->gc.last_pause_us));
    ds_dict_set_cstr(vm, d, "max_pause_us", ds_num(vm->gc.max_pause_us));
    ds_pop_roots(vm, 1);
    return ds_obj_val(d);
}

static DSValue gc_pause(DSVM *vm, int argc, DSValue *argv) {
    int pct = (int)arg_num(vm, argc, argv, 0, 150);
    if (pct < 50)
        pct = 50;
    if (pct > 1000)
        pct = 1000;
    vm->gc.pause_pct = pct;
    return ds_num(pct);
}

static DSValue gc_pause_now(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    vm->gc.paused = 1;
    return DS_VAL_NIL;
}

static DSValue gc_resume(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    vm->gc.paused = 0;
    return DS_VAL_NIL;
}

/* ============================================================================
 * physics MODULE
 * ============================================================================ */

static DSDict *phys_methods(DSVM *vm) {
    DSDict *m = ds_dict_new(vm, 16);
    return m;
}

static DSValue ph_new(DSVM *vm, int argc, DSValue *argv) {
    float gx = (float)arg_num(vm, argc, argv, 0, 0.0f);
    float gy = (float)arg_num(vm, argc, argv, 1, 980.0f);
    DSPhysWorld *w = ds_phys_world_new(gx, gy);
    if (!w) {
        ds_error(vm, 0, "не удалось создать физический мир");
        return DS_VAL_NIL;
    }
    DSNativeObj *no = ds_native_new(vm, "physics.World", w, 4096, (DSNativeFinalizer)ds_phys_world_free,
                                    phys_methods(vm));
    return no ? ds_obj_val(no) : DS_VAL_NIL;
}

static DSPhysWorld *phys_world(DSValue v) {
    if (!ds_obj_is(v, DS_OBJ_NATIVE))
        return NULL;
    DSNativeObj *no = AS_NATIVE(v);
    if (!no->type_tag || strcmp(no->type_tag, "physics.World") != 0)
        return NULL;
    return (DSPhysWorld *)no->ptr;
}

static DSValue ph_add_body(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (!w) {
        ds_error(vm, 0, "physics: ожидался мир");
        return ds_num(-1);
    }
    const char *shape = arg_str(vm, argc, argv, 1, "circle");
    double x = arg_num(vm, argc, argv, 2, 0), y = arg_num(vm, argc, argv, 3, 0);
    double a = arg_num(vm, argc, argv, 4, 10), mass = arg_num(vm, argc, argv, 5, 1);
    double rest = arg_num(vm, argc, argv, 6, 0.4);
    if (strcmp(shape, "box") == 0 || strcmp(shape, "rect") == 0)
        return ds_num(ds_phys_add_box(w, (float)x, (float)y, (float)a, (float)mass, (float)(mass > 0 ? 1.0 : 0.0), (float)rest));
    return ds_num(ds_phys_add_circle(w, (float)x, (float)y, (float)a, (float)mass, (float)rest));
}

static DSValue ph_step(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (!w)
        return ds_num(0);
    float dt = (float)arg_num(vm, argc, argv, 1, 1.0 / 60.0);
    int sub = (int)arg_num(vm, argc, argv, 2, 1);
    return ds_num(ds_phys_step(w, dt, sub));
}

static DSValue ph_body(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    int id = (int)arg_num(vm, argc, argv, 1, -1);
    if (!w || id < 0)
        return DS_VAL_NIL;
    float x, y, vx, vy, ww, h, shape;
    int sh = 0;
    if (!ds_phys_get_body(w, id, &x, &y, &vx, &vy, &ww, &h, &sh))
        return DS_VAL_NIL;
    shape = (float)sh;
    DSDict *d = ds_dict_new(vm, 8);
    if (!d)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(d));
    ds_dict_set_cstr(vm, d, "x", ds_num(x));
    ds_dict_set_cstr(vm, d, "y", ds_num(y));
    ds_dict_set_cstr(vm, d, "vx", ds_num(vx));
    ds_dict_set_cstr(vm, d, "vy", ds_num(vy));
    ds_dict_set_cstr(vm, d, "w", ds_num(ww));
    ds_dict_set_cstr(vm, d, "h", ds_num(h));
    ds_dict_set_cstr(vm, d, "shape", ds_num(shape));
    ds_pop_roots(vm, 1);
    return ds_obj_val(d);
}

static DSValue ph_set_pos(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (w)
        ds_phys_set_pos(w, (int)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                        (float)arg_num(vm, argc, argv, 3, 0));
    return DS_VAL_NIL;
}

static DSValue ph_set_vel(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (w)
        ds_phys_set_vel(w, (int)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                        (float)arg_num(vm, argc, argv, 3, 0));
    return DS_VAL_NIL;
}

static DSValue ph_impulse(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (w)
        ds_phys_apply_impulse(w, (int)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                              (float)arg_num(vm, argc, argv, 3, 0));
    return DS_VAL_NIL;
}

static DSValue ph_remove(DSVM *vm, int argc, DSValue *argv) {
    DSPhysWorld *w = phys_world(arg(vm, argc, argv, 0));
    if (w)
        ds_phys_remove_body(w, (int)arg_num(vm, argc, argv, 1, -1));
    return DS_VAL_NIL;
}

/* ============================================================================
 * tween MODULE
 * ============================================================================ */

typedef struct TweenState {
    double from, to, t, dur;
    int active;
    int easing;
} TweenState;

static void tween_free(void *p) { free(p); }

static DSValue tw_new(DSVM *vm, int argc, DSValue *argv) {
    TweenState *t = (TweenState *)calloc(1, sizeof(TweenState));
    if (!t)
        return DS_VAL_NIL;
    t->from = arg_num(vm, argc, argv, 0, 0);
    t->to = arg_num(vm, argc, argv, 1, 1);
    t->dur = arg_num(vm, argc, argv, 2, 1.0);
    t->easing = (int)arg_num(vm, argc, argv, 3, 0);
    t->active = 1;
    DSNativeObj *no = ds_native_new(vm, "tween", t, sizeof(TweenState), tween_free, NULL);
    return no ? ds_obj_val(no) : DS_VAL_NIL;
}

static TweenState *tw_state(DSValue v) {
    if (!ds_obj_is(v, DS_OBJ_NATIVE))
        return NULL;
    DSNativeObj *no = AS_NATIVE(v);
    if (!no->type_tag || strcmp(no->type_tag, "tween") != 0)
        return NULL;
    return (TweenState *)no->ptr;
}

static double ease_apply(int easing, double t) {
    switch (easing) {
    case 1: return t * t;                     /* in quad */
    case 2: return t * (2.0 - t);             /* out quad */
    case 3: return t < 0.5 ? 2 * t * t : -1 + (4 - 2 * t) * t; /* in-out quad */
    case 4: return t * t * t;                 /* in cubic */
    case 5: return 1 - pow(1 - t, 3);         /* out cubic */
    case 6: return t < 0.5 ? 4 * t * t * t : 1 - pow(-2 * t + 2, 3) / 2; /* in-out cubic */
    default: return t;                        /* linear */
    }
}

static DSValue tw_update(DSVM *vm, int argc, DSValue *argv) {
    TweenState *t = tw_state(arg(vm, argc, argv, 0));
    double dt = arg_num(vm, argc, argv, 1, 1.0 / 60.0);
    if (!t)
        return ds_num(0);
    if (!t->active)
        return ds_num(t->to);
    t->t += dt;
    double k = t->dur > 0 ? t->t / t->dur : 1.0;
    if (k >= 1.0) {
        k = 1.0;
        t->active = 0;
    }
    double e = ease_apply(t->easing, k);
    return ds_num(t->from + (t->to - t->from) * e);
}

/* ============================================================================
 * REGISTRATION
 * ============================================================================ */

DSValue ds_fs_read_for_love(DSVM *vm, const char *name) {
    DSValue argv[1] = {ds_obj_val(ds_string_const(vm, name ? name : ""))};
    return fs_read(vm, 1, argv);
}

int ds_fs_write_for_love(DSVM *vm, const char *name, DSString *content) {
    DSValue argv[2];
    argv[0] = ds_obj_val(ds_string_const(vm, name ? name : ""));
    argv[1] = content ? ds_obj_val(content) : DS_VAL_NIL;
    fs_write(vm, 2, argv);
    return !vm->has_error;
}

static void register_globals(DSVM *vm) {
    DSDict *b = vm->builtins;
    reg(vm, b, "print", bi_print);
    reg(vm, b, "len", bi_len);
    reg(vm, b, "range", bi_range);
    reg(vm, b, "type", bi_type);
    reg(vm, b, "str", bi_str);
    reg(vm, b, "int", bi_int);
    reg(vm, b, "float", bi_float);
    reg(vm, b, "bool", bi_bool);
    reg(vm, b, "list", bi_list);
    reg(vm, b, "dict", bi_dict);
    reg(vm, b, "abs", bi_abs);
    reg(vm, b, "min", bi_min);
    reg(vm, b, "max", bi_max);
    reg(vm, b, "sum", bi_sum);
    reg(vm, b, "round", bi_round);
    reg(vm, b, "floor", bi_floor);
    reg(vm, b, "ceil", bi_ceil);
    reg(vm, b, "sorted", bi_sorted);
    reg(vm, b, "reversed", bi_reversed);
    reg(vm, b, "enumerate", bi_enumerate);
    reg(vm, b, "zip", bi_zip);
    reg(vm, b, "map", bi_map);
    reg(vm, b, "filter", bi_filter);
    reg(vm, b, "any", bi_any);
    reg(vm, b, "all", bi_all);
    reg(vm, b, "assert", bi_assert);
    reg(vm, b, "input", bi_input);
    reg(vm, b, "clamp", bi_clamp);
    reg(vm, b, "lerp", bi_lerp);
    reg(vm, b, "sign", bi_sign);
    reg(vm, b, "dist", bi_dist);
    reg(vm, b, "hasattr", bi_hasattr);
    reg(vm, b, "getattr", bi_getattr);
    reg(vm, b, "setattr", bi_setattr);
    reg(vm, b, "callable", bi_callable);
    reg(vm, b, "iter", bi_iter);
    reg(vm, b, "copy", bi_copy);
}

static void register_math(DSVM *vm) {
    DSDict *m = make_module(vm, "math");
    reg_num(vm, m, "pi", 3.14159265358979323846);
    reg_num(vm, m, "tau", 6.28318530717958647692);
    reg_num(vm, m, "e", 2.71828182845904523536);
    reg_num(vm, m, "inf", INFINITY);
    reg_num(vm, m, "nan", NAN);
    reg(vm, m, "sin", m_sin);
    reg(vm, m, "cos", m_cos);
    reg(vm, m, "tan", m_tan);
    reg(vm, m, "asin", m_asin);
    reg(vm, m, "acos", m_acos);
    reg(vm, m, "atan", m_atan);
    reg(vm, m, "atan2", m_atan2);
    reg(vm, m, "sqrt", m_sqrt);
    reg(vm, m, "pow", m_pow);
    reg(vm, m, "log", m_log);
    reg(vm, m, "exp", m_exp);
    reg(vm, m, "abs", m_abs);
    reg(vm, m, "floor", m_floor);
    reg(vm, m, "ceil", m_ceil);
    reg(vm, m, "fmod", m_fmod);
    reg(vm, m, "hypot", m_hypot);
    reg(vm, m, "deg", m_deg);
    reg(vm, m, "rad", m_rad);
    reg(vm, m, "clamp", m_clamp);
    reg(vm, m, "lerp", m_lerp);
    reg(vm, m, "sign", m_sign);
    reg(vm, m, "min", m_min);
    reg(vm, m, "max", m_max);
    reg(vm, m, "smoothstep", m_smoothstep);
    reg(vm, m, "noise2", m_noise2);
    reg(vm, m, "noise3", m_noise3);
    reg(vm, m, "fbm", m_fbm);
}

static void register_random(DSVM *vm) {
    DSDict *m = make_module(vm, "random");
    reg(vm, m, "random", rnd_random);
    reg(vm, m, "rand", rnd_random);
    reg(vm, m, "uniform", rnd_float);
    reg(vm, m, "int", rnd_int);
    reg(vm, m, "randint", rnd_int);
    reg(vm, m, "float", rnd_float);
    reg(vm, m, "choice", rnd_choice);
    reg(vm, m, "shuffle", rnd_shuffle);
    reg(vm, m, "seed", rnd_seed);
}

static void register_json(DSVM *vm) {
    DSDict *m = make_module(vm, "json");
    reg(vm, m, "encode", json_encode);
    reg(vm, m, "stringify", json_encode);
    reg(vm, m, "decode", json_decode);
    reg(vm, m, "parse", json_decode);
}

static void register_time(DSVM *vm) {
    DSDict *m = make_module(vm, "time");
    reg(vm, m, "now", t_now);
    reg(vm, m, "clock", t_clock);
    reg(vm, m, "wall", t_wall);
    reg(vm, m, "sleep", t_sleep);
    reg(vm, m, "dt", t_dt);
    reg(vm, m, "fps", t_fps);
}

static void register_fs(DSVM *vm) {
    DSDict *m = make_module(vm, "fs");
    reg(vm, m, "read", fs_read);
    reg(vm, m, "write", fs_write);
    reg(vm, m, "exists", fs_exists);
    reg(vm, m, "remove", fs_remove);
    reg(vm, m, "delete", fs_remove);
}

static void register_re(DSVM *vm) {
    DSDict *m = make_module(vm, "re");
    reg(vm, m, "find", re_find);
    reg(vm, m, "find_all", re_find_all);
    reg(vm, m, "replace", re_replace);
    reg(vm, m, "match", re_match);
}

static void register_color(DSVM *vm) {
    DSDict *m = make_module(vm, "color");
    reg(vm, m, "rgb", c_rgb);
    reg(vm, m, "rgba", c_rgba);
    reg(vm, m, "hsv", c_hsv);
    reg(vm, m, "lerp", c_lerp);
    reg(vm, m, "unpack", c_unpack);
    reg(vm, m, "alpha", c_alpha);
    reg_num(vm, m, "white", 0xFFFFFFFFu);
    reg_num(vm, m, "black", 0xFF000000u);
    reg_num(vm, m, "red", 0xFFFF3B30u);
    reg_num(vm, m, "green", 0xFF34C759u);
    reg_num(vm, m, "blue", 0xFF0A84FFu);
    reg_num(vm, m, "yellow", 0xFFFFD60Au);
    reg_num(vm, m, "cyan", 0xFF32D2E0u);
    reg_num(vm, m, "magenta", 0xFFFF2D95u);
    reg_num(vm, m, "orange", 0xFFFF9F0Au);
    reg_num(vm, m, "gray", 0xFF8E8E93u);
}

static void register_vec(DSVM *vm) {
    DSDict *m = make_module(vm, "vec");
    reg(vm, m, "new", v2_make);
    reg(vm, m, "add", v2_add);
    reg(vm, m, "sub", v2_sub);
    reg(vm, m, "scale", v2_scale);
    reg(vm, m, "len", v2_len);
    reg(vm, m, "norm", v2_norm);
    reg(vm, m, "dot", v2_dot);
    reg(vm, m, "dist", v2_dist);
    reg(vm, m, "angle", v2_angle);
}

static void register_gc(DSVM *vm) {
    DSDict *m = make_module(vm, "gc");
    reg(vm, m, "collect", gc_collect);
    reg(vm, m, "stats", gc_stats);
    reg(vm, m, "pause", gc_pause);
    reg(vm, m, "stop", gc_pause_now);
    reg(vm, m, "restart", gc_resume);
}

static void register_physics(DSVM *vm) {
    DSDict *m = make_module(vm, "physics");
    reg(vm, m, "new", ph_new);
    reg(vm, m, "add", ph_add_body);
    reg(vm, m, "step", ph_step);
    reg(vm, m, "body", ph_body);
    reg(vm, m, "set_pos", ph_set_pos);
    reg(vm, m, "set_vel", ph_set_vel);
    reg(vm, m, "impulse", ph_impulse);
    reg(vm, m, "remove", ph_remove);
}

static void register_tween(DSVM *vm) {
    DSDict *m = make_module(vm, "tween");
    reg(vm, m, "new", tw_new);
    reg(vm, m, "update", tw_update);
}

static void register_methods(DSVM *vm) {
    DSDict *s = vm->str_methods;
    reg_method(vm, s, "upper", sm_upper);
    reg_method(vm, s, "lower", sm_lower);
    reg_method(vm, s, "strip", sm_strip);
    reg_method(vm, s, "trim", sm_strip);
    reg_method(vm, s, "split", sm_split);
    reg_method(vm, s, "join", sm_join);
    reg_method(vm, s, "replace", sm_replace);
    reg_method(vm, s, "find", sm_find);
    reg_method(vm, s, "index", sm_find);
    reg_method(vm, s, "startswith", sm_startswith);
    reg_method(vm, s, "endswith", sm_endswith);
    reg_method(vm, s, "substr", sm_substr);
    reg_method(vm, s, "sub", sm_substr);
    reg_method(vm, s, "char_at", sm_char_at);
    reg_method(vm, s, "repeat", sm_repeat);
    reg_method(vm, s, "chars", sm_chars);
    reg_method(vm, s, "len", sm_len);

    DSDict *l = vm->list_methods;
    reg_method(vm, l, "append", lm_append);
    reg_method(vm, l, "push", lm_append);
    reg_method(vm, l, "extend", lm_extend);
    reg_method(vm, l, "pop", lm_pop);
    reg_method(vm, l, "insert", lm_insert);
    reg_method(vm, l, "remove", lm_remove);
    reg_method(vm, l, "index", lm_index);
    reg_method(vm, l, "clear", lm_clear);
    reg_method(vm, l, "copy", lm_copy);
    reg_method(vm, l, "reverse", lm_reverse);
    reg_method(vm, l, "sort", lm_sort);
    reg_method(vm, l, "count", lm_count);

    DSDict *d = vm->dict_methods;
    reg_method(vm, d, "keys", dm_keys);
    reg_method(vm, d, "values", dm_values);
    reg_method(vm, d, "items", dm_items);
    reg_method(vm, d, "get", dm_get);
    reg_method(vm, d, "set", dm_set);
    reg_method(vm, d, "has", dm_has);
    reg_method(vm, d, "remove", dm_remove);
    reg_method(vm, d, "pop", dm_remove);
    reg_method(vm, d, "clear", dm_clear);
    reg_method(vm, d, "copy", dm_copy);
}

void ds_register_builtins(DSVM *vm) {
    register_globals(vm);
    register_math(vm);
    register_random(vm);
    register_json(vm);
    register_time(vm);
    register_fs(vm);
    register_re(vm);
    register_color(vm);
    register_vec(vm);
    register_gc(vm);
    register_physics(vm);
    register_tween(vm);
    register_methods(vm);
}
