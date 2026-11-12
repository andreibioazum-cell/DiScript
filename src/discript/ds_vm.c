/* DiScript VM: direct-threaded execution, operator runtime, containers,
 * OOP dispatch, inline-cached properties/methods, error reporting, REPL. */

#include "discript/ds_ast.h"
#include "discript/ds_lexer.h"
#include "discript/discript.h"
#include "engine/runtime.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * ERRORS
 * ============================================================================ */

void ds_error(DSVM *vm, int line, const char *fmt, ...) {
    if (vm->has_error)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(vm->err_msg, sizeof(vm->err_msg), fmt, ap);
    va_end(ap);
    vm->err_line = line;
    if (vm->err_file[0] == '\0')
        snprintf(vm->err_file, sizeof(vm->err_file), "main.ds");
    vm->has_error = 1;
}

void ds_vm_traceback(DSVM *vm, char *out, size_t cap) {
    if (!out || cap == 0)
        return;
    out[0] = '\0';
    size_t used = 0;
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        DSCallFrame *f = &vm->frames[i];
        const char *nm = "<модуль>";
        int line = f->line;
        if (f->closure && f->closure->proto && f->closure->proto->name)
            nm = f->closure->proto->name->chars;
        else if (f->c_name)
            nm = f->c_name;
        int n = snprintf(out + used, cap - used, "%s  в '%s' (строка %d)\n",
                         used == 0 ? "" : "  ", nm, line);
        if (n < 0 || (size_t)n >= cap - used)
            break;
        used += (size_t)n;
    }
}

/* ============================================================================
 * VALUE SEMANTICS
 * ============================================================================ */

const char *ds_type_name(DSValue v) {
    if (ds_is_num(v))
        return "число";
    if (v == DS_VAL_NIL)
        return "None";
    if (ds_is_bool(v))
        return "bool";
    if (!ds_is_obj(v))
        return "значение";
    switch (ds_as_obj(v)->tt) {
    case DS_OBJ_STRING: return "str";
    case DS_OBJ_LIST: return "list";
    case DS_OBJ_DICT: return "dict";
    case DS_OBJ_FUNC: return "функция";
    case DS_OBJ_CLOSURE: return "функция";
    case DS_OBJ_CFUNC: return "встроенная функция";
    case DS_OBJ_BOUND: return "метод";
    case DS_OBJ_CLASS: return "класс";
    case DS_OBJ_INSTANCE: {
        DSInstance *in = AS_INSTANCE(v);
        return in->klass && in->klass->name ? in->klass->name->chars : "объект";
    }
    case DS_OBJ_NATIVE: {
        DSNativeObj *no = AS_NATIVE(v);
        return no->type_tag ? no->type_tag : "объект";
    }
    case DS_OBJ_MODULE: return "модуль";
    default: return "объект";
    }
}

int ds_is_truthy(DSValue v) {
    if (ds_is_num(v))
        return ds_as_num(v) != 0.0 && !isnan(ds_as_num(v));
    if (v == DS_VAL_NIL)
        return 0;
    if (v == DS_VAL_FALSE)
        return 0;
    if (v == DS_VAL_TRUE)
        return 1;
    if (!ds_is_obj(v))
        return 1;
    switch (ds_as_obj(v)->tt) {
    case DS_OBJ_STRING: return AS_STRING(v)->len > 0;
    case DS_OBJ_LIST: return AS_LIST(v)->len > 0;
    case DS_OBJ_DICT: return AS_DICT(v)->count > 0;
    default: return 1;
    }
}

int ds_values_equal(DSValue a, DSValue b) {
    if (a == b)
        return 1;
    if (ds_is_num(a) && ds_is_num(b))
        return ds_as_num(a) == ds_as_num(b);
    if (!ds_is_obj(a) || !ds_is_obj(b))
        return 0;
    DSGCObject *oa = ds_as_obj(a);
    DSGCObject *ob = ds_as_obj(b);

    if (oa->tt == DS_OBJ_STRING && ob->tt == DS_OBJ_STRING)
        return AS_STRING(a)->len == AS_STRING(b)->len &&
               memcmp(AS_STRING(a)->chars, AS_STRING(b)->chars, AS_STRING(a)->len) == 0;
    if (oa->tt == DS_OBJ_LIST && ob->tt == DS_OBJ_LIST) {
        DSList *la = AS_LIST(a), *lb = AS_LIST(b);
        if (la->len != lb->len)
            return 0;
        for (int i = 0; i < la->len; i++)
            if (!ds_values_equal(la->items[i], lb->items[i]))
                return 0;
        return 1;
    }
    if (oa->tt == DS_OBJ_DICT && ob->tt == DS_OBJ_DICT) {
        DSDict *da = AS_DICT(a), *db = AS_DICT(b);
        if (da->count != db->count)
            return 0;
        for (int i = 0; i < da->cap; i++) {
            if (da->entries[i].state != 1)
                continue;
            DSValue other;
            if (!ds_dict_get(db, da->entries[i].key, &other))
                return 0;
            if (!ds_values_equal(da->entries[i].val, other))
                return 0;
        }
        return 1;
    }
    return 0;
}

uint32_t ds_value_hash(DSValue v) {
    if (ds_is_num(v)) {
        double d = ds_as_num(v);
        if (d == floor(d) && fabs(d) < 9e15)
            return (uint32_t)((int64_t)d) * 2654435761u;
        uint64_t bits;
        memcpy(&bits, &d, 8);
        return (uint32_t)(bits ^ (bits >> 32)) * 2246822519u;
    }
    if (v == DS_VAL_NIL)
        return 7u;
    if (v == DS_VAL_TRUE)
        return 11u;
    if (v == DS_VAL_FALSE)
        return 13u;
    if (!ds_is_obj(v))
        return 17u;
    DSGCObject *o = ds_as_obj(v);
    if (o->tt == DS_OBJ_STRING) {
        DSString *s = (DSString *)o;
        if (s->hash == 0)
            s->hash = ds_hash_bytes(s->chars, s->len);
        return s->hash ? s->hash : 1u;
    }
    return (uint32_t)(((uintptr_t)o >> 4) * 2654435761u);
}

double ds_to_number(DSVM *vm, DSValue v) {
    if (ds_is_num(v))
        return ds_as_num(v);
    if (ds_is_bool(v))
        return ds_as_bool(v) ? 1.0 : 0.0;
    if (v == DS_VAL_NIL)
        return 0.0;
    if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(v);
        char *endp = NULL;
        double d = strtod(s->chars, &endp);
        if (endp && *endp == '\0')
            return d;
    }
    if (vm)
        ds_error(vm, 0, "невозможно преобразовать '%s' в число", ds_type_name(v));
    return 0.0;
}

long long ds_to_int(DSVM *vm, DSValue v) {
    double d = ds_to_number(vm, v);
    if (d >= 9.2e18)
        return (long long)9.2e18;
    if (d <= -9.2e18)
        return (long long)-9.2e18;
    return (long long)d;
}

/* ============================================================================
 * NUMBER FORMATTING (Python-compatible-ish repr)
 * ============================================================================ */

static void format_number(double d, char *out, size_t cap, int int_bias) {
    if (isnan(d)) {
        snprintf(out, cap, "nan");
        return;
    }
    if (isinf(d)) {
        snprintf(out, cap, d > 0 ? "inf" : "-inf");
        return;
    }
    if (d == 0.0) {
        snprintf(out, cap, "0");
        return;
    }
    if (d == floor(d) && fabs(d) < 1e16) {
        (void)int_bias;
        snprintf(out, cap, "%.0f", d);
        return;
    }
    snprintf(out, cap, "%.14g", d);
    /* ensure a decimal marker so numbers look like floats */
    if (int_bias == 0 && !strchr(out, '.') && !strchr(out, 'e') && !strchr(out, 'n') && !strchr(out, 'i')) {
        size_t n = strlen(out);
        if (n + 2 < cap) {
            out[n] = '.';
            out[n + 1] = '0';
            out[n + 2] = '\0';
        }
    }
}

