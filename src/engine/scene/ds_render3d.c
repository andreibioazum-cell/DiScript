// src/engine/scene/ds_render3d.c
#include "engine/scene/ds_render3d.h"
#include <string.h>

#define MAX_CLIP_VERTS 8

typedef struct {
    float x, y, z, w; // view-space position
    uint32_t color;
    float invz;
} R3Vert;

int ds_r3d_init(DSRender3D *r, int width, int height, uint32_t *color, float *depth) {
    if (!r)
        return 0;
    if (!color || !depth || width <= 0 || height <= 0 || width > DS_R3D_MAX_DIM || height > DS_R3D_MAX_DIM) {
        /* Leave the renderer in a clearly unusable state so every draw call
         * short-circuits rather than walking off the end of a buffer. */
        r->width = r->height = 0;
        r->color = NULL;
        r->depth = NULL;
        r->clear = 0xFF10131Au;
        r->stats_tris = r->stats_pixels = r->stats_culled = 0;
        return 0;
    }
    r->width = width;
    r->height = height;
    r->color = color;
    r->depth = depth;
    r->clear = 0xFF10131Au;
    r->stats_tris = r->stats_pixels = r->stats_culled = 0;
    return 1;
}

void ds_r3d_clear(DSRender3D *r) {
    if (!r || !r->color || !r->depth) return;
    int n = r->width * r->height;
    for (int i = 0; i < n; i++) {
        r->color[i] = r->clear;
        r->depth[i] = 0.0f; // 0 is infinitely far in the 1/z convention
    }
    r->stats_tris = r->stats_pixels = r->stats_culled = 0;
}

/* ---- camera -------------------------------------------------------------- */
void ds_camera_default(DSCamera *c) {
    if (!c) return;
    c->target = v3(0, 0, 0);
    c->yaw_deg = 35.0f;
    c->pitch_deg = 22.0f;
    c->distance = 9.0f;
    c->fov_deg = 60.0f;
    c->znear = 0.1f;
    c->zfar = 500.0f;
}

Vec3 ds_camera_eye(const DSCamera *c) {
    float yaw = c->yaw_deg * 0.0174532925f;
    float pitch = c->pitch_deg * 0.0174532925f;
    float cp = ds_cosf(pitch);
    /* Pitch is elevation: +90 looks straight down from above, -90 from below.
     * The eye sits on a sphere of radius `distance` around the target. */
    Vec3 off = v3(-ds_sinf(yaw) * cp, ds_sinf(pitch), -ds_cosf(yaw) * cp);
    return v3_add(c->target, v3_scale(off, c->distance));
}

Mat4 ds_camera_view(const DSCamera *c) {
    return mat4_look_at(ds_camera_eye(c), c->target, v3(0, 1, 0));
}

Mat4 ds_camera_proj(const DSCamera *c, float aspect) {
    return mat4_perspective(c->fov_deg * 0.0174532925f, aspect, c->znear, c->zfar);
}

void ds_camera_orbit(DSCamera *c, float dyaw, float dpitch) {
    if (!c) return;
    c->yaw_deg += dyaw;
    c->pitch_deg += dpitch;
    if (c->pitch_deg > 89.0f) c->pitch_deg = 89.0f;
    if (c->pitch_deg < -89.0f) c->pitch_deg = -89.0f;
}

void ds_camera_zoom(DSCamera *c, float delta) {
    if (!c) return;
    c->distance *= (delta < 0.0f ? 0.85f : 1.18f);
    if (c->distance < 0.5f) c->distance = 0.5f;
    if (c->distance > 400.0f) c->distance = 400.0f;
}

void ds_camera_pan(DSCamera *c, float dx, float dy) {
    if (!c) return;
    /* Pan in the camera's own right/up plane so dragging feels direct. */
    float yaw = c->yaw_deg * 0.0174532925f;
    Vec3 right = v3(ds_cosf(yaw), 0.0f, -ds_sinf(yaw));
    Vec3 up = v3(0, 1, 0);
    float k = c->distance * 0.0025f;
    c->target = v3_add(c->target, v3_add(v3_scale(right, -dx * k), v3_scale(up, dy * k)));
}

/* ---- projection ---------------------------------------------------------- */
static int project_view(const DSRender3D *r, Mat4 proj, Vec4 vp, float *sx, float *sy, float *invz) {
    float w = vp.w;
    if (w <= 1e-6f) return 0;
    float ndc_x = vp.x / w;
    float ndc_y = vp.y / w;
    *sx = (ndc_x * 0.5f + 0.5f) * (float)r->width;
    /* NDC +y points up, screen +y points down. */
    *sy = (0.5f - ndc_y * 0.5f) * (float)r->height;
    *invz = 1.0f / w;
    return 1;
}

