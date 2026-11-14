// tools/host_test/test_scene.c
//
// Headless tests for the scene graph: parenting, transform composition,
// animated easing, removal/compaction, and mesh generation. These run on the
// host and need no GPU, so they check the maths rather than pixels.
#include "engine/scene/ds_scene.h"
#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond, ...)                                                        \
    do {                                                                        \
        checks++;                                                               \
        if (!(cond)) {                                                          \
            failures++;                                                         \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                       \
            printf(__VA_ARGS__);                                                \
            printf("\n");                                                       \
        }                                                                       \
    } while (0)

static int near(float a, float b, float eps) { return (a > b ? a - b : b - a) <= eps; }

static void test_hierarchy(void) {
    DSScene s;
    ds_scene_init(&s);
    int model = ds_scene_add(&s, DS_NODE_MODEL, s.root, "House");
    int part = ds_scene_add(&s, DS_NODE_PART, model, "Wall");
    CHECK(model >= 0 && part >= 0, "add returned a node");
    CHECK(s.nodes[part].parent == model, "child records its parent");
    CHECK(s.nodes[model].parent == s.root, "model sits under the world");

    // Moving the parent must move the child, since transforms compose.
    ds_scene_set_transform(&s, model, v3(10, 0, 0), v3(0, 0, 0), v3(1, 1, 1));
    ds_scene_set_transform(&s, part, v3(2, 0, 0), v3(0, 0, 0), v3(1, 1, 1));
    Vec3 w = ds_scene_world_position(&s, part);
    CHECK(near(w.x, 12.0f, 1e-4f), "child world x should be 12, got %f", w.x);

    // A 90 degree parent rotation should swing the child onto -z.
    ds_scene_set_transform(&s, model, v3(0, 0, 0), v3(0, 90, 0), v3(1, 1, 1));
    w = ds_scene_world_position(&s, part);
    CHECK(near(w.z, -2.0f, 1e-3f), "rotated child world z should be -2, got %f", w.z);

    CHECK(ds_scene_find(&s, "House") == model, "find by name");
    CHECK(ds_scene_find(&s, "Nope") == DS_SCENE_NO_NODE, "missing name returns NO_NODE");
}

static void test_cycle_guard(void) {
    DSScene s;
    ds_scene_init(&s);
    int a = ds_scene_add(&s, DS_NODE_MODEL, s.root, "A");
    int b = ds_scene_add(&s, DS_NODE_MODEL, a, "B");
    // Making A a child of B would create a cycle and must be refused.
    CHECK(ds_scene_set_parent(&s, a, b) == 0, "cycle is refused");
    CHECK(ds_scene_set_parent(&s, b, s.root) == 1, "legal reparent works");
    CHECK(s.nodes[b].parent == s.root, "reparent updated the parent");
}

static void test_remove(void) {
    DSScene s;
    ds_scene_init(&s);
    int a = ds_scene_add(&s, DS_NODE_MODEL, s.root, "A");
    int b = ds_scene_add(&s, DS_NODE_PART, a, "B");
    ds_scene_add(&s, DS_NODE_PART, s.root, "C");
    (void)b;
    int before = s.node_count;
    ds_scene_remove(&s, a); // removes A and B
    CHECK(s.node_count == before - 2, "subtree removed (%d -> %d)", before, s.node_count);
    CHECK(ds_scene_find(&s, "C") != DS_SCENE_NO_NODE, "surviving node still findable");
    CHECK(ds_scene_find(&s, "A") == DS_SCENE_NO_NODE, "removed node is gone");
}

static void test_animation(void) {
    DSScene s;
    ds_scene_init(&s);
    int p = ds_scene_add(&s, DS_NODE_PART, s.root, "Mover");
    ds_scene_set_transform(&s, p, v3(0, 0, 0), v3(0, 0, 0), v3(1, 1, 1));
    ds_scene_animate(&s, p, v3(10, 0, 0), v3(0, 0, 0), 10.0f, 0);
    // A single 1/60 s step must move it part of the way, not all of it.
    ds_scene_tick(&s, 1.0 / 60.0);
    float x1 = s.nodes[p].position.x;
    CHECK(x1 > 0.0f && x1 < 10.0f, "animation is in flight, x=%f", x1);
    // After enough time it lands exactly on target and deactivates.
    for (int i = 0; i < 200; i++) ds_scene_tick(&s, 1.0 / 60.0);
    CHECK(near(s.nodes[p].position.x, 10.0f, 1e-3f), "animation reached target, x=%f", s.nodes[p].position.x);
    CHECK(!s.nodes[p].anim_active, "animation switched itself off");
}