DSString *ds_val_to_string(DSVM *vm, DSValue v) {
    char buf[64];
    if (ds_is_num(v)) {
        format_number(ds_as_num(v), buf, sizeof(buf), 0);
        return ds_string_new(vm, buf, strlen(buf));
    }
    if (v == DS_VAL_NIL)
        return ds_string_new(vm, "None", 4);
    if (v == DS_VAL_TRUE)
        return ds_string_new(vm, "True", 4);
    if (v == DS_VAL_FALSE)
        return ds_string_new(vm, "False", 5);
    if (!ds_is_obj(v)) {
        snprintf(buf, sizeof(buf), "<значение>");
        return ds_string_new(vm, buf, strlen(buf));
    }
    switch (ds_as_obj(v)->tt) {
    case DS_OBJ_STRING:
        return AS_STRING(v);
    case DS_OBJ_LIST: {
        DSList *l = AS_LIST(v);
        /* Build with a growable buffer */
        size_t cap = 64, len = 0;
        char *b = (char *)malloc(cap);
        if (!b)
            return ds_string_new(vm, "[]", 2);
        b[len++] = '[';
        ds_push_root(vm, v);
        for (int i = 0; i < l->len; i++) {
            DSString *item = ds_val_to_string(vm, l->items[i]);
            int is_str = ds_obj_is(l->items[i], DS_OBJ_STRING);
            size_t need = len + (is_str ? item->len + 4 : item->len + 3);
            if (need > cap) {
                while (cap < need)
                    cap *= 2;
                char *nb = (char *)realloc(b, cap);
                if (!nb)
                    break;
                b = nb;
            }
            if (i > 0) {
                b[len++] = ',';
                b[len++] = ' ';
            }
            if (is_str) {
                b[len++] = '\'';
                memcpy(b + len, item->chars, item->len);
                len += item->len;
                b[len++] = '\'';
            } else {
                memcpy(b + len, item->chars, item->len);
                len += item->len;
            }
        }
        ds_pop_roots(vm, 1);
        if (len + 2 > cap) {
            char *nb = (char *)realloc(b, len + 2);
            if (nb)
                b = nb;
        }
        b[len++] = ']';
        DSString *res = ds_string_new(vm, b, len);
        free(b);
        return res;
    }
    case DS_OBJ_DICT: {
        DSDict *d = AS_DICT(v);
        size_t cap = 64, len = 0;
        char *b = (char *)malloc(cap);
        if (!b)
            return ds_string_new(vm, "{}", 2);
        b[len++] = '{';
        ds_push_root(vm, v);
        for (int i = 0; i < d->cap; i++) {
            if (d->entries[i].state != 1)
                continue;
            DSString *k = ds_val_to_string(vm, d->entries[i].key);
            DSString *val = ds_val_to_string(vm, d->entries[i].val);
            int k_str = ds_obj_is(d->entries[i].key, DS_OBJ_STRING);
            int v_str = ds_obj_is(d->entries[i].val, DS_OBJ_STRING);
            size_t need = len + k->len + val->len + 8;
            if (need > cap) {
                while (cap < need)
                    cap *= 2;
                char *nb = (char *)realloc(b, cap);
                if (!nb)
                    break;
                b = nb;
            }
            if (len > 1) {
                b[len++] = ',';
                b[len++] = ' ';
            }
            if (k_str) {
                b[len++] = '\'';
                memcpy(b + len, k->chars, k->len);
                len += k->len;
                b[len++] = '\'';
            } else {
                memcpy(b + len, k->chars, k->len);
                len += k->len;
            }
            b[len++] = ':';
            b[len++] = ' ';
            if (v_str) {
                b[len++] = '\'';
                memcpy(b + len, val->chars, val->len);
                len += val->len;
                b[len++] = '\'';
            } else {
                memcpy(b + len, val->chars, val->len);
                len += val->len;
            }
        }
        ds_pop_roots(vm, 1);
        if (len + 2 > cap) {
            char *nb = (char *)realloc(b, len + 2);
            if (nb)
                b = nb;
        }
        b[len++] = '}';
        DSString *res = ds_string_new(vm, b, len);
        free(b);
        return res;
    }
    case DS_OBJ_CLASS: {
        DSClass *k = AS_CLASS(v);
        snprintf(buf, sizeof(buf), "<class %s>", k->name ? k->name->chars : "?");
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_INSTANCE: {
        DSInstance *in = AS_INSTANCE(v);
        snprintf(buf, sizeof(buf), "<%s объект>", in->klass && in->klass->name ? in->klass->name->chars : "?");
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_CFUNC: {
        DSCFunc *cf = AS_CFUNC(v);
        snprintf(buf, sizeof(buf), "<built-in %s>", cf->name ? cf->name : "?");
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_CLOSURE: {
        DSClosure *cl = AS_CLOSURE(v);
        snprintf(buf, sizeof(buf), "<function %s>",
                 cl->proto && cl->proto->name ? cl->proto->name->chars : "?");
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_BOUND: {
        DSBound *b = AS_BOUND(v);
        DSString *inner = ds_val_to_string(vm, b->fn);
        snprintf(buf, sizeof(buf), "<bound %s>", inner->chars);
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_MODULE: {
        DSModule *m = AS_MODULE(v);
        snprintf(buf, sizeof(buf), "<module %s>", m->name ? m->name->chars : "?");
        return ds_string_new(vm, buf, strlen(buf));
    }
    case DS_OBJ_NATIVE: {
        DSNativeObj *no = AS_NATIVE(v);
        snprintf(buf, sizeof(buf), "<%s>", no->type_tag ? no->type_tag : "native");
        return ds_string_new(vm, buf, strlen(buf));
    }
    default:
        return ds_string_new(vm, "<объект>", 14);
    }
}

/* ============================================================================
 * FORMAT SPECS (f-strings)
 * ============================================================================ */

static void fmt_apply_spec(DSVM *vm, DSValue v, DSString *spec, char *out, size_t cap) {
    (void)vm;
    if (!spec || spec->len == 0) {
        DSString *s = ds_val_to_string(vm, v);
        snprintf(out, cap, "%s", s->chars);
        return;
    }
    /* Parse: [[fill]align][sign][#][0][width][.precision][type] */
    const char *p = spec->chars;
    const char *end = spec->chars + spec->len;
    char align = 0;
    char fill = ' ';
    char type = 0;
    int width = 0;
    int prec = -1;

    if (end - p >= 2 && (p[1] == '<' || p[1] == '>' || p[1] == '^')) {
        fill = p[0];
        align = p[1];
        p += 2;
    } else if (end - p >= 1 && (*p == '<' || *p == '>' || *p == '^')) {
        align = *p++;
    }
    if (p < end && (*p == '+' || *p == '-' || *p == ' '))
        p++;
    if (p < end && *p == '#')
        p++;
    if (p < end && *p == '0') {
        fill = '0';
        if (!align)
            align = '>';
        p++;
    }
    while (p < end && *p >= '0' && *p <= '9') {
        width = width * 10 + (*p - '0');
        p++;
    }
    if (p < end && *p == '.') {
        p++;
        prec = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            prec = prec * 10 + (*p - '0');
            p++;
        }
    }
    if (p < end)
        type = *p;

    char body[256];
    body[0] = '\0';
    if (ds_is_num(v)) {
        double d = ds_as_num(v);
        if (type == 'f' || type == 'F') {
            snprintf(body, sizeof(body), "%.*f", prec < 0 ? 6 : prec, d);
        } else if (type == 'e' || type == 'E') {
            snprintf(body, sizeof(body), "%.*e", prec < 0 ? 6 : prec, d);
        } else if (type == 'x') {
            snprintf(body, sizeof(body), "%llx", (unsigned long long)(long long)d);
        } else if (type == 'X') {
            snprintf(body, sizeof(body), "%llX", (unsigned long long)(long long)d);
        } else if (type == 'b') {
            unsigned long long u = (unsigned long long)(long long)d;
            char tmp[80];
            int n = 0;
            if (u == 0)
                tmp[n++] = '0';
            while (u > 0 && n < 79) {
                tmp[n++] = (char)('0' + (u & 1u));
                u >>= 1;
            }
            int k = 0;
            while (n > 0 && k < (int)sizeof(body) - 1)
                body[k++] = tmp[--n];
            body[k] = '\0';
        } else if (type == 'd' || type == 'i') {
            snprintf(body, sizeof(body), "%lld", (long long)d);
        } else if (type == 'g' || type == 'G') {
            snprintf(body, sizeof(body), "%.*g", prec < 0 ? 6 : prec, d);
        } else if (type == '%') {
            snprintf(body, sizeof(body), "%.*f%%", prec < 0 ? 6 : prec, d * 100.0);
        } else {
            char tmp[64];
            if (prec >= 0)
                snprintf(tmp, sizeof(tmp), "%.*f", prec, d);
            else
                format_number(d, tmp, sizeof(tmp), 0);
            snprintf(body, sizeof(body), "%s", tmp);
        }
    } else if (ds_obj_is(v, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(v);
        int n = (int)s->len;
        if (prec >= 0 && prec < n)
            n = prec;
        snprintf(body, sizeof(body), "%.*s", n, s->chars);
    } else {
        DSString *s = ds_val_to_string(vm, v);
        snprintf(body, sizeof(body), "%.*s", (int)s->len, s->chars);
    }

    int blen = (int)strlen(body);
    int pad = width - blen;
    if (pad <= 0) {
        snprintf(out, cap, "%s", body);
        return;
    }
    size_t o = 0;
    if (align == '<') {
        o = (size_t)blen;
        if (o > cap) o = cap;
        memcpy(out, body, o);
        while (o + 1 < cap && pad-- > 0)
            out[o++] = fill;
        out[o < cap ? o : cap - 1] = '\0';
    } else if (align == '^') {
        int left = pad / 2;
        for (int i = 0; i < left && o + 1 < cap; i++)
            out[o++] = fill;
        for (int i = 0; i < blen && o + 1 < cap; i++)
            out[o++] = body[i];
        for (int i = 0; i < pad - left && o + 1 < cap; i++)
            out[o++] = fill;
        out[o] = '\0';
    } else {
        for (int i = 0; i < pad && o + 1 < cap; i++)
            out[o++] = fill;
        for (int i = 0; i < blen && o + 1 < cap; i++)
            out[o++] = body[i];
        out[o] = '\0';
    }
}

DSValue ds_format_value(DSVM *vm, DSValue v, DSString *spec) {
    char buf[512];
    fmt_apply_spec(vm, v, spec, buf, sizeof(buf));
    return ds_obj_val(ds_string_new(vm, buf, strlen(buf)));
}

/* ============================================================================
 * ARITHMETIC / COMPARISON / CONTAINER OPERATORS
 * ============================================================================ */

static int64_t to_i64(DSValue v) { return (int64_t)ds_as_num(v); }

static DSValue str_concat_vals(DSVM *vm, DSValue a, DSValue b) {
    DSString *sa = ds_val_to_string(vm, a);
    DSString *sb = ds_val_to_string(vm, b);
    return ds_obj_val(ds_string_concat(vm, sa, sb));
}

static DSValue list_concat(DSVM *vm, DSList *a, DSList *b) {
    DSList *r = ds_list_new(vm, a->len + b->len);
    if (!r)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(r));
    ds_push_root(vm, ds_obj_val(a));
    ds_push_root(vm, ds_obj_val(b));
    for (int i = 0; i < a->len; i++)
        ds_list_push(vm, r, a->items[i]);
    for (int i = 0; i < b->len; i++)
        ds_list_push(vm, r, b->items[i]);
    ds_pop_roots(vm, 3);
    return ds_obj_val(r);
}

static DSValue list_repeat(DSVM *vm, DSList *a, int times) {
    if (times < 0)
        times = 0;
    DSList *r = ds_list_new(vm, a->len * times);
    if (!r)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(r));
    ds_push_root(vm, ds_obj_val(a));
    for (int k = 0; k < times; k++)
        for (int i = 0; i < a->len; i++)
            ds_list_push(vm, r, a->items[i]);
    ds_pop_roots(vm, 2);
    return ds_obj_val(r);
}

static DSValue str_repeat(DSVM *vm, DSString *s, int times) {
    if (times < 0)
        times = 0;
    size_t total = (size_t)s->len * (size_t)times;
    if (total == 0)
        return ds_obj_val(ds_string_new(vm, "", 0));
    char *buf = (char *)malloc(total + 1);
    if (!buf)
        return DS_VAL_NIL;
    for (int i = 0; i < times; i++)
        memcpy(buf + (size_t)i * s->len, s->chars, s->len);
    DSString *r = ds_string_new(vm, buf, total);
    free(buf);
    return ds_obj_val(r);
}

static int compare_values(DSVM *vm, DSValue a, DSValue b, int *out) {
    if (ds_is_num(a) && ds_is_num(b)) {
        double x = ds_as_num(a), y = ds_as_num(b);
        *out = (x < y) ? -1 : (x > y ? 1 : 0);
        return 1;
    }
    if (ds_obj_is(a, DS_OBJ_STRING) && ds_obj_is(b, DS_OBJ_STRING)) {
        DSString *sa = AS_STRING(a), *sb = AS_STRING(b);
        int n = (int)(sa->len < sb->len ? sa->len : sb->len);
        int c = n > 0 ? memcmp(sa->chars, sb->chars, (size_t)n) : 0;
        if (c == 0)
            c = (sa->len < sb->len) ? -1 : (sa->len > sb->len ? 1 : 0);
        *out = c < 0 ? -1 : (c > 0 ? 1 : 0);
        return 1;
    }
    if (ds_is_bool(a) && ds_is_bool(b)) {
        int x = ds_as_bool(a), y = ds_as_bool(b);
        *out = x - y;
        return 1;
    }
    (void)vm;
    return 0;
}

int ds_val_in(DSVM *vm, DSValue needle, DSValue haystack) {
    if (ds_obj_is(haystack, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(haystack);
        for (int i = 0; i < l->len; i++)
            if (ds_values_equal(needle, l->items[i]))
                return 1;
        return 0;
    }
    if (ds_obj_is(haystack, DS_OBJ_DICT)) {
        DSValue tmp;
        return ds_dict_get(AS_DICT(haystack), needle, &tmp);
    }
    if (ds_obj_is(haystack, DS_OBJ_STRING)) {
        DSString *h = AS_STRING(haystack);
        if (ds_obj_is(needle, DS_OBJ_STRING)) {
            DSString *n = AS_STRING(needle);
            if (n->len == 0)
                return 1;
            if (n->len > h->len)
                return 0;
            for (uint32_t i = 0; i + n->len <= h->len; i++)
                if (memcmp(h->chars + i, n->chars, n->len) == 0)
                    return 1;
            return 0;
        }
    }
    (void)vm;
    return 0;
}

/* Try user-defined dunder operators on instances/classes */
static DSValue try_dunder(DSVM *vm, DSValue obj, const char *name, int argc, DSValue *argv) {
    if (!ds_obj_is(obj, DS_OBJ_INSTANCE) && !ds_obj_is(obj, DS_OBJ_CLASS) && !ds_obj_is(obj, DS_OBJ_NATIVE))
        return DS_VAL_NIL;
    DSString *mn = ds_string_const(vm, name);
    DSValue fn;
    if (!ds_lookup_method(vm, obj, mn, &fn)) {
        vm->has_error = 0;
        vm->err_msg[0] = '\0';
        return DS_VAL_NIL;
    }
    ds_push_root(vm, fn);
    ds_push_root(vm, obj);
    DSValue r = ds_call_value(vm, fn, argc, argv);
    ds_pop_roots(vm, 2);
    return r;
}

DSValue ds_binop(DSVM *vm, int op, DSValue a, DSValue b) {
    switch (op) {
    case OP_ADD:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num(ds_as_num(a) + ds_as_num(b));
        if (ds_obj_is(a, DS_OBJ_LIST) && ds_obj_is(b, DS_OBJ_LIST))
            return list_concat(vm, AS_LIST(a), AS_LIST(b));
        if (ds_obj_is(a, DS_OBJ_STRING) && ds_obj_is(b, DS_OBJ_STRING))
            return ds_obj_val(ds_string_concat(vm, AS_STRING(a), AS_STRING(b)));
        if (ds_obj_is(a, DS_OBJ_STRING) || ds_obj_is(b, DS_OBJ_STRING))
            return str_concat_vals(vm, a, b);
        {
            DSValue r = try_dunder(vm, a, "__add__", 1, &b);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "нельзя сложить '%s' и '%s'", ds_type_name(a), ds_type_name(b));
        return DS_VAL_NIL;

    case OP_SUB:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num(ds_as_num(a) - ds_as_num(b));
        {
            DSValue r = try_dunder(vm, a, "__sub__", 1, &b);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "нельзя вычесть '%s' из '%s'", ds_type_name(b), ds_type_name(a));
        return DS_VAL_NIL;

    case OP_MUL:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num(ds_as_num(a) * ds_as_num(b));
        if (ds_obj_is(a, DS_OBJ_LIST) && ds_is_num(b))
            return list_repeat(vm, AS_LIST(a), (int)ds_as_num(b));
        if (ds_is_num(a) && ds_obj_is(b, DS_OBJ_LIST))
            return list_repeat(vm, AS_LIST(b), (int)ds_as_num(a));
        if (ds_obj_is(a, DS_OBJ_STRING) && ds_is_num(b))
            return str_repeat(vm, AS_STRING(a), (int)ds_as_num(b));
        if (ds_is_num(a) && ds_obj_is(b, DS_OBJ_STRING))
            return str_repeat(vm, AS_STRING(b), (int)ds_as_num(a));
        {
            DSValue r = try_dunder(vm, a, "__mul__", 1, &b);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "нельзя умножить '%s' на '%s'", ds_type_name(a), ds_type_name(b));
        return DS_VAL_NIL;

    case OP_DIV:
        if (ds_is_num(a) && ds_is_num(b)) {
            double d = ds_as_num(b);
            if (d == 0.0) {
                ds_error(vm, 0, "деление на ноль");
                return DS_VAL_NIL;
            }
            return ds_num(ds_as_num(a) / d);
        }
        {
            DSValue r = try_dunder(vm, a, "__div__", 1, &b);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "нельзя делить '%s' на '%s'", ds_type_name(a), ds_type_name(b));
        return DS_VAL_NIL;

    case OP_FLOORDIV:
        if (ds_is_num(a) && ds_is_num(b)) {
            double d = ds_as_num(b);
            if (d == 0.0) {
                ds_error(vm, 0, "деление на ноль");
                return DS_VAL_NIL;
            }
            return ds_num(floor(ds_as_num(a) / d));
        }
        ds_error(vm, 0, "// требует числа");
        return DS_VAL_NIL;

    case OP_MOD:
        if (ds_is_num(a) && ds_is_num(b)) {
            double x = ds_as_num(a), y = ds_as_num(b);
            if (y == 0.0) {
                ds_error(vm, 0, "деление на ноль (остаток)");
                return DS_VAL_NIL;
            }
            double r = fmod(x, y);
            if (r != 0.0 && ((r < 0) != (y < 0)))
                r += y;
            return ds_num(r);
        }
        if (ds_obj_is(a, DS_OBJ_STRING)) {
            /* printf-style % formatting */
            DSString *fmt = AS_STRING(a);
            char buf[512];
            if (ds_is_num(b))
                snprintf(buf, sizeof(buf), fmt->chars, ds_as_num(b));
            else if (ds_obj_is(b, DS_OBJ_STRING))
                snprintf(buf, sizeof(buf), fmt->chars, AS_STRING(b)->chars);
            else {
                DSString *s = ds_val_to_string(vm, b);
                snprintf(buf, sizeof(buf), fmt->chars, s->chars);
            }
            return ds_obj_val(ds_string_new(vm, buf, strlen(buf)));
        }
        ds_error(vm, 0, "%% требует число или строку");
        return DS_VAL_NIL;

    case OP_POW:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num(pow(ds_as_num(a), ds_as_num(b)));
        ds_error(vm, 0, "** требует числа");
        return DS_VAL_NIL;

    case OP_EQ: return ds_bool(ds_values_equal(a, b));
    case OP_NE: return ds_bool(!ds_values_equal(a, b));

    case OP_LT:
    case OP_LE:
    case OP_GT:
    case OP_GE: {
        int c = 0;
        if (compare_values(vm, a, b, &c)) {
            switch (op) {
            case OP_LT: return ds_bool(c < 0);
            case OP_LE: return ds_bool(c <= 0);
            case OP_GT: return ds_bool(c > 0);
            default: return ds_bool(c >= 0);
            }
        }
        {
            const char *dunder = (op == OP_LT) ? "__lt__" : (op == OP_LE ? "__le__" : (op == OP_GT ? "__gt__" : "__ge__"));
            DSValue r = try_dunder(vm, a, dunder, 1, &b);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "нельзя сравнить '%s' и '%s'", ds_type_name(a), ds_type_name(b));
        return DS_VAL_NIL;
    }

    case OP_BITAND:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num((double)(to_i64(a) & to_i64(b)));
        ds_error(vm, 0, "& требует целые числа");
        return DS_VAL_NIL;
    case OP_BITOR:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num((double)(to_i64(a) | to_i64(b)));
        if (ds_obj_is(a, DS_OBJ_DICT) && ds_obj_is(b, DS_OBJ_DICT)) {
            DSDict *r = ds_dict_new(vm, AS_DICT(a)->count + AS_DICT(b)->count);
            ds_push_root(vm, ds_obj_val(r));
            ds_push_root(vm, a);
            ds_push_root(vm, b);
            DSDict *da = AS_DICT(a), *db = AS_DICT(b);
            for (int i = 0; i < da->cap; i++)
                if (da->entries[i].state == 1)
                    ds_dict_set(vm, r, da->entries[i].key, da->entries[i].val);
            for (int i = 0; i < db->cap; i++)
                if (db->entries[i].state == 1)
                    ds_dict_set(vm, r, db->entries[i].key, db->entries[i].val);
            ds_pop_roots(vm, 3);
            return ds_obj_val(r);
        }
        ds_error(vm, 0, "| требует целые числа или dict");
        return DS_VAL_NIL;
    case OP_BITXOR:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num((double)(to_i64(a) ^ to_i64(b)));
        ds_error(vm, 0, "^ требует целые числа");
        return DS_VAL_NIL;
    case OP_SHL:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num((double)(to_i64(a) << (to_i64(b) & 63)));
        ds_error(vm, 0, "<< требует целые числа");
        return DS_VAL_NIL;
    case OP_SHR:
        if (ds_is_num(a) && ds_is_num(b))
            return ds_num((double)(to_i64(a) >> (to_i64(b) & 63)));
        ds_error(vm, 0, ">> требует целые числа");
        return DS_VAL_NIL;

    case OP_IN: return ds_bool(ds_val_in(vm, a, b));
    case OP_NOTIN: return ds_bool(!ds_val_in(vm, a, b));
    case OP_IS: return ds_bool(ds_values_equal(a, b));
    case OP_ISNOT: return ds_bool(!ds_values_equal(a, b));
    default:
        ds_error(vm, 0, "неизвестная операция");
        return DS_VAL_NIL;
    }
}

DSValue ds_unop(DSVM *vm, int op, DSValue a) {
    switch (op) {
    case OP_SUB:
        if (ds_is_num(a))
            return ds_num(-ds_as_num(a));
        {
            DSValue r = try_dunder(vm, a, "__neg__", 0, NULL);
            if (!ds_is_nil(r) || vm->has_error)
                return r;
        }
        ds_error(vm, 0, "унарный '-' требует число");
        return DS_VAL_NIL;
    case OP_ISNOT: /* logical not */
        return ds_bool(!ds_is_truthy(a));
    case OP_BITXOR:
        if (ds_is_num(a))
            return ds_num((double)(~to_i64(a)));
        ds_error(vm, 0, "'~' требует целое число");
        return DS_VAL_NIL;
    default:
        return a;
    }
}

/* ============================================================================
 * NAME LOOKUP & STORAGE
 * ============================================================================ */

static DSValue builtin_lookup(DSVM *vm, DSString *name) {
    DSValue out;
    if (ds_dict_get_str(vm->builtins, name, &out))
        return out;
    return DS_VAL_NIL;
}

DSValue ds_lookup_name(DSVM *vm, DSModule *mod, DSString *name) {
    if (!name)
        return DS_VAL_NIL;
    if (mod && mod->dict) {
        DSValue out;
        if (ds_dict_get_str(mod->dict, name, &out))
            return out;
    }
    DSValue b = builtin_lookup(vm, name);
    if (!ds_is_nil(b))
        return b;
    if (vm->builtins) {
        DSValue out;
        if (ds_dict_get_str(vm->builtins, name, &out))
            return out;
    }
    ds_error(vm, 0, "имя '%s' не определено", name->chars);
    return DS_VAL_NIL;
}

void ds_store_name(DSVM *vm, DSModule *mod, DSString *name, DSValue v) {
    if (!mod)
        mod = vm->main_module;
    if (!mod)
        return;
    ds_push_root(vm, v);
    ds_dict_set(vm, mod->dict, ds_obj_val(name), v);
    for (int i = 0; i < mod->global_count; i++) {
        if (mod->global_names[i] == name) {
            mod->global_slots[i] = v;
            break;
        }
    }
    ds_pop_roots(vm, 1);
}

/* ============================================================================
 * CONTAINER INDEXING
 * ============================================================================ */

DSValue ds_get_index(DSVM *vm, DSValue container, DSValue idx) {
    if (ds_obj_is(container, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(container);
        if (!ds_is_num(idx)) {
            ds_error(vm, 0, "индекс списка должен быть числом, а не '%s'", ds_type_name(idx));
            return DS_VAL_NIL;
        }
        int i = (int)ds_as_num(idx);
        if (i < 0)
            i += l->len;
        if (i < 0 || i >= l->len) {
            ds_error(vm, 0, "индекс %d вне диапазона (длина %d)", (int)ds_as_num(idx), l->len);
            return DS_VAL_NIL;
        }
        return l->items[i];
    }
    if (ds_obj_is(container, DS_OBJ_DICT)) {
        DSValue out;
        if (ds_dict_get(AS_DICT(container), idx, &out))
            return out;
        DSString *k = ds_val_to_string(vm, idx);
        ds_error(vm, 0, "ключ '%s' не найден в словаре", k->chars);
        return DS_VAL_NIL;
    }
    if (ds_obj_is(container, DS_OBJ_STRING)) {
        DSString *s = AS_STRING(container);
        if (!ds_is_num(idx)) {
            ds_error(vm, 0, "индекс строки должен быть числом");
            return DS_VAL_NIL;
        }
        int i = (int)ds_as_num(idx);
        int len = (int)s->len;
        if (i < 0)
            i += len;
        if (i < 0 || i >= len) {
            ds_error(vm, 0, "индекс %d вне строки (длина %d)", (int)ds_as_num(idx), len);
            return DS_VAL_NIL;
        }
        /* Return one UTF-8 codepoint */
        int start = i, end = i + 1;
        while (end < len && ((uint8_t)s->chars[end] & 0xC0) == 0x80)
            end++;
        return ds_obj_val(ds_string_new(vm, s->chars + start, (size_t)(end - start)));
    }
    if (ds_obj_is(container, DS_OBJ_INSTANCE)) {
        DSString *mn = ds_string_const(vm, "__getitem__");
        DSValue fn;
        if (ds_lookup_method(vm, container, mn, &fn)) {
            DSValue r = ds_call_value(vm, fn, 1, &idx);
            return r;
        }
        vm->has_error = 0;
        ds_error(vm, 0, "%s не поддерживает индексацию", ds_type_name(container));
        return DS_VAL_NIL;
    }
    ds_error(vm, 0, "'%s' не поддерживает индексацию", ds_type_name(container));
    return DS_VAL_NIL;
}

void ds_set_index(DSVM *vm, DSValue container, DSValue idx, DSValue val) {
    if (ds_obj_is(container, DS_OBJ_LIST)) {
        DSList *l = AS_LIST(container);
        int i = (int)ds_to_number(vm, idx);
        if (i < 0)
            i += l->len;
        if (i < 0) {
            ds_error(vm, 0, "индекс %d вне диапазона списка", (int)ds_to_number(vm, idx));
            return;
        }
        while (l->len <= i)
            ds_list_push(vm, l, DS_VAL_NIL);
        l->items[i] = val;
        ds_gc_write_barrier_back(vm, &l->hdr, val);
        return;
    }
    if (ds_obj_is(container, DS_OBJ_DICT)) {
        ds_dict_set(vm, AS_DICT(container), idx, val);
        return;
    }
    if (ds_obj_is(container, DS_OBJ_INSTANCE)) {
        DSString *mn = ds_string_const(vm, "__setitem__");
        DSValue fn;
        if (ds_lookup_method(vm, container, mn, &fn)) {
            DSValue argv[2] = {idx, val};
            ds_call_value(vm, fn, 2, argv);
            return;
        }
        vm->has_error = 0;
    }
    ds_error(vm, 0, "'%s' не поддерживает присваивание по индексу", ds_type_name(container));
}

/* ============================================================================
 * PROPERTY / METHOD DISPATCH (inline caches)
 * ============================================================================ */

static int dict_get_str_fast(DSDict *d, DSString *key, DSValue *out) {
    return ds_dict_get_str(d, key, out);
}

int ds_lookup_method(DSVM *vm, DSValue recv, DSString *name, DSValue *out_fn) {
    if (ds_obj_is(recv, DS_OBJ_INSTANCE)) {
        DSInstance *in = AS_INSTANCE(recv);
        for (DSClass *k = in->klass; k; k = k->superclass) {
            DSValue m;
            if (k->methods && ds_dict_get_str(k->methods, name, &m)) {
                *out_fn = m;
                return 1;
            }
        }
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_CLASS)) {
        DSClass *k = AS_CLASS(recv);
        for (; k; k = k->superclass) {
            DSValue m;
            if (k->methods && ds_dict_get_str(k->methods, name, &m)) {
                *out_fn = m;
                return 1;
            }
        }
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_NATIVE)) {
        DSNativeObj *no = AS_NATIVE(recv);
        if (no->methods && ds_dict_get_str(no->methods, name, out_fn))
            return 1;
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_MODULE)) {
        DSModule *m = AS_MODULE(recv);
        if (m->dict && ds_dict_get_str(m->dict, name, out_fn))
            return 1;
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_STRING)) {
        if (vm->str_methods && ds_dict_get_str(vm->str_methods, name, out_fn))
            return 1;
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_LIST)) {
        if (vm->list_methods && ds_dict_get_str(vm->list_methods, name, out_fn))
            return 1;
        return 0;
    }
    if (ds_obj_is(recv, DS_OBJ_DICT)) {
        if (vm->dict_methods && ds_dict_get_str(vm->dict_methods, name, out_fn))
            return 1;
        return 0;
    }
    return 0;
}

