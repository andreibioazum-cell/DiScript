/* Lua 5.4-grade Tri-Color Incremental & Generational Garbage Collector
 * with 64KB Segregated mmap Slabs, SSO Interned Strings, and Write Barriers. */

/* mmap's MAP_ANONYMOUS is not visible under strict -std=c99 without this. */
#ifndef _WIN32
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#endif

#include "discript/ds_internal.h"
#include "engine/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
/* The collector allocates whole page-aligned slabs. Windows has no mmap(), so
 * map the three calls it needs onto VirtualAlloc/VirtualFree, which give the
 * same guarantees: zeroed, page-aligned, private memory. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define PROT_READ 0x1
#define PROT_WRITE 0x2
#define MAP_PRIVATE 0x02
#define MAP_ANONYMOUS 0x20
#define MAP_FAILED ((void *)-1)
static void *ds_mmap_win(size_t size) {
    void *p = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    return p ? p : MAP_FAILED;
}
#define mmap(addr, size, prot, flags, fd, off) ds_mmap_win(size)
#define munmap(addr, size) (VirtualFree((addr), 0, MEM_RELEASE) != 0 ? 0 : -1)
#else
#include <sys/mman.h>
#endif

#define SLAB_PAGE_BYTES 65536u
#define GC_INITIAL_MINOR (64u * 1024u)
#define GC_INITIAL_MAJOR (256u * 1024u)

struct DSGCSlab {
    DSGCSlab *next;
    DSGCSlab *prev;
    void *free_list;
    uint32_t cell_size;
    uint32_t capacity;
    uint32_t live_count;
    uint32_t class_id;
};

typedef struct SlabCell {
    struct SlabCell *next;
} SlabCell;

/* Locate enclosing 64KB slab by aligning pointer down? No — OS mmap is page-aligned (4KB/16KB),
 * not 64KB-aligned! Instead store a tiny 8-byte DSGCSlab* prefix right before each cell! */
typedef struct CellPrefix {
    DSGCSlab *slab;
    uint64_t _pad; /* keeps payload 16-byte aligned! */
} CellPrefix;

static DSGCSlab *slab_create(uint32_t payload_size, uint32_t class_id) {
    void *mem = mmap(NULL, SLAB_PAGE_BYTES, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED)
        return NULL;
    DSGCSlab *s = (DSGCSlab *)mem;
    memset(s, 0, sizeof(*s));
    uint32_t stride = (uint32_t)sizeof(CellPrefix) + payload_size;
    stride = (stride + 15u) & ~15u;
    s->cell_size = payload_size;
    s->class_id = class_id;

    size_t hdr = (sizeof(DSGCSlab) + 15u) & ~15u;
    uint8_t *cur = (uint8_t *)mem + hdr;
    uint8_t *end = (uint8_t *)mem + SLAB_PAGE_BYTES;
    void *head = NULL;
    uint32_t cap = 0;
    while (cur + stride <= end) {
        CellPrefix *pfx = (CellPrefix *)cur;
        pfx->slab = s;
        SlabCell *cell = (SlabCell *)(cur + sizeof(CellPrefix));
        cell->next = (SlabCell *)head;
        head = cell;
        cur += stride;
        cap++;
    }
    s->free_list = head;
    s->capacity = cap;
    return s;
}

static void *slab_alloc(DSGCState *gc, size_t struct_size, uint8_t *out_class) {
    uint32_t cls = (struct_size <= 64) ? 0u : ((struct_size <= 144) ? 1u : 2u);
    uint32_t cell_bytes = (cls == 0) ? 64u : 144u;
    if (cls == 2) {
        size_t map_sz = (struct_size + sizeof(CellPrefix) + 4095u) & ~4095u;
        void *mem = mmap(NULL, map_sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mem == MAP_FAILED)
            return NULL;
        CellPrefix *pfx = (CellPrefix *)mem;
        pfx->slab = (DSGCSlab *)(uintptr_t)map_sz; /* stores byte length */
        *out_class = 2;
        return (uint8_t *)mem + sizeof(CellPrefix);
    }

    /* Find a slab of matching class with a free cell */
    DSGCSlab *target = NULL;
    for (DSGCSlab *s = gc->slabs; s; s = s->next) {
        if (s->class_id == cls && s->free_list != NULL) {
            target = s;
            break;
        }
    }
    if (!target) {
        target = slab_create(cell_bytes, cls);
        if (!target)
            return NULL;
        target->next = gc->slabs;
        if (gc->slabs)
            gc->slabs->prev = target;
        gc->slabs = target;
    }
    SlabCell *cell = (SlabCell *)target->free_list;
    target->free_list = cell->next;
    target->live_count++;
    *out_class = (uint8_t)cls;
    return cell;
}

static void slab_free(DSGCState *gc, void *obj_ptr, uint8_t slab_class) {
    if (!obj_ptr)
        return;
    CellPrefix *pfx = (CellPrefix *)((uint8_t *)obj_ptr - sizeof(CellPrefix));
    if (slab_class == 2) {
        size_t map_sz = (size_t)(uintptr_t)pfx->slab;
        munmap(pfx, map_sz);
        return;
    }
    DSGCSlab *s = pfx->slab;
    SlabCell *cell = (SlabCell *)obj_ptr;
    cell->next = (SlabCell *)s->free_list;
    s->free_list = cell;
    if (s->live_count > 0)
        s->live_count--;

    /* Return empty 64KB slab straight to OS RAM if another slab of same class exists! */
    if (s->live_count == 0) {
        int others = 0;
        for (DSGCSlab *it = gc->slabs; it; it = it->next) {
            if (it != s && it->class_id == s->class_id) {
                others = 1;
                break;
            }
        }
        if (others) {
            if (s->prev)
                s->prev->next = s->next;
            else
                gc->slabs = s->next;
            if (s->next)
                s->next->prev = s->prev;
            munmap(s, SLAB_PAGE_BYTES);
        }
    }
}

uint32_t ds_hash_bytes(const char *s, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint8_t)s[i];
        h *= 16777619u;
    }
    return h ? h : 1u;
}

static void gc_account_add(DSGCState *gc, size_t n) {
    gc->bytes_allocated += n;
    if (gc->bytes_allocated > gc->peak_bytes)
        gc->peak_bytes = gc->bytes_allocated;
}

static void gc_account_sub(DSGCState *gc, size_t n) {
    if (gc->bytes_allocated >= n)
        gc->bytes_allocated -= n;
    else
        gc->bytes_allocated = 0;
}

void ds_gc_init(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    memset(gc, 0, sizeof(*gc));
    gc->current_white = GC_WHITE0;
    gc->mode = GC_MODE_GENERATIONAL;
    gc->phase = GC_PHASE_IDLE;
    gc->next_minor_threshold = GC_INITIAL_MINOR;
    gc->next_major_threshold = GC_INITIAL_MAJOR;
    gc->pause_pct = 150;
    gc->stepmul_pct = 200;

    gc->intern_cap = 512;
    gc->intern_buckets = (DSString **)calloc((size_t)gc->intern_cap, sizeof(DSString *));
}

