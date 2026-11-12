/* Direct-Threaded Compiler: lowers AST into specialized C closures.
 * There is no bytecode, no dispatch loop: every expression node gets a
 * dedicated function pointer chosen at compile time, and pure-numeric
 * functions are additionally lowered to native AArch64/x86_64 machine code. */

#include "discript/ds_ast.h"
#include "discript/ds_lexer.h"
#include "discript/discript.h"
#include "engine/runtime.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * COMPILATION ARENA (refcounted, frees whole chunks at once)
 * ============================================================================ */

DSCompiledArena *ds_arena_create(void) {
    DSCompiledArena *a = (DSCompiledArena *)calloc(1, sizeof(DSCompiledArena));
    if (a)
        a->refcount = 1;
    return a;
}

void *ds_arena_alloc(DSCompiledArena *arena, size_t size, size_t align) {
    if (!arena)
        return NULL;
    if (align < 16)
        align = 16;
    DSCompiledChunk *c = arena->chunks;
    if (c) {
        size_t off = (c->used + align - 1u) & ~(align - 1u);
        if (off + size <= c->cap) {
            c->used = off + size;
            return c->data + off;
        }
    }
    size_t cap = 65536;
    if (size + 64 > cap)
        cap = (size + 128 + 65535u) & ~(size_t)65535u;
    DSCompiledChunk *nc = (DSCompiledChunk *)malloc(sizeof(DSCompiledChunk) + cap + 64);
    if (!nc)
        return NULL;
    uint8_t *base = (uint8_t *)(nc + 1);
    uint8_t *aligned = (uint8_t *)(((uintptr_t)base + 63u) & ~(uintptr_t)63u);
    nc->next = arena->chunks;
    nc->data = aligned;
    nc->cap = cap;
    nc->used = (size + align - 1u) & ~(align - 1u);
    arena->chunks = nc;
    return nc->data;
}

void ds_arena_retain(DSCompiledArena *arena) {
    if (arena)
        arena->refcount++;
}

void ds_arena_release(DSCompiledArena *arena) {
    if (!arena)
        return;
    if (--arena->refcount > 0)
        return;
    DSCompiledChunk *c = arena->chunks;
    while (c) {
        DSCompiledChunk *nx = c->next;
        free(c);
        c = nx;
    }
    free(arena);
}

/* ============================================================================
 * EXPRESSION EVALUATORS
 * ============================================================================ */

static DSValue ex_num(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    (void)vm;
    (void)slots;
    if (n->aux == 1)
        return n->num != 0.0 ? DS_VAL_TRUE : DS_VAL_FALSE;
    if (n->aux == 2)
        return DS_VAL_NIL;
    return ds_num(n->num);
}

static DSValue ex_str(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    (void)vm;
    (void)slots;
    return ds_obj_val(n->str);
}

static DSValue ex_name_global(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    (void)slots;
    DSModule *mod = n->aux ? NULL : (DSModule *)(uintptr_t)n->count;
    (void)mod;
    if (vm->frame_count > 0)
        mod = vm->frames[vm->frame_count - 1].module;
    if (!mod)
        mod = vm->main_module;
    return ds_lookup_name(vm, mod, n->str);
}

static DSValue ex_name_local(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    (void)vm;
    return slots[n->index];
}

static DSValue ex_name_upval(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSClosure *cl = slots[0] ? AS_CLOSURE(slots[0]) : NULL;
    if (!cl || n->index >= cl->upval_count)
        return DS_VAL_NIL;
    DSUpvalue *uv = cl->upvalues[n->index];
    return uv ? *uv->location : DS_VAL_NIL;
}

static DSValue ex_list(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSList *l = ds_list_new(vm, n->count);
    if (!l)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(l));
    for (int i = 0; i < n->count; i++) {
        DSValue v = n->items[i] ? n->items[i]->eval(vm, slots, n->items[i]) : DS_VAL_NIL;
        ds_list_push(vm, l, v);
        if (vm->has_error)
            break;
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(l);
}

static DSValue ex_dict(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSDict *d = ds_dict_new(vm, n->count * 2);
    if (!d)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(d));
    for (int i = 0; i < n->count; i++) {
        DSValue k = n->keys[i]->eval(vm, slots, n->keys[i]);
        DSValue v = n->items[i]->eval(vm, slots, n->items[i]);
        ds_dict_set(vm, d, k, v);
        if (vm->has_error)
            break;
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(d);
}

static DSValue ex_binop_add(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    DSValue b = n->b->eval(vm, slots, n->b);
    if ((a | b) & DS_QNAN_MASK)
        return ds_binop(vm, OP_ADD, a, b);
    return ds_num(ds_as_num(a) + ds_as_num(b));
}

static DSValue ex_binop_sub(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    DSValue b = n->b->eval(vm, slots, n->b);
    if ((a | b) & DS_QNAN_MASK)
        return ds_binop(vm, OP_SUB, a, b);
    return ds_num(ds_as_num(a) - ds_as_num(b));
}

static DSValue ex_binop_mul(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    DSValue b = n->b->eval(vm, slots, n->b);
    if ((a | b) & DS_QNAN_MASK)
        return ds_binop(vm, OP_MUL, a, b);
    return ds_num(ds_as_num(a) * ds_as_num(b));
}

static DSValue ex_binop_div(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    DSValue b = n->b->eval(vm, slots, n->b);
    if ((a | b) & DS_QNAN_MASK)
        return ds_binop(vm, OP_DIV, a, b);
    return ds_num(ds_as_num(a) / ds_as_num(b));
}

static DSValue ex_binop_generic(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    DSValue b = n->b->eval(vm, slots, n->b);
    return ds_binop(vm, n->op, a, b);
}

static DSValue ex_binop_and(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    if (!ds_is_truthy(a))
        return a;
    return n->b->eval(vm, slots, n->b);
}

static DSValue ex_binop_or(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    if (ds_is_truthy(a))
        return a;
    return n->b->eval(vm, slots, n->b);
}

static DSValue ex_unop(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue a = n->a->eval(vm, slots, n->a);
    return ds_unop(vm, n->op, a);
}

static DSValue ex_ternary(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue cond = n->b->eval(vm, slots, n->b);
    if (ds_is_truthy(cond))
        return n->a->eval(vm, slots, n->a);
    return n->c ? n->c->eval(vm, slots, n->c) : DS_VAL_NIL;
}

static DSValue ex_dot(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue obj = n->a->eval(vm, slots, n->a);
    return ds_get_property(vm, obj, n->str, (DSInlineCache *)&n->ic);
}

static DSValue ex_index(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue obj = n->a->eval(vm, slots, n->a);
    DSValue idx = n->b->eval(vm, slots, n->b);
    return ds_get_index(vm, obj, idx);
}

static DSValue ex_slice(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue obj = n->a->eval(vm, slots, n->a);
    DSValue lo = n->b ? n->b->eval(vm, slots, n->b) : DS_VAL_NIL;
    DSValue hi = n->c ? n->c->eval(vm, slots, n->c) : DS_VAL_NIL;
    if (!ds_obj_is(obj, DS_OBJ_LIST)) {
        if (ds_obj_is(obj, DS_OBJ_STRING)) {
            DSString *s = AS_STRING(obj);
            int len = (int)s->len;
            int a = ds_is_nil(lo) ? 0 : (int)ds_to_number(vm, lo);
            int b = ds_is_nil(hi) ? len : (int)ds_to_number(vm, hi);
            if (a < 0) a += len;
            if (b < 0) b += len;
            if (a < 0) a = 0;
            if (b > len) b = len;
            if (b < a) b = a;
            return ds_obj_val(ds_string_new(vm, s->chars + a, (size_t)(b - a)));
        }
        ds_error(vm, n->line, "срез поддерживается только для list и str");
        return DS_VAL_NIL;
    }
    DSList *l = AS_LIST(obj);
    int len = l->len;
    int a = ds_is_nil(lo) ? 0 : (int)ds_to_number(vm, lo);
    int b = ds_is_nil(hi) ? len : (int)ds_to_number(vm, hi);
    if (a < 0) a += len;
    if (b < 0) b += len;
    if (a < 0) a = 0;
    if (b > len) b = len;
    if (b < a) b = a;
    DSList *res = ds_list_new(vm, b - a);
    if (!res)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(res));
    for (int i = a; i < b; i++)
        ds_list_push(vm, res, l->items[i]);
    ds_pop_roots(vm, 1);
    return ds_obj_val(res);
}

static DSValue ex_fmt(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue v = n->a->eval(vm, slots, n->a);
    return ds_format_value(vm, v, n->b ? n->b->str : NULL);
}

static DSValue ex_lambda(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSFuncProto *proto = n->proto;
    if (!proto)
        return DS_VAL_NIL;
    if (!proto->module)
        proto->module = (vm->frame_count > 0) ? vm->frames[vm->frame_count - 1].module : vm->main_module;
    if (!proto->filename)
        proto->filename = ds_string_const(vm, vm->err_file[0] ? vm->err_file : "main.ds");
    ds_push_root(vm, ds_obj_val(proto));
    DSClosure *cl = ds_closure_new(vm, proto);
    if (!cl) {
        ds_pop_roots(vm, 1);
        return DS_VAL_NIL;
    }
    ds_push_root(vm, ds_obj_val(cl));
    for (int i = 0; i < proto->num_upvalues; i++) {
        DSUpvalDesc d = proto->upvals[i];
        if (d.is_local)
            cl->upvalues[i] = ds_capture_upvalue(vm, &slots[d.index]);
        else {
            DSClosure *outer = slots[0] ? AS_CLOSURE(slots[0]) : NULL;
            if (outer && d.index < outer->upval_count)
                cl->upvalues[i] = outer->upvalues[d.index];
        }
    }
    ds_pop_roots(vm, 2);
    return ds_obj_val(cl);
}

