int png_reset(const char *req) {
    const char *n = norm_name(req);
    Texture *t;
    if (!n)
        return 0;
    t = find_tx(n);
    if (!t)
        return 0;
    ds_vk_texture_gpu_release(t);
    free(t->gray_backup);
    t->gray_backup = NULL;
    free(t->pixels);
    t->pixels = NULL;
    t->w = 0;
    t->h = 0;
    t->opaque = 0;
    t->upload_tries = 0;
    return load_png(n) != NULL;
}

/* Reload every cached texture from the active pack (or the packaged asset). */
int ds_assets_reload_textures(void) {
    Texture *t;
    int count = 0;
    for (t = textures; t; t = t->next) {
        if (t->name && png_reset(t->name) == 1)
            count++;
    }
    return count;
}

/* Replace (or create) a texture from an in-memory PNG. Used by the mod loader:
 * a mod ships ordinary.png and the game pushes it over the shipped asset
 * without touching assets/ on disk. Returns 1 when the texture is live. */
int png_load_memory(const char *req, const void *data, int size) {
    const char *n = norm_name(req);
    if (!n || !data || size <= 0 || size > (16 * 1024 * 1024))
        return 0;
    int w = 0, h = 0, ch = 0;
    stbi_uc *dec = stbi_load_from_memory((const stbi_uc *)data, size, &w, &h, &ch, STBI_rgb_alpha);
    if (!dec || w <= 0 || h <= 0 || w > 2048 || h > 2048) {
        stbi_image_free(dec);
        ds_log_err("mod texture refused: %s", n);
        return 0;
    }
    Texture *t = find_tx(n);
    if (!t) {
        t = (Texture *)calloc(1, sizeof(*t));
        if (!t) {
            stbi_image_free(dec);
            return 0;
        }
        t->name = strdup_safe(n);
        if (!t->name) {
            free(t);
            stbi_image_free(dec);
            return 0;
        }
        t->next = textures;
        textures = t;
    }
    size_t pc = (size_t)w * (size_t)h;
    uint32_t *px = (uint32_t *)malloc(pc * sizeof(*px));
    if (!px) {
        stbi_image_free(dec);
        return 0;
    }
    int opaque = 1;
    for (size_t i = 0; i < pc; i++) {
        uint8_t a = dec[i * 4 + 3];
        px[i] = dec[i * 4] | ((uint32_t)dec[i * 4 + 1] << 8) | ((uint32_t)dec[i * 4 + 2] << 16) |
                ((uint32_t)a << 24);
        if (a != 255)
            opaque = 0;
    }
    stbi_image_free(dec);
    ds_vk_texture_gpu_release(t);
    free(t->gray_backup);
    t->gray_backup = NULL;
    free(t->pixels);
    t->pixels = px;
    t->w = w;
    t->h = h;
    t->opaque = opaque;
    t->upload_tries = 0;
    texture_gray_apply(t);
    ds_vk_pending_texture(t);
    ds_log("mod texture applied: %s (%dx%d)", n, w, h);
    return 1;
}

/* Read a whole packaged asset into a caller buffer (mod example zip, etc.). */
/* Texture pack export needs the size before it allocates the buffer. */
static int ensure_font(void) {
    if (font)
        return 1;
    if (font_tried)
        return 0;
    font_tried = 1;
    uint8_t *data = NULL;
    size_t sz = 0;
    if (!open_asset(DS_FONT_ASSET, &data, &sz)) {
        ds_log_err("font not loaded: %s not found", DS_FONT_ASSET);
        return 0;
    }
    font = ds_font_create(data, sz, DS_FONT_PIXEL_HEIGHT);
    free(data);
    if (!font) {
        ds_log_err("font not loaded: could not parse TrueType font");
        return 0;
    }
    vk_pending_font = 1;
    ds_log("font loaded: %s", DS_FONT_ASSET);
    return 1;
}