void *ds_gc_alloc_obj(DSVM *vm, size_t struct_size, DSObjType tt, size_t extra_bytes) {
    DSGCState *gc = &vm->gc;
    if (!gc->paused && gc->bytes_allocated > gc->next_minor_threshold) {
        ds_gc_check(vm);
    }
    uint8_t cls = 0;
    DSGCObject *obj = (DSGCObject *)slab_alloc(gc, struct_size, &cls);
    if (!obj) {
        ds_error(vm, 0, "нехватка памяти GC");
        return NULL;
    }
    memset(obj, 0, struct_size);
    obj->tt = (uint8_t)tt;
    /* Objects allocated during active incremental MARK are born BLACK so they cannot be swept! */
    obj->color = (gc->phase == GC_PHASE_MARK) ? GC_BLACK : gc->current_white;
    obj->age = GC_AGE_NEW;
    obj->slab_class = cls;
    obj->extra_bytes = (uint32_t)extra_bytes;

    obj->gc_next = gc->young_head;
    gc->young_head = obj;

    size_t cost = (cls == 0 ? 64u : (cls == 1 ? 144u : struct_size)) + extra_bytes;
    gc_account_add(gc, cost);
    gc->object_count++;
    return obj;
}

void *ds_gc_realloc_buf(DSVM *vm, DSGCObject *owner, void *old_ptr, size_t old_bytes, size_t new_bytes) {
    DSGCState *gc = &vm->gc;
    if (new_bytes == 0) {
        free(old_ptr);
        gc_account_sub(gc, old_bytes);
        if (owner && owner->extra_bytes >= old_bytes)
            owner->extra_bytes -= (uint32_t)old_bytes;
        return NULL;
    }
    void *np = realloc(old_ptr, new_bytes);
    if (!np)
        return NULL;
    if (new_bytes > old_bytes) {
        size_t diff = new_bytes - old_bytes;
        gc_account_add(gc, diff);
        if (owner)
            owner->extra_bytes += (uint32_t)diff;
    } else {
        size_t diff = old_bytes - new_bytes;
        gc_account_sub(gc, diff);
        if (owner && owner->extra_bytes >= diff)
            owner->extra_bytes -= (uint32_t)diff;
    }
    return np;
}

void ds_gc_free_buf(DSVM *vm, DSGCObject *owner, void *ptr, size_t bytes) {
    if (!ptr)
        return;
    free(ptr);
    gc_account_sub(&vm->gc, bytes);
    if (owner && owner->extra_bytes >= bytes)
        owner->extra_bytes -= (uint32_t)bytes;
}

/* ---- Tri-Color & Generational Write Barriers ---------------------------- */
/* True when `o` carries the white that this cycle is collecting. Objects that
 * just survived an atomic phase are repainted with the *next* white, so an
 * `is_white()` test must always name the collected white explicitly. */
static inline int is_dead(const DSGCState *gc, const DSGCObject *o, uint8_t collect_white) {
    (void)gc;
    return o->color == collect_white;
}

static inline int is_young(const DSGCObject *o) {
    return o->age == GC_AGE_NEW || o->age == GC_AGE_SURVIVAL;
}

/* How many direct children of `o` are still young. A minor collection only has
 * to re-walk containers that can reach the nursery; without this test every
 * minor pause re-traced the whole module singleton table and every remembered
 * list from end to end, which made collecting a growing list quadratic. */
static int young_children_of(const DSGCObject *o) {
    if (!o)
        return 0;
    switch (o->tt) {
    case DS_OBJ_LIST: {
        const DSList *l = (const DSList *)o;
        int n = 0;
        for (int i = 0; i < l->len; i++)
            if (ds_is_obj(l->items[i]) && is_young(ds_as_obj(l->items[i])))
                n++;
        return n;
    }
    case DS_OBJ_DICT: {
        const DSDict *d = (const DSDict *)o;
        int n = 0;
        for (int i = 0; i < d->cap; i++) {
            if (d->entries[i].state != 1)
                continue;
            if (ds_is_obj(d->entries[i].key) && is_young(ds_as_obj(d->entries[i].key)))
                n++;
            if (ds_is_obj(d->entries[i].val) && is_young(ds_as_obj(d->entries[i].val)))
                n++;
        }
        return n;
    }
    case DS_OBJ_MODULE: {
        const DSModule *m = (const DSModule *)o;
        int n = 0;
        for (int i = 0; i < m->global_count; i++)
            if (ds_is_obj(m->global_slots[i]) && is_young(ds_as_obj(m->global_slots[i])))
                n++;
        if (m->dict && is_young((DSGCObject *)m->dict))
            n++;
        return n;
    }
    default:
        return 1; /* unknown shape: assume it can still reach the nursery */
    }
}

static void mark_obj(DSGCState *gc, DSGCObject *o, int minor_only) {
    if (!o)
        return;
    if (minor_only && !is_young(o))
        return;
    if (!is_dead(gc, o, gc->collect_white))
        return;
    /* Strings and CFuncs have no outgoing references: color straight to BLACK! */
    if (o->tt == DS_OBJ_STRING || o->tt == DS_OBJ_CFUNC) {
        o->color = GC_BLACK;
        return;
    }
    o->color = GC_GRAY;
    o->gray_next = gc->gray_head;
    gc->gray_head = o;
}

static inline void mark_val(DSGCState *gc, DSValue v, int minor_only) {
    if (ds_is_obj(v))
        mark_obj(gc, ds_as_obj(v), minor_only);
}

/* ---- Remembered set ------------------------------------------------------
 * A container that lives in the old generation but points into the nursery is
 * queued here so every minor collection traces it. `GC_AGE_TOUCHED` doubles as
 * the "already queued" flag, which keeps insertion O(1).
 *
 * Every removal has to clear BOTH the flag and `touch_next`: leaving a stale
 * `touch_next` behind lets a later push splice an old chain into the list,
 * which shows up as an infinite loop (a cycle) or as a walk over freed slab
 * memory. All unlinks therefore go through the helpers below. */
static void gc_remember(DSGCState *gc, DSGCObject *parent) {
    parent->age = GC_AGE_TOUCHED;
    parent->touch_next = gc->touched_head;
    gc->touched_head = parent;
}

/* Unlink one member: O(n), used only when a queued object is about to be
 * freed or has aged out of the nursery. */
static void gc_forget(DSGCState *gc, DSGCObject *o) {
    DSGCObject **link = &gc->touched_head;
    while (*link) {
        if (*link == o) {
            *link = o->touch_next;
            break;
        }
        link = &(*link)->touch_next;
    }
    o->touch_next = NULL;
    if (o->age == GC_AGE_TOUCHED)
        o->age = GC_AGE_OLD;
}

/* Drop the whole set and clear every flag, so the next cycle starts clean. */
static void gc_forget_all(DSGCState *gc) {
    DSGCObject *t = gc->touched_head;
    gc->touched_head = NULL;
    while (t) {
        DSGCObject *next = t->touch_next;
        t->touch_next = NULL;
        if (t->age == GC_AGE_TOUCHED)
            t->age = GC_AGE_OLD;
        t = next;
    }
}

