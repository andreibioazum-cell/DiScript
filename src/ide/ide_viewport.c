// src/ide/ide_viewport.c
#include "ide/ide_viewport.h"
#include "engine/runtime.h"
#include <string.h>

void ide_viewport_init(IDEViewport *v) {
    if (!v)
        return;
    memset(v, 0, sizeof(*v));
    v->width = 240;
    v->height = 180;
    v->block = 2;
    if (v->width > IDE_VP_MAX_W) v->width = IDE_VP_MAX_W;
    if (v->height > IDE_VP_MAX_H) v->height = IDE_VP_MAX_H;
    ds_r3d_init(&v->r, v->width, v->height, v->color, v->depth);
    ds_camera_default(&v->camera);
    v->visible = 1;
    v->show_grid = 1;
}

void ide_viewport_resize(IDEViewport *v, int screen_w, int screen_h, int block) {
    if (!v)
        return;
    if (block < 1) block = 1;
    int w = screen_w / block;
    int h = screen_h / block;
    if (w < 16) w = 16;
    if (h < 16) h = 16;
    /* The framebuffer is a fixed-size array in this struct, so a request for
     * more than it holds has to be clamped -- oversizing it silently wrote
     * past the end of the arrays. */
    if (w > IDE_VP_MAX_W) w = IDE_VP_MAX_W;
    if (h > IDE_VP_MAX_H) h = IDE_VP_MAX_H;
    if (w == v->width && h == v->height && block == v->block)
        return;
    v->width = w;
    v->height = h;
    v->block = block;
    ds_r3d_init(&v->r, w, h, v->color, v->depth);
}

void ide_viewport_focus(IDEViewport *v, DSScene *scene) {
    if (!v || !scene)
        return;
    /* Frame the whole tree: centre on the bounding box of all parts and pull
     * back far enough that the box fits the vertical field of view. */
    Aabb box = aabb_empty();
    Aabb all = aabb_empty();
    for (int i = 0; i < scene->node_count; i++) {
        if (scene->nodes[i].kind != DS_NODE_PART || !scene->nodes[i].visible)
            continue;
        Vec3 c = ds_scene_world_position(scene, i);
        Vec3 h = v3_scale(scene->nodes[i].size, 0.5f);
        all = aabb_add_point(all, v3_sub(c, h));
        all = aabb_add_point(all, v3_add(c, h));
        /* A large flat slab near the bottom of the scene is a ground plane, not
         * the subject. Counting it would frame the horizon and shrink whatever
         * the script actually built. */
        int flat = scene->nodes[i].size.y < 0.35f * (scene->nodes[i].size.x > scene->nodes[i].size.z
                                                         ? scene->nodes[i].size.x
                                                         : scene->nodes[i].size.z);
        if (flat && scene->nodes[i].size.x >= 8.0f)
            continue;
        box = aabb_add_point(box, v3_sub(c, h));
        box = aabb_add_point(box, v3_add(c, h));
    }
    if (!aabb_valid(box) && aabb_valid(all))
        box = all; /* only ground in the scene: fall back to everything */
    if (!aabb_valid(box)) {
        v->camera.target = v3(0, 0, 0);
        v->camera.distance = 9.0f;
        return;
    }
    v->camera.target = aabb_center(box);
    float radius = aabb_radius(box);
    if (radius < 0.6f) radius = 0.6f;
    /* distance so the sphere subtends the vertical fov, with a margin */
    v->camera.distance = radius / ds_sinf(v->camera.fov_deg * 0.5f * 0.0174532925f) * 1.25f;
    /* Look down on the subject. A ground plane extends in all directions, so
     * from a low angle its near edge fills the bottom of the frame and its far
     * edge climbs toward the horizon. Looking down turns it into a floor the
     * subject sits on. */
    if (v->camera.pitch_deg < 30.0f)
        v->camera.pitch_deg = 32.0f;
    /* If a ground slab is much wider than the subject, the camera would sit
     * inside its footprint even at a comfortable distance. Push the camera back
     * to just beyond the slab's reach so the whole floor reads as a base. */
    float ground_reach = 0.0f;
    for (int i = 0; i < scene->node_count; i++) {
        const DSNode *n = &scene->nodes[i];
        if (n->kind != DS_NODE_PART || !n->visible)
            continue;
        if (!(n->size.y < 0.35f * (n->size.x > n->size.z ? n->size.x : n->size.z)))
            continue;
        float reach = 0.5f * (n->size.x > n->size.z ? n->size.x : n->size.z);
        Vec3 c = ds_scene_world_position(scene, i);
        float d = v3_len(v3_sub(c, v->camera.target)) + reach;
        if (d > ground_reach)
            ground_reach = d;
    }
    /* Only nudge, never let a decorative floor dictate the framing: the
     * subject stays the thing you can actually see. */
    if (ground_reach > 0.0f) {
        float wanted = ground_reach * 0.85f;
        if (v->camera.distance < wanted)
            v->camera.distance = wanted;
        float cap = radius * 6.0f;
        if (cap > 2.0f && v->camera.distance > cap)
            v->camera.distance = cap;
    }
}