static DSCmd *push(DSCommand t) {
    if (!frame_open)
        return NULL;
    if (cmd_n == cmd_cap) {
        size_t cap = cmd_cap ? cmd_cap * 2 : 256;
        if (cap < cmd_n || cap > SIZE_MAX / sizeof(*cmds)) {
            ds_runtime_error("too many renderer commands");
            return NULL;
        }
        DSCmd *nc = (DSCmd *)realloc(cmds, cap * sizeof(*cmds));
        if (!nc) {
            ds_runtime_error("out of memory in command buffer");
            return NULL;
        }
        cmds = nc;
        cmd_cap = cap;
    }
    DSCmd *c = &cmds[cmd_n++];
    memset(c, 0, sizeof(*c));
    c->t = t;
    return c;
}
static void discard_commands(void) {
    for (size_t i = 0; i < cmd_n; i++) {
        if (cmds[i].t == DS_CMD_TEXT) {
            free(cmds[i].v.tt.s);
            cmds[i].v.tt.s = NULL;
        } else if (cmds[i].t == DS_CMD_TEXT_RAINBOW) {
            free(cmds[i].v.tr.s);
            cmds[i].v.tr.s = NULL;
        }
    }
    cmd_n = 0;
}

void ds_release_assets(void) {

    for (Texture *t = textures; t; t = t->next) ds_vk_texture_gpu_release(t);
    ds_vk_pending_reset();
    ds_vk_font_gpu_release();
    Texture *t = textures;
    while (t) {
        Texture *n = t->next;
        free(t->pixels);
        free(t->gray_backup);
        free(t->name);
        free(t);
        t = n;
    }
    textures = NULL;
    ds_font_destroy(font);
    font = NULL;
    font_tried = 0;
    amgr = NULL;
}
void ds_set_asset_manager(AAssetManager *a) {
    if (amgr != a) {
        ds_release_assets();
        amgr = a;
    }
    if (!amgr)
        ds_runtime_error("Android asset manager is unavailable");
}
int png_load(const char *n) {
    return load_png(n) != NULL;
}

int tex_ready(const char *name) {
    const char *n = norm_name(name);
    if (!n)
        return 0;
    Texture *t = find_tx(n);

    if (!t)
        t = load_png(n);
    if (!t || !t->pixels)
        return 0;
    if (t->gpu.uploaded)
        return 1;
    ds_vk_pending_texture(t);
    /* Wait until GPU upload completes before marking drawable. */
    return 0;
}

void rect(float x, float y, float w, float h, uint32_t c) {
    DSCmd *p = push(DS_CMD_RECT);
    if (!p)
        return;
    p->v.rc.x = x;
    p->v.rc.y = y;
    p->v.rc.w = w;
    p->v.rc.h = h;
    p->v.rc.c = pack_c(c);
}
void roundrect(float x, float y, float w, float h, float r, uint32_t c) {
    DSCmd *p = push(DS_CMD_ROUND);
    if (!p)
        return;
    p->v.rr.x = x;
    p->v.rr.y = y;
    p->v.rr.w = w;
    p->v.rr.h = h;
    p->v.rr.r = r;
    p->v.rr.c = pack_c(c);
}

void rect_rot(float x, float y, float w, float h, float ang, uint32_t c) {
    DSCmd *p = push(DS_CMD_RECT_ROT);
    if (!p)
        return;
    p->v.rot.x = x;
    p->v.rot.y = y;
    p->v.rot.w = w;
    p->v.rot.h = h;
    p->v.rot.ang = ang;
    p->v.rot.c = pack_c(c);
}
void circle(float x, float y, float r, uint32_t c) {
    DSCmd *p = push(DS_CMD_CIRCLE);
    if (!p)
        return;
    p->v.ci.x = x;
    p->v.ci.y = y;
    p->v.ci.r = r;
    p->v.ci.c = pack_c(c);
}
void sector(float x, float y, float r, float start_deg, float sweep_deg, uint32_t c) {
    DSCmd *p = push(DS_CMD_SECTOR);
    if (!p)
        return;
    p->v.sec.x = x;
    p->v.sec.y = y;
    p->v.sec.r = r;
    p->v.sec.start_deg = start_deg;
    p->v.sec.sweep_deg = sweep_deg;
    p->v.sec.c = pack_c(c);
}
void ring(float x, float y, float r, float t, uint32_t c) {
    DSCmd *p = push(DS_CMD_RING);
    if (!p)
        return;
    p->v.rg.x = x;
    p->v.rg.y = y;
    p->v.rg.r = r;
    p->v.rg.th = t;
    p->v.rg.c = pack_c(c);
}
void line(float x1, float y1, float x2, float y2, float thickness, uint32_t c) {
    DSCmd *p = push(DS_CMD_LINE);
    if (!p)
        return;
    p->v.ln.x1 = x1;
    p->v.ln.y1 = y1;
    p->v.ln.x2 = x2;
    p->v.ln.y2 = y2;
    p->v.ln.th = thickness;
    p->v.ln.c = pack_c(c);
}
void tex(float x, float y, const char *name, float a, float s) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    DSCmd *p = push(DS_CMD_TEX);
    if (!p)
        return;
    p->v.tx.x = x;
    p->v.tx.y = y;
    p->v.tx.a = a;
    p->v.tx.sc = s;
    p->v.tx.tx = t;
}
void tex_tint(float x, float y, const char *name, float a, float s, uint32_t c) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    DSCmd *p = push(DS_CMD_TEX_TINT);
    if (!p)
        return;
    p->v.tx2.x = x;
    p->v.tx2.y = y;
    p->v.tx2.a = a;
    p->v.tx2.sc = s;
    p->v.tx2.tx = t;
    p->v.tx2.c = pack_c(c);
}

