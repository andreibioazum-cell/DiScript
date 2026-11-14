// src/discript/ds_scene_api.c
//
// The `scene` global: what a script uses to build and animate a world.
//
//   local base = scene.part("Base", "block")
//   base.size(20, 1, 20)
//   base.color(110, 130, 100)
//
//   local house = scene.model("House")
//   house.move(0, 1, 0)
//   local roof = scene.part("Roof", "cone")
//   roof.parent(house)
//   roof.animate(0, 4, 0, 45)     -- ease upward, turning 45 degrees
//
// A node handle is an instance of one shared class whose methods dispatch by
// the node id stored in a field, so handles are cheap to create and all behave
// the same. A handle whose node was removed becomes a no-op instead of a
// crash, because scripts keep these across frames.
#include "discript/ds_internal.h"
#include "engine/scene/ds_scene.h"
#include <stdlib.h>
#include <string.h>

/* ---- argument helpers ---------------------------------------------------- */
static double arg_num(DSVM *vm, int argc, DSValue *argv, int i, double def) {
    (void)vm;
    if (i >= argc || !ds_is_num(argv[i]))
        return def;
    return ds_as_num(argv[i]);
}

static const char *arg_str(DSVM *vm, int argc, DSValue *argv, int i, const char *def) {
    (void)vm;
    if (i >= argc || !ds_obj_is(argv[i], DS_OBJ_STRING))
        return def;
    return ((DSString *)ds_as_obj(argv[i]))->chars;
}

/* ---- scene lifetime ------------------------------------------------------ */
static DSScene *scene_of(DSVM *vm) {
    if (!vm->scene) {
        /* The scene is VM-lifetime state, not a script value: it is a large
         * fixed-size arena (a few megabytes with the node table). It must not
         * go through the object allocator, whose slabs are sized for small
         * objects -- allocating it there corrupted the heap. */
        DSScene *s = (DSScene *)malloc(sizeof(DSScene));
        if (!s)
            return NULL;
        ds_scene_init(s);
        vm->scene = s;
    }
    return vm->scene;
}

DSScene *ds_scene_api_get(DSVM *vm) { return vm ? vm->scene : NULL; }

void ds_scene_api_tick(DSVM *vm, double dt) {
    if (!vm || !vm->scene || dt <= 0.0)
        return;
    ds_scene_tick(vm->scene, dt);
    vm->scene_time += dt;
}

/* ---- handles ------------------------------------------------------------- */
#define HANDLE_FIELD "node"
#define HANDLE_GEN "gen"

static DSScene *scene_of(DSVM *vm);

/* Node index behind a handle, or DS_SCENE_NO_NODE once the node is gone.
 * Index alone is not enough: removing a node compacts the array, so a stale
 * index would address whatever moved into that slot. */
static int node_of(DSVM *vm, DSValue v) {
    if (!ds_obj_is(v, DS_OBJ_INSTANCE))
        return DS_SCENE_NO_NODE;
    DSInstance *inst = (DSInstance *)ds_as_obj(v);
    if (!inst->fields)
        return DS_SCENE_NO_NODE;
    DSValue nv, gv;
    if (!ds_dict_get(inst->fields, ds_obj_val(ds_string_const(vm, HANDLE_FIELD)), &nv) || !ds_is_num(nv))
        return DS_SCENE_NO_NODE;
    DSScene *s = scene_of(vm);
    if (!s)
        return DS_SCENE_NO_NODE;
    int idx = (int)ds_as_num(nv);
    if (!ds_dict_get(inst->fields, ds_obj_val(ds_string_const(vm, HANDLE_GEN)), &gv) || !ds_is_num(gv))
        return DS_SCENE_NO_NODE;
    unsigned stamp = (unsigned)ds_as_num(gv);
    if (ds_scene_generation(s, idx) != stamp)
        return DS_SCENE_NO_NODE; /* the node was removed */
    return idx;
}

static DSValue make_handle(DSVM *vm, int node) {
    DSScene *s = scene_of(vm);
    if (node < 0 || !s || node >= s->node_count)
        return DS_VAL_NIL;
    if (ds_is_nil(vm->scene_class) || !ds_obj_is(vm->scene_class, DS_OBJ_CLASS))
        return DS_VAL_NIL;
    DSInstance *inst = ds_instance_new(vm, (DSClass *)ds_as_obj(vm->scene_class));
    if (!inst)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(inst));
    DSDict *fields = ds_dict_new(vm, 4);
    if (fields) {
        ds_push_root(vm, ds_obj_val(fields));
        inst->fields = fields;
        ds_dict_set(vm, fields, ds_obj_val(ds_string_const(vm, HANDLE_FIELD)), ds_num((double)node));
        ds_dict_set(vm, fields, ds_obj_val(ds_string_const(vm, HANDLE_GEN)),
                    ds_num((double)ds_scene_generation(s, node)));
        ds_pop_roots(vm, 1);
    }
    ds_pop_roots(vm, 1);
    return ds_obj_val(inst);
}