static DSValue bind_method(DSVM *vm, DSValue recv, DSValue fn) {
    DSBound *b = ds_bound_new(vm, recv, fn);
    return b ? ds_obj_val(b) : DS_VAL_NIL;
}

DSValue ds_get_property(DSVM *vm, DSValue obj, DSString *name, DSInlineCache *ic) {
    (void)ic;
    if (ds_obj_is(obj, DS_OBJ_INSTANCE)) {
        DSInstance *in = AS_INSTANCE(obj);
        DSValue v;
        if (in->fields && dict_get_str_fast(in->fields, name, &v))
            return v;
        DSValue fn;
        if (ds_lookup_method(vm, obj, name, &fn))
            return bind_method(vm, obj, fn);
        ds_error(vm, 0, "у объекта '%s' нет поля '%s'", ds_type_name(obj), name->chars);
        return DS_VAL_NIL;
    }
    if (ds_obj_is(obj, DS_OBJ_CLASS)) {
        DSValue fn;
        if (ds_lookup_method(vm, obj, name, &fn))
            return bind_method(vm, obj, fn);
        ds_error(vm, 0, "у класса '%s' нет метода '%s'", ds_type_name(obj), name->chars);
        return DS_VAL_NIL;
    }
    if (ds_obj_is(obj, DS_OBJ_MODULE)) {
        DSModule *m = AS_MODULE(obj);
        DSValue v;
        if (m->dict && ds_dict_get_str(m->dict, name, &v))
            return v;
        ds_error(vm, 0, "модуль '%s' не содержит '%s'", m->name ? m->name->chars : "?", name->chars);
        return DS_VAL_NIL;
    }
    if (ds_obj_is(obj, DS_OBJ_DICT)) {
        DSValue v;
        if (ds_dict_get_str(AS_DICT(obj), name, &v))
            return v;
        DSValue fn;
        if (vm->dict_methods && ds_dict_get_str(vm->dict_methods, name, &fn))
            return bind_method(vm, obj, fn);
        ds_error(vm, 0, "ключ '%s' не найден", name->chars);
        return DS_VAL_NIL;
    }
    DSValue fn;
    if (ds_lookup_method(vm, obj, name, &fn))
        return bind_method(vm, obj, fn);
    ds_error(vm, 0, "у значения типа '%s' нет атрибута '%s'", ds_type_name(obj), name->chars);
    return DS_VAL_NIL;
}

