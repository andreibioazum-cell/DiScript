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

/* Kept for source compatibility with callers that predate real text colours.
 * It used to flatten nearly every colour to white, which would erase syntax
 * highlighting, so it now passes the colour straight through. */
static uint32_t text_force_white(uint32_t c) { return c; }
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

/* Advance of a single monospaced column, used by the code editor to align the
 * caret and the selection bands with the glyphs. */
/* Line pitch: how far the baseline advances between lines of text. */
/* ---- composite shape helpers ------------------------------------------- *
 * These used to live in the game and are part of the drawing API the language
 * exposes. They are built from the primitives above rather than from new
 * render commands, so they need no backend support. */

/* Texture size in pixels, for scripts that lay out images themselves. */
int tex_size(const char *name, int *out_w, int *out_h) {
    if (out_w)
        *out_w = 0;
    if (out_h)
        *out_h = 0;
    if (!name || !*name)
        return 0;
    Texture *t = load_png(name);
    if (!t)
        return 0;
    if (out_w)
        *out_w = t->w;
    if (out_h)
        *out_h = t->h;
    return 1;
}

/* Draw with an independent scale per axis and an explicit origin. Composed from
 * tex_tint(), so it works on every backend without a new render command. */
void tex_ex(const char *name, float x, float y, float angle_rad, float sx, float sy, float ox, float oy,
            uint32_t tint) {
    int w = 0, h = 0;
    if (!name || !tex_size(name, &w, &h) || w <= 0 || h <= 0)
        return;
    /* geo_tex places the image centred on the point it is given, so shift by
     * half the scaled size to make (x, y) the top-left corner, then move by the
     * anchor. Passing x/y straight through drew every sprite half its width up
     * and to the left of where the script asked for it. */
    float sc = (fabsf(sx) > fabsf(sy)) ? sx : sy;
    if (sc == 0.0f)
        return;
    float hw = (float)w * 0.5f * sc;
    float hh = (float)h * 0.5f * sc;
    tex_tint(x + hw - ox * sc, y + hh - oy * sc, name, angle_rad, sc, tint);
}

void rect_line(float x, float y, float w, float h, float th, uint32_t c) {
    if (w <= 0 || h <= 0 || th <= 0)
        return;
    if (th * 2.0f >= w || th * 2.0f >= h) {
        rect(x, y, w, h, c);
        return;
    }
    rect(x, y, w, th, c);
    rect(x, y + h - th, w, th, c);
    rect(x, y + th, th, h - 2.0f * th, c);
    rect(x + w - th, y + th, th, h - 2.0f * th, c);
}

void ellipse_fill(float x, float y, float rx, float ry, uint32_t c) {
    if (rx <= 0 || ry <= 0)
        return;
    /* Stacked horizontal spans: exact for the common sizes the UI uses and
     * much cheaper than a per-pixel path. */
    int steps = (int)(ry * 2.0f);
    if (steps < 2)
        steps = 2;
    if (steps > 256)
        steps = 256;
    float band = (ry * 2.0f) / (float)steps;
    for (int i = 0; i < steps; i++) {
        float dy = -ry + band * ((float)i + 0.5f);
        float k = 1.0f - (dy * dy) / (ry * ry);
        if (k <= 0.0f)
            continue;
        float half = rx * sqrtf(k);
        rect(x - half, y - ry + band * (float)i, half * 2.0f, band + 0.75f, c);
    }
}

void ellipse_line(float x, float y, float rx, float ry, float thickness, uint32_t c) {
    if (rx <= 0 || ry <= 0 || thickness <= 0)
        return;
    int steps = (int)((rx + ry) * 3.0f);
    if (steps < 12)
        steps = 12;
    if (steps > 256)
        steps = 256;
    float prev_x = x + rx, prev_y = y;
    for (int i = 1; i <= steps; i++) {
        float a = 6.283185307179586f * (float)i / (float)steps;
        float px = x + cosf(a) * rx, py = y + sinf(a) * ry;
        line(prev_x, prev_y, px, py, thickness, c);
        prev_x = px;
        prev_y = py;
    }
}