/* Every method receives the handle as argv[0] because the VM binds methods
 * with the receiver already in front. */
static int self_node(DSVM *vm, int argc, DSValue *argv) {
    if (argc < 1)
        return DS_SCENE_NO_NODE;
    return node_of(vm, argv[0]);
}

static DSScene *self_scene(DSVM *vm, int argc, DSValue *argv) {
    DSScene *s = scene_of(vm);
    (void)argc;
    (void)argv;
    return s;
}

/* A method on a handle whose node was removed does nothing: scripts keep
 * references across frames and a removal is a normal thing to do, not an
 * error. Only a call on something that is not a handle at all is reported. */
#define NEED_NODE()                                                        \
    DSScene *s = self_scene(vm, argc, argv);                               \
    int node = self_node(vm, argc, argv);                                  \
    if (!s || node == DS_SCENE_NO_NODE || node >= s->node_count) {         \
        if (argc < 1 || !ds_obj_is(argv[0], DS_OBJ_INSTANCE))              \
            ds_error(vm, 0, "scene: ожидался Part, Model или Folder");     \
        return argv[0];                                                    \
    }

/* ---- construction -------------------------------------------------------- */
static int shape_from_name(const char *name) {
    if (!name)
        return DS_SHAPE_BLOCK;
    if (strcmp(name, "sphere") == 0 || strcmp(name, "ball") == 0 || strcmp(name, "шар") == 0)
        return DS_SHAPE_SPHERE;
    if (strcmp(name, "cylinder") == 0 || strcmp(name, "цилиндр") == 0)
        return DS_SHAPE_CYLINDER;
    if (strcmp(name, "wedge") == 0 || strcmp(name, "ramp") == 0 || strcmp(name, "клин") == 0)
        return DS_SHAPE_WEDGE;
    if (strcmp(name, "cone") == 0 || strcmp(name, "конус") == 0)
        return DS_SHAPE_CONE;
    return DS_SHAPE_BLOCK;
}

static DSValue make_kind(DSVM *vm, int kind, int argc, DSValue *argv, const char *def_name) {
    DSScene *s = scene_of(vm);
    if (!s) {
        ds_error(vm, 0, "scene: не хватает памяти");
        return DS_VAL_NIL;
    }
    const char *name = arg_str(vm, argc, argv, 0, def_name);
    int node = ds_scene_add(s, kind, s->root, name);
    if (node == DS_SCENE_NO_NODE) {
        ds_error(vm, 0, "scene: сцена переполнена");
        return DS_VAL_NIL;
    }
    return make_handle(vm, node);
}

static DSValue sc_part(DSVM *vm, int argc, DSValue *argv) {
    DSValue h = make_kind(vm, DS_NODE_PART, argc, argv, "Part");
    if (ds_is_nil(h))
        return h;
    int node = node_of(vm, h);
    const char *shape_name = arg_str(vm, argc, argv, 1, "block");
    if (vm->scene && node != DS_SCENE_NO_NODE)
        vm->scene->nodes[node].shape = shape_from_name(shape_name);
    return h;
}

static DSValue sc_model(DSVM *vm, int argc, DSValue *argv) { return make_kind(vm, DS_NODE_MODEL, argc, argv, "Model"); }
static DSValue sc_folder(DSVM *vm, int argc, DSValue *argv) { return make_kind(vm, DS_NODE_FOLDER, argc, argv, "Folder"); }

/* ---- transform ----------------------------------------------------------- */
static DSValue m_parent(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    if (argc < 2) {
        ds_error(vm, 0, "parent: нужен второй объект");
        return DS_VAL_NIL;
    }
    int parent = node_of(vm, argv[1]);
    if (parent == DS_SCENE_NO_NODE) {
        ds_error(vm, 0, "parent: родитель должен быть Part, Model или Folder");
        return DS_VAL_NIL;
    }
    if (!ds_scene_set_parent(s, node, parent)) {
        ds_error(vm, 0, "parent: нельзя вложить объект сам в себя");
        return DS_VAL_NIL;
    }
    return argv[0];
}