static DSValue ex_call(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    DSValue callee = n->a->eval(vm, slots, n->a);
    if (vm->has_error)
        return DS_VAL_NIL;

    DSValue argv_buf[16];
    DSValue *argv = argv_buf;
    if (n->count > 16) {
        argv = (DSValue *)malloc(sizeof(DSValue) * (size_t)n->count);
        if (!argv)
            return DS_VAL_NIL;
    }
    for (int i = 0; i < n->count; i++)
        argv[i] = n->items[i]->eval(vm, slots, n->items[i]);
    DSValue r = ds_call_value(vm, callee, n->count, argv);
    if (argv != argv_buf)
        free(argv);
    return r;
}

/* ============================================================================
 * STATEMENT EXECUTORS
 * ============================================================================ */

static DSExecSignal st_expr(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSValue v = n->expr->eval(vm, slots, n->expr);
    if (vm->has_error)
        return EXEC_ERROR;
    if (n->flags & 2) {
        vm->repl_last_value = v;
        if (vm->auto_display && !ds_is_nil(v)) {
            DSString *sv = ds_val_to_string(vm, v);
            if (vm->print_cb)
                vm->print_cb(sv->chars, 0, vm->print_ud);
            else
                ds_console_raw(0, sv->chars);
        }
    }
    return EXEC_OK;
}

static void assign_to(DSVM *vm, DSValue *slots, DSExprNode *target, DSValue value, int line) {
    switch (target->kind) {
    case EX_NAME: {
        DSModule *mod = (vm->frame_count > 0) ? vm->frames[vm->frame_count - 1].module : vm->main_module;
        if (target->scope == NAME_LOCAL) {
            DSValue *loc = &slots[target->index];
            if (*loc == DS_VAL_NIL) {
                /* newly assigned local that is captured: preserve via upvalue */
                *loc = value;
            } else {
                *loc = value;
            }
            return;
        }
        if (target->scope == NAME_UPVAL) {
            DSClosure *cl = slots[0] ? AS_CLOSURE(slots[0]) : NULL;
            if (cl && target->index < cl->upval_count) {
                DSUpvalue *uv = cl->upvalues[target->index];
                if (uv) {
                    *uv->location = value;
                    ds_gc_write_barrier(vm, &uv->hdr, value);
                }
            }
            return;
        }
        ds_store_name(vm, mod, target->str, value);
        return;
    }
    case EX_DOT: {
        DSValue obj = target->a->eval(vm, slots, target->a);
        ds_set_property(vm, obj, target->str, value);
        return;
    }
    case EX_INDEX: {
        DSValue cont = target->a->eval(vm, slots, target->a);
        DSValue idx = target->b->eval(vm, slots, target->b);
        ds_set_index(vm, cont, idx, value);
        return;
    }
    case EX_LIST: {
        /* tuple unpacking: a, b = expr */
        DSIter it;
        if (!ds_iter_open(vm, value, &it))
            return;
        for (int i = 0; i < target->count; i++) {
            DSValue v = DS_VAL_NIL;
            ds_iter_next(vm, &it, &v);
            if (i < target->count - 1 && target->items[i]->kind == EX_LIST) {
                assign_to(vm, slots, target->items[i], v, line);
            } else {
                assign_to(vm, slots, target->items[i], v, line);
            }
        }
        return;
    }
    default:
        ds_error(vm, line, "недопустимая цель присваивания");
        return;
    }
}

/* ---- comprehensions -------------------------------------------------------
 * `[expr for t in it if c for u in f]` and `{k: v for ...}` are evaluated by
 * walking the clause list recursively: a loop clause opens an iterator and
 * rebinds its target per element, a filter clause skips elements that fail.
 * The result container is rooted for the whole walk because each iteration may
 * allocate (and therefore collect). */
typedef struct ComprehRun {
    DSVM *vm;
    DSValue *slots;
    const DSExprNode *node;
    DSValue out; /* the list or dict being filled */
} ComprehRun;

static void comp_run_clause(ComprehRun *r, int ci) {
    const DSExprNode *n = r->node;
    if (r->vm->has_error)
        return;
    if (ci >= n->clause_count) {
        /* All clauses satisfied: produce one element. */
        if (n->is_dict_comprehension) {
            DSValue k = n->a->eval(r->vm, r->slots, n->a);
            DSValue v = n->b->eval(r->vm, r->slots, n->b);
            if (r->vm->has_error)
                return;
            ds_push_root(r->vm, k);
            ds_push_root(r->vm, v);
            ds_dict_set(r->vm, AS_DICT(r->out), k, v);
            ds_pop_roots(r->vm, 2);
        } else {
            DSValue v = n->a->eval(r->vm, r->slots, n->a);
            if (r->vm->has_error)
                return;
            ds_list_push(r->vm, AS_LIST(r->out), v);
        }
        return;
    }
    const DSExprNode *cl = n->clauses[ci];
    if (cl->kind == EX_COMPR_IF) {
        DSValue ok = cl->a->eval(r->vm, r->slots, cl->a);
        if (r->vm->has_error)
            return;
        if (ds_is_truthy(ok))
            comp_run_clause(r, ci + 1);
        return;
    }
    DSValue iterable = cl->b->eval(r->vm, r->slots, cl->b);
    if (r->vm->has_error)
        return;
    ds_push_root(r->vm, iterable);
    DSIter it;
    if (!ds_iter_open(r->vm, iterable, &it)) {
        ds_pop_roots(r->vm, 1);
        return;
    }
    for (;;) {
        if (r->vm->stop_requested)
            break;
        DSValue item = DS_VAL_NIL;
        int more = ds_iter_next(r->vm, &it, &item);
        if (r->vm->has_error || !more)
            break;
        ds_push_root(r->vm, item);
        assign_to(r->vm, r->slots, cl->target, item, cl->line);
        ds_pop_roots(r->vm, 1);
        if (r->vm->has_error)
            break;
        comp_run_clause(r, ci + 1);
        if (r->vm->has_error)
            break;
    }
    ds_pop_roots(r->vm, 1);
}

static DSValue ex_compreh(DSVM *vm, DSValue *slots, const DSExprNode *n) {
    ComprehRun r;
    r.vm = vm;
    r.slots = slots;
    r.node = n;
    if (n->is_dict_comprehension) {
        DSDict *d = ds_dict_new(vm, 0);
        if (!d)
            return DS_VAL_NIL;
        r.out = ds_obj_val(d);
    } else {
        DSList *l = ds_list_new(vm, 0);
        if (!l)
            return DS_VAL_NIL;
        r.out = ds_obj_val(l);
    }
    ds_push_root(vm, r.out);

    /* Python semantics: a comprehension has its own scope, so the loop variables
     * must not leak into (or clobber) the enclosing function's locals. */
    DSValue saved[64];
    int saved_slot[64];
    DSString *saved_name[64];
    int nsaved = 0;
    DSModule *mod = (vm->frame_count > 0) ? vm->frames[vm->frame_count - 1].module : vm->main_module;
    int at_module_level = (n->save_scope == NAME_GLOBAL);
    for (int i = 0; i < n->save_count && nsaved < 64; i++) {
        DSExprNode *t = n->save_targets[i];
        if (!t || t->kind != EX_NAME)
            continue;
        if (at_module_level && mod && t->str) {
            /* Module scope: the loop variable is a plain global. Use a silent
             * lookup — the name usually does not exist yet, and ds_lookup_name()
             * would raise "имя не определено" for it. */
            DSValue prev = DS_VAL_NIL;
            saved_slot[nsaved] = -1;
            saved_name[nsaved] = t->str;
            saved[nsaved] = (t->str && mod->dict && ds_dict_get_str(mod->dict, t->str, &prev)) ? prev : DS_VAL_NIL;
            nsaved++;
        } else if (t->scope == NAME_LOCAL) {
            saved_slot[nsaved] = t->index;
            saved_name[nsaved] = NULL;
            saved[nsaved] = slots[t->index];
            nsaved++;
        }
    }

    if (n->clause_count > 0)
        comp_run_clause(&r, 0);

    for (int i = 0; i < nsaved; i++) {
        if (saved_slot[i] >= 0)
            slots[saved_slot[i]] = saved[i];
        else if (mod && saved_name[i])
            ds_store_name(vm, mod, saved_name[i], saved[i]);
    }

    ds_pop_roots(vm, 1);
    return r.out;
}


static DSExecSignal st_assign(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSValue v = n->expr->eval(vm, slots, n->expr);
    if (vm->has_error)
        return EXEC_ERROR;
    ds_push_root(vm, v);
    for (int i = 0; i < n->ntargets; i++)
        assign_to(vm, slots, n->targets[i], v, n->line);
    ds_pop_roots(vm, 1);
    return vm->has_error ? EXEC_ERROR : EXEC_OK;
}