void ds_set_property(DSVM *vm, DSValue obj, DSString *name, DSValue val) {
    if (ds_obj_is(obj, DS_OBJ_INSTANCE)) {
        DSInstance *in = AS_INSTANCE(obj);
        if (!in->fields)
            in->fields = ds_dict_new(vm, 8);
        ds_dict_set(vm, in->fields, ds_obj_val(name), val);
        return;
    }
    if (ds_obj_is(obj, DS_OBJ_CLASS)) {
        DSClass *k = AS_CLASS(obj);
        if (!k->methods)
            k->methods = ds_dict_new(vm, 8);
        ds_dict_set(vm, k->methods, ds_obj_val(name), val);
        return;
    }
    if (ds_obj_is(obj, DS_OBJ_MODULE)) {
        DSModule *m = AS_MODULE(obj);
        ds_dict_set(vm, m->dict, ds_obj_val(name), val);
        return;
    }
    if (ds_obj_is(obj, DS_OBJ_DICT)) {
        ds_dict_set(vm, AS_DICT(obj), ds_obj_val(name), val);
        return;
    }
    ds_error(vm, 0, "нельзя задать атрибут '%s' у '%s'", name->chars, ds_type_name(obj));
}

DSValue ds_invoke_method(DSVM *vm, DSValue recv, DSString *name, int argc, DSValue *argv, DSInlineCache *ic) {
    DSValue fn;
    if (!ds_lookup_method(vm, recv, name, &fn)) {
        ds_error(vm, 0, "у '%s' нет метода '%s'", ds_type_name(recv), name->chars);
        return DS_VAL_NIL;
    }
    (void)ic;
    DSValue args[8];
    DSValue *use = argv;
    if (argc < 7) {
        args[0] = recv;
        for (int i = 0; i < argc; i++)
            args[i + 1] = argv[i];
        use = args;
        argc++;
    }
    return ds_call_value(vm, fn, argc, use);
}

