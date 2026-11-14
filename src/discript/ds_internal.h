#ifndef DISCRIPT_INTERNAL_H
#define DISCRIPT_INTERNAL_H

#include "discript/discript.h"
#include "cpp/ds_cpp.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================================
 * 1. 64-BIT NaN-BOXED VALUE REPRESENTATION
 * All GC objects are allocated out of mmap-backed 64KB Segregated Slabs,
 * guaranteeing canonical 48-bit untagged addresses even under Android 14/15
 * ARM64 Scudo TBI/MTE, and zero heap allocation for numbers/bools/None!
 * ============================================================================ */

typedef uint64_t DSValue;
typedef struct DSGCObject DSGCObject;

#define DS_QNAN_MASK  0x7FFC000000000000ULL
#define DS_SIGN_BIT   0x8000000000000000ULL
#define DS_OBJ_TAG    (DS_SIGN_BIT | DS_QNAN_MASK)
#define DS_PTR_MASK   0x0003FFFFFFFFFFFFULL

#define DS_VAL_NIL    (DS_QNAN_MASK | 1ULL)
#define DS_VAL_FALSE  (DS_QNAN_MASK | 2ULL)
#define DS_VAL_TRUE   (DS_QNAN_MASK | 3ULL)

static inline DSValue ds_num(double n) {
    uint64_t u;
    if (isnan(n))
        return 0x7FF8000000000000ULL; /* canonical non-tagged IEEE NaN */
    __builtin_memcpy(&u, &n, sizeof(u));
    return u;
}

static inline int ds_is_num(DSValue v) {
    return (v & DS_QNAN_MASK) != DS_QNAN_MASK;
}

static inline double ds_as_num(DSValue v) {
    double d;
    __builtin_memcpy(&d, &v, sizeof(d));
    return d;
}

static inline DSValue ds_bool(int b) {
    return b ? DS_VAL_TRUE : DS_VAL_FALSE;
}

static inline int ds_is_nil(DSValue v)   { return v == DS_VAL_NIL; }
static inline int ds_is_bool(DSValue v)  { return v == DS_VAL_TRUE || v == DS_VAL_FALSE; }
static inline int ds_as_bool(DSValue v)  { return v == DS_VAL_TRUE; }

static inline int ds_is_obj(DSValue v) {
    return (v & DS_OBJ_TAG) == DS_OBJ_TAG;
}

static inline DSGCObject *ds_as_obj(DSValue v) {
    return (DSGCObject *)(uintptr_t)(v & DS_PTR_MASK);
}

static inline DSValue ds_obj_val(const void *obj) {
    return DS_OBJ_TAG | ((uint64_t)(uintptr_t)obj & DS_PTR_MASK);
}

/* ============================================================================
 * 2. LUA 5.4-STYLE TRI-COLOR INCREMENTAL + GENERATIONAL GC OBJECTS
 * ============================================================================ */

typedef enum {
    DS_OBJ_STRING = 1,
    DS_OBJ_LIST = 2,
    DS_OBJ_DICT = 3,
    DS_OBJ_FUNC = 4,
    DS_OBJ_CLOSURE = 5,
    DS_OBJ_UPVALUE = 6,
    DS_OBJ_CLASS = 7,
    DS_OBJ_INSTANCE = 8,
    DS_OBJ_CFUNC = 9,
    DS_OBJ_NATIVE = 10,
    DS_OBJ_MODULE = 11,
    DS_OBJ_BOUND = 12
} DSObjType;

typedef enum {
    GC_WHITE0 = 0,
    GC_WHITE1 = 1,
    GC_GRAY = 2,
    GC_BLACK = 3
} GCColor;

typedef enum {
    GC_AGE_NEW = 0,
    GC_AGE_SURVIVAL = 1,
    GC_AGE_OLD = 2,
    GC_AGE_TOUCHED = 3
} GCAge;