static DSExecSignal st_augassign(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSExprNode *t = n->targets[0];
    DSValue cur;
    DSValue rhs = n->expr->eval(vm, slots, n->expr);
    if (vm->has_error)
        return EXEC_ERROR;
    ds_push_root(vm, rhs);
    if (t->kind == EX_NAME) {
        cur = t->eval(vm, slots, t);
    } else if (t->kind == EX_DOT) {
        DSValue obj = t->a->eval(vm, slots, t->a);
        cur = ds_get_property(vm, obj, t->str, NULL);
    } else if (t->kind == EX_INDEX) {
        DSValue cont = t->a->eval(vm, slots, t->a);
        DSValue idx = t->b->eval(vm, slots, t->b);
        cur = ds_get_index(vm, cont, idx);
    } else {
        ds_pop_roots(vm, 1);
        ds_error(vm, n->line, "недопустимая цель '+='");
        return EXEC_ERROR;
    }
    ds_push_root(vm, cur);
    DSValue res = ds_binop(vm, n->op, cur, rhs);
    ds_push_root(vm, res);
    assign_to(vm, slots, t, res, n->line);
    ds_pop_roots(vm, 3);
    return vm->has_error ? EXEC_ERROR : EXEC_OK;
}

static DSExecSignal st_if(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    for (int i = 0; i < n->nbranches; i++) {
        if (!n->conds[i])
            continue;
        DSValue c = n->conds[i]->eval(vm, slots, n->conds[i]);
        if (vm->has_error)
            return EXEC_ERROR;
        if (ds_is_truthy(c)) {
            const DSStmtNode *s = n->bodies[i];
            while (s) {
                DSExecSignal sig = s->exec(vm, slots, s);
                if (sig != EXEC_OK)
                    return sig;
                s = s->next;
            }
            return EXEC_OK;
        }
    }
    const DSStmtNode *s = n->orelse;
    while (s) {
        DSExecSignal sig = s->exec(vm, slots, s);
        if (sig != EXEC_OK)
            return sig;
        s = s->next;
    }
    return EXEC_OK;
}

static DSExecSignal st_while(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    for (;;) {
        if (vm->stop_requested)
            return EXEC_ERROR;
        DSValue c = n->cond->eval(vm, slots, n->cond);
        if (vm->has_error)
            return EXEC_ERROR;
        if (!ds_is_truthy(c))
            break;
        const DSStmtNode *s = n->body;
        DSExecSignal brk = EXEC_OK;
        while (s) {
            DSExecSignal sig = s->exec(vm, slots, s);
            if (sig == EXEC_BREAK) {
                brk = EXEC_BREAK;
                break;
            }
            if (sig == EXEC_CONTINUE)
                break;
            if (sig != EXEC_OK)
                return sig;
            s = s->next;
        }
        if (brk == EXEC_BREAK)
            return EXEC_OK;
    }
    const DSStmtNode *s = n->orelse;
    while (s) {
        DSExecSignal sig = s->exec(vm, slots, s);
        if (sig != EXEC_OK)
            return sig;
        s = s->next;
    }
    return EXEC_OK;
}

static DSExecSignal st_for(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSExprNode *tgt = n->targets[0];
    DSValue iterable = n->expr->eval(vm, slots, n->expr);
    if (vm->has_error)
        return EXEC_ERROR;
    ds_push_root(vm, iterable);

    DSIter it;
    if (!ds_iter_open(vm, iterable, &it)) {
        ds_pop_roots(vm, 1);
        return EXEC_ERROR;
    }

    DSExecSignal result = EXEC_OK;
    for (;;) {
        if (vm->stop_requested) {
            result = EXEC_ERROR;
            break;
        }
        DSValue item = DS_VAL_NIL;
        int more = ds_iter_next(vm, &it, &item);
        if (vm->has_error) {
            result = EXEC_ERROR;
            break;
        }
        if (!more)
            break;
        ds_push_root(vm, item);
        assign_to(vm, slots, tgt, item, n->line);
        ds_pop_roots(vm, 1);
        if (vm->has_error) {
            result = EXEC_ERROR;
            break;
        }
        const DSStmtNode *s = n->body;
        int brk = 0;
        while (s) {
            DSExecSignal sig = s->exec(vm, slots, s);
            if (sig == EXEC_BREAK) {
                brk = 1;
                break;
            }
            if (sig == EXEC_CONTINUE)
                break;
            if (sig != EXEC_OK) {
                result = sig;
                s = NULL;
                break;
            }
            s = s->next;
        }
        if (result != EXEC_OK)
            break;
        if (brk) {
            result = EXEC_OK;
            break;
        }
    }
    ds_pop_roots(vm, 1);
    if (result != EXEC_OK)
        return result;
    const DSStmtNode *s = n->orelse;
    while (s) {
        DSExecSignal sig = s->exec(vm, slots, s);
        if (sig != EXEC_OK)
            return sig;
        s = s->next;
    }
    return EXEC_OK;
}

/* Numeric specialized range loop: for i in range(a, b, step) */
static DSExecSignal st_for_range(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSValue *sl = slots;
    DSValue a = n->items[0] ? n->items[0]->eval(vm, sl, n->items[0]) : ds_num(0.0);
    DSValue b = n->items[1]->eval(vm, sl, n->items[1]);
    DSValue st = n->items[2] ? n->items[2]->eval(vm, sl, n->items[2]) : ds_num(1.0);
    if (vm->has_error)
        return EXEC_ERROR;
    double start = ds_to_number(vm, a), stop = ds_to_number(vm, b), step = ds_to_number(vm, st);
    if (vm->has_error)
        return EXEC_ERROR;
    if (step == 0.0) {
        ds_error(vm, n->line, "шаг range() не может быть нулём");
        return EXEC_ERROR;
    }
    DSExprNode *tgt = n->targets[0];
    for (double i = start; step > 0 ? i < stop : i > stop; i += step) {
        if (vm->stop_requested)
            return EXEC_ERROR;
        assign_to(vm, slots, tgt, ds_num(i), n->line);
        if (vm->has_error)
            return EXEC_ERROR;
        const DSStmtNode *s = n->body;
        int brk = 0;
        while (s) {
            DSExecSignal sig = s->exec(vm, slots, s);
            if (sig == EXEC_BREAK) {
                brk = 1;
                break;
            }
            if (sig == EXEC_CONTINUE)
                break;
            if (sig != EXEC_OK)
                return sig;
            s = s->next;
        }
        if (brk)
            return EXEC_OK;
    }
    const DSStmtNode *s = n->orelse;
    while (s) {
        DSExecSignal sig = s->exec(vm, slots, s);
        if (sig != EXEC_OK)
            return sig;
        s = s->next;
    }
    return EXEC_OK;
}

static DSExecSignal st_return(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    vm->return_reg = n->expr ? n->expr->eval(vm, slots, n->expr) : DS_VAL_NIL;
    return EXEC_RETURN;
}

static DSExecSignal st_pass(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    (void)vm;
    (void)slots;
    (void)n;
    return EXEC_OK;
}
static DSExecSignal st_break(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    (void)vm;
    (void)slots;
    (void)n;
    return EXEC_BREAK;
}
static DSExecSignal st_continue(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    (void)vm;
    (void)slots;
    (void)n;
    return EXEC_CONTINUE;
}

/* Stores a def/class/import binding respecting the statically resolved scope. */
static void store_bound(DSVM *vm, DSValue *slots, const DSStmtNode *n, DSValue v) {
    if (n->target_scope == NAME_LOCAL) {
        slots[n->target_index] = v;
        return;
    }
    if (n->target_scope == NAME_UPVAL) {
        DSClosure *cl = slots[0] ? AS_CLOSURE(slots[0]) : NULL;
        if (cl && n->target_index < cl->upval_count) {
            DSUpvalue *uv = cl->upvalues[n->target_index];
            if (uv) {
                *uv->location = v;
                ds_gc_write_barrier(vm, &uv->hdr, v);
            }
        }
        return;
    }
    DSModule *mod = (vm->frame_count > 0) ? vm->frames[vm->frame_count - 1].module : vm->main_module;
    ds_store_name(vm, mod, n->name, v);
}

static DSExecSignal st_def(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSFuncProto *proto = n->proto;
    if (!proto)
        return EXEC_ERROR;
    if (!proto->module)
        proto->module = (vm->frame_count > 0) ? vm->frames[vm->frame_count - 1].module : vm->main_module;
    if (!proto->filename)
        proto->filename = ds_string_const(vm, vm->err_file[0] ? vm->err_file : "main.ds");
    ds_push_root(vm, ds_obj_val(proto));

    DSClosure *cl = ds_closure_new(vm, proto);
    if (!cl) {
        ds_pop_roots(vm, 1);
        return EXEC_ERROR;
    }
    ds_push_root(vm, ds_obj_val(cl));
    /* capture upvalues */
    for (int i = 0; i < proto->num_upvalues; i++) {
        DSUpvalDesc d = proto->upvals[i];
        if (d.is_local) {
            cl->upvalues[i] = ds_capture_upvalue(vm, &slots[d.index]);
        } else {
            DSClosure *outer = slots[0] ? AS_CLOSURE(slots[0]) : NULL;
            if (outer && d.index < outer->upval_count)
                cl->upvalues[i] = outer->upvalues[d.index];
        }
    }
    store_bound(vm, slots, n, ds_obj_val(cl));
    ds_pop_roots(vm, 2);
    return vm->has_error ? EXEC_ERROR : EXEC_OK;
}

