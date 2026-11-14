// src/engine/scene/ds_render3d.h
//
// A small software renderer for the scene graph. It projects DSTri triangles
// through the camera, clips them against the near plane, and fills them into
// a caller-provided colour buffer with a z-buffer for correct occlusion.
//
// This is deliberately CPU-only: it works identically on Android, Windows and
// the headless host test, needs no shader pipeline, and is fast enough for the
// low triangle counts a scripted scene produces (a few thousand). The pixels
// it writes go through the same engine surface the 2D draw calls use.
#ifndef DS_RENDER3D_H
#define DS_RENDER3D_H

#include "engine/scene/ds_math3d.h"
#include "engine/scene/ds_scene.h"
#include <stdint.h>

typedef struct {
    int width, height;
    uint32_t *color;  // width*height, 0xAARRGGBB
    float *depth;     // width*height, 1/z proxy; larger is nearer
    uint32_t clear;   // background colour
    int stats_tris, stats_pixels, stats_culled;
} DSRender3D;

// Attach a framebuffer. Both buffers are owned by the caller and must be at
// least width*height entries. Returns 0 if the arguments are unusable, which
// the rasteriser then treats as "nothing to draw" instead of writing past the
// end of the buffer.
int ds_r3d_init(DSRender3D *r, int width, int height, uint32_t *color, float *depth);
// Never larger than this on either axis; init rejects more.
#define DS_R3D_MAX_DIM 4096

void ds_r3d_clear(DSRender3D *r);

// Render a triangle soup from the given camera. Triangles are in world space;
// `view` and `proj` are separate so clipping happens in view space, where the
// near plane is the simple z < -near test.
void ds_r3d_draw(DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj);
// Same, with an explicit near plane so the clip matches the projection.
void ds_r3d_draw_ex(DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj, float znear);

// Draw a whole scene: build the mesh and rasterise it. Returns triangles drawn.
int ds_r3d_draw_scene(DSRender3D *r, DSScene *s, Vec3 eye, Vec3 target, Vec3 up,
                      float fov_deg, float znear, float zfar);

// ---- Camera helpers ------------------------------------------------------
// An orbit camera: yaw/pitch around a target at a distance, which is the
// natural control for a Studio-style viewport.
typedef struct {
    Vec3 target;
    float yaw_deg, pitch_deg;
    float distance;
    float fov_deg;
    float znear, zfar;
} DSCamera;

void ds_camera_default(DSCamera *c);
Vec3 ds_camera_eye(const DSCamera *c);
Mat4 ds_camera_view(const DSCamera *c);
Mat4 ds_camera_proj(const DSCamera *c, float aspect);
void ds_camera_orbit(DSCamera *c, float dyaw, float dpitch);
void ds_camera_zoom(DSCamera *c, float delta);
void ds_camera_pan(DSCamera *c, float dx, float dy);

// Project a world point to screen pixels. Returns 0 when behind the camera.
int ds_r3d_project(const DSRender3D *r, Mat4 view, Mat4 proj, Vec3 world, float *out_x, float *out_y);
// Cast a screen ray and return the nearest triangle hit, or -1.
int ds_r3d_pick(const DSRender3D *r, const DSTri *tris, int count, Mat4 view, Mat4 proj,
                float sx, float sy);

#endif