struct DSGCObject {
    DSGCObject *gc_next;
    DSGCObject *gray_next;  /* mark stack link ONLY */
    DSGCObject *touch_next; /* remembered set link ONLY (kept separate so the
                             * mark stack can never be corrupted mid-mark) */
    uint8_t tt;
    uint8_t color;
    uint8_t age;
    uint8_t slab_class; /* 0 = 64B slab, 1 = 128B slab, 2 = mmap oversized */
    uint32_t extra_bytes;
};

static inline int ds_obj_is(DSValue v, DSObjType tt) {
    return ds_is_obj(v) && ds_as_obj(v)->tt == (uint8_t)tt;
}

typedef struct DSString {
    DSGCObject hdr;
    uint32_t hash;
    uint32_t len;
    uint8_t pinned;
    char *chars; /* inline for short strings inside 64B/128B slab, or heap for large */
    char inline_buf[24];
} DSString;

typedef struct DSList {
    DSGCObject hdr;
    int len;
    int cap;
    DSValue *items;
} DSList;

typedef struct DSDictEntry {
    DSValue key;   /* DS_VAL_NIL = empty, DS_VAL_FALSE with tomb_flag = tombstone */
    DSValue val;
    uint32_t hash;
    uint8_t state; /* 0=empty, 1=occupied, 2=tombstone */
} DSDictEntry;

typedef struct DSDict {
    DSGCObject hdr;
    int count;
    int used; /* occupied + tombstones */
    int cap;  /* power of two */
    DSDictEntry *entries;
} DSDict;

typedef struct DSUpvalue {
    DSGCObject hdr;
    DSValue *location; /* points to stack slot while open, or &closed when closed */
    DSValue closed;
    struct DSUpvalue *next_open;
} DSUpvalue;

typedef struct DSModule {
    DSGCObject hdr;
    DSString *name;
    DSDict *dict;
    DSValue *global_slots;
    DSString **global_names;
    int global_count;
    int global_cap;
} DSModule;

typedef DSValue (*DSNativeFn)(DSVM *vm, int argc, DSValue *argv);

typedef struct DSCFunc {
    DSGCObject hdr;
    const char *name;
    DSNativeFn fn;
    void *userdata;
} DSCFunc;

typedef void (*DSNativeFinalizer)(void *ptr);

/* Native objects expose behaviour through `methods` and, optionally, through
 * these two hooks for value-like properties. A sprite wants `ball.x = 10` to
 * work, not `ball.set_x(10)`: a property hook keeps the script readable without
 * making every native object allocate a dictionary. Both return 1 when they
 * handled the name. */
struct DSNativeObj;
typedef int (*DSNativeGetProp)(DSVM *vm, struct DSNativeObj *self, const char *name, DSValue *out);
typedef int (*DSNativeSetProp)(DSVM *vm, struct DSNativeObj *self, const char *name, DSValue val);

typedef struct DSNativeObj {
    DSGCObject hdr;
    const char *type_tag;
    void *ptr;
    size_t mem_cost;
    DSNativeFinalizer finalizer;
    DSDict *methods;
    DSNativeGetProp get_prop;
    DSNativeSetProp set_prop;
} DSNativeObj;

/* Forward declarations for compiled function & OOP */
typedef struct DSCompiledArena DSCompiledArena;
typedef struct DSStmtNode DSStmtNode;
typedef struct DSExprNode DSExprNode;

typedef struct DSUpvalDesc {
    uint8_t is_local;
    uint8_t index;
} DSUpvalDesc;

typedef struct DSFuncProto {
    DSGCObject hdr;
    DSString *name;
    DSString *filename;
    int arity;
    int min_arity;
    int has_varargs;
    int num_locals;
    int num_upvalues;
    int def_line;
    DSValue *defaults;
    DSString **params;        /* parameter names, arity entries */
    struct DSExprNode **default_exprs; /* per-parameter default expressions */
    int num_defaults;
    DSUpvalDesc *upvals;
    const DSStmtNode *body;
    DSModule *module;
    DSCompiledArena *arena; /* refcounted compilation arena owning Direct-Threaded nodes */
    DSJitFunc *jit_fn;      /* optional ARM64 / x86_64 native machine code! */
} DSFuncProto;