static DSExecSignal st_class(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSClass *super = NULL;
    if (n->expr) {
        DSValue sv = n->expr->eval(vm, slots, n->expr);
        if (ds_obj_is(sv, DS_OBJ_CLASS)) {
            super = AS_CLASS(sv);
            ds_push_root(vm, sv);
        }
    }
    DSClass *kl = ds_class_new(vm, n->name, super);
    if (!kl) {
        if (super)
            ds_pop_roots(vm, 1);
        return EXEC_ERROR;
    }
    ds_push_root(vm, ds_obj_val(kl));
    store_bound(vm, slots, n, ds_obj_val(kl));

    /* Methods: compile each def in the class body into a closure bound to this class dict */
    for (const DSStmtNode *s = n->body; s; s = s->next) {
        if (s->kind != ST_DEF || !s->proto)
            continue;
        DSFuncProto *proto = s->proto;
        proto->module = vm->frames[vm->frame_count - 1].module;
        proto->filename = ds_string_const(vm, vm->err_file[0] ? vm->err_file : "main.ds");
        ds_push_root(vm, ds_obj_val(proto));
        DSClosure *cl = ds_closure_new(vm, proto);
        if (cl) {
            ds_push_root(vm, ds_obj_val(cl));
            for (int i = 0; i < proto->num_upvalues; i++) {
                DSUpvalDesc d = proto->upvals[i];
                cl->upvalues[i] = d.is_local ? ds_capture_upvalue(vm, &slots[d.index]) : NULL;
            }
            DSString *mname = s->name;
            ds_dict_set(vm, kl->methods, ds_obj_val(mname), ds_obj_val(cl));
            if (mname->len == 8 && memcmp(mname->chars, "__init__", 8) == 0)
                kl->init_fn = ds_obj_val(cl);
            ds_pop_roots(vm, 1);
        }
        ds_pop_roots(vm, 1);
    }
    ds_pop_roots(vm, super ? 2 : 1);
    return vm->has_error ? EXEC_ERROR : EXEC_OK;
}

static DSExecSignal st_import(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    (void)slots;
    DSModule *mod = vm->frames[vm->frame_count - 1].module;
    if (n->flags & 1) {
        /* from X import a, b */
        DSValue modv = ds_lookup_name(vm, mod, n->name);
        if (vm->has_error)
            return EXEC_ERROR;
        for (int i = 0; i < n->count; i++) {
            DSExprNode *nm = n->items[i];
            DSValue v = ds_get_property(vm, modv, nm->str, NULL);
            DSString *bind = nm->b ? nm->b->str : nm->str;
            ds_store_name(vm, mod, bind, v);
        }
    } else {
        for (int i = 0; i < n->count; i++) {
            DSExprNode *nm = n->items[i];
            DSValue v = ds_lookup_name(vm, mod, nm->str);
            if (vm->has_error)
                return EXEC_ERROR;
            DSString *bind = nm->b ? nm->b->str : nm->str;
            if (!nm->b) {
                /* bind first dotted segment */
                if (strchr(bind->chars, '.')) {
                    size_t k = (size_t)(strchr(bind->chars, '.') - bind->chars);
                    bind = ds_string_new(vm, bind->chars, k);
                }
            }
            ds_store_name(vm, mod, bind, v);
        }
    }
    return EXEC_OK;
}

static DSExecSignal st_del(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSExprNode *t = n->expr;
    if (t && t->kind == EX_INDEX) {
        DSValue cont = t->a->eval(vm, slots, t->a);
        DSValue idx = t->b->eval(vm, slots, t->b);
        if (ds_obj_is(cont, DS_OBJ_DICT))
            ds_dict_del(AS_DICT(cont), idx);
        else if (ds_obj_is(cont, DS_OBJ_LIST)) {
            DSList *l = AS_LIST(cont);
            int i = (int)ds_to_number(vm, idx);
            if (i < 0) i += l->len;
            if (i >= 0 && i < l->len) {
                memmove(&l->items[i], &l->items[i + 1], sizeof(DSValue) * (size_t)(l->len - i - 1));
                l->len--;
            }
        }
        return EXEC_OK;
    }
    if (t && t->kind == EX_NAME) {
        DSModule *mod = vm->frames[vm->frame_count - 1].module;
        if (t->scope == NAME_LOCAL)
            slots[t->index] = DS_VAL_NIL;
        else
            ds_dict_del(mod->dict, ds_obj_val(t->str));
        return EXEC_OK;
    }
    ds_error(vm, n->line, "недопустимая цель 'del'");
    return EXEC_ERROR;
}

static DSExecSignal st_assert(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    DSValue v = n->expr->eval(vm, slots, n->expr);
    if (!ds_is_truthy(v)) {
        ds_error(vm, n->line, "assert: условие ложно");
        return EXEC_ERROR;
    }
    return EXEC_OK;
}

static DSExecSignal st_global(DSVM *vm, DSValue *slots, const DSStmtNode *n) {
    (void)vm;
    (void)slots;
    (void)n;
    return EXEC_OK;
}

/* ============================================================================
 * SCOPE RESOLUTION (no bytecode: only name->slot binding decisions)
 *
 * Slot 0 of every function's slot array holds the executing closure itself.
 * Locals live in slots[1..n]. Every function has a `chain` vector that maps
 * its own upvalue indices to the parent's locals / parent's upvalues.
 * ============================================================================ */

typedef struct Scope {
    struct Scope *parent;
    DSString **names;
    int count;
    int cap;
    int is_function;
    /* Names listed in a `global a, b` statement: assignments to them must go to
     * the module table instead of creating a fresh local. */
    DSString **globals;
    int global_count;
    int global_cap;
    /* The upvalue chain of this function, so a deeply nested closure can insert
     * forwarding entries into every function between itself and the owner. */
    struct UpvalChain *chain;
} Scope;

typedef struct UpvalChain {
    DSUpvalDesc *descs;
    int count;
    int cap;
} UpvalChain;

static int scope_is_global(Scope *s, DSString *name) {
    for (int i = 0; i < s->global_count; i++)
        if (s->globals[i] == name)
            return 1;
    return 0;
}

static void scope_declare_global(Scope *s, DSString *name) {
    if (!name || scope_is_global(s, name))
        return;
    if (s->global_count == s->global_cap) {
        int nc = s->global_cap ? s->global_cap * 2 : 8;
        DSString **ng = (DSString **)realloc(s->globals, (size_t)nc * sizeof(DSString *));
        if (!ng)
            return;
        s->globals = ng;
        s->global_cap = nc;
    }
    s->globals[s->global_count++] = name;
}

static int scope_find(Scope *s, DSString *name) {
    for (int i = 0; i < s->count; i++)
        if (s->names[i] == name)
            return i;
    return -1;
}

static int scope_add(Scope *s, DSString *name) {
    if (s->count == s->cap) {
        int ncap = s->cap < 8 ? 8 : s->cap * 2;
        DSString **nn = (DSString **)realloc(s->names, (size_t)ncap * sizeof(DSString *));
        if (!nn)
            return -1;
        s->names = nn;
        s->cap = ncap;
    }
    s->names[s->count] = name;
    return s->count++;
}

static int chain_add(UpvalChain *u, int is_local, int index) {
    for (int i = 0; i < u->count; i++)
        if (u->descs[i].is_local == is_local && u->descs[i].index == index)
            return i;
    if (u->count >= 250)
        return 0;
    if (u->count == u->cap) {
        int ncap = u->cap < 8 ? 8 : u->cap * 2;
        DSUpvalDesc *nd = (DSUpvalDesc *)realloc(u->descs, (size_t)ncap * sizeof(DSUpvalDesc));
        if (!nd)
            return 0;
        u->descs = nd;
        u->cap = ncap;
    }
    u->descs[u->count].is_local = (uint8_t)is_local;
    u->descs[u->count].index = (uint8_t)index;
    return u->count++;
}

static void compile_expr(DSVM *vm, Scope *scope, UpvalChain *chain, DSExprNode *e);
static void compile_function(DSVM *vm, Scope *outer, UpvalChain *outer_chain, DSFuncProto *fn);
static void compile_stmt(DSVM *vm, Scope *scope, UpvalChain *chain, DSStmtNode *s);
static void compile_block(DSVM *vm, Scope *scope, UpvalChain *chain, const DSStmtNode *block);

static void declare_target_names(Scope *scope, DSExprNode *t);
static void compile_target(DSVM *vm, Scope *scope, UpvalChain *chain, DSExprNode *t);
static void assign_to(DSVM *vm, DSValue *slots, DSExprNode *target, DSValue value, int line);

/* Find `name` in an enclosing function and build the upvalue chain that carries
 * it down to `scope`.
 *
 * A link with `is_local == 1` means "local slot `index` of the immediately
 * enclosing function"; `is_local == 0` means "upvalue `index` of the immediately
 * enclosing closure" (see the capture loop in `st_def`). Capturing a variable two
 * or more functions up therefore has to insert a forwarding upvalue into every
 * function in between — otherwise the innermost closure points at a slot of its
 * own parent that means something else entirely, which made
 * `def outer(): def middle(): def inner(): return x` evaluate to the function
 * itself instead of `x`. */
