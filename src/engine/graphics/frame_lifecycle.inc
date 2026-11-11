int ds_graphics_init(AAssetManager *a, ANativeWindow *window) {
    if (amgr != a) {
        ds_release_assets();
        amgr = a;
    }
    if (ds_vk_init_backend(window))
        return 1;
    ds_vk_note_failure("renderer start (no Vulkan error code)", 0);
    return 0;
}
int ds_graphics_begin_frame(Buffer *b) {
    if (!b || b->width <= 0 || b->height <= 0 || b->stride < b->width)
        return 0;

    uint32_t s = (uint32_t)gfx_auto_scale;
    uint32_t ow = (uint32_t)b->width / s, oh = (uint32_t)b->height / s;
    if (ow < 1)
        ow = 1;
    if (oh < 1)
        oh = 1;
    if (!ds_vk_begin_frame_backend((uint32_t)b->width, (uint32_t)b->height, ow, oh))
        return 0;
    /* Decay screen shake and sample a new offset each frame. */
    if (gfx_shake_left > 0) {
        gfx_shake_left -= (float)dt;
        if (gfx_shake_left < 0)
            gfx_shake_left = 0;
        float k = gfx_shake_dur > 0 ? gfx_shake_left / gfx_shake_dur : 0;
        float a = gfx_shake_amp * k * k;
        gfx_shake_dx = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * a;
        gfx_shake_dy = ((float)rand() / (float)RAND_MAX * 2.0f - 1.0f) * a;
    } else {
        gfx_shake_dx = 0;
        gfx_shake_dy = 0;
    }
    /* Upload pending textures before draw code queries texture readiness. */
    ds_vk_process_pending_uploads();
    discard_commands();
    cur_buf = b;
    frame_open = 1;
    return 1;
}
void ds_graphics_end_frame(void) {
    if (!frame_open)
        return;
    ds_vk_end_frame_backend();
    ds_vk_pace_frame();
    /* Release queued text buffers and reset command count. */
    discard_commands();
    frame_open = 0;
    cur_buf = NULL;
}
void ds_graphics_cancel_frame(void) {
    discard_commands();
    frame_open = 0;
    cur_buf = NULL;
}
void ds_graphics_error_screen(const char *m) {
    if (!frame_open)
        return;

    discard_commands();
    rect(0.0f, 0.0f, (float)(cur_buf ? cur_buf->width : screen_w), (float)(cur_buf ? cur_buf->height : screen_h),
         pack_c(0xff1c0b10));
    if (!ensure_font())
        return;
    int y = 12;
    text_scaled("=== DISCRIPT RUNTIME ERROR ===", 16, (float)y, 0xffe6e6e6u, 0.7f);
    y += 30;
    if (m && *m) {
        text_scaled(m, 16, (float)y, 0xffef7a85u, 0.6f);
        y += 28;
    }
    y += 4;
    text_scaled("--- console (last lines) ---", 16, (float)y, pack_c(0xff6f7684), 0.55f);
    y += 24;
    int n = console_count();
    int start = n > 16 ? n - 16 : 0;
    for (int i = start; i < n; i++) {
        const char *ln = console_line(i);
        if (!ln || !*ln) {
            y += 19;
            continue;
        }
        uint32_t col = console_type(i) ? 0xffb06a62 : 0xff7c828e;
        text_scaled(ln, 16, (float)y, col, 0.55f);
        y += 19;
        if (y > (cur_buf ? cur_buf->height : screen_h) - 8)
            break;
    }
}

void ds_graphics_window_lost(void) {
    ds_graphics_cancel_frame();
    vk_rebuild_window = NULL;
    vk_rebuild_tries = 0;
    ds_vk_surface_lost();
}
void ds_graphics_shutdown(void) {
    vk_rebuild_window = NULL;
    vk_rebuild_tries = 0;
    ds_graphics_cancel_frame();
    ds_vk_shutdown_backend();
    gfx_autoscale_reset();
    ds_release_assets();
    free(cmds);
    cmds = NULL;
    cmd_cap = 0;
    cmd_n = 0;
    free(geo_verts);
    geo_verts = NULL;
    geo_vcap = geo_vn = 0;
    free(geo_idx);
    geo_idx = NULL;
    geo_icap = geo_in = 0;
    geo_trig_free();
    cmd_arena_free_all();
}
#include "engine/font.c"