typedef struct DSClosure {
    DSGCObject hdr;
    DSFuncProto *proto;
    int upval_count;
    DSUpvalue **upvalues;
} DSClosure;

typedef struct DSClass {
    DSGCObject hdr;
    DSString *name;
    struct DSClass *superclass;
    DSDict *methods;
    DSValue init_fn; /* cached __init__ closure for zero-lookup instantiation */
} DSClass;

typedef struct DSInstance {
    DSGCObject hdr;
    DSClass *klass;
    DSDict *fields;
} DSInstance;

typedef struct DSBound {
    DSGCObject hdr;
    DSValue recv;
    DSValue fn;
} DSBound;

#define AS_BOUND(v) ((DSBound *)ds_as_obj(v))

#define AS_STRING(v)   ((DSString *)ds_as_obj(v))
#define AS_CSTR(v)     (((DSString *)ds_as_obj(v))->chars)
#define AS_LIST(v)     ((DSList *)ds_as_obj(v))
#define AS_DICT(v)     ((DSDict *)ds_as_obj(v))
#define AS_PROTO(v)    ((DSFuncProto *)ds_as_obj(v))
#define AS_CLOSURE(v)  ((DSClosure *)ds_as_obj(v))
#define AS_CLASS(v)    ((DSClass *)ds_as_obj(v))
#define AS_INSTANCE(v) ((DSInstance *)ds_as_obj(v))
#define AS_CFUNC(v)    ((DSCFunc *)ds_as_obj(v))
#define AS_NATIVE(v)   ((DSNativeObj *)ds_as_obj(v))
#define AS_MODULE(v)   ((DSModule *)ds_as_obj(v))

/* ============================================================================
 * 3. DIRECT-THREADED COMPILER EXECUTION ARENA (ZERO BYTECODE!)
 * ============================================================================ */

typedef enum {
    EXEC_OK = 0,
    EXEC_RETURN = 1,
    EXEC_BREAK = 2,
    EXEC_CONTINUE = 3,
    EXEC_ERROR = 4
} DSExecSignal;

typedef DSValue (*DSExprEvalFn)(DSVM *vm, DSValue *slots, const DSExprNode *node);
typedef DSExecSignal (*DSStmtExecFn)(DSVM *vm, DSValue *slots, const DSStmtNode *node);

struct DSCompiledArena {
    int refcount;
    struct DSCompiledChunk *chunks;
};

/* Inline Cache cell for zero-overhead global/property/method dispatch */
typedef struct DSInlineCache {
    const void *shape; /* DSClass* or DSDict* */
    DSValue cached_fn;
    int slot_hint;
} DSInlineCache;

/* ============================================================================
 * 4. GARBAGE COLLECTOR & VM STATE
 * ============================================================================ */

typedef enum {
    GC_MODE_GENERATIONAL = 0,
    GC_MODE_INCREMENTAL = 1
} DSGCMode;

typedef enum {
    GC_PHASE_IDLE = 0,
    GC_PHASE_MARK = 1,
    GC_PHASE_ATOMIC = 2,
    GC_PHASE_SWEEP_OLD = 3
} DSGCPhase;

typedef struct DSGCSlab DSGCSlab;