static int resolve_upval(Scope *scope, UpvalChain *chain, DSString *name) {
    Scope *between[64];
    int nbetween = 0;
    Scope *owner = NULL;
    int owner_slot = -1;

    for (Scope *s = scope->parent; s; s = s->parent) {
        if (!s->is_function)
            continue;
        int idx = scope_find(s, name);
        if (idx >= 0) {
            owner = s;
            owner_slot = idx;
            break;
        }
        if (nbetween >= 64)
            return -1;
        between[nbetween++] = s;
    }
    if (!owner)
        return -1;

    /* Walk back outwards->inwards, so each step can point at the upvalue index
     * that the previous (outer) step assigned. */
    int parent_index = owner_slot;
    int parent_is_local = 1;
    int result = -1;
    for (int i = nbetween; i >= 0; i--) {
        Scope *fn = (i == 0) ? scope : between[i - 1];
        UpvalChain *target = fn->chain ? fn->chain : chain;
        int u = chain_add(target, parent_is_local, parent_index);
        if (fn == scope)
            result = u;
        parent_index = u;
        parent_is_local = 0;
    }
    return result;
}

/* Evaluate the binding decision for one NAME node. */
static void bind_name(Scope *scope, UpvalChain *chain, DSExprNode *e, int store) {
    if (!scope->is_function) {
        e->scope = NAME_GLOBAL;
        return;
    }
    int local = scope_find(scope, e->str);
    if (local >= 0) {
        e->scope = NAME_LOCAL;
        e->index = local;
        return;
    }
    if (store) {
        if (scope_is_global(scope, e->str)) {
            e->scope = NAME_GLOBAL;
            return;
        }
        e->scope = NAME_LOCAL;
        e->index = scope_add(scope, e->str);
        return;
    }
    int uv = resolve_upval(scope, chain, e->str);
    if (uv >= 0) {
        e->scope = NAME_UPVAL;
        e->index = uv;
        return;
    }
    e->scope = NAME_GLOBAL;
}

static void compile_expr(DSVM *vm, Scope *scope, UpvalChain *chain, DSExprNode *e) {
    if (!e)
        return;
    switch (e->kind) {
    case EX_NUM: e->eval = ex_num; return;
    case EX_STR: e->eval = ex_str; return;
    case EX_NAME:
        bind_name(scope, chain, e, 0);
        e->eval = (e->scope == NAME_LOCAL) ? ex_name_local : (e->scope == NAME_UPVAL ? ex_name_upval : ex_name_global);
        return;
    case EX_LIST:
        for (int i = 0; i < e->count; i++)
            compile_expr(vm, scope, chain, e->items[i]);
        e->eval = ex_list;
        return;
    case EX_DICT:
        for (int i = 0; i < e->count; i++) {
            compile_expr(vm, scope, chain, e->keys[i]);
            compile_expr(vm, scope, chain, e->items[i]);
        }
        e->eval = ex_dict;
        return;
    case EX_BINOP:
        compile_expr(vm, scope, chain, e->a);
        compile_expr(vm, scope, chain, e->b);
        switch (e->op) {
        case OP_ADD: e->eval = ex_binop_add; break;
        case OP_SUB: e->eval = ex_binop_sub; break;
        case OP_MUL: e->eval = ex_binop_mul; break;
        case OP_DIV: e->eval = ex_binop_div; break;
        case OP_AND: e->eval = ex_binop_and; break;
        case OP_OR: e->eval = ex_binop_or; break;
        default: e->eval = ex_binop_generic; break;
        }
        return;
    case EX_UNOP:
        compile_expr(vm, scope, chain, e->a);
        e->eval = ex_unop;
        return;
    case EX_TERNARY:
        compile_expr(vm, scope, chain, e->a);
        compile_expr(vm, scope, chain, e->b);
        compile_expr(vm, scope, chain, e->c);
        e->eval = ex_ternary;
        return;
    case EX_CALL:
        compile_expr(vm, scope, chain, e->a);
        for (int i = 0; i < e->count; i++)
            compile_expr(vm, scope, chain, e->items[i]);
        e->eval = ex_call;
        return;
    case EX_DOT:
        compile_expr(vm, scope, chain, e->a);
        e->eval = ex_dot;
        return;
    case EX_INDEX:
        compile_expr(vm, scope, chain, e->a);
        compile_expr(vm, scope, chain, e->b);
        e->eval = ex_index;
        return;
    case EX_SLICE:
        compile_expr(vm, scope, chain, e->a);
        compile_expr(vm, scope, chain, e->b);
        compile_expr(vm, scope, chain, e->c);
        e->eval = ex_slice;
        return;
    case EX_FMT:
        compile_expr(vm, scope, chain, e->a);
        e->eval = ex_fmt;
        return;
    case EX_COMPREH: {
        /* Loop targets become locals of the enclosing function; the yielded
         * expression and the filters see them. */
        for (int i = 0; i < e->clause_count; i++) {
            DSExprNode *cl = e->clauses[i];
            if (cl->target)
                declare_target_names(scope, cl->target);
        }
        compile_expr(vm, scope, chain, e->a);
        if (e->is_dict_comprehension)
            compile_expr(vm, scope, chain, e->b);
        for (int i = 0; i < e->clause_count; i++) {
            DSExprNode *cl = e->clauses[i];
            if (cl->a)
                compile_expr(vm, scope, chain, cl->a);
            if (cl->b)
                compile_expr(vm, scope, chain, cl->b);
            if (cl->target)
                compile_target(vm, scope, chain, cl->target);
        }
        /* Remember which locals to restore once the comprehension is done. */
        e->save_count = e->clause_count;
        e->save_targets = (DSExprNode **)calloc((size_t)e->clause_count + 1u, sizeof(DSExprNode *));
        for (int i = 0; i < e->clause_count; i++)
            e->save_targets[i] = e->clauses[i]->target;
        e->save_scope = scope->is_function ? NAME_LOCAL : NAME_GLOBAL;
        e->eval = ex_compreh;
        return;
    }
    case EX_LAMBDA:
        if (e->proto)
            compile_function(vm, scope, chain, e->proto);
        e->eval = ex_lambda;
        return;
    default:
        e->eval = ex_num;
        return;
    }
}

static void compile_target(DSVM *vm, Scope *scope, UpvalChain *chain, DSExprNode *t) {
    if (!t)
        return;
    switch (t->kind) {
    case EX_NAME:
        bind_name(scope, chain, t, 1);
        t->eval = (t->scope == NAME_LOCAL) ? ex_name_local : (t->scope == NAME_UPVAL ? ex_name_upval : ex_name_global);
        return;
    case EX_DOT:
        compile_expr(vm, scope, chain, t->a);
        return;
    case EX_INDEX:
        compile_expr(vm, scope, chain, t->a);
        compile_expr(vm, scope, chain, t->b);
        return;
    case EX_LIST:
        for (int i = 0; i < t->count; i++)
            compile_target(vm, scope, chain, t->items[i]);
        return;
    default:
        compile_expr(vm, scope, chain, t);
        return;
    }
}

/* Bind every plain name in an assignment target as a local of `scope`. */
static void declare_target_names(Scope *scope, DSExprNode *t) {
    if (!t)
        return;
    if (t->kind == EX_COMPREH) {
        /* A nested comprehension inside the body still needs its loop variables
         * available to the expression that yields it. */
        for (int i = 0; i < t->clause_count; i++) {
            declare_target_names(scope, t->clauses[i]->target);
            declare_target_names(scope, t->clauses[i]->a);
            declare_target_names(scope, t->clauses[i]->b);
        }
        declare_target_names(scope, t->a);
        declare_target_names(scope, t->b);
        return;
    }
    if (t->kind == EX_NAME) {
        if (!scope->is_function)
            return; /* module-level names are plain globals */
        if (t->str && scope_find(scope, t->str) < 0 && !scope_is_global(scope, t->str))
            scope_add(scope, t->str);
        return;
    }
    if (t->kind == EX_LIST) {
        for (int i = 0; i < t->count; i++)
            declare_target_names(scope, t->items[i]);
    }
}

/* Python-style whole-function local pre-declaration. */
static void predeclare(Scope *scope, DSStmtNode *block) {
    if (!scope->is_function)
        return;
    /* `global x` must be collected first so writes elsewhere bind to the module. */
    for (DSStmtNode *s = block; s; s = s->next) {
        if (s->kind == ST_GLOBAL) {
            for (int i = 0; i < s->count; i++)
                if (s->items[i] && s->items[i]->kind == EX_NAME)
                    scope_declare_global(scope, s->items[i]->str);
        }
    }
    for (DSStmtNode *s = block; s; s = s->next) {
        if (s->targets) {
            for (int i = 0; i < s->ntargets; i++) {
                DSExprNode *t = s->targets[i];
                while (t && t->kind == EX_LIST) {
                    for (int k = 0; k < t->count; k++)
                        if (t->items[k] && t->items[k]->kind == EX_NAME)
                            scope_add(scope, t->items[k]->str);
                    t = NULL;
                }
                if (t && t->kind == EX_NAME && !scope_is_global(scope, t->str))
                    scope_add(scope, t->str);
            }
        }
        if ((s->kind == ST_DEF || s->kind == ST_CLASS) && s->name)
            scope_add(scope, s->name);
        if (s->kind == ST_IMPORT) {
            for (int i = 0; i < s->count; i++) {
                DSExprNode *nm = s->items[i];
                scope_add(scope, nm->b ? nm->b->str : nm->str);
            }
        }
        if (s->kind == ST_PASS || s->kind == ST_GLOBAL)
            continue;
        if (s->kind == ST_FOR && s->targets) {
            DSExprNode *t = s->targets[0];
            if (t && t->kind == EX_LIST) {
                for (int k = 0; k < t->count; k++)
                    if (t->items[k] && t->items[k]->kind == EX_NAME)
                        scope_add(scope, t->items[k]->str);
            }
        }
    }
}

