// src/engine/scene/ds_scene.c
#include "engine/scene/ds_scene.h"
#include <string.h>

/* ---- small helpers ------------------------------------------------------ */
static void copy_name(char *dst, const char *src) {
    if (!src) {
        dst[0] = '\0';
        return;
    }
    int i = 0;
    for (; i < DS_SCENE_MAX_NAME - 1 && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static float lerpf(float a, float b, float t) { return a + (b - a) * t; }

void ds_scene_init(DSScene *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->node_count = 0;
    s->root = ds_scene_add(s, DS_NODE_MODEL, DS_SCENE_NO_NODE, "World");
    s->ambient = v3(0.45f, 0.47f, 0.55f);
    s->sun_dir = v3_norm(v3(0.4f, -0.8f, 0.45f));
    s->sun_strength = 0.75f;
    s->time = 0.0;
    s->dirty = 1;
    s->next_generation = 0;
}

int ds_scene_add(DSScene *s, int kind, int parent, const char *name) {
    if (!s || s->node_count >= DS_SCENE_MAX_NODES) return DS_SCENE_NO_NODE;
    if (parent != DS_SCENE_NO_NODE && (parent < 0 || parent >= s->node_count)) return DS_SCENE_NO_NODE;

    int idx = s->node_count++;
    DSNode *n = &s->nodes[idx];
    memset(n, 0, sizeof(*n));
    n->kind = kind;
    n->shape = DS_SHAPE_BLOCK;
    n->parent = parent;
    n->first_child = DS_SCENE_NO_NODE;
    n->next_sibling = DS_SCENE_NO_NODE;
    n->position = v3(0, 0, 0);
    n->orientation = v3(0, 0, 0);
    n->size = v3(1, 1, 1);
    n->scale = v3(1, 1, 1);
    n->color = 0xFFB0B8C4u;
    n->transparency = 0.0f;
    n->visible = 1;
    n->anchored = 1;
    n->anim_speed = 6.0f;
    copy_name(n->name, name);

    if (parent != DS_SCENE_NO_NODE) {
        /* Push to the front of the parent's child list: order never matters
         * for rendering, and front insertion is O(1). */
        n->next_sibling = s->nodes[parent].first_child;
        s->nodes[parent].first_child = idx;
    }
    n->generation = ++s->next_generation;
    s->dirty = 1;
    return idx;
}

unsigned ds_scene_generation(const DSScene *s, int node) {
    if (!s || node < 0 || node >= s->node_count)
        return 0;
    return s->nodes[node].generation;
}

void ds_scene_remove(DSScene *s, int node) {
    if (!s || node < 0 || node >= s->node_count) return;
    /* Unlink from the parent's child list. */
    int p = s->nodes[node].parent;
    if (p != DS_SCENE_NO_NODE) {
        int *link = &s->nodes[p].first_child;
        while (*link != DS_SCENE_NO_NODE) {
            if (*link == node) {
                *link = s->nodes[node].next_sibling;
                break;
            }
            link = &s->nodes[*link].next_sibling;
        }
    }
    /* Remove the subtree depth-first, then compact the array so indices stay
     * dense. Remapping is done in one pass with a translation table. */
    int removed[DS_SCENE_MAX_NODES];
    memset(removed, 0, sizeof(removed));
    int stack[DS_SCENE_MAX_NODES];
    int sp = 0;
    stack[sp++] = node;
    while (sp > 0) {
        int cur = stack[--sp];
        removed[cur] = 1;
        for (int c = s->nodes[cur].first_child; c != DS_SCENE_NO_NODE; c = s->nodes[c].next_sibling)
            stack[sp++] = c;
    }
    int map[DS_SCENE_MAX_NODES];
    memset(map, 0, sizeof(map));
    int write = 0;
    for (int i = 0; i < s->node_count; i++) {
        if (removed[i]) {
            map[i] = DS_SCENE_NO_NODE;
            continue;
        }
        map[i] = write;
        if (write != i) s->nodes[write] = s->nodes[i];
        write++;
    }
    s->node_count = write;
    if (s->root != DS_SCENE_NO_NODE) s->root = map[s->root];
    for (int i = 0; i < s->node_count; i++) {
        DSNode *n = &s->nodes[i];
        n->parent = (n->parent == DS_SCENE_NO_NODE) ? DS_SCENE_NO_NODE : map[n->parent];
        n->first_child = (n->first_child == DS_SCENE_NO_NODE) ? DS_SCENE_NO_NODE : map[n->first_child];
        n->next_sibling = (n->next_sibling == DS_SCENE_NO_NODE) ? DS_SCENE_NO_NODE : map[n->next_sibling];
    }
    s->dirty = 1;
}

int ds_scene_find(DSScene *s, const char *name) {
    if (!s || !name) return DS_SCENE_NO_NODE;
    for (int i = 0; i < s->node_count; i++)
        if (strcmp(s->nodes[i].name, name) == 0) return i;
    return DS_SCENE_NO_NODE;
}

int ds_scene_set_parent(DSScene *s, int node, int parent) {
    if (!s || node < 0 || node >= s->node_count) return 0;
    if (parent == node) return 0;
    /* Refuse to build a cycle: walk up from the prospective parent. */
    for (int p = parent; p != DS_SCENE_NO_NODE; p = s->nodes[p].parent)
        if (p == node) return 0;
    int old = s->nodes[node].parent;
    if (old != DS_SCENE_NO_NODE) {
        int *link = &s->nodes[old].first_child;
        while (*link != DS_SCENE_NO_NODE) {
            if (*link == node) {
                *link = s->nodes[node].next_sibling;
                break;
            }
            link = &s->nodes[*link].next_sibling;
        }
    }
    s->nodes[node].parent = parent;
    s->nodes[node].next_sibling = DS_SCENE_NO_NODE;
    if (parent != DS_SCENE_NO_NODE) {
        s->nodes[node].next_sibling = s->nodes[parent].first_child;
        s->nodes[parent].first_child = node;
    }
    s->dirty = 1;
    return 1;
}

void ds_scene_set_transform(DSScene *s, int node, Vec3 pos, Vec3 rot_deg, Vec3 size) {
    if (!s || node < 0 || node >= s->node_count) return;
    DSNode *n = &s->nodes[node];
    if (size.x > 0.0f) n->size = size;
    n->position = pos;
    n->orientation = rot_deg;
    s->dirty = 1;
}

void ds_scene_set_color(DSScene *s, int node, uint32_t color) {
    if (!s || node < 0 || node >= s->node_count) return;
    s->nodes[node].color = color;
    s->dirty = 1;
}

void ds_scene_set_transparency(DSScene *s, int node, float t) {
    if (!s || node < 0 || node >= s->node_count) return;
    s->nodes[node].transparency = clampf(t, 0.0f, 1.0f);
    s->dirty = 1;
}

Mat4 ds_scene_local_matrix(const DSScene *s, int node) {
    if (!s || node < 0 || node >= s->node_count) return mat4_identity();
    const DSNode *n = &s->nodes[node];
    Mat4 t = mat4_translate(n->position);
    Mat4 r = mat4_euler(v3(n->orientation.x * 0.0174532925f, n->orientation.y * 0.0174532925f,
                           n->orientation.z * 0.0174532925f));
    Mat4 sc = mat4_scale(n->scale);
    return mat4_mul(mat4_mul(t, r), sc);
}

Mat4 ds_scene_world_matrix(const DSScene *s, int node) {
    Mat4 m = mat4_identity();
    /* Walk to the root collecting nodes, then compose top-down. Depth is
     * bounded by the node count, and the chain is short in practice. */
    int chain[DS_SCENE_MAX_NODES];
    int depth = 0;
    for (int n = node; n != DS_SCENE_NO_NODE && depth < DS_SCENE_MAX_NODES; n = s->nodes[n].parent)
        chain[depth++] = n;
    for (int i = depth - 1; i >= 0; i--) m = mat4_mul(m, ds_scene_local_matrix(s, chain[i]));
    return m;
}

Vec3 ds_scene_world_position(const DSScene *s, int node) {
    Mat4 m = ds_scene_world_matrix(s, node);
    return v3(m.m[0][3], m.m[1][3], m.m[2][3]);
}

void ds_scene_animate(DSScene *s, int node, Vec3 pos, Vec3 rot_deg, float speed, int mode) {
    if (!s || node < 0 || node >= s->node_count) return;
    DSNode *n = &s->nodes[node];
    n->anim_active = 1;
    n->anim_position = pos;
    n->anim_orientation = rot_deg;
    n->anim_speed = speed > 0.01f ? speed : 6.0f;
    n->anim_mode = mode;
    n->anim_t = 0.0f;
}

void ds_scene_stop_animating(DSScene *s, int node) {
    if (!s || node < 0 || node >= s->node_count) return;
    s->nodes[node].anim_active = 0;
}

void ds_scene_spin(DSScene *s, int node, float deg_per_sec) {
    if (!s || node < 0 || node >= s->node_count) return;
    s->nodes[node].anim_mode = 3; // spin is its own mode
    s->nodes[node].anim_speed = deg_per_sec;
}

void ds_scene_tick(DSScene *s, double dt) {
    if (!s || dt <= 0.0) return;
    s->time += dt;
    /* Clamp the step so a stalled frame cannot teleport an animation. Spins are
     * steady-state, so they take the real (clamped) elapsed time rather than
     * the per-frame cap: a 1 s tick means a full second of rotation. */
    float d = (float)(dt > 0.1 ? 0.1 : dt);
    float ds = (float)(dt > 1.0 ? 1.0 : dt);
    for (int i = 0; i < s->node_count; i++) {
        DSNode *n = &s->nodes[i];
        if (n->anim_mode == 3) {
            n->orientation.y += n->anim_speed * ds;
            if (n->orientation.y > 360.0f) n->orientation.y -= 360.0f;
            if (n->orientation.y < -360.0f) n->orientation.y += 360.0f;
            s->dirty = 1;
            continue;
        }
        if (!n->anim_active) continue;
        n->anim_t += d * n->anim_speed;
        float t = n->anim_t;
        if (t > 1.0f) t = 1.0f;
        /* Ease-out cubic for mode 0, linear for 1, a little overshoot for 2. */
        float e;
        if (n->anim_mode == 1) {
            e = t;
        } else if (n->anim_mode == 2) {
            float u = t - 1.0f;
            e = 1.0f + 2.70158f * u * u * u + 1.70158f * u * u;
        } else {
            float u = 1.0f - t;
            e = 1.0f - u * u * u;
        }
        n->position = v3(lerpf(n->position.x, n->anim_position.x, e),
                         lerpf(n->position.y, n->anim_position.y, e),
                         lerpf(n->position.z, n->anim_position.z, e));
        n->orientation = v3(lerpf(n->orientation.x, n->anim_orientation.x, e),
                            lerpf(n->orientation.y, n->anim_orientation.y, e),
                            lerpf(n->orientation.z, n->anim_orientation.z, e));
        if (t >= 1.0f) n->anim_active = 0;
        s->dirty = 1;
    }
}

/* ---- mesh generation ---------------------------------------------------- */
// Emit one triangle with a flat normal derived from its winding.
static void emit_tri(DSTri *tris, int *count, int max_tris, int node, Vec3 a, Vec3 b, Vec3 c, uint32_t color) {
    if (*count >= max_tris) return;
    Vec3 n = v3_norm(v3_cross(v3_sub(b, a), v3_sub(c, a)));
    DSTri *t = &tris[(*count)++];
    t->x[0] = a.x; t->y[0] = a.y; t->z[0] = a.z;
    t->x[1] = b.x; t->y[1] = b.y; t->z[1] = b.z;
    t->x[2] = c.x; t->y[2] = c.y; t->z[2] = c.z;
    t->nx = n.x; t->ny = n.y; t->nz = n.z;
    t->color = color;
    t->node = node;
}

// A quad as two triangles, given counter-clockwise corners.
static void emit_quad(DSTri *tris, int *count, int max_tris, int node, Vec3 a, Vec3 b, Vec3 c, Vec3 d, uint32_t color) {
    emit_tri(tris, count, max_tris, node, a, b, c, color);
    emit_tri(tris, count, max_tris, node, a, c, d, color);
}

static Vec3 xf_point(Mat4 m, Vec3 p) {
    Vec4 r = mat4_apply(m, (Vec4){p.x, p.y, p.z, 1.0f});
    return v3(r.x, r.y, r.z);
}

// Darken/lighten a packed colour by a scalar in [0, 2].
static uint32_t shade_color(uint32_t c, float f) {
    int a = (int)((c >> 24) & 0xFF);
    int r = (int)((c >> 16) & 0xFF);
    int g = (int)((c >> 8) & 0xFF);
    int b = (int)(c & 0xFF);
    r = (int)(r * f); if (r > 255) r = 255; if (r < 0) r = 0;
    g = (int)(g * f); if (g > 255) g = 255; if (g < 0) g = 0;
    b = (int)(b * f); if (b > 255) b = 255; if (b < 0) b = 0;
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static void build_block(DSTri *tris, int *count, int max_tris, int node, Mat4 m, Vec3 color_unused, uint32_t color) {
    (void)color_unused;
    Vec3 h = v3(0.5f, 0.5f, 0.5f);
    Vec3 p000 = xf_point(m, v3(-h.x, -h.y, -h.z));
    Vec3 p001 = xf_point(m, v3(-h.x, -h.y, h.z));
    Vec3 p010 = xf_point(m, v3(-h.x, h.y, -h.z));
    Vec3 p011 = xf_point(m, v3(-h.x, h.y, h.z));
    Vec3 p100 = xf_point(m, v3(h.x, -h.y, -h.z));
    Vec3 p101 = xf_point(m, v3(h.x, -h.y, h.z));
    Vec3 p110 = xf_point(m, v3(h.x, h.y, -h.z));
    Vec3 p111 = xf_point(m, v3(h.x, h.y, h.z));
    // Three shades give the box readable edges under flat shading.
    uint32_t cx = shade_color(color, 0.82f);
    uint32_t cy = shade_color(color, 1.0f);
    uint32_t cz = shade_color(color, 0.9f);
    emit_quad(tris, count, max_tris, node, p001, p101, p111, p011, cz); // +z
    emit_quad(tris, count, max_tris, node, p100, p000, p010, p110, cz); // -z
    emit_quad(tris, count, max_tris, node, p101, p100, p110, p111, cx); // +x
    emit_quad(tris, count, max_tris, node, p000, p001, p011, p010, cx); // -x
    emit_quad(tris, count, max_tris, node, p011, p111, p110, p010, cy); // +y
    emit_quad(tris, count, max_tris, node, p000, p100, p101, p001, cy); // -y
}

static void build_wedge(DSTri *tris, int *count, int max_tris, int node, Mat4 m, uint32_t color) {
    // A ramp: full height at -z, zero height at +z.
    Vec3 p000 = xf_point(m, v3(-0.5f, -0.5f, -0.5f));
    Vec3 p100 = xf_point(m, v3(0.5f, -0.5f, -0.5f));
    Vec3 p101 = xf_point(m, v3(0.5f, -0.5f, 0.5f));
    Vec3 p001 = xf_point(m, v3(-0.5f, -0.5f, 0.5f));
    Vec3 p010 = xf_point(m, v3(-0.5f, 0.5f, -0.5f));
    Vec3 p110 = xf_point(m, v3(0.5f, 0.5f, -0.5f));
    uint32_t side = shade_color(color, 0.8f);
    uint32_t ramp = shade_color(color, 1.05f);
    emit_quad(tris, count, max_tris, node, p001, p101, p100, p000, shade_color(color, 0.92f)); // bottom
    emit_quad(tris, count, max_tris, node, p000, p010, p110, p100, side);                        // back
    emit_tri(tris, count, max_tris, node, p001, p010, p000, side);                                // -x
    emit_tri(tris, count, max_tris, node, p101, p100, p110, side);                                // +x
    emit_quad(tris, count, max_tris, node, p001, p010, p110, p101, ramp);                        // slope
}

static void build_sphere(DSTri *tris, int *count, int max_tris, int node, Mat4 m, uint32_t color) {
    const int rings = 8, segs = 12;
    for (int r = 0; r < rings; r++) {
        float v0 = (float)r / rings, v1 = (float)(r + 1) / rings;
        float t0 = v0 * 3.14159265f, t1 = v1 * 3.14159265f;
        for (int s = 0; s < segs; s++) {
            float u0 = (float)s / segs * 6.28318531f, u1 = (float)(s + 1) / segs * 6.28318531f;
            Vec3 a = v3(-ds_cosf(u0) * ds_sinf(t0) * 0.5f, ds_cosf(t0) * 0.5f, ds_sinf(u0) * ds_sinf(t0) * 0.5f);
            Vec3 b = v3(-ds_cosf(u1) * ds_sinf(t0) * 0.5f, ds_cosf(t0) * 0.5f, ds_sinf(u1) * ds_sinf(t0) * 0.5f);
            Vec3 c = v3(-ds_cosf(u1) * ds_sinf(t1) * 0.5f, ds_cosf(t1) * 0.5f, ds_sinf(u1) * ds_sinf(t1) * 0.5f);
            Vec3 d = v3(-ds_cosf(u0) * ds_sinf(t1) * 0.5f, ds_cosf(t1) * 0.5f, ds_sinf(u0) * ds_sinf(t1) * 0.5f);
            uint32_t sc = shade_color(color, 0.8f + 0.25f * ds_cosf(t0));
            emit_quad(tris, count, max_tris, node, xf_point(m, a), xf_point(m, b), xf_point(m, c), xf_point(m, d), sc);
        }
    }
}

static void build_cylinder(DSTri *tris, int *count, int max_tris, int node, Mat4 m, uint32_t color) {
    const int segs = 14;
    uint32_t side_c = shade_color(color, 0.88f);
    for (int s = 0; s < segs; s++) {
        float u0 = (float)s / segs * 6.28318531f, u1 = (float)(s + 1) / segs * 6.28318531f;
        Vec3 b0 = xf_point(m, v3(ds_cosf(u0) * 0.5f, -0.5f, ds_sinf(u0) * 0.5f));
        Vec3 b1 = xf_point(m, v3(ds_cosf(u1) * 0.5f, -0.5f, ds_sinf(u1) * 0.5f));
        Vec3 t0 = xf_point(m, v3(ds_cosf(u0) * 0.5f, 0.5f, ds_sinf(u0) * 0.5f));
        Vec3 t1 = xf_point(m, v3(ds_cosf(u1) * 0.5f, 0.5f, ds_sinf(u1) * 0.5f));
        emit_quad(tris, count, max_tris, node, b0, b1, t1, t0, side_c);
        if (s > 0) {
            Vec3 ctop = xf_point(m, v3(0, 0.5f, 0));
            Vec3 cbot = xf_point(m, v3(0, -0.5f, 0));
            emit_tri(tris, count, max_tris, node, ctop, t0, t1, shade_color(color, 1.0f));
            emit_tri(tris, count, max_tris, node, cbot, b1, b0, shade_color(color, 0.7f));
        }
    }
}

static void build_cone(DSTri *tris, int *count, int max_tris, int node, Mat4 m, uint32_t color) {
    const int segs = 14;
    Vec3 apex = xf_point(m, v3(0, 0.5f, 0));
    Vec3 base_c = xf_point(m, v3(0, -0.5f, 0));
    for (int s = 0; s < segs; s++) {
        float u0 = (float)s / segs * 6.28318531f, u1 = (float)(s + 1) / segs * 6.28318531f;
        Vec3 b0 = xf_point(m, v3(ds_cosf(u0) * 0.5f, -0.5f, ds_sinf(u0) * 0.5f));
        Vec3 b1 = xf_point(m, v3(ds_cosf(u1) * 0.5f, -0.5f, ds_sinf(u1) * 0.5f));
        emit_tri(tris, count, max_tris, node, b0, b1, apex, shade_color(color, 0.95f));
        emit_tri(tris, count, max_tris, node, base_c, b1, b0, shade_color(color, 0.7f));
    }
}

static void build_node_mesh(DSScene *s, int node, Mat4 parent_m, DSTri *tris, int *count, int max_tris) {
    if (node < 0 || node >= s->node_count) return;
    DSNode *n = &s->nodes[node];
    if (!n->visible) return;

    Mat4 local = ds_scene_local_matrix(s, node);
    Mat4 world = mat4_mul(parent_m, local);

    if (n->kind == DS_NODE_PART && n->transparency < 0.999f) {
        Mat4 sized = mat4_mul(world, mat4_scale(n->size));
        uint32_t c = n->color;
        switch (n->shape) {
        case DS_SHAPE_SPHERE: build_sphere(tris, count, max_tris, node, sized, c); break;
        case DS_SHAPE_CYLINDER: build_cylinder(tris, count, max_tris, node, sized, c); break;
        case DS_SHAPE_WEDGE: build_wedge(tris, count, max_tris, node, sized, c); break;
        case DS_SHAPE_CONE: build_cone(tris, count, max_tris, node, sized, c); break;
        case DS_SHAPE_BLOCK:
        default: build_block(tris, count, max_tris, node, sized, v3(0, 0, 0), c); break;
        }
    }
    for (int c = n->first_child; c != DS_SCENE_NO_NODE; c = s->nodes[c].next_sibling)
        build_node_mesh(s, c, world, tris, count, max_tris);
}

int ds_scene_build_mesh(DSScene *s, Mat4 view_proj, Mat4 view, DSTri *out_tris, int max_tris) {
    (void)view_proj;
    (void)view;
    if (!s || !out_tris || max_tris <= 0) return 0;
    int count = 0;
    Mat4 identity = mat4_identity();
    for (int c = s->nodes[s->root].first_child; c != DS_SCENE_NO_NODE; c = s->nodes[c].next_sibling)
        build_node_mesh(s, c, identity, out_tris, &count, max_tris);
    s->dirty = 0;
    return count;
}