/* Draw the framebuffer as blocks. Rows of identical colour are merged into one
 * rect, which cuts the draw-call count dramatically on flat-shaded scenes
 * (sky, ground, large faces) and keeps the editor's 2D path fast. */
void ide_viewport_draw(IDEViewport *v, DSScene *scene, float x, float y, float w, float h) {
    if (!v || !v->visible)
        return;
    /* Fit the framebuffer into the available rectangle without stretching the
     * aspect ratio: letterbox like a real viewport. */
    float aspect = (float)v->width / (float)v->height;
    float dw = w, dh = w / aspect;
    if (dh > h) {
        dh = h;
        dw = h * aspect;
    }
    float ox = x + (w - dw) * 0.5f;
    float oy = y + (h - dh) * 0.5f;

    if (scene) {
        ds_r3d_clear(&v->r);
        Vec3 eye = ds_camera_eye(&v->camera);
        v->stats_tris = ds_r3d_draw_scene(&v->r, scene, eye, v->camera.target, v3(0, 1, 0),
                                          v->camera.fov_deg, v->camera.znear, v->camera.zfar);
    }

    float px = dw / (float)v->width;
    float py = dh / (float)v->height;
    int cell = v->block;
    for (int by = 0; by < v->height; by += cell) {
        int row_end = by + cell;
        if (row_end > v->height) row_end = v->height;
        for (int bx = 0; bx < v->width; bx += cell) {
            uint32_t c = v->color[by * v->width + bx];
            int run = cell;
            while (bx + run < v->width && run < 32 && v->color[by * v->width + bx + run] == c)
                run += cell;
            int col_end = bx + run;
            if (col_end > v->width) col_end = v->width;
            rect(ox + bx * px, oy + by * py, (col_end - bx) * px, (row_end - by) * py, c);
            bx = col_end - cell;
        }
    }
    (void)ox;
    (void)oy;
}

int ide_viewport_pointer(IDEViewport *v, int pressed, float x, float y) {
    if (!v || !v->visible)
        return 0;
    if (pressed == 1) {
        v->last_x = (int)x;
        v->last_y = (int)y;
        v->orbit_active = 1;
        return 1;
    }
    if (pressed == 0 && v->orbit_active) {
        float dx = x - (float)v->last_x;
        float dy = y - (float)v->last_y;
        v->last_x = (int)x;
        v->last_y = (int)y;
        ds_camera_orbit(&v->camera, dx * 0.4f, dy * 0.4f);
        return 1;
    }
    if (pressed == -1) {
        v->orbit_active = 0;
        return 1;
    }
    return 0;
}

void ide_viewport_zoom(IDEViewport *v, float delta) {
    if (!v)
        return;
    ds_camera_zoom(&v->camera, delta);
}