static void compile_block(DSVM *vm, Scope *scope, UpvalChain *chain, const DSStmtNode *cblock) {
    DSStmtNode *block = (DSStmtNode *)cblock;
    predeclare(scope, block);
    for (DSStmtNode *s = block; s; s = s->next)
        compile_stmt(vm, scope, chain, s);
}

static void compile_function(DSVM *vm, Scope *outer, UpvalChain *outer_chain, DSFuncProto *fn) {
    if (!fn)
        return;
    UpvalChain fchain;
    memset(&fchain, 0, sizeof(fchain));

    Scope fscope;
    memset(&fscope, 0, sizeof(fscope));
    fscope.parent = outer;
    fscope.is_function = 1;
    fscope.chain = &fchain;
    scope_add(&fscope, NULL); /* slot 0: the closure itself */
    for (int i = 0; i < fn->arity; i++)
        scope_add(&fscope, fn->params ? fn->params[i] : NULL);

    compile_block(vm, &fscope, &fchain, fn->body);
    fn->num_upvalues = fchain.count;
    fn->upvals = fchain.descs;
    fn->num_locals = fscope.count;
    if (fn->num_locals < fn->arity + 1)
        fn->num_locals = fn->arity + 1;
    free(fscope.names);
    free(fscope.globals);
}

static void bind_target_name(Scope *scope, UpvalChain *chain, DSStmtNode *s, DSString *name) {
    if (!name) {
        s->target_scope = NAME_GLOBAL;
        return;
    }
    if (!scope->is_function) {
        s->target_scope = NAME_GLOBAL;
        return;
    }
    int idx = scope_find(scope, name);
    if (idx >= 0) {
        s->target_scope = NAME_LOCAL;
        s->target_index = idx;
        return;
    }
    if (scope_is_global(scope, name)) {
        s->target_scope = NAME_GLOBAL;
        return;
    }
    int uv = resolve_upval(scope, chain, name);
    if (uv >= 0) {
        s->target_scope = NAME_UPVAL;
        s->target_index = uv;
        return;
    }
    s->target_scope = NAME_LOCAL;
    s->target_index = scope_add(scope, name);
}

static void compile_stmt(DSVM *vm, Scope *scope, UpvalChain *chain, DSStmtNode *s) {
    switch (s->kind) {
    case ST_EXPR:
        compile_expr(vm, scope, chain, s->expr);
        s->exec = st_expr;
        return;
    case ST_ASSIGN:
        compile_expr(vm, scope, chain, s->expr);
        for (int i = 0; i < s->ntargets; i++)
            compile_target(vm, scope, chain, s->targets[i]);
        s->exec = st_assign;
        return;
    case ST_AUGASSIGN:
        compile_target(vm, scope, chain, s->targets[0]);
        compile_expr(vm, scope, chain, s->expr);
        s->exec = st_augassign;
        return;
    case ST_IF:
        for (int i = 0; i < s->nbranches; i++) {
            compile_expr(vm, scope, chain, s->conds[i]);
            compile_block(vm, scope, chain, s->bodies[i]);
        }
        compile_block(vm, scope, chain, s->orelse);
        s->exec = st_if;
        return;
    case ST_WHILE:
        compile_expr(vm, scope, chain, s->cond);
        compile_block(vm, scope, chain, s->body);
        compile_block(vm, scope, chain, s->orelse);
        s->exec = st_while;
        return;
    case ST_FOR: {
        compile_expr(vm, scope, chain, s->expr);
        compile_target(vm, scope, chain, s->targets[0]);
        compile_block(vm, scope, chain, s->body);
        compile_block(vm, scope, chain, s->orelse);
        s->exec = st_for;
        DSExprNode *it = s->expr;
        if (it && it->kind == EX_CALL && it->a && it->a->kind == EX_NAME && it->a->str &&
            it->a->str->len == 5 && memcmp(it->a->str->chars, "range", 5) == 0 && it->count >= 1 && it->count <= 3 &&
            s->targets[0] && s->targets[0]->kind == EX_NAME) {
            DSExprNode **args = (DSExprNode **)malloc(sizeof(DSExprNode *) * 3);
            if (args) {
                if (it->count == 1) {
                    args[0] = NULL;
                    args[1] = it->items[0];
                    args[2] = NULL;
                } else if (it->count == 2) {
                    args[0] = it->items[0];
                    args[1] = it->items[1];
                    args[2] = NULL;
                } else {
                    args[0] = it->items[0];
                    args[1] = it->items[1];
                    args[2] = it->items[2];
                }
                s->items = args;
                s->count = 3;
                s->exec = st_for_range;
            }
        }
        return;
    }
    case ST_BREAK: s->exec = st_break; return;
    case ST_CONTINUE: s->exec = st_continue; return;
    case ST_RETURN:
        compile_expr(vm, scope, chain, s->expr);
        s->exec = st_return;
        return;
    case ST_PASS: s->exec = st_pass; return;
    case ST_DEF:
        bind_target_name(scope, chain, s, s->name);
        compile_function(vm, scope, chain, s->proto);
        s->exec = st_def;
        return;
    case ST_CLASS:
        bind_target_name(scope, chain, s, s->name);
        compile_expr(vm, scope, chain, s->expr);
        for (DSStmtNode *m = s->body; m; m = m->next)
            if (m->kind == ST_DEF)
                compile_function(vm, scope, chain, m->proto);
        s->exec = st_class;
        return;
    case ST_IMPORT:
        for (int i = 0; i < s->count; i++)
            compile_expr(vm, scope, chain, s->items[i]->b);
        s->exec = st_import;
        return;
    case ST_DEL:
        compile_target(vm, scope, chain, s->expr);
        s->exec = st_del;
        return;
    case ST_ASSERT:
        compile_expr(vm, scope, chain, s->expr);
        s->exec = st_assert;
        return;
    case ST_GLOBAL:
        s->exec = st_global;
        return;
    default:
        s->exec = st_pass;
        return;
    }
}

/* ============================================================================
 * NUMERIC JIT LOWERING
 * Pure-numeric functions (straight-line, no calls/containers) become native
 * AArch64 / x86_64 machine code. Falls back silently to C closures.
 * ============================================================================ */

typedef struct JitBuild {
    DSJitInst *insts;
    int count;
    int cap;
    int next_reg;
    int max_reg;
    int src_reg[64]; /* node -> register map is too big; use per-node slot array */
    int failed;
} JitBuild;

static void jb_emit(JitBuild *jb, uint8_t op, uint8_t dst, uint8_t a, uint8_t b, int32_t imm, double k) {
    if (jb->failed)
        return;
    if (jb->count == jb->cap) {
        int ncap = jb->cap < 32 ? 32 : jb->cap * 2;
        DSJitInst *ni = (DSJitInst *)realloc(jb->insts, (size_t)ncap * sizeof(DSJitInst));
        if (!ni) {
            jb->failed = 1;
            return;
        }
        jb->insts = ni;
        jb->cap = ncap;
    }
    DSJitInst in;
    memset(&in, 0, sizeof(in));
    in.op = op;
    in.dst = dst;
    in.a = a;
    in.b = b;
    in.imm = imm;
    in.k = k;
    jb->insts[jb->count++] = in;
    if (dst >= jb->max_reg)
        jb->max_reg = dst + 1;
    if (a >= jb->max_reg)
        jb->max_reg = a + 1;
    if (b >= jb->max_reg)
        jb->max_reg = b + 1;
}

/* Attempt to compile an expression into the JIT program. Returns register id or -1. */
static int jit_expr(JitBuild *jb, DSExprNode *e, int depth) {
    if (!e || jb->failed || depth > 32)
        return -1;
    switch (e->kind) {
    case EX_NUM:
        if (e->aux != 0)
            return -1;
        if (jb->next_reg >= 14)
            return -1;
        {
            int r = jb->next_reg++;
            jb_emit(jb, DS_JOP_LOAD_CONST, (uint8_t)r, 0, 0, 0, e->num);
            return r;
        }
    case EX_NAME:
        /* Only locals: slot 0 is the closure, the arguments follow it. */
        if (e->scope != NAME_LOCAL || e->index <= 0 || e->index >= 64)
            return -1;
        if (jb->next_reg >= 14)
            return -1;
        {
            int r = jb->next_reg++;
            jb_emit(jb, DS_JOP_LOAD_SLOT, (uint8_t)r, 0, 0, e->index, 0.0);
            return r;
        }
    case EX_BINOP: {
        int a = jit_expr(jb, e->a, depth + 1);
        int b = jit_expr(jb, e->b, depth + 1);
        if (a < 0 || b < 0)
            return -1;
        if (jb->next_reg >= 14)
            return -1;
        int r = jb->next_reg++;
        uint8_t op;
        switch (e->op) {
        case OP_ADD: op = DS_JOP_ADD; break;
        case OP_SUB: op = DS_JOP_SUB; break;
        case OP_MUL: op = DS_JOP_MUL; break;
        case OP_DIV: op = DS_JOP_DIV; break;
        case OP_LT: op = DS_JOP_CMP_LT; break;
        case OP_LE: op = DS_JOP_CMP_LE; break;
        case OP_GT: op = DS_JOP_CMP_GT; break;
        case OP_GE: op = DS_JOP_CMP_GE; break;
        case OP_EQ: op = DS_JOP_CMP_EQ; break;
        case OP_NE: op = DS_JOP_CMP_NE; break;
        case OP_MOD: op = DS_JOP_MOD; break;
        default: return -1;
        }
        jb_emit(jb, op, (uint8_t)r, (uint8_t)a, (uint8_t)b, 0, 0.0);
        return r;
    }
    case EX_UNOP:
        if (e->op != OP_SUB)
            return -1;
        {
            int a = jit_expr(jb, e->a, depth + 1);
            if (a < 0)
                return -1;
            if (jb->next_reg >= 14)
                return -1;
            int r = jb->next_reg++;
            jb_emit(jb, DS_JOP_NEG, (uint8_t)r, (uint8_t)a, 0, 0, 0.0);
            return r;
        }
    case EX_TERNARY: {
        /* cond ? a : b  -> SELECT */
        int cond = jit_expr(jb, e->b, depth + 1);
        int a = jit_expr(jb, e->a, depth + 1);
        int b = e->c ? jit_expr(jb, e->c, depth + 1) : -1;
        if (cond < 0 || a < 0 || b < 0)
            return -1;
        if (jb->next_reg >= 14)
            return -1;
        int r = jb->next_reg++;
        jb_emit(jb, DS_JOP_SELECT, (uint8_t)r, (uint8_t)a, (uint8_t)b, cond, 0.0);
        return r;
    }
    default:
        return -1;
    }
}

