// src/ide/ide_viewport.h
//
// The Studio-style 3D viewport shown next to the code. It renders the script's
// scene with ds_render3d into a small framebuffer and blits the result to the
// editor surface as a grid of filled rectangles.
//
// The framebuffer is intentionally low resolution (the editor draws with
// rects, not per-pixel writes): at a 2-3 px block size a 300x200 frame still
// reads as a clean image while costing a few thousand draw calls.
#ifndef IDE_VIEWPORT_H
#define IDE_VIEWPORT_H

#include "engine/scene/ds_scene.h"
#include "engine/scene/ds_render3d.h"

#define IDE_VP_MAX_W 640
#define IDE_VP_MAX_H 480

typedef struct IDEViewport {
    int width, height;   // framebuffer size in pixels
    int block;           // on-screen pixels per framebuffer pixel
    uint32_t color[IDE_VP_MAX_W * IDE_VP_MAX_H];
    float depth[IDE_VP_MAX_W * IDE_VP_MAX_H];
    DSRender3D r;

    DSCamera camera;
    int orbit_active;    // dragging with one finger orbits
    int pan_active;      // two-finger drag (or shift-drag) pans
    int last_x, last_y;
    int show_grid;

    int visible;         // 0 = the code fills the area
    int stats_tris;
} IDEViewport;

void ide_viewport_init(IDEViewport *v);
// Resize the framebuffer to the pixel size the viewport occupies on screen.
void ide_viewport_resize(IDEViewport *v, int screen_w, int screen_h, int block);

// Render the scene and draw the result into [x, y, w, h] on the editor surface.
void ide_viewport_draw(IDEViewport *v, DSScene *scene, float x, float y, float w, float h);

// Pointer handling in screen space. `pressed` is 1 on down, 0 while moving,
// -1 on release. Returns 1 when the event was consumed.
int ide_viewport_pointer(IDEViewport *v, int pressed, float x, float y);
// Wheel / pinch zoom, positive delta zooms in.
void ide_viewport_zoom(IDEViewport *v, float delta);
void ide_viewport_focus(IDEViewport *v, DSScene *scene);

#endif