void ds_gc_write_barrier(DSVM *vm, DSGCObject *parent, DSValue child) {
    if (!parent || !ds_is_obj(child))
        return;
    DSGCObject *co = ds_as_obj(child);
    DSGCState *gc = &vm->gc;
    if (gc->phase == GC_PHASE_MARK && parent->color == GC_BLACK && is_dead(gc, co, gc->collect_white)) {
        mark_obj(gc, co, 0);
    }
    /* Remembered-set barrier: a non-young container that starts pointing at a
     * young object must be re-traced by *every* minor collection until the next
     * major cycle clears the set. GC_AGE_TOUCHED doubles as the "queued" flag. */
    if (!is_young(parent) && is_young(co) && parent->age != GC_AGE_TOUCHED)
        gc_remember(gc, parent);
}

void ds_gc_write_barrier_back(DSVM *vm, DSGCObject *container, DSValue stored) {
    if (!container || !ds_is_obj(stored))
        return;
    DSGCObject *so = ds_as_obj(stored);
    DSGCState *gc = &vm->gc;
    /* Lua backward barrier: turn BLACK container back to GRAY_AGAIN in O(1) */
    if (gc->phase == GC_PHASE_MARK && container->color == GC_BLACK && is_dead(gc, so, gc->collect_white)) {
        container->color = GC_GRAY;
        container->gray_next = gc->gray_again_head;
        gc->gray_again_head = container;
    }
    /* Lua generational remembered-set barrier (kept across minor cycles) */
    if (!is_young(container) && is_young(so) && container->age != GC_AGE_TOUCHED)
        gc_remember(gc, container);
}

static size_t trace_blacken(DSGCState *gc, DSGCObject *o, int minor_only) {
    o->color = GC_BLACK;
    switch (o->tt) {
    case DS_OBJ_LIST: {
        DSList *l = (DSList *)o;
        for (int i = 0; i < l->len; i++)
            mark_val(gc, l->items[i], minor_only);
        return sizeof(DSList) + (size_t)l->cap * sizeof(DSValue);
    }
    case DS_OBJ_DICT: {
        DSDict *d = (DSDict *)o;
        for (int i = 0; i < d->cap; i++) {
            if (d->entries[i].state == 1) {
                mark_val(gc, d->entries[i].key, minor_only);
                mark_val(gc, d->entries[i].val, minor_only);
            }
        }
        return sizeof(DSDict) + (size_t)d->cap * sizeof(DSDictEntry);
    }
    case DS_OBJ_UPVALUE: {
        DSUpvalue *uv = (DSUpvalue *)o;
        mark_val(gc, uv->closed, minor_only);
        return sizeof(DSUpvalue);
    }
    case DS_OBJ_FUNC: {
        DSFuncProto *fn = (DSFuncProto *)o;
        mark_obj(gc, (DSGCObject *)fn->name, minor_only);
        mark_obj(gc, (DSGCObject *)fn->filename, minor_only);
        mark_obj(gc, (DSGCObject *)fn->module, minor_only);
        for (int i = 0; i < (fn->arity - fn->min_arity); i++)
            mark_val(gc, fn->defaults[i], minor_only);
        return sizeof(DSFuncProto);
    }
    case DS_OBJ_CLOSURE: {
        DSClosure *cl = (DSClosure *)o;
        mark_obj(gc, (DSGCObject *)cl->proto, minor_only);
        for (int i = 0; i < cl->upval_count; i++)
            mark_obj(gc, (DSGCObject *)cl->upvalues[i], minor_only);
        return sizeof(DSClosure) + (size_t)cl->upval_count * sizeof(DSUpvalue *);
    }
    case DS_OBJ_BOUND: {
        DSBound *b = (DSBound *)o;
        mark_val(gc, b->recv, minor_only);
        mark_val(gc, b->fn, minor_only);
        return sizeof(DSBound);
    }
    case DS_OBJ_CLASS: {
        DSClass *kl = (DSClass *)o;
        mark_obj(gc, (DSGCObject *)kl->name, minor_only);
        mark_obj(gc, (DSGCObject *)kl->superclass, minor_only);
        mark_obj(gc, (DSGCObject *)kl->methods, minor_only);
        mark_val(gc, kl->init_fn, minor_only);
        return sizeof(DSClass);
    }
    case DS_OBJ_INSTANCE: {
        DSInstance *in = (DSInstance *)o;
        mark_obj(gc, (DSGCObject *)in->klass, minor_only);
        mark_obj(gc, (DSGCObject *)in->fields, minor_only);
        return sizeof(DSInstance);
    }
    case DS_OBJ_NATIVE: {
        DSNativeObj *no = (DSNativeObj *)o;
        mark_obj(gc, (DSGCObject *)no->methods, minor_only);
        return sizeof(DSNativeObj) + no->mem_cost;
    }
    case DS_OBJ_MODULE: {
        DSModule *m = (DSModule *)o;
        mark_obj(gc, (DSGCObject *)m->name, minor_only);
        mark_obj(gc, (DSGCObject *)m->dict, minor_only);
        for (int i = 0; i < m->global_count; i++) {
            mark_val(gc, m->global_slots[i], minor_only);
            mark_obj(gc, (DSGCObject *)m->global_names[i], minor_only);
        }
        return sizeof(DSModule) + (size_t)m->global_cap * sizeof(DSValue);
    }
    default:
        return 64u;
    }
}