typedef struct DSGCState {
    DSGCSlab *slabs;
    void *free_64;
    void *free_128;

    DSGCObject *young_head;
    DSGCObject *old_head;
    DSGCObject *gray_head;
    DSGCObject *gray_again_head;
    DSGCObject *touched_head;
    DSGCObject **sweep_cursor;

    uint8_t current_white; /* white used for objects created right now */
    uint8_t collect_white; /* white that means "unreachable" in this cycle */
    uint8_t mode;
    uint8_t phase;
    uint8_t paused;

    size_t bytes_allocated;
    size_t peak_bytes;
    size_t next_minor_threshold;
    size_t next_major_threshold;
    size_t object_count;
    int pause_pct;   /* Lua GC pause (default 150) */
    int stepmul_pct; /* Lua GC stepmul (default 200) */

    uint64_t young_runs;
    uint64_t major_runs;
    double last_pause_us;
    double max_pause_us;

    /* String Intern Table */
    DSString **intern_buckets;
    int intern_count; /* live entries */
    int intern_used;  /* live + tombstones (probe occupancy) */
    int intern_cap;
} DSGCState;

#define DS_MAX_STACK   8192
#define DS_MAX_FRAMES  256
#define DS_MAX_ROOTS   128

typedef struct DSCallFrame {
    DSClosure *closure;
    const char *c_name;
    DSModule *module;
    DSValue *slots;
    int line;
} DSCallFrame;

/* LÖVE2D Graphics Transform + State inside VM */
#define DS_LOVE_MAX_STACK 16
#define DS_LOVE_MAX_TOUCHES 10

typedef struct DSLoveTransform {
    float tx, ty;
    float rot;
    float sx, sy;
} DSLoveTransform;

typedef struct DSLoveTouch {
    int active;
    int id;
    float x, y;
} DSLoveTouch;

typedef struct DSLoveState {
    float vp_x, vp_y, vp_w, vp_h;
    uint32_t bg_color;
    uint32_t draw_color;
    float line_width;
    DSLoveTransform xf_stack[DS_LOVE_MAX_STACK];
    int xf_sp;
    int scissor_on;
    float sc_x, sc_y, sc_w, sc_h;

    DSLoveTouch touches[DS_LOVE_MAX_TOUCHES];
    float mouse_x, mouse_y;
    int mouse_down;
    uint8_t keys_down[64]; /* hashed / indexed common LÖVE2D key names */

    double start_time;
    double last_dt;
    double fps_smooth;
    uint64_t rng_state[4];

    DSValue cb_load;
    DSValue cb_update;
    DSValue cb_draw;
    DSValue cb_touchpressed;
    DSValue cb_touchmoved;
    DSValue cb_touchreleased;
    DSValue cb_mousepressed;
    DSValue cb_mousereleased;
    DSValue cb_mousemoved;
    DSValue cb_keypressed;
    DSValue cb_keyreleased;

    DSDict *love_mod_dict;
    int img_counter;
} DSLoveState;

/* The script-visible 3D scene. Held by value in the VM so it needs no
 * lifetime management of its own; `scene_class` is the shared class every
 * node handle is an instance of. */
struct DSScene;
typedef struct DSScene DSScene;
struct DSClass;
typedef struct DSClass DSClass;

struct DSVM {
    DSGCState gc;

    DSValue stack[DS_MAX_STACK];
    DSValue *sp;

    DSCallFrame frames[DS_MAX_FRAMES];
    int frame_count;

    DSUpvalue *open_upvalues;

    /* Temporary roots: a stack of values that must survive a collection while
     * native code holds them. It grows on demand -- a fixed array with a silent
     * drop on overflow is a use-after-free waiting to happen, and deep call
     * chains (a native function called in a loop) legitimately need more than
     * the initial budget. */
    DSValue *temp_roots;
    int temp_root_count;
    int temp_root_cap;

    DSValue return_reg;

    DSModule *main_module;
    DSDict *modules;
    DSDict *builtins;

    /* Built-in method tables for primitive/container types */
    DSDict *str_methods;
    DSDict *list_methods;
    DSDict *dict_methods;

    DSLoveState love;

    DSPrintCallback print_cb;
    void *print_ud;
    char project_dir[512];

    int has_error;
    int stop_requested;
    uint32_t step_budget;
    uint64_t deadline_ns;