static uint32_t text_force_white(uint32_t c) {

    if (c == RGB(255,68,68) || c == RGB(79,195,247) || c == RGB(255,51,51) || c == RGB(51,168,255) || c == RGB(255,213,74) || c == RGB(255,221,51))
        return c;

    if ((c & 0x00ffffffu) == 0x00202020u)
        return c;
    return (c & 0xff000000u) | 0x00ffffffu;
}
void text_scaled(const char *s, float x, float y, uint32_t c, float sc) {
    if (!frame_open || !s || !ensure_font())
        return;
    DSCmd *p = push(DS_CMD_TEXT);
    if (!p)
        return;

    p->v.tt.s = strdup_safe(s);
    if (!p->v.tt.s) {
                /* Drop text command on allocation failure. */
                ds_log_err("renderer: no memory to copy text");
        cmd_n = cmd_n - 1;
        return;
    }
    p->v.tt.x = x;
    p->v.tt.y = y;
    p->v.tt.sc = sc;
    p->v.tt.c = pack_c(text_force_white(c));
}
void text(const char *s, float x, float y, uint32_t c) {
    text_scaled(s, x, y, c, 1.0f);
}

int text_ink_width(const char *s) {
    if (!s || !ensure_font())
        return 0;
    const DSFontGlyph *ref = ds_font_glyph(font, 'S');
    float pen = -(ref ? ref->bearing_x : 0);
    int first = 1;
    float minL = 0, maxR = 0;
    for (const char *c = s; *c;) {
        int cp = utf8_dec(&c);
        const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
        if (!g)
            continue;
        float dl = pen + g->bearing_x, dr = dl + g->width;
        if (first || dl < minL)
            minL = dl;
        if (first || dr > maxR)
            maxR = dr;
        first = 0;
        pen += g->advance;
    }
    return first ? 0 : (int)(maxR - minL + 0.5f);
}
int text_ink_height(const char *s) {
    if (!s || !ensure_font())
        return 0;
    const DSFontGlyph *ref = ds_font_glyph(font, 'S');
    float base = ref ? ref->bearing_top : 0;
    int first = 1;
    float minT = 0, maxB = 0;
    for (const char *c = s; *c;) {
        int cp = utf8_dec(&c);
        const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
        if (!g)
            continue;
        float dt = base - g->bearing_top, db = dt + g->height;

        if (db > base)
            db = base;
        if (first || dt < minT)
            minT = dt;
        if (first || db > maxB)
            maxB = db;
        first = 0;
    }
    return first ? 0 : (int)(maxB - minT + 0.5f);
}
int text_ink_top(const char *s) {
    if (!s || !ensure_font())
        return 0;
    const DSFontGlyph *ref = ds_font_glyph(font, 'S');
    float base = ref ? ref->bearing_top : 0;
    int first = 1;
    float minT = 0;
    for (const char *c = s; *c;) {
        int cp = utf8_dec(&c);
        const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
        if (!g)
            continue;
        float dt = base - g->bearing_top;
        if (first || dt < minT)
            minT = dt;
        first = 0;
    }
    return first ? 0 : (int)floorf(minT);
}