DSValue ds_call_method(DSVM *vm, DSValue recv, const char *name, int argc, DSValue *argv) {
    DSString *n = ds_string_const(vm, name);
    return ds_invoke_method(vm, recv, n, argc, argv, NULL);
}

/* ============================================================================
 * FUNCTION CALLS (direct-threaded closure bodies)
 * ============================================================================ */

static DSValue call_closure(DSVM *vm, DSClosure *cl, int argc, DSValue *argv) {
    DSFuncProto *proto = cl ? cl->proto : NULL;
    if (!proto) {
        ds_error(vm, 0, "вызов пустой функции");
        return DS_VAL_NIL;
    }
    if (argc < proto->min_arity || (argc > proto->arity && !proto->has_varargs)) {
        ds_error(vm, 0, "%s() ожидает %d..%d аргументов, получено %d",
                 proto->name ? proto->name->chars : "функция", proto->min_arity, proto->arity, argc);
        return DS_VAL_NIL;
    }
    if (vm->frame_count >= DS_MAX_FRAMES) {
        ds_error(vm, 0, "слишком глубокая рекурсия (переполнение стека вызовов)");
        return DS_VAL_NIL;
    }

    int nslots = proto->num_locals > 0 ? proto->num_locals : proto->arity + 1;
    if (nslots < 1)
        nslots = 1;
    DSValue *base = vm->sp;
    if ((size_t)(base - vm->stack) + (size_t)nslots + 8 >= DS_MAX_STACK) {
        ds_error(vm, 0, "переполнение стека (слишком глубокая рекурсия)");
        return DS_VAL_NIL;
    }
    for (int i = 0; i < nslots; i++)
        base[i] = DS_VAL_NIL;
    base[0] = ds_obj_val(cl);
    for (int i = 0; i < proto->arity; i++) {
        if (i < argc)
            base[1 + i] = argv[i];
        else if (proto->defaults)
            base[1 + i] = proto->defaults[i];
        else
            base[1 + i] = DS_VAL_NIL;
    }
    vm->sp = base + nslots;

    DSCallFrame *fr = &vm->frames[vm->frame_count++];
    DSCallFrame saved = *fr;
    fr->closure = cl;
    fr->c_name = proto->name ? proto->name->chars : "?";
    fr->module = proto->module ? proto->module : vm->main_module;
    fr->slots = base;
    fr->line = proto->def_line;

    DSValue result = DS_VAL_NIL;
    vm->return_reg = DS_VAL_NIL;
    if (vm->has_error) {
        vm->frame_count--;
        *fr = saved;
        vm->sp = base;
        return DS_VAL_NIL;
    }

    const DSStmtNode *s = proto->body;
    while (s) {
        fr->line = s->line;
        DSExecSignal sig = s->exec(vm, base, s);
        if (sig == EXEC_RETURN) {
            result = vm->return_reg;
            break;
        }
        if (sig == EXEC_ERROR) {
            result = DS_VAL_NIL;
            break;
        }
        if (sig == EXEC_BREAK || sig == EXEC_CONTINUE)
            break;
        s = s->next;
    }

    ds_close_upvalues(vm, base);
    vm->sp = base;
    vm->frame_count--;
    *fr = saved;
    return result;
}