int ds_r3d_project(const DSRender3D *r, Mat4 view, Mat4 proj, Vec3 world, float *out_x, float *out_y) {
    Vec4 vp = mat4_apply(view, (Vec4){world.x, world.y, world.z, 1.0f});
    if (vp.z >= -1e-4f) return 0; // at or behind the camera
    Vec4 cp = mat4_apply(proj, vp);
    float invz;
    return project_view(r, proj, cp, out_x, out_y, &invz);
}

/* ---- rasterisation ------------------------------------------------------- */
static void fill_tri(DSRender3D *r, const R3Vert *a, const R3Vert *b, const R3Vert *c) {
    float minx = a->x < b->x ? (a->x < c->x ? a->x : c->x) : (b->x < c->x ? b->x : c->x);
    float maxx = a->x > b->x ? (a->x > c->x ? a->x : c->x) : (b->x > c->x ? b->x : c->x);
    float miny = a->y < b->y ? (a->y < c->y ? a->y : c->y) : (b->y < c->y ? b->y : c->y);
    float maxy = a->y > b->y ? (a->y > c->y ? a->y : c->y) : (b->y > c->y ? b->y : c->y);
    if (maxx < 0 || maxy < 0 || minx >= (float)r->width || miny >= (float)r->height) return;

    int x0 = (int)minx, x1 = (int)maxx + 1;
    int y0 = (int)miny, y1 = (int)maxy + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > r->width) x1 = r->width;
    if (y1 > r->height) y1 = r->height;

    float d = (b->y - c->y) * (a->x - c->x) + (c->x - b->x) * (a->y - c->y);
    if (d > -1e-9f && d < 1e-9f) return; // degenerate
    float inv_d = 1.0f / d;

    uint32_t cr = (a->color >> 16) & 0xFF, cg = (a->color >> 8) & 0xFF, cb = a->color & 0xFF;
    int px_count = 0;
    for (int y = y0; y < y1; y++) {
        float fy = (float)y + 0.5f;
        for (int x = x0; x < x1; x++) {
            float fx = (float)x + 0.5f;
            float w0 = ((b->y - c->y) * (fx - c->x) + (c->x - b->x) * (fy - c->y)) * inv_d;
            float w1 = ((c->y - a->y) * (fx - c->x) + (a->x - c->x) * (fy - c->y)) * inv_d;
            float w2 = 1.0f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) continue;
            float invz = w0 * a->invz + w1 * b->invz + w2 * c->invz;
            int idx = y * r->width + x;
            if (invz <= r->depth[idx]) continue; // already nearer
            r->depth[idx] = invz;
            /* Cheap depth cue: distant faces blend toward the background.
             * invz is 1/distance, so the fog has to shrink as invz grows --
             * using invz directly darkened near surfaces and brightened far
             * ones, which reads as completely flat. */
            float dist = 1.0f / invz;
            float fog = 1.0f;
            const float fog_start = 12.0f, fog_end = 70.0f;
            if (dist > fog_start) {
                float t = (dist - fog_start) / (fog_end - fog_start);
                if (t > 1.0f) t = 1.0f;
                fog = 1.0f - t * 0.45f;
            }
            uint32_t rr = (uint32_t)(cr * fog), gg = (uint32_t)(cg * fog), bb = (uint32_t)(cb * fog);
            r->color[idx] = 0xFF000000u | (rr << 16) | (gg << 8) | bb;
            px_count++;
        }
    }
    r->stats_pixels += px_count;
}

// Clip a triangle against the near plane (view space z < -znear).
static int clip_near(const R3Vert *in, int n, R3Vert *out, float znear) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        const R3Vert *a = &in[i];
        const R3Vert *b = &in[(i + 1) % n];
        int a_in = a->z <= -znear;
        int b_in = b->z <= -znear;
        if (a_in) out[m++] = *a;
        if (a_in != b_in) {
            float t = (-znear - a->z) / (b->z - a->z);
            R3Vert v;
            v.x = a->x + (b->x - a->x) * t;
            v.y = a->y + (b->y - a->y) * t;
            v.z = a->z + (b->z - a->z) * t;
            v.w = a->w + (b->w - a->w) * t;
            v.color = a->color;
            v.invz = 0.0f;
            out[m++] = v;
        }
        if (m >= MAX_CLIP_VERTS - 1) break;
    }
    return m;
}

