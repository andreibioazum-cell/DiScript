/* Native 1:1 render scale and periodic frame-time logging. */
static int gfx_auto_scale = 1;

int ds_graphics_pixel_scale(void) { return gfx_auto_scale; }

#define GFX_REPORT_SEC 5.0

static double gfx_report_acc;
static int gfx_report_frames;

static void gfx_autoscale_new_window(void) {
    gfx_auto_scale = 1;
    gfx_report_acc = 0;
    gfx_report_frames = 0;
}

static void gfx_autoscale_reset(void) { gfx_autoscale_new_window(); }

void ds_graphics_report_frame_interval(double sec) {
    /* Ignore paused or invalid frame intervals. */
    if (!(sec > 0.0) || sec > 0.25)
        return;
    gfx_report_acc += sec;
    gfx_report_frames++;
    if (gfx_report_acc < GFX_REPORT_SEC)
        return;
    ds_log("graphics: %.1f fps, frame %.1f ms, internal 1/%d, present %s, swapchain %ux%u, recreates %u",
           (double)gfx_report_frames / gfx_report_acc, 1000.0 * gfx_report_acc / (double)gfx_report_frames,
           gfx_auto_scale, ds_vk_present_mode_name(vk_sw_present_mode), (unsigned)vk_sw_extent.width,
           (unsigned)vk_sw_extent.height, vk_sw_recreates);
    gfx_report_acc = 0;
    gfx_report_frames = 0;
}