static void mark_roots(DSVM *vm, int minor_only) {
    DSGCState *gc = &vm->gc;
    for (DSValue *slot = vm->stack; slot < vm->sp; slot++)
        mark_val(gc, *slot, minor_only);
    for (int i = 0; i < vm->frame_count; i++) {
        mark_obj(gc, (DSGCObject *)vm->frames[i].closure, minor_only);
        mark_obj(gc, (DSGCObject *)vm->frames[i].module, minor_only);
    }
    for (DSUpvalue *uv = vm->open_upvalues; uv; uv = uv->next_open)
        mark_obj(gc, (DSGCObject *)uv, minor_only);
    for (int i = 0; i < vm->temp_root_count; i++)
        mark_val(gc, vm->temp_roots[i], minor_only);
    mark_val(gc, vm->return_reg, minor_only);

    mark_obj(gc, (DSGCObject *)vm->main_module, minor_only);
    mark_obj(gc, (DSGCObject *)vm->modules, minor_only);
    mark_obj(gc, (DSGCObject *)vm->builtins, minor_only);
    mark_obj(gc, (DSGCObject *)vm->str_methods, minor_only);
    mark_obj(gc, (DSGCObject *)vm->list_methods, minor_only);
    mark_obj(gc, (DSGCObject *)vm->dict_methods, minor_only);
    mark_obj(gc, (DSGCObject *)vm->love.love_mod_dict, minor_only);

    mark_val(gc, vm->love.cb_load, minor_only);
    mark_val(gc, vm->love.cb_update, minor_only);
    mark_val(gc, vm->love.cb_draw, minor_only);
    mark_val(gc, vm->love.cb_touchpressed, minor_only);
    mark_val(gc, vm->love.cb_touchmoved, minor_only);
    mark_val(gc, vm->love.cb_touchreleased, minor_only);
    mark_val(gc, vm->love.cb_mousepressed, minor_only);
    mark_val(gc, vm->love.cb_mousereleased, minor_only);
    mark_val(gc, vm->love.cb_mousemoved, minor_only);
    mark_val(gc, vm->love.cb_keypressed, minor_only);
    mark_val(gc, vm->love.cb_keyreleased, minor_only);

    /* A minor collection has to descend from old roots only where they can
     * still reach the nursery. Re-tracing unconditionally is correct but makes
     * every pause proportional to the whole heap, so a list that keeps growing
     * turns the run quadratic. */
    if (minor_only) {
        DSGCObject *mod = (DSGCObject *)vm->main_module;
        if (mod && !is_young(mod) && young_children_of(mod) != 0)
            trace_blacken(gc, mod, 1);
        DSGCObject *mod_dict = vm->main_module ? (DSGCObject *)vm->main_module->dict : NULL;
        if (mod_dict && !is_young(mod_dict) && young_children_of(mod_dict) != 0)
            trace_blacken(gc, mod_dict, 1);
        if (vm->modules && !is_young((DSGCObject *)vm->modules) && young_children_of((DSGCObject *)vm->modules) != 0)
            trace_blacken(gc, (DSGCObject *)vm->modules, 1);
        if (vm->love.love_mod_dict && !is_young((DSGCObject *)vm->love.love_mod_dict) &&
            young_children_of((DSGCObject *)vm->love.love_mod_dict) != 0)
            trace_blacken(gc, (DSGCObject *)vm->love.love_mod_dict, 1);
        /* The remembered set persists until the next major cycle: a container
         * may hold a NEW child now and a SURVIVAL child after this collection.
         * Recount first so all-old containers cost an O(n) scan, not a full
         * blacken (which also re-queues their gray children). */
        for (DSGCObject *t = gc->touched_head; t; t = t->touch_next)
            if (young_children_of(t) != 0)
                trace_blacken(gc, t, 1);
    }
}

static void free_single_obj(DSVM *vm, DSGCObject *o) {
    DSGCState *gc = &vm->gc;
    size_t base_cost = (o->slab_class == 0 ? 64u : (o->slab_class == 1 ? 144u : sizeof(DSGCObject))) + o->extra_bytes;

    switch (o->tt) {
    case DS_OBJ_STRING: {
        DSString *s = (DSString *)o;
        if (s->chars && s->chars != s->inline_buf)
            free(s->chars);
        break;
    }
    case DS_OBJ_LIST:
        free(((DSList *)o)->items);
        break;
    case DS_OBJ_DICT:
        free(((DSDict *)o)->entries);
        break;
    case DS_OBJ_FUNC: {
        DSFuncProto *fn = (DSFuncProto *)o;
        free(fn->defaults);
        free(fn->params);
        free(fn->default_exprs);
        free(fn->upvals);
        if (fn->jit_fn) {
            ds_jit_free(fn->jit_fn);
            if (vm->jit_fn_count > 0)
                vm->jit_fn_count--;
        }
        if (fn->arena)
            ds_arena_release(fn->arena);
        break;
    }
    case DS_OBJ_CLOSURE:
        free(((DSClosure *)o)->upvalues);
        break;
    case DS_OBJ_NATIVE: {
        DSNativeObj *no = (DSNativeObj *)o;
        if (no->finalizer && no->ptr)
            no->finalizer(no->ptr);
        break;
    }
    case DS_OBJ_MODULE: {
        DSModule *m = (DSModule *)o;
        free(m->global_slots);
        free(m->global_names);
        break;
    }
    default:
        break;
    }

    gc_account_sub(gc, base_cost);
    if (gc->object_count > 0)
        gc->object_count--;
    slab_free(gc, o, o->slab_class);
}

#define INTERN_TOMB ((DSString *)(uintptr_t)1)

static void sweep_intern_table(DSGCState *gc, int minor_only) {
    for (int i = 0; i < gc->intern_cap; i++) {
        DSString *s = gc->intern_buckets[i];
        if (!s || s == INTERN_TOMB)
            continue;
        if (s->pinned)
            continue;
        if (minor_only && !is_young(&s->hdr))
            continue;
        if (is_dead(gc, &s->hdr, gc->collect_white)) {
            gc->intern_buckets[i] = INTERN_TOMB;
            gc->intern_count--;
        }
    }
}

void ds_gc_collect_minor(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    if (gc->paused || gc->phase != GC_PHASE_IDLE)
        return;

    uint64_t t0 = ds_monotonic_ns();
    gc->paused = 1;
    gc->gray_head = NULL;
    /* Everything allocated since the last cycle carries current_white. */
    gc->collect_white = gc->current_white;

    mark_roots(vm, 1);
    while (gc->gray_head) {
        DSGCObject *o = gc->gray_head;
        gc->gray_head = o->gray_next;
        o->gray_next = NULL;
        trace_blacken(gc, o, 1);
    }

    sweep_intern_table(gc, 1);

    DSGCObject **link = &gc->young_head;
    while (*link) {
        DSGCObject *o = *link;
        if (o->tt == DS_OBJ_STRING && ((DSString *)o)->pinned) {
            o->color = gc->current_white;
            *link = o->gc_next;
            o->age = GC_AGE_OLD;
            o->gc_next = gc->old_head;
            gc->old_head = o;
            continue;
        }
        if (is_dead(gc, o, gc->collect_white)) {
            *link = o->gc_next;
            /* A dead container may still sit in the remembered set; unlinking it
             * here keeps the set from pointing at slab memory that is about to
             * be recycled. */
            if (o->age == GC_AGE_TOUCHED)
                gc_forget(gc, o);
            free_single_obj(vm, o);
        } else {
            o->color = gc->current_white;
            if (o->age == GC_AGE_NEW) {
                o->age = GC_AGE_SURVIVAL;
                link = &o->gc_next;
            } else if (o->age == GC_AGE_TOUCHED) {
                /* Queued in the remembered set: move to the old list but keep the
                 * flag, otherwise the next barrier push re-links an object that
                 * is already on the list and splices a cycle into it. */
                *link = o->gc_next;
                o->gc_next = gc->old_head;
                gc->old_head = o;
            } else {
                /* Promote survival -> OLD */
                *link = o->gc_next;
                o->age = GC_AGE_OLD;
                o->gc_next = gc->old_head;
                gc->old_head = o;
            }
        }
    }

    gc->young_runs++;
    size_t step_add = gc->bytes_allocated / 2;
    if (step_add < GC_INITIAL_MINOR)
        step_add = GC_INITIAL_MINOR;
    gc->next_minor_threshold = gc->bytes_allocated + step_add;
    gc->paused = 0;

    double us = (double)(ds_monotonic_ns() - t0) * 1e-3;
    gc->last_pause_us = us;
    if (us > gc->max_pause_us)
        gc->max_pause_us = us;
}