static DSValue call_cfunc(DSVM *vm, DSCFunc *cf, int argc, DSValue *argv) {
    if (vm->frame_count >= DS_MAX_FRAMES) {
        ds_error(vm, 0, "слишком глубокая рекурсия");
        return DS_VAL_NIL;
    }
    DSCallFrame *fr = &vm->frames[vm->frame_count++];
    DSCallFrame saved = *fr;
    fr->closure = NULL;
    fr->c_name = cf->name ? cf->name : "native";
    fr->module = vm->main_module;
    fr->slots = vm->sp;
    fr->line = 0;
    DSValue r = cf->fn(vm, argc, argv);
    vm->frame_count--;
    *fr = saved;
    return r;
}

static DSValue call_instance(DSVM *vm, DSInstance *in, int argc, DSValue *argv) {
    DSClass *k = in->klass;
    DSValue init = DS_VAL_NIL;
    for (DSClass *c = k; c; c = c->superclass) {
        if (!ds_is_nil(c->init_fn)) {
            init = c->init_fn;
            break;
        }
    }
    if (ds_is_nil(init)) {
        if (argc > 0) {
            ds_error(vm, 0, "класс '%s' не принимает аргументов", k && k->name ? k->name->chars : "?");
            return DS_VAL_NIL;
        }
        return DS_VAL_NIL;
    }
    DSValue self = ds_obj_val(in);
    DSValue *args = (DSValue *)malloc(sizeof(DSValue) * (size_t)(argc + 1));
    if (!args)
        return DS_VAL_NIL;
    args[0] = self;
    for (int i = 0; i < argc; i++)
        args[i + 1] = argv[i];
    ds_push_root(vm, self);
    ds_push_root(vm, init);
    if (ds_obj_is(init, DS_OBJ_CLOSURE))
        call_closure(vm, AS_CLOSURE(init), argc + 1, args);
    else
        ds_call_value(vm, init, argc + 1, args);
    ds_pop_roots(vm, 2);
    free(args);
    return DS_VAL_NIL;
}