/* ----------------------------------------------------------------------------
 * Statement lowering
 *
 * The IR is a flat list with instruction-index branches, so structured control
 * flow maps onto it directly:
 *
 *   if c: A else: B       c; JZ else; A; JMP end; else: B; end:
 *   while c: A            top: c; JZ end; A; JMP top; end:
 *   for i in range(n)     i=0; top: cmp; JZ end; A; i+=1; JMP top; end:
 *
 * Only pure numeric code is accepted. Anything the lowering does not fully
 * understand returns -1 and the whole function stays interpreted, so compiled
 * code can never disagree with the VM. jit_diff_test.py checks that promise.
 * ------------------------------------------------------------------------- */
typedef struct JitCtl {
    int break_patch[16];  /* instruction indices of pending `break` jumps */
    int nbreak;
    int continue_patch[16];
    int ncont;
} JitCtl;

typedef struct JitScope {
    JitBuild *jb;
    JitCtl ctl;    /* the innermost loop, for break/continue */
    int reg_floor; /* lowest register a statement may clobber */
} JitScope;

static int jit_stmt(JitScope *js, DSStmtNode *s);

/* A value slot is a compiler-assigned local; arguments occupy 1..arity. */
static int jit_slot_of(DSExprNode *e) {
    if (!e || e->kind != EX_NAME || e->scope != NAME_LOCAL)
        return -1;
    if (e->index <= 0 || e->index > 4095)
        return -1;
    return e->index;
}

/* Lowers `target = expr` for a local slot. 1 = done, 0 = not a local store. */
static int jit_assign_local(JitScope *js, DSStmtNode *s) {
    if (!s || s->kind != ST_ASSIGN || s->ntargets != 1)
        return 0;
    int slot = jit_slot_of(s->targets[0]);
    if (slot < 0)
        return 0;
    int r = jit_expr(js->jb, s->expr, 0);
    if (r < 0)
        return 0;
    jb_emit(js->jb, DS_JOP_STORE_SLOT, 0, (uint8_t)r, 0, slot, 0.0);
    return js->jb->failed ? -1 : 1;
}

static int jit_cond(JitScope *js, DSExprNode *cond, int invert) {
    if (!cond)
        return -1;
    int r = jit_expr(js->jb, cond, 0);
    if (r < 0)
        return -1;
    jb_emit(js->jb, invert ? DS_JOP_JMP_IF_NOT_ZERO : DS_JOP_JMP_IF_ZERO, (uint8_t)r, 0, 0, 0, 0.0);
    return js->jb->count - 1;
}

static int jit_block_into(JitScope *js, DSStmtNode *block) {
    for (DSStmtNode *s = block; s; s = s->next) {
        if (jit_stmt(js, s) < 0 || js->jb->failed)
            return -1;
    }
    return 0;
}

static int jit_stmt(JitScope *js, DSStmtNode *s) {
    if (!s || js->jb->failed)
        return -1;
    /* Values never outlive one statement (everything is stored back to a slot),
     * so the register allocator rewinds here. That is what lets a long body fit
     * in the 14 available registers. */
    if (s->kind == ST_ASSIGN || s->kind == ST_AUGASSIGN || s->kind == ST_EXPR ||
        s->kind == ST_RETURN || s->kind == ST_IF)
        js->jb->next_reg = js->reg_floor;

    switch (s->kind) {
    case ST_PASS:
        return 0;
    case ST_EXPR:
        /* A bare expression statement only matters in the REPL, but a call
         * could have side effects, so reject those. */
        if (!s->expr)
            return 0;
        if (s->expr->kind == EX_CALL || s->expr->kind == EX_NAME)
            return -1;
        jb_emit(js->jb, DS_JOP_NOP, 0, 0, 0, 0, 0.0);
        return 0;
    case ST_ASSIGN: {
        int r = jit_assign_local(js, s);
        return r > 0 ? 0 : -1;
    }
    case ST_AUGASSIGN: {
        if (s->ntargets != 1 || js->jb->next_reg >= 14)
            return -1;
        int slot = jit_slot_of(s->targets[0]);
        if (slot < 0)
            return -1;
        int lhs = js->jb->next_reg++;
        jb_emit(js->jb, DS_JOP_LOAD_SLOT, (uint8_t)lhs, 0, 0, slot, 0.0);
        int rhs = jit_expr(js->jb, s->expr, 0);
        if (rhs < 0 || js->jb->next_reg >= 14)
            return -1;
        int r = js->jb->next_reg++;
        uint8_t op;
        switch (s->op) {
        case OP_ADD: op = DS_JOP_ADD; break;
        case OP_SUB: op = DS_JOP_SUB; break;
        case OP_MUL: op = DS_JOP_MUL; break;
        case OP_DIV: op = DS_JOP_DIV; break;
        default: return -1;
        }
        jb_emit(js->jb, op, (uint8_t)r, (uint8_t)lhs, (uint8_t)rhs, 0, 0.0);
        jb_emit(js->jb, DS_JOP_STORE_SLOT, 0, (uint8_t)r, 0, slot, 0.0);
        return 0;
    }
    case ST_IF: {
        if (s->nbranches <= 0)
            return -1;
        /* Chained elifs become a cascade: each arm jumps past the rest. */
        int end_jumps[16];
        int nend = 0;
        for (int i = 0; i < s->nbranches; i++) {
            int jz = jit_cond(js, s->conds[i], 0);
            if (jz < 0)
                return -1;
            if (jit_block_into(js, s->bodies[i]) < 0)
                return -1;
            if (i + 1 < s->nbranches || s->orelse) {
                jb_emit(js->jb, DS_JOP_JMP, 0, 0, 0, 0, 0.0);
                if (nend >= 16)
                    return -1;
                end_jumps[nend++] = js->jb->count - 1;
            }
            js->jb->insts[jz].imm = js->jb->count; /* false -> next arm */
        }
        if (s->orelse && jit_block_into(js, s->orelse) < 0)
            return -1;
        for (int i = 0; i < nend; i++)
            js->jb->insts[end_jumps[i]].imm = js->jb->count;
        return 0;
    }
    case ST_WHILE: {
        int top = js->jb->count;
        int jz = jit_cond(js, s->cond, 0);
        if (jz < 0)
            return -1;
        JitCtl saved = js->ctl;
        js->ctl.nbreak = 0;
        js->ctl.ncont = 0;
        if (jit_block_into(js, s->body) < 0) {
            js->ctl = saved;
            return -1;
        }
        for (int i = 0; i < js->ctl.ncont; i++) /* continue -> re-test */
            js->jb->insts[js->ctl.continue_patch[i]].imm = top;
        jb_emit(js->jb, DS_JOP_JMP, 0, 0, 0, top, 0.0);
        int after = js->jb->count;
        js->jb->insts[jz].imm = after;
        for (int i = 0; i < js->ctl.nbreak; i++)
            js->jb->insts[js->ctl.break_patch[i]].imm = after;
        js->ctl = saved;
        if (s->orelse && jit_block_into(js, s->orelse) < 0)
            return -1;
        return 0;
    }
    case ST_FOR: {
        /* Only `for i in range(a, b)` with a numeric local target; explicit
         * steps and general iterables stay interpreted. */
        if (!s->targets || s->ntargets != 1)
            return -1;
        int slot = jit_slot_of(s->targets[0]);
        if (slot < 0)
            return -1;
        if (!s->items || s->count != 3 || !s->items[1] || s->items[2])
            return -1;
        if (s->items[0]) {
            int r = jit_expr(js->jb, s->items[0], 0);
            if (r < 0)
                return -1;
            jb_emit(js->jb, DS_JOP_STORE_SLOT, 0, (uint8_t)r, 0, slot, 0.0);
        } else {
            jb_emit(js->jb, DS_JOP_MOV_CONST_SLOT, 0, 0, 0, slot, 0.0);
        }
        int stop = jit_expr(js->jb, s->items[1], 0);
        if (stop < 0 || js->jb->next_reg >= 14)
            return -1;
        int saved_floor = js->reg_floor;
        if (stop >= js->reg_floor)
            js->reg_floor = stop + 1; /* `stop` stays live for the whole loop */
        int top = js->jb->count;
        int iv = js->jb->next_reg++;
        if (iv >= 14)
            return -1;
        jb_emit(js->jb, DS_JOP_LOAD_SLOT, (uint8_t)iv, 0, 0, slot, 0.0);
        int c = js->jb->next_reg++;
        if (c >= 14)
            return -1;
        jb_emit(js->jb, DS_JOP_CMP_LT, (uint8_t)c, (uint8_t)iv, (uint8_t)stop, 0, 0.0);
        jb_emit(js->jb, DS_JOP_JMP_IF_ZERO, (uint8_t)c, 0, 0, 0, 0.0);
        int jz = js->jb->count - 1;
        JitCtl saved = js->ctl;
        js->ctl.nbreak = 0;
        js->ctl.ncont = 0;
        if (jit_block_into(js, s->body) < 0) {
            js->ctl = saved;
            js->reg_floor = saved_floor;
            return -1;
        }
        int inc_begin = js->jb->count;
        for (int i = 0; i < js->ctl.ncont; i++) /* continue -> increment */
            js->jb->insts[js->ctl.continue_patch[i]].imm = inc_begin;
        int cur = js->jb->next_reg++;
        int one = js->jb->next_reg++;
        int sum = js->jb->next_reg++;
        if (cur >= 14 || one >= 14 || sum >= 14) {
            js->ctl = saved;
            js->reg_floor = saved_floor;
            return -1;
        }
        jb_emit(js->jb, DS_JOP_LOAD_SLOT, (uint8_t)cur, 0, 0, slot, 0.0);
        jb_emit(js->jb, DS_JOP_LOAD_CONST, (uint8_t)one, 0, 0, 0, 1.0);
        jb_emit(js->jb, DS_JOP_ADD, (uint8_t)sum, (uint8_t)cur, (uint8_t)one, 0, 0.0);
        jb_emit(js->jb, DS_JOP_STORE_SLOT, 0, (uint8_t)sum, 0, slot, 0.0);
        jb_emit(js->jb, DS_JOP_JMP, 0, 0, 0, top, 0.0);
        int after = js->jb->count;
        js->jb->insts[jz].imm = after;
        for (int i = 0; i < js->ctl.nbreak; i++)
            js->jb->insts[js->ctl.break_patch[i]].imm = after;
        js->ctl = saved;
        js->reg_floor = saved_floor;
        if (s->orelse && jit_block_into(js, s->orelse) < 0)
            return -1;
        return 0;
    }
    case ST_BREAK:
        if (js->ctl.nbreak >= 16)
            return -1;
        jb_emit(js->jb, DS_JOP_JMP, 0, 0, 0, 0, 0.0);
        js->ctl.break_patch[js->ctl.nbreak++] = js->jb->count - 1;
        return 0;
    case ST_CONTINUE:
        if (js->ctl.ncont >= 16)
            return -1;
        jb_emit(js->jb, DS_JOP_JMP, 0, 0, 0, 0, 0.0);
        js->ctl.continue_patch[js->ctl.ncont++] = js->jb->count - 1;
        return 0;
    case ST_RETURN: {
        if (!s->expr)
            return -1; /* a bare return yields nil, which the IR cannot express */
        int r = jit_expr(js->jb, s->expr, 0);
        if (r < 0)
            return -1;
        jb_emit(js->jb, DS_JOP_RET, 0, (uint8_t)r, 0, 0, 0.0);
        return 0;
    }
    default:
        return -1; /* calls, defs, imports, classes: leave those to the VM */
    }
}

