#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine/scene/ds_scene.h"
#include "engine/scene/ds_render3d.h"

static void write_ppm(const char *path, DSRender3D *r) {
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", r->width, r->height);
    for (int i = 0; i < r->width * r->height; i++) {
        uint32_t c = r->color[i];
        unsigned char px[3] = {(c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
}

int main(void) {
    const int W = 640, H = 480;
    uint32_t *color = malloc(sizeof(uint32_t) * W * H);
    float *depth = malloc(sizeof(float) * W * H);
    DSRender3D r;
    ds_r3d_init(&r, W, H, color, depth);

    DSScene s;
    ds_scene_init(&s);

    // ground
    int ground = ds_scene_add(&s, DS_NODE_PART, s.root, "Ground");
    ds_scene_set_transform(&s, ground, v3(0, -1, 0), v3(0, 0, 0), v3(16, 0.4, 16));
    ds_scene_set_color(&s, ground, 0xFF6E7F5Cu);
    s.nodes[ground].shape = DS_SHAPE_BLOCK;

    // house
    int house = ds_scene_add(&s, DS_NODE_MODEL, s.root, "House");
    int wall = ds_scene_add(&s, DS_NODE_PART, house, "Wall");
    ds_scene_set_transform(&s, wall, v3(0, 0, 0), v3(0, 0, 0), v3(3, 2, 3));
    ds_scene_set_color(&s, wall, 0xFFC8A882u);

    int roof = ds_scene_add(&s, DS_NODE_PART, house, "Roof");
    ds_scene_set_transform(&s, roof, v3(0, 1.4, 0), v3(0, 0, 0), v3(3.6, 1.2, 3.6));
    ds_scene_set_color(&s, roof, 0xFFB04A44u);
    s.nodes[roof].shape = DS_SHAPE_CONE;

    int door = ds_scene_add(&s, DS_NODE_PART, house, "Door");
    ds_scene_set_transform(&s, door, v3(0, -0.3, 1.55), v3(0, 0, 0), v3(0.9, 1.5, 0.2));
    ds_scene_set_color(&s, door, 0xFF5A4030u);

    // sphere + cylinder props
    int ball = ds_scene_add(&s, DS_NODE_PART, s.root, "Ball");
    ds_scene_set_transform(&s, ball, v3(-2.6, -0.4, 1.2), v3(0,0,0), v3(1.4,1.4,1.4));
    ds_scene_set_color(&s, ball, 0xFF4C8FD6u);
    s.nodes[ball].shape = DS_SHAPE_SPHERE;

    int cyl = ds_scene_add(&s, DS_NODE_PART, s.root, "Pillar");
    ds_scene_set_transform(&s, cyl, v3(2.6, 0.2, 0.6), v3(0,0,0), v3(0.9, 2.8, 0.9));
    ds_scene_set_color(&s, cyl, 0xFFD8D2C4u);
    s.nodes[cyl].shape = DS_SHAPE_CYLINDER;

    int ramp = ds_scene_add(&s, DS_NODE_PART, s.root, "Ramp");
    ds_scene_set_transform(&s, ramp, v3(2.4, -0.8, -2.2), v3(0, 30, 0), v3(2.2, 1.0, 2.2));
    ds_scene_set_color(&s, ramp, 0xFF8A7B5Eu);
    s.nodes[ramp].shape = DS_SHAPE_WEDGE;

    DSCamera cam;
    ds_camera_default(&cam);
    cam.distance = 17.0f;
    cam.pitch_deg = 24.0f;

    ds_r3d_clear(&r);
    ds_r3d_draw_scene(&r, &s, ds_camera_eye(&cam), cam.target, v3(0,1,0), cam.fov_deg, cam.znear, cam.zfar);
    write_ppm("build/pc/studio-scene.ppm", &r);
    printf("tris=%d px=%d culled=%d nodes=%d\n", r.stats_tris, r.stats_pixels, r.stats_culled, s.node_count);

    // picking: click the middle of the screen, should hit the house wall/door
    static DSTri tris[DS_SCENE_MAX_TRIS];
    Mat4 view = ds_camera_view(&cam);
    Mat4 proj = ds_camera_proj(&cam, (float)W / H);
    int n = ds_scene_build_mesh(&s, mat4_mul(proj, view), view, tris, DS_SCENE_MAX_TRIS);
    int hit = ds_r3d_pick(&r, tris, n, view, proj, W / 2.0f, H * 0.55f);
    printf("pick_center=%d (%s)\n", hit, hit >= 0 ? s.nodes[hit].name : "none");
    int miss = ds_r3d_pick(&r, tris, n, view, proj, 2.0f, 2.0f);
    printf("pick_corner=%d\n", miss);
    return 0;
}