DSValue ds_call_value(DSVM *vm, DSValue callee, int argc, DSValue *argv) {
    if (vm->has_error)
        return DS_VAL_NIL;
    if (ds_obj_is(callee, DS_OBJ_CLOSURE)) {
        DSClosure *cl = AS_CLOSURE(callee);
        DSFuncProto *proto = cl->proto;
        if (proto && proto->jit_fn && argc == proto->arity && !vm->has_error) {
            /* Native machine-code fast path. Only entered when every argument
             * is already a number: compiled code works on raw doubles and has
             * no route back to the interpreter for a type error. */
            int numeric = 1;
            for (int i = 0; i < argc; i++) {
                if (!ds_is_num(argv[i])) {
                    numeric = 0;
                    break;
                }
            }
            DSJitEntryFn fn = numeric ? ds_jit_entry(proto->jit_fn) : NULL;
            if (fn) {
                /* Mirrors an interpreter frame: slot 0 is the closure, then the
                 * arguments, then the locals the compiler assigned slots to. */
                DSValue slots[JIT_MAX_SLOTS];
                int nslots = proto->num_locals > 0 ? proto->num_locals : proto->arity + 1;
                if (nslots < 1 || nslots > JIT_MAX_SLOTS)
                    return call_closure(vm, cl, argc, argv);
                for (int i = 0; i < nslots; i++)
                    slots[i] = DS_VAL_NIL;
                slots[0] = callee;
                for (int i = 0; i < argc; i++)
                    slots[1 + i] = argv[i];
                vm->jit_calls++;
                double out = fn((const double *)slots);
                if (isnan(out))
                    return call_closure(vm, cl, argc, argv); /* fell off the end */
                return ds_num(out);
            }
        }
        return call_closure(vm, cl, argc, argv);
    }
    if (ds_obj_is(callee, DS_OBJ_CFUNC)) {
        DSCFunc *cf = AS_CFUNC(callee);
        return call_cfunc(vm, cf, argc, argv);
    }
    if (ds_obj_is(callee, DS_OBJ_BOUND)) {
        DSBound *b = AS_BOUND(callee);
        DSValue inner = b->fn;
        DSValue *args = (DSValue *)malloc(sizeof(DSValue) * (size_t)(argc + 1));
        if (!args)
            return DS_VAL_NIL;
        args[0] = b->recv;
        for (int i = 0; i < argc; i++)
            args[i + 1] = argv[i];
        ds_push_root(vm, b->recv);
        ds_push_root(vm, inner);
        DSValue r = ds_call_value(vm, inner, argc + 1, args);
        ds_pop_roots(vm, 2);
        free(args);
        return r;
    }
    if (ds_obj_is(callee, DS_OBJ_CLASS)) {
        DSClass *kl = AS_CLASS(callee);
        ds_push_root(vm, callee);
        DSInstance *in = ds_instance_new(vm, kl);
        if (!in) {
            ds_pop_roots(vm, 1);
            return DS_VAL_NIL;
        }
        ds_push_root(vm, ds_obj_val(in));
        call_instance(vm, in, argc, argv);
        ds_pop_roots(vm, 2);
        return ds_obj_val(in);
    }
    if (ds_obj_is(callee, DS_OBJ_NATIVE)) {
        DSNativeObj *no = AS_NATIVE(callee);
        if (no->methods) {
            DSValue call_fn;
            if (ds_dict_get_str(no->methods, ds_string_const(vm, "__call__"), &call_fn)) {
                DSValue *args = (DSValue *)malloc(sizeof(DSValue) * (size_t)(argc + 1));
                if (!args)
                    return DS_VAL_NIL;
                args[0] = callee;
                for (int i = 0; i < argc; i++)
                    args[i + 1] = argv[i];
                DSValue r = ds_call_value(vm, call_fn, argc + 1, args);
                free(args);
                return r;
            }
        }
        ds_error(vm, 0, "объект '%s' не вызываемый", no->type_tag ? no->type_tag : "?");
        return DS_VAL_NIL;
    }
    if (ds_obj_is(callee, DS_OBJ_INSTANCE)) {
        DSValue fn;
        if (ds_lookup_method(vm, callee, ds_string_const(vm, "__call__"), &fn)) {
            DSValue *args = (DSValue *)malloc(sizeof(DSValue) * (size_t)(argc + 1));
            if (!args)
                return DS_VAL_NIL;
            args[0] = callee;
            for (int i = 0; i < argc; i++)
                args[i + 1] = argv[i];
            ds_push_root(vm, callee);
            ds_push_root(vm, fn);
            DSValue r = ds_call_value(vm, fn, argc + 1, args);
            ds_pop_roots(vm, 2);
            free(args);
            return r;
        }
        vm->has_error = 0;
        ds_error(vm, 0, "объект '%s' не вызываемый", ds_type_name(callee));
        return DS_VAL_NIL;
    }
    ds_error(vm, 0, "значение типа '%s' не вызываемое", ds_type_name(callee));
    return DS_VAL_NIL;
}

/* ============================================================================
 * ITERATION PROTOCOL
 * ============================================================================ */

int ds_iter_open(DSVM *vm, DSValue obj, DSIter *it) {
    memset(it, 0, sizeof(*it));
    it->obj = obj;
    if (ds_obj_is(obj, DS_OBJ_LIST)) {
        it->kind = 0;
        it->list = AS_LIST(obj);
        return 1;
    }
    if (ds_obj_is(obj, DS_OBJ_STRING)) {
        it->kind = 2;
        it->str = AS_STRING(obj);
        return 1;
    }
    if (ds_obj_is(obj, DS_OBJ_DICT)) {
        it->kind = 1;
        it->dict = AS_DICT(obj);
        return 1;
    }
    if (ds_obj_is(obj, DS_OBJ_NATIVE)) {
        DSNativeObj *no = AS_NATIVE(obj);
        if (no->methods) {
            DSValue fn;
            if (ds_dict_get_str(no->methods, ds_string_const(vm, "__iter__"), &fn)) {
                it->kind = 4;
                return 1;
            }
        }
    }
    if (ds_obj_is(obj, DS_OBJ_INSTANCE)) {
        it->kind = 5;
        return 1;
    }
    ds_error(vm, 0, "'%s' не является итерируемым объектом", ds_type_name(obj));
    return 0;
}

int ds_iter_next(DSVM *vm, DSIter *it, DSValue *out) {
    switch (it->kind) {
    case 0: {
        DSList *l = it->list;
        if (it->idx >= l->len)
            return 0;
        *out = l->items[it->idx++];
        return 1;
    }
    case 1: {
        DSDict *d = it->dict;
        while (it->idx < d->cap) {
            DSDictEntry *e = &d->entries[it->idx++];
            if (e->state == 1) {
                *out = e->key;
                return 1;
            }
        }
        return 0;
    }
    case 2: {
        DSString *s = it->str;
        if (it->idx >= (int)s->len)
            return 0;
        int start = it->idx;
        int end = start + 1;
        while (end < (int)s->len && ((uint8_t)s->chars[end] & 0xC0) == 0x80)
            end++;
        it->idx = end;
        *out = ds_obj_val(ds_string_new(vm, s->chars + start, (size_t)(end - start)));
        return 1;
    }
    case 4: {
        DSNativeObj *no = AS_NATIVE(it->obj);
        DSValue fn;
        if (!ds_dict_get_str(no->methods, ds_string_const(vm, "__iter_next__"), &fn))
            return 0;
        DSValue r = ds_call_value(vm, fn, 0, NULL);
        if (vm->has_error)
            return 0;
        if (ds_is_nil(r))
            return 0;
        *out = r;
        return 1;
    }
    case 5: {
        DSValue fn;
        if (!ds_lookup_method(vm, it->obj, ds_string_const(vm, "__iter__"), &fn))
            return 0;
        DSValue r = ds_call_value(vm, fn, 0, NULL);
        if (vm->has_error)
            return 0;
        DSIter inner;
        if (!ds_iter_open(vm, r, &inner))
            return 0;
        DSValue item = DS_VAL_NIL;
        int more = ds_iter_next(vm, &inner, &item);
        if (more)
            *out = item;
        return more;
    }
    default:
        return 0;
    }
}