static DSValue m_move(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    s->nodes[node].position = v3((float)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                                 (float)arg_num(vm, argc, argv, 3, 0));
    s->dirty = 1;
    return argv[0];
}

static DSValue m_rotate(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    s->nodes[node].orientation = v3((float)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                                    (float)arg_num(vm, argc, argv, 3, 0));
    s->dirty = 1;
    return argv[0];
}

static DSValue m_size(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    float x = (float)arg_num(vm, argc, argv, 1, 1), y = (float)arg_num(vm, argc, argv, 2, 1),
          z = (float)arg_num(vm, argc, argv, 3, 1);
    if (x <= 0.0f || y <= 0.0f || z <= 0.0f) {
        ds_error(vm, 0, "size: размер должен быть больше нуля");
        return DS_VAL_NIL;
    }
    s->nodes[node].size = v3(x, y, z);
    s->dirty = 1;
    return argv[0];
}

static DSValue m_color(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    int r = (int)arg_num(vm, argc, argv, 1, 180), g = (int)arg_num(vm, argc, argv, 2, 180),
        b = (int)arg_num(vm, argc, argv, 3, 180);
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    s->nodes[node].color = 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    s->dirty = 1;
    return argv[0];
}

static DSValue m_transparency(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    ds_scene_set_transparency(s, node, (float)arg_num(vm, argc, argv, 1, 0));
    return argv[0];
}

static DSValue m_visible(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    int on = 1;
    if (argc > 1 && ds_is_num(argv[1]))
        on = ds_as_num(argv[1]) != 0.0;
    s->nodes[node].visible = on;
    s->dirty = 1;
    return argv[0];
}

static DSValue m_remove(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    if (node == s->root)
        return DS_VAL_NIL;
    ds_scene_remove(s, node);
    return DS_VAL_NIL;
}

/* ---- animation ----------------------------------------------------------- */
static DSValue m_animate(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    Vec3 pos = v3((float)arg_num(vm, argc, argv, 1, 0), (float)arg_num(vm, argc, argv, 2, 0),
                  (float)arg_num(vm, argc, argv, 3, 0));
    Vec3 rot = v3((float)arg_num(vm, argc, argv, 4, 0), (float)arg_num(vm, argc, argv, 5, 0),
                  (float)arg_num(vm, argc, argv, 6, 0));
    float speed = (float)arg_num(vm, argc, argv, 7, 2.0);
    const char *mode_name = arg_str(vm, argc, argv, 8, "ease");
    int mode = 0;
    if (strcmp(mode_name, "linear") == 0)
        mode = 1;
    else if (strcmp(mode_name, "overshoot") == 0 || strcmp(mode_name, "pop") == 0)
        mode = 2;
    ds_scene_animate(s, node, pos, rot, speed, mode);
    return argv[0];
}

static DSValue m_spin(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    ds_scene_spin(s, node, (float)arg_num(vm, argc, argv, 1, 90.0));
    return argv[0];
}

static DSValue m_stop(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    ds_scene_stop_animating(s, node);
    s->nodes[node].anim_mode = 0;
    return argv[0];
}

/* ---- queries ------------------------------------------------------------- */
static DSValue m_position(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    Vec3 w = ds_scene_world_position(s, node);
    DSDict *out = ds_dict_new(vm, 4);
    if (!out)
        return DS_VAL_NIL;
    ds_push_root(vm, ds_obj_val(out));
    ds_dict_set(vm, out, ds_obj_val(ds_string_const(vm, "x")), ds_num(w.x));
    ds_dict_set(vm, out, ds_obj_val(ds_string_const(vm, "y")), ds_num(w.y));
    ds_dict_set(vm, out, ds_obj_val(ds_string_const(vm, "z")), ds_num(w.z));
    ds_pop_roots(vm, 1);
    return ds_obj_val(out);
}

static DSValue m_name(DSVM *vm, int argc, DSValue *argv) {
    NEED_NODE();
    return ds_obj_val(ds_string_const(vm, s->nodes[node].name));
}

/* ---- scene-level functions ----------------------------------------------- */
static DSValue sc_find(DSVM *vm, int argc, DSValue *argv) {
    DSScene *s = scene_of(vm);
    const char *name = arg_str(vm, argc, argv, 0, NULL);
    if (!s || !name) {
        ds_error(vm, 0, "scene.find: нужно имя объекта");
        return DS_VAL_NIL;
    }
    int node = ds_scene_find(s, name);
    if (node == DS_SCENE_NO_NODE)
        return DS_VAL_NIL;
    return make_handle(vm, node);
}