/* Compiles a whole numeric function body, or returns NULL if any part of it is
 * outside the supported subset. */
static DSJitFunc *jit_try_compile(DSFuncProto *fn) {
    if (!fn || !ds_jit_supported() || !fn->body)
        return NULL;
    if (fn->has_varargs || fn->arity < 1 || fn->arity > 8)
        return NULL;
    if (fn->num_locals > 64)
        return NULL;

    JitBuild jb;
    memset(&jb, 0, sizeof(jb));
    JitScope js;
    memset(&js, 0, sizeof(js));
    js.jb = &jb;
    js.reg_floor = 0;

    if (jit_block_into(&js, (DSStmtNode *)fn->body) < 0 || jb.failed) {
        if (getenv("DS_JIT_DEBUG"))
            fprintf(stderr, "[jit] %s: lowering rejected (count=%d failed=%d)\n",
                    fn->name && fn->name->chars ? fn->name->chars : "?", jb.count, jb.failed);
        free(jb.insts);
        return NULL;
    }
    /* Falling off the end yields nil, which the IR cannot return. A NaN
     * sentinel tells the call site to re-run the function interpreted; since a
     * JIT-eligible body has no side effects, that is observationally identical. */
    if (jb.next_reg < 14) {
        int r = jb.next_reg++;
        uint64_t nan_bits = 0x7FF8000000000000ULL;
        double nan_val;
        memcpy(&nan_val, &nan_bits, sizeof(nan_val));
        jb_emit(&jb, DS_JOP_LOAD_CONST, (uint8_t)r, 0, 0, 0, nan_val);
        jb_emit(&jb, DS_JOP_RET, 0, (uint8_t)r, 0, 0, 0.0);
    }
    if (jb.failed) {
        free(jb.insts);
        return NULL;
    }
    int nregs = jb.max_reg;
    if (nregs < 1)
        nregs = 1;
    if (nregs > 14)
        nregs = 14;
    DSJitFunc *jit = ds_jit_compile(jb.insts, jb.count, nregs);
    if (!jit && getenv("DS_JIT_DEBUG"))
        fprintf(stderr, "[jit] %s: emitter rejected (%d insts)\n",
                fn->name && fn->name->chars ? fn->name->chars : "?", jb.count);
    free(jb.insts);
    return jit;
}

static void jit_walk_protos(DSVM *vm, const DSStmtNode *block);

static void jit_consider(DSVM *vm, DSFuncProto *fn) {
    if (!fn || !vm->jit_enabled)
        return;
    if (fn->jit_fn)
        return;
    DSJitFunc *j = jit_try_compile(fn);
    if (j) {
        fn->jit_fn = j;
        vm->jit_fn_count++;
    }
}

static void jit_walk_protos(DSVM *vm, const DSStmtNode *cblock) {
    DSStmtNode *block = (DSStmtNode *)cblock;
    for (DSStmtNode *s = block; s; s = s->next) {
        if (s->kind == ST_DEF && s->proto) {
            jit_consider(vm, s->proto);
            jit_walk_protos(vm, s->proto->body);
        }
        if (s->kind == ST_CLASS) {
            for (DSStmtNode *m = s->body; m; m = m->next) {
                if (m->kind == ST_DEF && m->proto) {
                    jit_consider(vm, m->proto);
                    jit_walk_protos(vm, m->proto->body);
                }
            }
        }
        if (s->body)
            jit_walk_protos(vm, s->body);
        if (s->orelse)
            jit_walk_protos(vm, s->orelse);
        for (int i = 0; i < s->nbranches; i++)
            jit_walk_protos(vm, s->bodies[i]);
    }
}

/* ============================================================================
 * MODULE COMPILATION ENTRY POINTS
 * ============================================================================ */

DSFuncProto *ds_compile_module(DSVM *vm, DSModule *mod, const char *filename, const char *src, size_t len, int is_repl) {
    uint64_t t0 = ds_monotonic_ns();
    DSCompiledArena *arena = ds_arena_create();
    if (!arena)
        return NULL;

    char err[512];
    int err_line = 0;
    err[0] = '\0';
    DSStmtNode *program = ds_parse_program(vm, arena, filename, src, len, is_repl, err, sizeof(err), &err_line);
    if (!program) {
        snprintf(vm->err_msg, sizeof(vm->err_msg), "%s", err[0] ? err : "ошибка разбора");
        snprintf(vm->err_file, sizeof(vm->err_file), "%s", filename ? filename : "?");
        vm->err_line = err_line;
        vm->has_error = 1;
        ds_arena_release(arena);
        return NULL;
    }

    DSFuncProto *proto = ds_proto_new(vm, ds_string_const(vm, "<module>"), ds_string_const(vm, filename ? filename : "?"),
                                      mod, arena);
    if (!proto) {
        ds_arena_release(arena);
        return NULL;
    }
    proto->def_line = 1;

    Scope gscope;
    memset(&gscope, 0, sizeof(gscope));
    gscope.parent = NULL;
    gscope.is_function = 0; /* top-level assignments are globals */
    /* Keep slot 0 free for the module closure itself, mirroring function scopes,
     * so comprehension loop variables can use the same slot numbering. */
    scope_add(&gscope, NULL);

    UpvalChain up;
    memset(&up, 0, sizeof(up));
    compile_block(vm, &gscope, &up, program);
    proto->num_locals = gscope.count;
    free(gscope.names);
    free(up.descs);

    proto->body = program;
    proto->module = mod;

    jit_walk_protos(vm, program);

    /* Mark the trailing expression for REPL echo */
    if (is_repl) {
        DSStmtNode *last = program;
        while (last && last->next)
            last = last->next;
        if (last && last->kind == ST_EXPR)
            last->flags |= 2;
    }

    vm->last_compile_ms = (double)(ds_monotonic_ns() - t0) * 1e-3;
    return proto;
}