DSValue ds_make_range(DSVM *vm, double start, double stop, double step) {
    if (step == 0.0) {
        ds_error(vm, 0, "шаг range() не может быть нулём");
        return DS_VAL_NIL;
    }
    int n = 0;
    if (step > 0) {
        for (double i = start; i < stop; i += step)
            n++;
    } else {
        for (double i = start; i > stop; i += step)
            n++;
    }
    DSList *l = ds_list_new(vm, n > 0 ? n : 0);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    double i = start;
    for (int k = 0; k < n; k++, i += step)
        ds_list_push(vm, l, ds_num(i));
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

/* ============================================================================
 * VM LIFECYCLE
 * ============================================================================ */

/* The native JIT is on unless the host asks for the reference interpreter
 * (DS_NOJIT=1). That is how jit_bench.py and jit_diff_test.py run both
 * execution paths over identical input. */
static void ds_vm_refresh_jit_flag(DSVM *vm);

DSVM *ds_vm_create(void) {
    DSVM *vm = (DSVM *)calloc(1, sizeof(DSVM));
    if (!vm)
        return NULL;
    ds_gc_init(vm);
    vm->sp = vm->stack;
    vm->frame_count = 0;
    vm->return_reg = DS_VAL_NIL;
    vm->has_error = 0;
    vm->stop_requested = 0;
    ds_vm_refresh_jit_flag(vm);

    vm->builtins = ds_dict_new(vm, 64);
    vm->modules = ds_dict_new(vm, 8);
    vm->str_methods = ds_dict_new(vm, 32);
    vm->list_methods = ds_dict_new(vm, 24);
    vm->dict_methods = ds_dict_new(vm, 16);

    DSString *main_name = ds_string_const(vm, "__main__");
    vm->main_module = ds_module_new(vm, main_name);
    ds_dict_set(vm, vm->modules, ds_obj_val(main_name), ds_obj_val(vm->main_module));

    ds_string_const(vm, "__name__");
    ds_store_name(vm, vm->main_module, ds_string_const(vm, "__name__"), ds_obj_val(ds_string_const(vm, "__main__")));

    ds_register_builtins(vm);
    ds_register_love(vm);

    vm->love.cb_load = DS_VAL_NIL;
    vm->love.cb_update = DS_VAL_NIL;
    vm->love.cb_draw = DS_VAL_NIL;
    vm->love.cb_touchpressed = DS_VAL_NIL;
    vm->love.cb_touchmoved = DS_VAL_NIL;
    vm->love.cb_touchreleased = DS_VAL_NIL;
    vm->love.cb_mousepressed = DS_VAL_NIL;
    vm->love.cb_mousereleased = DS_VAL_NIL;
    vm->love.cb_mousemoved = DS_VAL_NIL;
    vm->love.cb_keypressed = DS_VAL_NIL;
    vm->love.cb_keyreleased = DS_VAL_NIL;
    vm->love.draw_color = 0xFFFFFFFFu;
    vm->love.bg_color = 0xFF101418u;
    vm->love.line_width = 1.0f;
    vm->love.xf_sp = 0;
    vm->love.start_time = ds_time_sec();
    return vm;
}

void ds_vm_destroy(DSVM *vm) {
    if (!vm)
        return;
    ds_gc_free_all(vm);
    free(vm);
}

void ds_vm_set_print_callback(DSVM *vm, DSPrintCallback cb, void *userdata) {
    if (!vm)
        return;
    vm->print_cb = cb;
    vm->print_ud = userdata;
}

void ds_vm_set_project_dir(DSVM *vm, const char *dir) {
    if (!vm || !dir)
        return;
    snprintf(vm->project_dir, sizeof(vm->project_dir), "%s", dir);
}

void ds_vm_request_stop(DSVM *vm) {
    if (vm)
        vm->stop_requested = 1;
}

const char *ds_vm_last_error(const DSVM *vm) { return vm && vm->has_error ? vm->err_msg : ""; }
int ds_vm_last_error_line(const DSVM *vm) { return vm ? vm->err_line : 0; }
const char *ds_vm_last_error_file(const DSVM *vm) { return vm ? vm->err_file : ""; }

void ds_vm_get_stats(const DSVM *vm, DSVMStats *out) {
    if (!vm || !out)
        return;
    memset(out, 0, sizeof(*out));
    out->gc_bytes = vm->gc.bytes_allocated;
    out->gc_peak_bytes = vm->gc.peak_bytes;
    out->gc_objects = vm->gc.object_count;
    out->gc_young_runs = vm->gc.young_runs;
    out->gc_major_runs = vm->gc.major_runs;
    out->gc_last_pause_us = vm->gc.last_pause_us;
    out->gc_max_pause_us = vm->gc.max_pause_us;
    out->compile_ms = vm->last_compile_ms;
    out->exec_ms = vm->exec_ms;
    out->jit_calls = vm->jit_calls;
    out->jit_functions = vm->jit_fn_count;
    out->jit_available = ds_jit_supported();
}

/* The native JIT is on unless the host asks for the reference interpreter
 * (DS_NOJIT=1). That is how jit_bench.py and jit_diff_test.py run both
 * execution paths over identical input. */
static void ds_vm_refresh_jit_flag(DSVM *vm) {
    const char *nojit = getenv("DS_NOJIT");
    vm->jit_enabled = !(nojit && nojit[0] && nojit[0] != '0');
}

void ds_vm_reset(DSVM *vm) {
    if (!vm)
        return;
    ds_close_upvalues(vm, vm->stack);
    vm->sp = vm->stack;
    vm->frame_count = 0;
    vm->open_upvalues = NULL;
    vm->has_error = 0;
    vm->stop_requested = 0;
    ds_vm_refresh_jit_flag(vm);
    vm->err_msg[0] = '\0';
    vm->return_reg = DS_VAL_NIL;
    vm->temp_root_count = 0;
    ds_gc_collect_full(vm);
}

static void push_print(DSVM *vm, const char *line, int is_error) {
    if (vm->print_cb)
        vm->print_cb(line, is_error, vm->print_ud);
    else
        ds_console_raw(is_error, line);
}

static int run_block(DSVM *vm, DSFuncProto *proto, int print_last) {
    if (!proto)
        return 0;
    DSClosure *cl = ds_closure_new(vm, proto);
    if (!cl)
        return 0;
    ds_push_root(vm, ds_obj_val(cl));
    proto->module = vm->main_module;
    DSValue r = call_closure(vm, cl, 0, NULL);
    ds_pop_roots(vm, 1);
    (void)r;
    (void)print_last;
    return !vm->has_error;
}

int ds_vm_run_source(DSVM *vm, const char *filename, const char *source, size_t len) {
    if (!vm || !source)
        return 0;
    vm->has_error = 0;
    vm->stop_requested = 0;
    vm->err_msg[0] = '\0';
    snprintf(vm->err_file, sizeof(vm->err_file), "%s", filename ? filename : "main.ds");

    DSFuncProto *proto = ds_compile_module(vm, vm->main_module, filename, source, len, 0);
    if (!proto) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "Ошибка компиляции в %s:%d: %s", vm->err_file, vm->err_line, vm->err_msg);
        push_print(vm, msg, 1);
        return 0;
    }
    uint64_t run_t0 = ds_monotonic_ns();
    int ok = run_block(vm, proto, 0);
    vm->exec_ms = (double)(ds_monotonic_ns() - run_t0) * 1e-6;
    if (!ok) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "%s:%d: %s", vm->err_file, vm->err_line, vm->err_msg);
        push_print(vm, msg, 1);
    }
    /* Resolve load/update/draw/... now that the module body has run, so that
     * ds_vm_has_love_loop() is meaningful before the first love_load(). */
    ds_vm_love_refresh(vm);
    return ok;
}

int ds_vm_eval_repl(DSVM *vm, const char *line) {
    if (!vm || !line)
        return 0;
    vm->has_error = 0;
    vm->err_msg[0] = '\0';
    snprintf(vm->err_file, sizeof(vm->err_file), "repl");
    DSFuncProto *proto = ds_compile_module(vm, vm->main_module, "repl", line, strlen(line), 1);
    if (!proto) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "Ошибка: %s", vm->err_msg);
        push_print(vm, msg, 1);
        return 0;
    }
    DSClosure *cl = ds_closure_new(vm, proto);
    if (!cl)
        return 0;
    ds_push_root(vm, ds_obj_val(cl));
    proto->module = vm->main_module;

    /* Capture the value of a trailing expression statement */
    const DSStmtNode *last = proto->body;
    while (last && last->next)
        last = last->next;
    int want_print = last && last->kind == ST_EXPR && last->expr;

    vm->repl_last_value = DS_VAL_NIL;
    DSValue r = call_closure(vm, cl, 0, NULL);
    (void)r;
    ds_pop_roots(vm, 1);

    if (vm->has_error) {
        char msg[1100];
        snprintf(msg, sizeof(msg), "Ошибка: %s", vm->err_msg);
        push_print(vm, msg, 1);
        return 0;
    }
    if (want_print && !ds_is_nil(vm->repl_last_value)) {
        DSString *s = ds_val_to_string(vm, vm->repl_last_value);
        push_print(vm, s->chars, 0);
    }
    return 1;
}

void ds_vm_gc_frame_tick(DSVM *vm, double max_budget_us) {
    if (!vm)
        return;
    ds_gc_step_us(vm, max_budget_us);
}