void arc_line(float x, float y, float r, float start_deg, float sweep_deg, float thickness, int closed_pie,
              uint32_t c) {
    if (r <= 0 || thickness <= 0 || sweep_deg == 0.0f)
        return;
    int steps = (int)(fabsf(sweep_deg) / 6.0f);
    if (steps < 4)
        steps = 4;
    if (steps > 512)
        steps = 512;
    float a0 = start_deg * 0.017453292519943295f;
    float prev_x = x + cosf(a0) * r, prev_y = y + sinf(a0) * r;
    for (int i = 1; i <= steps; i++) {
        float a = a0 + (sweep_deg * 0.017453292519943295f) * (float)i / (float)steps;
        float px = x + cosf(a) * r, py = y + sinf(a) * r;
        line(prev_x, prev_y, px, py, thickness, c);
        prev_x = px;
        prev_y = py;
    }
    if (closed_pie) {
        float a1 = a0 + sweep_deg * 0.017453292519943295f;
        line(x, y, x + cosf(a0) * r, y + sinf(a0) * r, thickness, c);
        line(x, y, x + cosf(a1) * r, y + sinf(a1) * r, thickness, c);
    }
}

void poly_line(const float *xy, int point_count, float thickness, int closed, uint32_t c) {
    if (!xy || point_count < 2)
        return;
    for (int i = 1; i < point_count; i++)
        line(xy[(i - 1) * 2], xy[(i - 1) * 2 + 1], xy[i * 2], xy[i * 2 + 1], thickness, c);
    if (closed)
        line(xy[(point_count - 1) * 2], xy[(point_count - 1) * 2 + 1], xy[0], xy[1], thickness, c);
}

void poly_fill(const float *xy, int point_count, uint32_t c) {
    if (!xy || point_count < 3)
        return;
    /* Scanline fill: for every row, intersect the edges and fill the spans. */
    float min_y = xy[1], max_y = xy[1];
    for (int i = 0; i < point_count; i++) {
        if (xy[i * 2 + 1] < min_y)
            min_y = xy[i * 2 + 1];
        if (xy[i * 2 + 1] > max_y)
            max_y = xy[i * 2 + 1];
    }
    int rows = (int)(max_y - min_y);
    if (rows <= 0)
        return;
    if (rows > 512)
        rows = 512;
    float step = (max_y - min_y) / (float)rows;
    for (int r = 0; r < rows; r++) {
        float y = min_y + step * ((float)r + 0.5f);
        float left = 0.0f, right = 0.0f;
        int found = 0;
        for (int i = 0; i < point_count; i++) {
            int j = (i + 1) % point_count;
            float y1 = xy[i * 2 + 1], y2 = xy[j * 2 + 1];
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                float t = (y - y1) / (y2 - y1);
                float x = xy[i * 2] + t * (xy[j * 2] - xy[i * 2]);
                if (!found) {
                    left = right = x;
                    found = 1;
                } else {
                    if (x < left)
                        left = x;
                    if (x > right)
                        right = x;
                }
            }
        }
        if (found && right > left)
            rect(left, y - step * 0.5f, right - left, step + 0.75f, c);
    }
}

float text_line_height(float scale) {
    if (scale <= 0.0f || !ensure_font())
        return 0.0f;
    return ds_font_lineh(font) * scale;
}
float text_advance_of(const char *s, float scale) {
    if (!s || !*s || scale <= 0.0f || !ensure_font())
        return 0.0f;
    const char *c = s;
    int cp = utf8_dec(&c);
    const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
    return g ? g->advance * scale : 0.0f;
}
float text_width_scaled(const char *s, float scale) {
    if (!s || scale <= 0.0f || !ensure_font())
        return 0.0f;
    float pen = 0.0f;
    for (const char *c = s; *c;) {
        int cp = utf8_dec(&c);
        if (cp == '\n') {
            pen = 0.0f;
            continue;
        }
        const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
        if (!g)
            continue;
        pen += g->advance * scale;
    }
    return pen;
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