void ds_r3d_draw_ex(DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj, float znear) {
    if (!r || !r->color || !r->depth || r->width <= 0 || r->height <= 0 || !tris) return;
    for (int i = 0; i < count; i++) {
        const DSTri *t = &tris[i];
        Vec3 p[3] = {v3(t->x[0], t->y[0], t->z[0]), v3(t->x[1], t->y[1], t->z[1]),
                     v3(t->x[2], t->y[2], t->z[2])};
        Vec4 vv[3];
        for (int k = 0; k < 3; k++) vv[k] = mat4_apply(view, (Vec4){p[k].x, p[k].y, p[k].z, 1.0f});

        /* Backface culling. In view space the eye is at the origin and looks
         * down -z, so a triangle faces us when its geometric normal points back
         * toward the eye: dot(normal, a - eye) < 0, i.e. dot(normal, a) < 0.
         * Using the eye vector also handles triangles off to the side, which a
         * pure z-sign test gets wrong near the edge of a wide field of view. */
        Vec3 v0 = v3(vv[1].x - vv[0].x, vv[1].y - vv[0].y, vv[1].z - vv[0].z);
        Vec3 v1 = v3(vv[2].x - vv[0].x, vv[2].y - vv[0].y, vv[2].z - vv[0].z);
        Vec3 nrm = v3_cross(v0, v1);
        if (v3_dot(nrm, v3(vv[0].x, vv[0].y, vv[0].z)) > 0.0f) {
            r->stats_culled++;
            continue;
        }

        R3Vert poly[3];
        for (int k = 0; k < 3; k++) {
            poly[k].x = vv[k].x;
            poly[k].y = vv[k].y;
            poly[k].z = vv[k].z;
            poly[k].w = vv[k].w;
            poly[k].color = t->color;
            poly[k].invz = 0.0f;
        }
        R3Vert clipped[MAX_CLIP_VERTS];
        int n = clip_near(poly, 3, clipped, znear);
        if (n < 3) {
            r->stats_culled++;
            continue;
        }
        /* Project the clipped polygon and fan-triangulate it. */
        R3Vert proj_poly[MAX_CLIP_VERTS];
        int ok = 1;
        for (int k = 0; k < n; k++) {
            Vec4 cp = mat4_apply(proj, (Vec4){clipped[k].x, clipped[k].y, clipped[k].z, clipped[k].w});
            float invz;
            if (!project_view(r, proj, cp, &proj_poly[k].x, &proj_poly[k].y, &invz)) {
                ok = 0;
                break;
            }
            proj_poly[k].invz = invz;
            proj_poly[k].color = clipped[k].color;
        }
        if (!ok) {
            r->stats_culled++;
            continue;
        }
        for (int k = 1; k + 1 < n; k++) {
            fill_tri(r, &proj_poly[0], &proj_poly[k], &proj_poly[k + 1]);
            r->stats_tris++;
        }
    }
}

void ds_r3d_draw(DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj) {
    ds_r3d_draw_ex(r, tris, count, view, proj, 0.05f);
}

int ds_r3d_draw_scene(DSRender3D *r, DSScene *s, Vec3 eye, Vec3 target, Vec3 up, float fov_deg,
                      float znear, float zfar) {
    if (!r || !s || !r->color || r->width <= 0) return 0;
    static DSTri tris[DS_SCENE_MAX_TRIS];
    Mat4 view = mat4_look_at(eye, target, up);
    Mat4 proj = mat4_perspective(fov_deg * 0.0174532925f, (float)r->width / (float)r->height, znear, zfar);
    int n = ds_scene_build_mesh(s, mat4_mul(proj, view), view, tris, DS_SCENE_MAX_TRIS);
    ds_r3d_draw_ex(r, tris, n, view, proj, znear);
    return n;
}

/* ---- picking ------------------------------------------------------------- */
int ds_r3d_pick(const DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj, float sx,
                float sy) {
    if (!r || !tris) return -1;
    int best = -1;
    float best_z = 1e30f;
    for (int i = 0; i < count; i++) {
        const DSTri *t = &tris[i];
        float px[3], py[3];
        int vis = 1;
        for (int k = 0; k < 3; k++) {
            Vec3 p = v3(t->x[k], t->y[k], t->z[k]);
            if (!ds_r3d_project(r, view, proj, p, &px[k], &py[k])) {
                vis = 0;
                break;
            }
        }
        if (!vis) continue;
        float d = (py[1] - py[2]) * (px[0] - px[2]) + (px[2] - px[1]) * (py[0] - py[2]);
        if (d > -1e-6f && d < 1e-6f) continue;
        float inv = 1.0f / d;
        float w0 = ((py[1] - py[2]) * (sx - px[2]) + (px[2] - px[1]) * (sy - py[2])) * inv;
        float w1 = ((py[2] - py[0]) * (sx - px[2]) + (px[0] - px[2]) * (sy - py[2])) * inv;
        float w2 = 1.0f - w0 - w1;
        if (w0 < 0 || w1 < 0 || w2 < 0) continue;
        /* Depth at the hit, so overlapping triangles resolve to the nearest. */
        Vec4 c0 = mat4_apply(view, (Vec4){t->x[0], t->y[0], t->z[0], 1.0f});
        Vec4 c1 = mat4_apply(view, (Vec4){t->x[1], t->y[1], t->z[1], 1.0f});
        Vec4 c2 = mat4_apply(view, (Vec4){t->x[2], t->y[2], t->z[2], 1.0f});
        float z = w0 * c0.z + w1 * c1.z + w2 * c2.z;
        if (z < best_z) {
            best_z = z;
            best = t->node;
        }
    }
    return best;
}