static DSValue sc_count(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    DSScene *s = scene_of(vm);
    return ds_num(s ? s->node_count : 0);
}

static DSValue sc_clear(DSVM *vm, int argc, DSValue *argv) {
    (void)argc;
    (void)argv;
    DSScene *s = scene_of(vm);
    if (s)
        ds_scene_init(s);
    return DS_VAL_NIL;
}

static DSValue sc_tick(DSVM *vm, int argc, DSValue *argv) {
    DSScene *s = scene_of(vm);
    if (!s)
        return DS_VAL_NIL;
    ds_scene_tick(s, arg_num(vm, argc, argv, 0, 0.016));
    return DS_VAL_NIL;
}

static DSValue sc_ambient(DSVM *vm, int argc, DSValue *argv) {
    DSScene *s = scene_of(vm);
    if (!s)
        return DS_VAL_NIL;
    s->ambient = v3((float)arg_num(vm, argc, argv, 0, 0.45), (float)arg_num(vm, argc, argv, 1, 0.47),
                    (float)arg_num(vm, argc, argv, 2, 0.55));
    return DS_VAL_NIL;
}

/* ---- registration -------------------------------------------------------- */
static void reg_fn(DSVM *vm, DSDict *d, const char *name, DSNativeFn fn) {
    DSCFunc *f = ds_cfunc_new(vm, name, fn, NULL);
    if (!f)
        return;
    ds_push_root(vm, ds_obj_val(f));
    ds_dict_set(vm, d, ds_obj_val(ds_string_const(vm, name)), ds_obj_val(f));
    ds_pop_roots(vm, 1);
}

void ds_register_scene(DSVM *vm) {
    if (!vm)
        return;
    scene_of(vm);

    /* One class carries every node method; handles are instances of it.
     * ds_class_new already made the methods dict -- reuse it rather than
     * replacing it, and keep the class in a VM root so a collection cannot
     * free the class out from under the handles. */
    DSClass *cls = ds_class_new(vm, ds_string_const(vm, "SceneNode"), NULL);
    if (!cls)
        return;
    /* Keep the class reachable from the globals table. A temporary root is
     * popped at the end of the frame, which is not enough for an object that
     * every handle depends on for the whole run. */
    vm->scene_class = ds_obj_val(cls);
    ds_dict_set(vm, vm->builtins, ds_obj_val(ds_string_const(vm, "__SceneNode")), vm->scene_class);
    ds_push_root(vm, vm->scene_class);
    DSDict *methods = cls->methods;
    if (methods) {
        ds_push_root(vm, ds_obj_val(methods));
        reg_fn(vm, methods, "parent", m_parent);
        reg_fn(vm, methods, "move", m_move);
        reg_fn(vm, methods, "rotate", m_rotate);
        reg_fn(vm, methods, "size", m_size);
        reg_fn(vm, methods, "color", m_color);
        reg_fn(vm, methods, "transparency", m_transparency);
        reg_fn(vm, methods, "visible", m_visible);
        reg_fn(vm, methods, "remove", m_remove);
        reg_fn(vm, methods, "animate", m_animate);
        reg_fn(vm, methods, "spin", m_spin);
        reg_fn(vm, methods, "stop", m_stop);
        reg_fn(vm, methods, "position", m_position);
        reg_fn(vm, methods, "name", m_name);
        ds_pop_roots(vm, 1);
    }
    ds_pop_roots(vm, 1);

    DSDict *scene = ds_dict_new(vm, 16);
    if (!scene)
        return;
    ds_push_root(vm, ds_obj_val(scene));
    ds_dict_set(vm, vm->builtins, ds_obj_val(ds_string_const(vm, "scene")), ds_obj_val(scene));
    reg_fn(vm, scene, "part", sc_part);
    reg_fn(vm, scene, "model", sc_model);
    reg_fn(vm, scene, "folder", sc_folder);
    reg_fn(vm, scene, "find", sc_find);
    reg_fn(vm, scene, "count", sc_count);
    reg_fn(vm, scene, "clear", sc_clear);
    reg_fn(vm, scene, "tick", sc_tick);
    reg_fn(vm, scene, "ambient", sc_ambient);
    ds_pop_roots(vm, 1);
}
