// src/engine/scene/ds_scene.h
//
// The scene graph: a tree of Instances in the shape Roblox Studio uses, so a
// script can build a model out of Parts, parent them, animate them, and hand
// the whole tree to the renderer.
//
//   World (root)
//     Model "House"
//       Part "Roof"      shape=WEDGE, size=(4,2,4), color=...
//       Part "Wall"      shape=BLOCK
//       Model "Door"
//         Part "Handle"  anchored, offset from its parent
//
// Every node has a local transform (`position`, `orientation`, `scale`) and
// children are placed relative to their parent, which is what makes a model
// movable as a unit. `world_matrix()` composes the chain.
//
// Memory is a plain array-backed tree owned by the DSWorld: instances are
// indices, never raw pointers, so the tree can be reallocated as it grows and
// a stale handle degrades to "no such node" instead of a dangling pointer.
#ifndef DS_SCENE_H
#define DS_SCENE_H

#include "engine/scene/ds_math3d.h"
#include <stdint.h>

#define DS_SCENE_MAX_NODES 4096
#define DS_SCENE_MAX_VERTS 65536
#define DS_SCENE_MAX_TRIS 32768
#define DS_SCENE_MAX_NAME 48
#define DS_SCENE_NO_NODE (-1)

typedef enum {
    DS_SHAPE_BLOCK = 0, // 6 quads
    DS_SHAPE_SPHERE,    // lat/long approximation
    DS_SHAPE_CYLINDER,  // capped
    DS_SHAPE_WEDGE,     // ramp: a box with one slanted face
    DS_SHAPE_CONE,
    DS_SHAPE_COUNT
} DSShape;

typedef enum {
    DS_NODE_PART = 0,
    DS_NODE_MODEL,
    DS_NODE_FOLDER,
    DS_NODE_CAMERA,
    DS_NODE_LIGHT,
    DS_NODE_COUNT
} DSNodeKind;

// One triangle of the flat-shaded mesh the renderer consumes.
typedef struct {
    float x[3], y[3], z[3]; // the three corners in world space
    float nx, ny, nz;       // face normal, world space
    uint32_t color;         // opaque ARGB, already shaded
    int node;               // owning node, for picking/debugging
} DSTri;

typedef struct {
    int kind;                 // DSNodeKind
    int shape;                // DSShape, parts only
    int parent;               // DS_SCENE_NO_NODE for the root
    int first_child;          // intrusive child list, index linked
    int next_sibling;

    char name[DS_SCENE_MAX_NAME];
    /* Bumped on every creation, never reused for the same slot. Handles store
     * the stamp they were made with, so a handle to a removed node can be
     * recognised instead of silently addressing whichever node now occupies
     * that index after ds_scene_remove compacts the array. */
    unsigned generation;

    // Local transform, relative to the parent.
    Vec3 position;
    Vec3 orientation;         // Euler degrees, Y-X-Z
    Vec3 size;                // part dimensions before scale; also the model box
    Vec3 scale;

    uint32_t color;
    float transparency;       // 0 = opaque, 1 = invisible
    int anchored;             // purely informational; physics may use it later
    int visible;
    int cast_shadow;

    // Animation targets: when `anim_active`, the node eases toward
    // (anim_position, anim_orientation) instead of being set directly.
    int anim_active;
    Vec3 anim_position;
    Vec3 anim_orientation;
    float anim_speed;         // approach rate, units per second at 60 fps
    float anim_t;
    int anim_mode;            // 0 = ease out, 1 = linear, 2 = spring-like overshoot
} DSNode;

struct DSScene {
    DSNode nodes[DS_SCENE_MAX_NODES];
    int node_count;
    int root;
    Vec3 ambient;             // scene-wide light
    Vec3 sun_dir;             // direction *toward* the sun, normalized
    float sun_strength;
    double time;              // seconds, advanced by ds_scene_tick
    int dirty;                // set when the mesh cache must be rebuilt
    unsigned next_generation; // monotonic stamp handed to new nodes
};
typedef struct DSScene DSScene;

// ---- Lifecycle -----------------------------------------------------------
void ds_scene_init(DSScene *s);
// Stamp of a live node, or 0 when the index is out of range.
unsigned ds_scene_generation(const DSScene *s, int node);
int ds_scene_add(DSScene *s, int kind, int parent, const char *name);
void ds_scene_remove(DSScene *s, int node); // removes the subtree
int ds_scene_find(DSScene *s, const char *name); // depth-first by name
int ds_scene_set_parent(DSScene *s, int node, int parent);

// ---- Properties ----------------------------------------------------------
void ds_scene_set_transform(DSScene *s, int node, Vec3 pos, Vec3 rot_deg, Vec3 size);
void ds_scene_set_color(DSScene *s, int node, uint32_t color);
void ds_scene_set_transparency(DSScene *s, int node, float t);

// ---- Transform composition ----------------------------------------------
Mat4 ds_scene_local_matrix(const DSScene *s, int node);
Mat4 ds_scene_world_matrix(const DSScene *s, int node);
Vec3 ds_scene_world_position(const DSScene *s, int node);

// ---- Animation -----------------------------------------------------------
// Ease the node toward a target transform over the next frames. `mode` picks
// the curve: 0 = smooth approach, 1 = constant speed, 2 = slight overshoot.
void ds_scene_animate(DSScene *s, int node, Vec3 pos, Vec3 rot_deg, float speed, int mode);
void ds_scene_stop_animating(DSScene *s, int node);
// Spin a node permanently (radians per second about its own Y axis), which is
// what a rotating collectible or a fan blade wants.
void ds_scene_spin(DSScene *s, int node, float deg_per_sec);
void ds_scene_tick(DSScene *s, double dt);

// ---- Mesh building -------------------------------------------------------
// Rebuild the triangle soup for the whole visible tree. Culling is a plain
// frustum test in view space, so geometry behind the camera costs nothing.
int ds_scene_build_mesh(DSScene *s, Mat4 view_proj, Mat4 view, DSTri *out_tris, int max_tris);

#endif