static void start_major_cycle(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    gc->gray_head = NULL;
    gc->gray_again_head = NULL;
    gc_forget_all(gc); /* also clears GC_AGE_TOUCHED on every member */
    /* Freeze the "garbage" white for the whole cycle; the atomic phase later
     * flips current_white so freshly promoted objects are not mistaken for it. */
    gc->collect_white = gc->current_white;
    if (getenv("DS_GC_TRACE"))
        fprintf(stderr, "[gc] start_major bytes=%zu objs=%zu phase=%d\n", gc->bytes_allocated, gc->object_count, gc->phase);
    /* Ensure all objects start with current_white */
    for (DSGCObject *o = gc->young_head; o; o = o->gc_next)
        o->color = gc->current_white;
    for (DSGCObject *o = gc->old_head; o; o = o->gc_next) {
        o->color = gc->current_white;
        if (o->age == GC_AGE_TOUCHED)
            o->age = GC_AGE_OLD;
    }
    mark_roots(vm, 0);
    gc->phase = GC_PHASE_MARK;
}

static void finish_atomic_phase(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    /* Re-mark stack, open upvalues, and gray_again containers */
    mark_roots(vm, 0);
    while (gc->gray_again_head) {
        DSGCObject *o = gc->gray_again_head;
        gc->gray_again_head = o->gray_next;
        o->gray_next = NULL;
        trace_blacken(gc, o, 0);
    }
    while (gc->gray_head) {
        DSGCObject *o = gc->gray_head;
        gc->gray_head = o->gray_next;
        o->gray_next = NULL;
        trace_blacken(gc, o, 0);
    }

    sweep_intern_table(gc, 0);

    /* Flip white for objects allocated during upcoming incremental sweep */
    uint8_t next_white = (gc->current_white == GC_WHITE0) ? GC_WHITE1 : GC_WHITE0;
    gc->current_white = next_white;

    /* Sweep young list immediately and promote survivors to old_head */
    DSGCObject *y = gc->young_head;
    gc->young_head = NULL;
    while (y) {
        DSGCObject *nx = y->gc_next;
        int keep = (y->tt == DS_OBJ_STRING && ((DSString *)y)->pinned) || !is_dead(gc, y, gc->collect_white);
        if (!keep) {
            if (y->age == GC_AGE_TOUCHED)
                gc_forget(gc, y);
            free_single_obj(vm, y);
        } else {
            y->color = next_white;
            /* Keep GC_AGE_TOUCHED: it is both an age and the remembered-set
             * membership flag, and clearing it here would let the next barrier
             * re-link an object that is still on the list. */
            if (y->age != GC_AGE_TOUCHED)
                y->age = GC_AGE_OLD;
            y->gc_next = gc->old_head;
            gc->old_head = y;
        }
        y = nx;
    }

    gc->sweep_cursor = &gc->old_head;
    gc->phase = GC_PHASE_SWEEP_OLD;
    if (getenv("DS_GC_TRACE"))
        fprintf(stderr, "[gc] atomic done bytes=%zu objs=%zu young=%p old=%p\n", gc->bytes_allocated, gc->object_count,
                (void *)gc->young_head, (void *)gc->old_head);
}

static int sweep_old_slice(DSVM *vm, int max_count) {
    DSGCState *gc = &vm->gc;
    int processed = 0;
    while (*gc->sweep_cursor && processed < max_count) {
        DSGCObject *o = *gc->sweep_cursor;
        int pinned = (o->tt == DS_OBJ_STRING && ((DSString *)o)->pinned);
        if (!pinned && is_dead(gc, o, gc->collect_white)) {
            *gc->sweep_cursor = o->gc_next;
            if (o->age == GC_AGE_TOUCHED)
                gc_forget(gc, o);
            free_single_obj(vm, o);
        } else {
            o->color = gc->current_white;
            /* Do NOT downgrade GC_AGE_TOUCHED here: the object is still a member
             * of the remembered set, and clearing the flag while it stays linked
             * lets the next barrier push it again, pointing touch_next at itself
             * (an infinite loop) or at whatever the head used to be. Membership
             * is released in gc_forget/gc_forget_all only. */
            gc->sweep_cursor = &o->gc_next;
        }
        processed++;
    }
    if (*gc->sweep_cursor == NULL) {
        gc->major_runs++;
        if (getenv("DS_GC_TRACE"))
            fprintf(stderr, "[gc] sweep done bytes=%zu objs=%zu next_major=%zu\n", gc->bytes_allocated,
                    gc->object_count, gc->next_major_threshold);
        size_t live = gc->bytes_allocated < 65536u ? 65536u : gc->bytes_allocated;
        gc->next_major_threshold = (live * (size_t)gc->pause_pct) / 100u;
        if (gc->next_major_threshold < GC_INITIAL_MAJOR)
            gc->next_major_threshold = GC_INITIAL_MAJOR;
        gc->next_minor_threshold = gc->bytes_allocated + GC_INITIAL_MINOR;
        gc->phase = GC_PHASE_IDLE;
        return 1;
    }
    return 0;
}

void ds_gc_step_us(DSVM *vm, double budget_us) {
    DSGCState *gc = &vm->gc;
    if (gc->paused)
        return;
    if (budget_us < 10.0)
        budget_us = 10.0;

    uint64_t t0 = ds_monotonic_ns();
    uint64_t limit_ns = t0 + (uint64_t)(budget_us * 1000.0);
    gc->paused = 1;

    if (gc->phase == GC_PHASE_IDLE) {
        if (gc->mode == GC_MODE_GENERATIONAL && gc->bytes_allocated < gc->next_major_threshold) {
            gc->paused = 0;
            if (gc->bytes_allocated > gc->next_minor_threshold / 2 || gc->young_head != NULL)
                ds_gc_collect_minor(vm);
            return;
        }
        start_major_cycle(vm);
    }

    while (gc->phase != GC_PHASE_IDLE) {
        if (gc->phase == GC_PHASE_MARK) {
            int batch = 0;
            while (gc->gray_head && batch < 32) {
                DSGCObject *o = gc->gray_head;
                gc->gray_head = o->gray_next;
                o->gray_next = NULL;
                trace_blacken(gc, o, 0);
                batch++;
            }
            if (!gc->gray_head)
                gc->phase = GC_PHASE_ATOMIC;
        } else if (gc->phase == GC_PHASE_ATOMIC) {
            finish_atomic_phase(vm);
        } else if (gc->phase == GC_PHASE_SWEEP_OLD) {
            sweep_old_slice(vm, 64);
        }
        if (ds_monotonic_ns() >= limit_ns)
            break;
    }

    gc->paused = 0;
    double us = (double)(ds_monotonic_ns() - t0) * 1e-3;
    gc->last_pause_us = us;
    if (us > gc->max_pause_us)
        gc->max_pause_us = us;
}

void ds_gc_collect_full(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    uint64_t t0 = ds_monotonic_ns();
    gc->paused = 1;
    start_major_cycle(vm);
    while (gc->gray_head) {
        DSGCObject *o = gc->gray_head;
        gc->gray_head = o->gray_next;
        o->gray_next = NULL;
        trace_blacken(gc, o, 0);
    }
    finish_atomic_phase(vm);
    while (!sweep_old_slice(vm, 4096)) {}
    gc->paused = 0;

    double us = (double)(ds_monotonic_ns() - t0) * 1e-3;
    gc->last_pause_us = us;
    if (us > gc->max_pause_us)
        gc->max_pause_us = us;
}