    char err_msg[768];
    char err_file[128];
    int err_line;

    double last_compile_ms;
    double exec_ms; /* time spent running the program, excluding compilation */
    int jit_fn_count;
    int jit_enabled; /* DS_NOJIT=1 turns the native path off for A/B runs */
    uint64_t jit_calls;

    DSValue repl_last_value;
    int repl_echo;
    int auto_display;

    /* ---- 3D scene (created on first use) ---- */
    struct DSScene *scene;
    DSValue scene_class; /* SceneNode class; a root so the collector keeps it */
    double scene_time;    /* seconds of animation advanced so far */
};

/* ---- Temporary Root Guard Macros ---------------------------------------- */
int ds_push_root_slow(DSVM *vm, DSValue v);
static inline void ds_push_root(DSVM *vm, DSValue v) {
    if (vm->temp_root_count < vm->temp_root_cap) {
        vm->temp_roots[vm->temp_root_count++] = v;
        return;
    }
    ds_push_root_slow(vm, v);
}
static inline void ds_pop_roots(DSVM *vm, int n) {
    vm->temp_root_count -= n;
    if (vm->temp_root_count < 0)
        vm->temp_root_count = 0;
}

/* ---- GC & Object API (ds_gc.c) ------------------------------------------ */
void ds_gc_init(DSVM *vm);
void ds_gc_free_all(DSVM *vm);
void *ds_gc_alloc_obj(DSVM *vm, size_t struct_size, DSObjType tt, size_t extra_bytes);
void *ds_gc_realloc_buf(DSVM *vm, DSGCObject *owner, void *old_ptr, size_t old_bytes, size_t new_bytes);
void ds_gc_free_buf(DSVM *vm, DSGCObject *owner, void *ptr, size_t bytes);

void ds_gc_write_barrier(DSVM *vm, DSGCObject *parent, DSValue child);
void ds_gc_write_barrier_back(DSVM *vm, DSGCObject *container, DSValue stored);

void ds_gc_check(DSVM *vm);
void ds_gc_collect_minor(DSVM *vm);
void ds_gc_collect_full(DSVM *vm);
void ds_gc_step_us(DSVM *vm, double budget_us);

uint32_t ds_hash_bytes(const char *s, size_t len);
DSString *ds_string_new(DSVM *vm, const char *chars, size_t len);
DSString *ds_string_const(DSVM *vm, const char *cstr);
DSString *ds_string_concat(DSVM *vm, DSString *a, DSString *b);
DSString *ds_val_to_string(DSVM *vm, DSValue v);

DSList *ds_list_new(DSVM *vm, int initial_cap);
void ds_list_push(DSVM *vm, DSList *list, DSValue v);

DSDict *ds_dict_new(DSVM *vm, int initial_cap);
int ds_dict_get(const DSDict *dict, DSValue key, DSValue *out);
int ds_dict_get_str(const DSDict *dict, const DSString *key, DSValue *out);
void ds_dict_set(DSVM *vm, DSDict *dict, DSValue key, DSValue val);
void ds_dict_set_cstr(DSVM *vm, DSDict *dict, const char *key, DSValue val);
int ds_dict_del(DSDict *dict, DSValue key);

DSModule *ds_module_new(DSVM *vm, DSString *name);
int ds_module_ensure_slot(DSVM *vm, DSModule *mod, DSString *name);

DSCFunc *ds_cfunc_new(DSVM *vm, const char *name, DSNativeFn fn, void *ud);
DSNativeObj *ds_native_new(DSVM *vm, const char *tag, void *ptr, size_t cost, DSNativeFinalizer fin, DSDict *methods);
DSClass *ds_class_new(DSVM *vm, DSString *name, DSClass *superclass);
DSInstance *ds_instance_new(DSVM *vm, DSClass *klass);
DSFuncProto *ds_proto_new(DSVM *vm, DSString *name, DSString *filename, DSModule *mod, DSCompiledArena *arena);
DSClosure *ds_closure_new(DSVM *vm, DSFuncProto *proto);
DSBound *ds_bound_new(DSVM *vm, DSValue recv, DSValue fn);
DSUpvalue *ds_capture_upvalue(DSVM *vm, DSValue *local);
void ds_close_upvalues(DSVM *vm, DSValue *last_slot);