static void test_spin(void) {
    DSScene s;
    ds_scene_init(&s);
    int p = ds_scene_add(&s, DS_NODE_PART, s.root, "Fan");
    ds_scene_spin(&s, p, 90.0f); // 90 deg per second
    ds_scene_tick(&s, 1.0);
    CHECK(near(s.nodes[p].orientation.y, 90.0f, 0.5f), "one second is a quarter turn, got %f",
          s.nodes[p].orientation.y);
    // It must wrap rather than grow without bound.
    for (int i = 0; i < 20; i++) ds_scene_tick(&s, 1.0);
    CHECK(s.nodes[p].orientation.y <= 360.0f && s.nodes[p].orientation.y >= -360.0f,
          "spin wraps, got %f", s.nodes[p].orientation.y);
}

static void test_mesh(void) {
    DSScene s;
    ds_scene_init(&s);
    int p = ds_scene_add(&s, DS_NODE_PART, s.root, "Box");
    s.nodes[p].shape = DS_SHAPE_BLOCK;
    DSTri tris[512];
    Mat4 vp = mat4_identity();
    int n = ds_scene_build_mesh(&s, vp, vp, tris, 512);
    CHECK(n == 12, "a block is 12 triangles, got %d", n);
    for (int i = 0; i < n; i++)
        CHECK(tris[i].color >> 24 == 0xFF, "mesh colours are opaque");

    // Every shape must produce geometry, and the vertex positions must span
    // the size that was asked for.
    for (int shape = 0; shape < DS_SHAPE_COUNT; shape++) {
        s.nodes[p].shape = shape;
        s.nodes[p].size = v3(2, 2, 2);
        n = ds_scene_build_mesh(&s, vp, vp, tris, 512);
        CHECK(n > 0, "shape %d produced %d triangles", shape, n);
        Aabb b = aabb_empty();
        for (int i = 0; i < n; i++)
            for (int k = 0; k < 3; k++) b = aabb_add_point(b, v3(tris[i].x[k], tris[i].y[k], tris[i].z[k]));
        CHECK(near(b.max.x - b.min.x, 2.0f, 2e-2f), "shape %d width %f", shape, b.max.x - b.min.x);
    }

    // Invisible parts contribute nothing.
    s.nodes[p].visible = 0;
    s.nodes[p].shape = DS_SHAPE_BLOCK;
    CHECK(ds_scene_build_mesh(&s, vp, vp, tris, 512) == 0, "hidden part is skipped");
    s.nodes[p].visible = 1;
    // The triangle budget is respected.
    CHECK(ds_scene_build_mesh(&s, vp, vp, tris, 4) == 4, "budget clamped");
}

static void test_math(void) {
    CHECK(near(v3_len(v3(3, 4, 0)), 5.0f, 1e-5f), "length");
    CHECK(near(ds_sinf(0.0f), 0.0f, 1e-6f), "sin(0)");
    CHECK(near(ds_cosf(0.0f), 1.0f, 1e-6f), "cos(0)");
    CHECK(near(ds_sinf(1.5707963f), 1.0f, 1e-5f), "sin(pi/2)");
    CHECK(near(ds_cosf(3.1415927f), -1.0f, 1e-5f), "cos(pi)");
    // Look-at must place the eye at the origin of view space.
    Mat4 v = mat4_look_at(v3(0, 0, 5), v3(0, 0, 0), v3(0, 1, 0));
    Vec4 p = mat4_apply(v, (Vec4){0, 0, 5, 1});
    CHECK(near(p.x, 0.0f, 1e-4f) && near(p.y, 0.0f, 1e-4f) && near(p.z, 0.0f, 1e-4f),
          "eye maps to origin, got %f %f %f", p.x, p.y, p.z);
    // A point straight ahead lands in front of the camera (negative z).
    Vec4 ahead = mat4_apply(v, (Vec4){0, 0, 0, 1});
    CHECK(ahead.z < 0.0f, "target is in front of the eye, z=%f", ahead.z);
}

int main(void) {
    printf("scene graph tests\n");
    test_math();
    test_hierarchy();
    test_cycle_guard();
    test_remove();
    test_animation();
    test_spin();
    test_mesh();
    printf("%d/%d passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