void ds_gc_check(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    if (gc->paused)
        return;
    if (gc->phase != GC_PHASE_IDLE || gc->bytes_allocated >= gc->next_major_threshold) {
        ds_gc_step_us(vm, 120.0);
    } else if (gc->bytes_allocated >= gc->next_minor_threshold) {
        ds_gc_collect_minor(vm);
    }
}

void ds_gc_free_all(DSVM *vm) {
    DSGCState *gc = &vm->gc;
    gc->paused = 1;
    DSGCObject *lists[2] = {gc->young_head, gc->old_head};
    gc->young_head = NULL;
    gc->old_head = NULL;
    for (int k = 0; k < 2; k++) {
        DSGCObject *o = lists[k];
        while (o) {
            DSGCObject *nx = o->gc_next;
            free_single_obj(vm, o);
            o = nx;
        }
    }
    DSGCSlab *s = gc->slabs;
    gc->slabs = NULL;
    while (s) {
        DSGCSlab *nx = s->next;
        munmap(s, SLAB_PAGE_BYTES);
        s = nx;
    }
    free(gc->intern_buckets);
    gc->intern_buckets = NULL;
    gc->intern_cap = 0;
    gc->intern_count = 0;
    gc->intern_used = 0;
}

/* ============================================================================
 * INTERNED STRINGS & CORE OBJECT CONSTRUCTORS
 * ============================================================================ */

static void intern_grow(DSGCState *gc) {
    int ncap = gc->intern_cap * 2;
    DSString **nb = (DSString **)calloc((size_t)ncap, sizeof(DSString *));
    if (!nb)
        return;
    uint32_t mask = (uint32_t)ncap - 1u;
    int actual = 0;
    for (int i = 0; i < gc->intern_cap; i++) {
        DSString *s = gc->intern_buckets[i];
        if (!s || s == INTERN_TOMB)
            continue;
        uint32_t idx = s->hash & mask;
        while (nb[idx] != NULL)
            idx = (idx + 1u) & mask;
        nb[idx] = s;
        actual++;
    }
    free(gc->intern_buckets);
    gc->intern_buckets = nb;
    gc->intern_cap = ncap;
    gc->intern_count = actual;
    gc->intern_used = actual;
}

DSString *ds_string_new(DSVM *vm, const char *chars, size_t len) {
    if (!chars) {
        chars = "";
        len = 0;
    }
    DSGCState *gc = &vm->gc;
    uint32_t h = ds_hash_bytes(chars, len);

    /* Intern strings <= 128 bytes for O(1) pointer equality & zero duplicate RAM! */
    int do_intern = (len <= 128 && gc->intern_buckets != NULL);
    int tomb_slot = -1;
    if (do_intern) {
        if ((gc->intern_used + 1) * 4 >= gc->intern_cap * 3)
            intern_grow(gc);
        uint32_t mask = (uint32_t)gc->intern_cap - 1u;
        uint32_t idx = h & mask;
        int probes = 0;
        for (;;) {
            DSString *entry = gc->intern_buckets[idx];
            if (++probes > gc->intern_cap) {
                if (tomb_slot < 0)
                    tomb_slot = (int)idx;
                break;
            }
            if (entry == NULL) {
                if (tomb_slot < 0)
                    tomb_slot = (int)idx;
                break;
            } else if (entry == INTERN_TOMB) {
                if (tomb_slot < 0)
                    tomb_slot = (int)idx;
            } else if (entry->len == len && entry->hash == h && memcmp(entry->chars, chars, len) == 0) {
                /* Resurrection barrier if incremental mark is running! */
                if (gc->phase == GC_PHASE_MARK && is_dead(gc, &entry->hdr, gc->collect_white))
                    entry->hdr.color = GC_BLACK;
                return entry;
            }
            idx = (idx + 1u) & mask;
        }
    }

    size_t extra = (len < sizeof(((DSString *)0)->inline_buf)) ? 0 : (len + 1);
    DSString *s = (DSString *)ds_gc_alloc_obj(vm, sizeof(DSString), DS_OBJ_STRING, extra);
    if (!s)
        return NULL;
    s->hash = h;
    s->len = (uint32_t)len;
    if (extra == 0) {
        s->chars = s->inline_buf;
    } else {
        s->chars = (char *)malloc(len + 1);
        if (!s->chars) {
            s->chars = s->inline_buf;
            s->len = 0;
            len = 0;
        }
    }
    if (len > 0)
        memcpy(s->chars, chars, len);
    s->chars[len] = '\0';

    if (do_intern) {
        /* Re-probe in case ds_gc_alloc_obj triggered GC or table resize */
        if ((gc->intern_used + 1) * 4 >= gc->intern_cap * 3)
            intern_grow(gc);
        uint32_t mask = (uint32_t)gc->intern_cap - 1u;
        uint32_t idx = h & mask;
        int put = -1;
        int probes2 = 0;
        for (;;) {
            DSString *e = gc->intern_buckets[idx];
            if (++probes2 > gc->intern_cap) {
                put = (int)idx;
                break;
            }
            if (e == NULL) {
                if (put < 0) put = (int)idx;
                break;
            } else if (e == INTERN_TOMB && put < 0) {
                put = (int)idx;
            }
            idx = (idx + 1u) & mask;
        }
        if (gc->intern_buckets[put] == INTERN_TOMB) {
            /* reusing a tombstone keeps occupancy unchanged */
        } else {
            gc->intern_used++;
        }
        gc->intern_buckets[put] = s;
        gc->intern_count++;
    }
    return s;
}

DSString *ds_string_const(DSVM *vm, const char *cstr) {
    DSString *s = ds_string_new(vm, cstr, cstr ? strlen(cstr) : 0);
    if (s)
        s->pinned = 1;
    return s;
}

DSString *ds_string_concat(DSVM *vm, DSString *a, DSString *b) {
    if (a->len == 0)
        return b;
    if (b->len == 0)
        return a;
    size_t total = (size_t)a->len + (size_t)b->len;
    if (total < 24) {
        char stack_buf[24];
        memcpy(stack_buf, a->chars, a->len);
        memcpy(stack_buf + a->len, b->chars, b->len);
        return ds_string_new(vm, stack_buf, total);
    }
    ds_push_root(vm, ds_obj_val(a));
    ds_push_root(vm, ds_obj_val(b));
    char *tmp = (char *)malloc(total + 1);
    if (!tmp) {
        ds_pop_roots(vm, 2);
        return ds_string_new(vm, "", 0);
    }
    memcpy(tmp, a->chars, a->len);
    memcpy(tmp + a->len, b->chars, b->len);
    DSString *res = ds_string_new(vm, tmp, total);
    free(tmp);
    ds_pop_roots(vm, 2);
    return res;
}