/* ---- Value Semantics & Runtime Operations (ds_vm.c) --------------------- */
int ds_is_truthy(DSValue v);
int ds_values_equal(DSValue a, DSValue b);
uint32_t ds_value_hash(DSValue v);
const char *ds_type_name(DSValue v);

void ds_error(DSVM *vm, int line, const char *fmt, ...);
DSValue ds_call_value(DSVM *vm, DSValue callee, int argc, DSValue *argv);
DSValue ds_get_property(DSVM *vm, DSValue obj, DSString *name, DSInlineCache *ic);
void ds_set_property(DSVM *vm, DSValue obj, DSString *name, DSValue val);
DSValue ds_invoke_method(DSVM *vm, DSValue recv, DSString *name, int argc, DSValue *argv, DSInlineCache *ic);
DSValue ds_get_index(DSVM *vm, DSValue container, DSValue idx);
void ds_set_index(DSVM *vm, DSValue container, DSValue idx, DSValue val);
int ds_val_in(DSVM *vm, DSValue needle, DSValue haystack);

/* ---- Operator Runtime (ds_vm.c) ---------------------------------------- */
DSValue ds_binop(DSVM *vm, int op, DSValue a, DSValue b);
DSValue ds_unop(DSVM *vm, int op, DSValue a);
DSValue ds_lookup_name(DSVM *vm, DSModule *mod, DSString *name);
void ds_store_name(DSVM *vm, DSModule *mod, DSString *name, DSValue v);
typedef struct DSIter {
    int kind; /* 0=list 1=dict 2=string 3=range 4=native 5=instance-iter */
    int idx;
    double cur, end, step;
    DSList *list;
    DSDict *dict;
    DSString *str;
    DSValue obj;
} DSIter;

int ds_iter_open(DSVM *vm, DSValue obj, DSIter *it);
int ds_iter_next(DSVM *vm, DSIter *it, DSValue *out);
DSValue ds_format_value(DSVM *vm, DSValue v, DSString *spec);
double ds_to_number(DSVM *vm, DSValue v);
long long ds_to_int(DSVM *vm, DSValue v);
DSValue ds_make_range(DSVM *vm, double start, double stop, double step);
DSValue ds_call_method(DSVM *vm, DSValue recv, const char *name, int argc, DSValue *argv);
int ds_lookup_method(DSVM *vm, DSValue recv, DSString *name, DSValue *out_fn);
void ds_vm_traceback(DSVM *vm, char *out, size_t cap);

/* ---- Direct-Threaded Compiler (ds_compiler.c) --------------------------- */
DSCompiledArena *ds_arena_create(void);
void *ds_arena_alloc(DSCompiledArena *arena, size_t size, size_t align);
void ds_arena_retain(DSCompiledArena *arena);
void ds_arena_release(DSCompiledArena *arena);

DSFuncProto *ds_compile_module(DSVM *vm, DSModule *mod, const char *filename, const char *src, size_t len, int is_repl);

/* ---- Built-in Libraries & LÖVE2D Registration --------------------------- */
void ds_vm_set_draw_color(DSVM *vm, uint32_t argb);
void ds_register_builtins(DSVM *vm);
void ds_register_love(DSVM *vm);
void ds_register_scene(DSVM *vm);
/* The `sprite` global: 2D images for games written in DiScript. */
void ds_register_sprite(DSVM *vm);
void ds_scene_api_tick(DSVM *vm, double dt);
DSScene *ds_scene_api_get(DSVM *vm);

#endif
