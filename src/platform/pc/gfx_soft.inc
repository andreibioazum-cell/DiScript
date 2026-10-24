/* CPU framebuffer graphics backend implementing shared ds_vk_* hooks. */

static uint32_t *pc_fb;
static int pc_fb_w, pc_fb_h;
static uint32_t *pc_font_tex;
static int pc_font_w, pc_font_h;

/* State the shared files expect from the Vulkan backend. */
static int vk_pending_font = 0;
static long long vk_font_desc = 0;
static ANativeWindow *vk_rebuild_window = NULL;
static int vk_rebuild_tries = 0;
static int vk_sw_present_mode = 0;
static uint32_t vk_sw_recreates = 0;
typedef struct {
    uint32_t width, height;
} VkExtent2DPc;
static VkExtent2DPc vk_sw_extent = {0, 0};

static char pc_gfx_fail[256] = {0};

static const char *ds_vk_present_mode_name(int mode) {
    (void)mode;
    return "PC-SOFT";
}

static void ds_vk_note_failure(const char *what, int code) {
    snprintf(pc_gfx_fail, sizeof(pc_gfx_fail), "%s (code %d)", what ? what : "unknown", code);
    ds_log_err("renderer: %s", pc_gfx_fail);
}

static void pc_render_frame(void);

static int ds_vk_init_backend(ANativeWindow *window) {
    (void)window;
    pc_gfx_fail[0] = '\0';
    vk_sw_recreates = 0;
    ds_log("renderer: PC software rasterizer");
    return 1;
}

static int ds_vk_begin_frame_backend(uint32_t w, uint32_t h, uint32_t ow, uint32_t oh) {
    (void)ow;
    (void)oh;
    if (w < 1 || h < 1 || w > 8192 || h > 8192)
        return 0;
    if ((int)w != pc_fb_w || (int)h != pc_fb_h) {
        uint32_t *nb = (uint32_t *)realloc(pc_fb, (size_t)w * (size_t)h * sizeof(uint32_t));
        if (!nb) {
            ds_vk_note_failure("framebuffer alloc", 0);
            return 0;
        }
        pc_fb = nb;
        pc_fb_w = (int)w;
        pc_fb_h = (int)h;
        vk_sw_extent.width = w;
        vk_sw_extent.height = h;
    }
    return 1;
}

/* Build RGBA8 white+alpha font atlas for software text rendering. */
static void pc_build_font_atlas(void) {
    const uint8_t *alpha;
    int aw, ah;
    size_t i, n;
    if (!font)
        return;
    alpha = ds_font_alpha(font);
    aw = ds_font_aw(font);
    ah = ds_font_ah(font);
    if (!alpha || aw <= 0 || ah <= 0)
        return;
    n = (size_t)aw * (size_t)ah;
    if (pc_font_tex && (pc_font_w != aw || pc_font_h != ah)) {
        free(pc_font_tex);
        pc_font_tex = NULL;
    }
    if (!pc_font_tex) {
        pc_font_tex = (uint32_t *)malloc(n * sizeof(uint32_t));
        if (!pc_font_tex)
            return;
        pc_font_w = aw;
        pc_font_h = ah;
    }
    for (i = 0; i < n; i++)
        pc_font_tex[i] = 0x00FFFFFFu | ((uint32_t)alpha[i] << 24);
    vk_font_desc = 1;
}

static void ds_vk_process_pending_uploads(void) {
    if (vk_pending_font) {
        vk_pending_font = 0;
        pc_build_font_atlas();
    }
}

static void ds_vk_end_frame_backend(void) { pc_render_frame(); }

static void ds_vk_pace_frame(void) { /* pacing lives in the PC main loop */ }

static void ds_vk_surface_lost(void) {}

static void ds_vk_shutdown_backend(void) {
    free(pc_fb);
    pc_fb = NULL;
    pc_fb_w = pc_fb_h = 0;
    free(pc_font_tex);
    pc_font_tex = NULL;
    pc_font_w = pc_font_h = 0;
    vk_font_desc = 0;
    vk_pending_font = 0;
}

/* Mark RAM-resident texture ready for software sampling. */
static void ds_vk_pending_texture(Texture *t) {
    if (!t)
        return;
    t->gpu.uploaded = 1;
    t->gpu.desc = (long long)(uintptr_t)t;
    t->gpu.image = 1;
    t->gpu.memory = 1;
    t->gpu.view = 1;
}

static void ds_vk_texture_gpu_release(Texture *t) {
    if (!t)
        return;
    t->gpu.uploaded = 0;
    t->gpu.desc = 0;
    t->gpu.image = 0;
    t->gpu.memory = 0;
    t->gpu.view = 0;
}

static void ds_vk_pending_reset(void) {}

static void ds_vk_font_gpu_release(void) {
    free(pc_font_tex);
    pc_font_tex = NULL;
    pc_font_w = pc_font_h = 0;
    vk_font_desc = 0;
}

/* Return last software renderer failure message. */
const char *ds_graphics_failure(void) { return pc_gfx_fail[0] ? pc_gfx_fail : "unknown renderer failure"; }

int ds_graphics_show_failure(ANativeWindow *window, int attempts) {
    (void)window;
    ds_log_err("renderer failed %d time(s): %s", attempts, ds_graphics_failure());
    return 0;
}