DSList *ds_list_new(DSVM *vm, int initial_cap) {
    if (initial_cap < 0)
        initial_cap = 0;
    size_t extra = (size_t)initial_cap * sizeof(DSValue);
    DSList *l = (DSList *)ds_gc_alloc_obj(vm, sizeof(DSList), DS_OBJ_LIST, 0);
    if (!l)
        return NULL;
    if (initial_cap > 0) {
        l->items = (DSValue *)ds_gc_realloc_buf(vm, &l->hdr, NULL, 0, extra);
        if (l->items)
            l->cap = initial_cap;
    }
    return l;
}

void ds_list_push(DSVM *vm, DSList *list, DSValue v) {
    if (!list)
        return;
    if (list->len == list->cap) {
        int ncap = list->cap < 8 ? 8 : list->cap * 2;
        DSValue *ni = (DSValue *)ds_gc_realloc_buf(
            vm, &list->hdr, list->items, (size_t)list->cap * sizeof(DSValue), (size_t)ncap * sizeof(DSValue));
        if (!ni)
            return;
        list->items = ni;
        list->cap = ncap;
    }
    list->items[list->len++] = v;
    ds_gc_write_barrier_back(vm, &list->hdr, v);
}

static void dict_resize(DSVM *vm, DSDict *d, int ncap) {
    DSDictEntry *ne = (DSDictEntry *)calloc((size_t)ncap, sizeof(DSDictEntry));
    if (!ne)
        return;
    uint32_t mask = (uint32_t)ncap - 1u;
    int new_count = 0;
    for (int i = 0; i < d->cap; i++) {
        if (d->entries[i].state != 1)
            continue;
        uint32_t idx = d->entries[i].hash & mask;
        while (ne[idx].state == 1)
            idx = (idx + 1u) & mask;
        ne[idx] = d->entries[i];
        new_count++;
    }
    size_t old_bytes = (size_t)d->cap * sizeof(DSDictEntry);
    size_t new_bytes = (size_t)ncap * sizeof(DSDictEntry);
    free(d->entries);
    if (new_bytes >= old_bytes) {
        size_t diff = new_bytes - old_bytes;
        gc_account_add(&vm->gc, diff);
        d->hdr.extra_bytes += (uint32_t)diff;
    }
    d->entries = ne;
    d->cap = ncap;
    d->count = new_count;
    d->used = new_count;
}

DSDict *ds_dict_new(DSVM *vm, int initial_cap) {
    int cap = 8;
    while (cap < initial_cap) cap *= 2;
    DSDict *d = (DSDict *)ds_gc_alloc_obj(vm, sizeof(DSDict), DS_OBJ_DICT, 0);
    if (!d)
        return NULL;
    if (initial_cap > 0)
        dict_resize(vm, d, cap);
    return d;
}

int ds_dict_get_str(const DSDict *d, const DSString *key, DSValue *out) {
    if (!d || d->count == 0 || !d->entries || !key)
        return 0;
    uint32_t mask = (uint32_t)d->cap - 1u;
    uint32_t idx = key->hash & mask;
    for (;;) {
        const DSDictEntry *e = &d->entries[idx];
        if (e->state == 0)
            return 0;
        if (e->state == 1 && ds_obj_is(e->key, DS_OBJ_STRING)) {
            const DSString *ek = AS_STRING(e->key);
            if (ek == key || (ek->hash == key->hash && ek->len == key->len && memcmp(ek->chars, key->chars, key->len) == 0)) {
                if (out)
                    *out = e->val;
                return 1;
            }
        }
        idx = (idx + 1u) & mask;
    }
}

int ds_dict_get(const DSDict *d, DSValue key, DSValue *out) {
    if (ds_obj_is(key, DS_OBJ_STRING))
        return ds_dict_get_str(d, AS_STRING(key), out);
    if (!d || d->count == 0 || !d->entries)
        return 0;
    uint32_t h = ds_value_hash(key);
    uint32_t mask = (uint32_t)d->cap - 1u;
    uint32_t idx = h & mask;
    for (;;) {
        const DSDictEntry *e = &d->entries[idx];
        if (e->state == 0)
            return 0;
        if (e->state == 1 && e->hash == h && ds_values_equal(e->key, key)) {
            if (out)
                *out = e->val;
            return 1;
        }
        idx = (idx + 1u) & mask;
    }
}

void ds_dict_set(DSVM *vm, DSDict *d, DSValue key, DSValue val) {
    if (!d)
        return;
    if (d->cap == 0 || (d->used + 1) * 10 >= d->cap * 7) {
        ds_push_root(vm, key);
        ds_push_root(vm, val);
        dict_resize(vm, d, d->cap < 8 ? 8 : d->cap * 2);
        ds_pop_roots(vm, 2);
    }
    if (!d->entries)
        return;
    uint32_t h = ds_value_hash(key);
    uint32_t mask = (uint32_t)d->cap - 1u;
    uint32_t idx = h & mask;
    int tomb = -1;
    int key_is_str = ds_obj_is(key, DS_OBJ_STRING);
    DSString *ks = key_is_str ? AS_STRING(key) : NULL;

    for (;;) {
        DSDictEntry *e = &d->entries[idx];
        if (e->state == 0) {
            DSDictEntry *dst = (tomb >= 0) ? &d->entries[tomb] : e;
            if (tomb < 0)
                d->used++;
            d->count++;
            dst->key = key;
            dst->val = val;
            dst->hash = h;
            dst->state = 1;
            ds_gc_write_barrier_back(vm, &d->hdr, key);
            ds_gc_write_barrier_back(vm, &d->hdr, val);
            return;
        } else if (e->state == 2) {
            if (tomb < 0)
                tomb = (int)idx;
        } else if (e->hash == h) {
            int eq = (key_is_str && ds_obj_is(e->key, DS_OBJ_STRING) && AS_STRING(e->key) == ks)
                     ? 1
                     : ds_values_equal(e->key, key);
            if (eq) {
                e->val = val;
                ds_gc_write_barrier_back(vm, &d->hdr, val);
                return;
            }
        }
        idx = (idx + 1u) & mask;
    }
}

void ds_dict_set_cstr(DSVM *vm, DSDict *dict, const char *key, DSValue val) {
    ds_push_root(vm, val);
    ds_push_root(vm, ds_obj_val(dict));
    DSString *ks = ds_string_const(vm, key);
    ds_dict_set(vm, dict, ds_obj_val(ks), val);
    ds_pop_roots(vm, 2);
}

int ds_dict_del(DSDict *d, DSValue key) {
    if (!d || d->count == 0 || !d->entries)
        return 0;
    uint32_t h = ds_value_hash(key);
    uint32_t mask = (uint32_t)d->cap - 1u;
    uint32_t idx = h & mask;
    for (;;) {
        DSDictEntry *e = &d->entries[idx];
        if (e->state == 0)
            return 0;
        if (e->state == 1 && e->hash == h && ds_values_equal(e->key, key)) {
            e->key = DS_VAL_NIL;
            e->val = DS_VAL_NIL;
            e->state = 2;
            d->count--;
            return 1;
        }
        idx = (idx + 1u) & mask;
    }
}

DSModule *ds_module_new(DSVM *vm, DSString *name) {
    ds_push_root(vm, ds_obj_val(name));
    DSModule *m = (DSModule *)ds_gc_alloc_obj(vm, sizeof(DSModule), DS_OBJ_MODULE, 0);
    ds_push_root(vm, ds_obj_val(m));
    m->name = name;
    m->dict = ds_dict_new(vm, 32);
    ds_pop_roots(vm, 2);
    return m;
}

int ds_module_ensure_slot(DSVM *vm, DSModule *mod, DSString *name) {
    for (int i = 0; i < mod->global_count; i++) {
        if (mod->global_names[i] == name ||
            (mod->global_names[i]->len == name->len &&
             memcmp(mod->global_names[i]->chars, name->chars, name->len) == 0)) {
            return i;
        }
    }
    if (mod->global_count == mod->global_cap) {
        int ncap = mod->global_cap < 32 ? 32 : mod->global_cap * 2;
        DSValue *ns = (DSValue *)realloc(mod->global_slots, (size_t)ncap * sizeof(DSValue));
        DSString **nn = (DSString **)realloc(mod->global_names, (size_t)ncap * sizeof(DSString *));
        if (!ns || !nn) {
            free(ns);
            free(nn);
            return -1;
        }
        for (int k = mod->global_cap; k < ncap; k++) {
            ns[k] = DS_VAL_NIL;
            nn[k] = NULL;
        }
        mod->global_slots = ns;
        mod->global_names = nn;
        mod->global_cap = ncap;
    }
    int slot = mod->global_count++;
    mod->global_names[slot] = name;
    DSValue existing = DS_VAL_NIL;
    if (ds_dict_get_str(mod->dict, name, &existing))
        mod->global_slots[slot] = existing;
    else
        mod->global_slots[slot] = DS_VAL_NIL;
    ds_gc_write_barrier(vm, &mod->hdr, ds_obj_val(name));
    return slot;
}

DSCFunc *ds_cfunc_new(DSVM *vm, const char *name, DSNativeFn fn, void *ud) {
    DSCFunc *cf = (DSCFunc *)ds_gc_alloc_obj(vm, sizeof(DSCFunc), DS_OBJ_CFUNC, 0);
    cf->name = name;
    cf->fn = fn;
    cf->userdata = ud;
    return cf;
}

DSNativeObj *ds_native_new(DSVM *vm, const char *tag, void *ptr, size_t cost, DSNativeFinalizer fin, DSDict *methods) {
    if (methods)
        ds_push_root(vm, ds_obj_val(methods));
    DSNativeObj *no = (DSNativeObj *)ds_gc_alloc_obj(vm, sizeof(DSNativeObj), DS_OBJ_NATIVE, cost);
    if (methods)
        ds_pop_roots(vm, 1);
    no->type_tag = tag;
    no->ptr = ptr;
    no->mem_cost = cost;
    no->finalizer = fin;
    no->methods = methods;
    return no;
}

DSClass *ds_class_new(DSVM *vm, DSString *name, DSClass *superclass) {
    ds_push_root(vm, ds_obj_val(name));
    if (superclass)
        ds_push_root(vm, ds_obj_val(superclass));
    DSClass *kl = (DSClass *)ds_gc_alloc_obj(vm, sizeof(DSClass), DS_OBJ_CLASS, 0);
    ds_push_root(vm, ds_obj_val(kl));
    kl->name = name;
    kl->superclass = superclass;
    kl->methods = ds_dict_new(vm, 8);
    kl->init_fn = superclass ? superclass->init_fn : DS_VAL_NIL;
    ds_pop_roots(vm, superclass ? 3 : 2);
    return kl;
}

DSInstance *ds_instance_new(DSVM *vm, DSClass *klass) {
    ds_push_root(vm, ds_obj_val(klass));
    DSInstance *in = (DSInstance *)ds_gc_alloc_obj(vm, sizeof(DSInstance), DS_OBJ_INSTANCE, 0);
    ds_push_root(vm, ds_obj_val(in));
    in->klass = klass;
    in->fields = ds_dict_new(vm, 8);
    ds_pop_roots(vm, 2);
    return in;
}

DSFuncProto *ds_proto_new(DSVM *vm, DSString *name, DSString *filename, DSModule *mod, DSCompiledArena *arena) {
    ds_push_root(vm, ds_obj_val(name));
    ds_push_root(vm, ds_obj_val(filename));
    ds_push_root(vm, ds_obj_val(mod));
    DSFuncProto *fn = (DSFuncProto *)ds_gc_alloc_obj(vm, sizeof(DSFuncProto), DS_OBJ_FUNC, 0);
    fn->name = name;
    fn->filename = filename;
    fn->module = mod;
    fn->arena = arena;
    if (arena)
        ds_arena_retain(arena);
    ds_pop_roots(vm, 3);
    return fn;
}

DSBound *ds_bound_new(DSVM *vm, DSValue recv, DSValue fn) {
    ds_push_root(vm, recv);
    ds_push_root(vm, fn);
    DSBound *b = (DSBound *)ds_gc_alloc_obj(vm, sizeof(DSBound), DS_OBJ_BOUND, 0);
    if (b) {
        b->recv = recv;
        b->fn = fn;
        ds_gc_write_barrier(vm, &b->hdr, recv);
        ds_gc_write_barrier(vm, &b->hdr, fn);
    }
    ds_pop_roots(vm, 2);
    return b;
}

DSClosure *ds_closure_new(DSVM *vm, DSFuncProto *proto) {
    ds_push_root(vm, ds_obj_val(proto));
    DSClosure *cl = (DSClosure *)ds_gc_alloc_obj(
        vm, sizeof(DSClosure), DS_OBJ_CLOSURE, (size_t)proto->num_upvalues * sizeof(DSUpvalue *));
    cl->proto = proto;
    cl->upval_count = proto->num_upvalues;
    if (cl->upval_count > 0) {
        cl->upvalues = (DSUpvalue **)calloc((size_t)cl->upval_count, sizeof(DSUpvalue *));
    }
    ds_pop_roots(vm, 1);
    return cl;
}

DSUpvalue *ds_capture_upvalue(DSVM *vm, DSValue *local) {
    DSUpvalue *prev = NULL;
    DSUpvalue *cur = vm->open_upvalues;
    while (cur != NULL && cur->location > local) {
        prev = cur;
        cur = cur->next_open;
    }
    if (cur != NULL && cur->location == local)
        return cur;

    DSUpvalue *created = (DSUpvalue *)ds_gc_alloc_obj(vm, sizeof(DSUpvalue), DS_OBJ_UPVALUE, 0);
    created->location = local;
    created->closed = DS_VAL_NIL;
    created->next_open = cur;
    if (prev == NULL)
        vm->open_upvalues = created;
    else
        prev->next_open = created;
    return created;
}

void ds_close_upvalues(DSVM *vm, DSValue *last_slot) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last_slot) {
        DSUpvalue *uv = vm->open_upvalues;
        uv->closed = *uv->location;
        uv->location = &uv->closed;
        ds_gc_write_barrier(vm, &uv->hdr, uv->closed);
        vm->open_upvalues = uv->next_open;
    }
}
