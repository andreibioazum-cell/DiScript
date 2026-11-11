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

int ds_assets_reload_textures(void) {
    Texture *t;
    int count = 0;
    for (t = textures; t; t = t->next) {
        if (t->name && png_reset(t->name) == 1)
            count++;
    }
    return count;
}

int rgba_load_memory(const char *req, const uint32_t *rgba_pixels, int w, int h) {
    const char *n = norm_name(req);
    if (!n || !rgba_pixels || w <= 0 || h <= 0 || w > 2048 || h > 2048)
        return 0;
    Texture *t = find_tx(n);
    if (!t) {
        t = (Texture *)calloc(1, sizeof(*t));
        if (!t)
            return 0;
        t->name = strdup_safe(n);
        if (!t->name) {
            free(t);
            return 0;
        }
        t->next = textures;
        textures = t;
    }
    size_t pc = (size_t)w * (size_t)h;
    uint32_t *px = (uint32_t *)malloc(pc * sizeof(*px));
    if (!px)
        return 0;
    int opaque = 1;
    for (size_t i = 0; i < pc; i++) {
        uint32_t v = rgba_pixels[i];
        px[i] = v;
        if ((v >> 24) != 255u)
            opaque = 0;
    }
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
    return 1;
}

int png_load_memory(const char *req, const void *data, int size) {
    const char *n = norm_name(req);
    if (!n || !data || size <= 0 || size > (16 * 1024 * 1024))
        return 0;
    int w = 0, h = 0, ch = 0;
    stbi_uc *dec = stbi_load_from_memory((const stbi_uc *)data, size, &w, &h, &ch, STBI_rgb_alpha);
    if (!dec || w <= 0 || h <= 0 || w > 2048 || h > 2048) {
        stbi_image_free(dec);
        return 0;
    }
    size_t pc = (size_t)w * (size_t)h;
    uint32_t *px = (uint32_t *)malloc(pc * sizeof(*px));
    if (!px) {
        stbi_image_free(dec);
        return 0;
    }
    for (size_t i = 0; i < pc; i++) {
        px[i] = dec[i * 4] | ((uint32_t)dec[i * 4 + 1] << 8) | ((uint32_t)dec[i * 4 + 2] << 16) |
                ((uint32_t)dec[i * 4 + 3] << 24);
    }
    stbi_image_free(dec);
    int ok = rgba_load_memory(n, px, w, h);
    free(px);
    return ok;
}

static int ensure_font(void) {
    if (font)
        return 1;
    if (font_tried)
        return 0;
    font_tried = 1;
    uint8_t *data = NULL;
    size_t sz = 0;
    if (!open_asset(DS_FONT_ASSET, &data, &sz)) {
        if (!open_asset("fonts/ComicRelief-Regular.ttf", &data, &sz)) {
            ds_log_err("font not loaded: %s not found", DS_FONT_ASSET);
            return 0;
        }
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
        size_t cap = cmd_cap ? cmd_cap * 2 : 512;
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
    cmd_n = 0;
    cmd_arena_reset();
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

int png_load(const char *n) { return load_png(n) != NULL; }

int tex_size(const char *name, int *out_w, int *out_h) {
    const char *n = norm_name(name);
    if (!n)
        return 0;
    Texture *t = find_tx(n);
    if (!t)
        t = load_png(n);
    if (!t || !t->pixels)
        return 0;
    if (out_w)
        *out_w = t->w;
    if (out_h)
        *out_h = t->h;
    return 1;
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

void rect_line(float x, float y, float w, float h, float th, uint32_t c) {
    if (w <= 0 || h <= 0 || th <= 0)
        return;
    if (th * 2.0f >= w || th * 2.0f >= h) {
        rect(x, y, w, h, c);
        return;
    }
    rect(x, y, w, th, c);
    rect(x, y + h - th, w, th, c);
    rect(x, y + th, th, h - th * 2.0f, c);
    rect(x + w - th, y + th, th, h - th * 2.0f, c);
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

void ellipse_fill(float x, float y, float rx, float ry, uint32_t c) {
    DSCmd *p = push(DS_CMD_ELLIPSE);
    if (!p)
        return;
    p->v.el.x = x;
    p->v.el.y = y;
    p->v.el.rx = rx;
    p->v.el.ry = ry;
    p->v.el.c = pack_c(c);
}

void ellipse_line(float x, float y, float rx, float ry, float thickness, uint32_t c) {
    if (rx <= 0 || ry <= 0 || thickness <= 0)
        return;
    int segs = geo_circle_segs(rx > ry ? rx : ry);
    float prev_x = x + rx, prev_y = y;
    for (int i = 1; i <= segs; i++) {
        float a = (float)i * (float)(2.0 * M_PI) / (float)segs;
        float nx = x + cosf(a) * rx;
        float ny = y + sinf(a) * ry;
        line(prev_x, prev_y, nx, ny, thickness, c);
        prev_x = nx;
        prev_y = ny;
    }
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

void arc_line(float x, float y, float r, float start_deg, float sweep_deg, float thickness, int closed_pie, uint32_t c) {
    if (r <= 0 || sweep_deg == 0 || thickness <= 0)
        return;
    int n = (int)ceilf(fabsf(sweep_deg) / 6.0f);
    if (n < 2)
        n = 2;
    if (n > 72)
        n = 72;
    float a0 = start_deg * (float)(M_PI / 180.0);
    float px = x + cosf(a0) * r, py = y + sinf(a0) * r;
    float fx = px, fy = py;
    for (int i = 1; i <= n; i++) {
        float a = (start_deg + sweep_deg * ((float)i / (float)n)) * (float)(M_PI / 180.0);
        float nx = x + cosf(a) * r, ny = y + sinf(a) * r;
        line(px, py, nx, ny, thickness, c);
        px = nx;
        py = ny;
    }
    if (closed_pie) {
        line(x, y, fx, fy, thickness, c);
        line(px, py, x, y, thickness, c);
    }
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

void poly_fill(const float *xy, int point_count, uint32_t c) {
    if (!frame_open || !xy || point_count < 3)
        return;
    size_t bytes = (size_t)point_count * 2 * sizeof(float);
    float *copy = (float *)cmd_arena_alloc(bytes, sizeof(float));
    if (!copy)
        return;
    memcpy(copy, xy, bytes);
    DSCmd *p = push(DS_CMD_POLY);
    if (!p)
        return;
    p->v.poly.xy = copy;
    p->v.poly.count = point_count;
    p->v.poly.c = pack_c(c);
}

void poly_line(const float *xy, int point_count, float thickness, int closed, uint32_t c) {
    if (!xy || point_count < 2 || thickness <= 0)
        return;
    for (int i = 0; i + 1 < point_count; i++)
        line(xy[i * 2], xy[i * 2 + 1], xy[(i + 1) * 2], xy[(i + 1) * 2 + 1], thickness, c);
    if (closed && point_count >= 3)
        line(xy[(point_count - 1) * 2], xy[(point_count - 1) * 2 + 1], xy[0], xy[1], thickness, c);
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
    p->v.tx.a = a * (float)(M_PI / 180.0);
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
    p->v.tx2.a = a * (float)(M_PI / 180.0);
    p->v.tx2.sc = s;
    p->v.tx2.tx = t;
    p->v.tx2.c = pack_c(c);
}

void tex_ex(const char *name, float x, float y, float angle_rad, float sx, float sy, float ox, float oy, uint32_t tint) {
    if (!frame_open || !name)
        return;
    const char *n = norm_name(name);
    if (!n)
        return;
    Texture *t = find_tx(n);
    if (!t)
        t = load_png(n);
    if (!t || !t->pixels)
        return;
    if (!t->gpu.uploaded)
        ds_vk_pending_texture(t);
    DSCmd *p = push(DS_CMD_TEX_EX);
    if (!p)
        return;
    p->v.txe.x = x;
    p->v.txe.y = y;
    p->v.txe.ang_rad = angle_rad;
    p->v.txe.sx = sx;
    p->v.txe.sy = sy;
    p->v.txe.ox = ox;
    p->v.txe.oy = oy;
    p->v.txe.tx = t;
    p->v.txe.c = pack_c(tint);
}

void text_n(const char *s, size_t byte_len, float x, float y, uint32_t c, float sc) {
    if (!frame_open || !s || byte_len == 0 || sc <= 0.0f || !ensure_font())
        return;
    char *copy = cmd_arena_str_n(s, byte_len);
    if (!copy)
        return;
    DSCmd *p = push(DS_CMD_TEXT);
    if (!p)
        return;
    p->v.tt.s = copy;
    p->v.tt.x = x;
    p->v.tt.y = y;
    p->v.tt.sc = sc;
    p->v.tt.c = pack_c(c);
}

void text_scaled(const char *s, float x, float y, uint32_t c, float sc) {
    if (!s || !*s)
        return;
    text_n(s, strlen(s), x, y, c, sc);
}

void text(const char *s, float x, float y, uint32_t c) {
    text_scaled(s, x, y, c, 1.0f);
}

float text_width_n(const char *s, size_t byte_len, float scale) {
    if (!s || byte_len == 0 || scale <= 0.0f || !ensure_font())
        return 0.0f;
    const char *end = s + byte_len;
    float cur = 0.0f, best = 0.0f;
    const char *p = s;
    while (p < end && *p) {
        int cp = utf8_dec(&p);
        if (p > end)
            break;
        if (cp == '\n') {
            if (cur > best)
                best = cur;
            cur = 0.0f;
            continue;
        }
        const DSFontGlyph *g = ds_font_glyph(font, (uint32_t)cp);
        if (g)
            cur += g->advance * scale;
    }
    return cur > best ? cur : best;
}

float text_width(const char *s, float scale) {
    return s ? text_width_n(s, strlen(s), scale) : 0.0f;
}

float text_line_height(float scale) {
    if (scale <= 0.0f || !ensure_font())
        return (float)DS_FONT_PIXEL_HEIGHT * (scale > 0.0f ? scale : 1.0f);
    return ds_font_lineh(font) * scale;
}

float text_mono_advance(float scale) {
    if (scale <= 0.0f || !ensure_font())
        return 14.0f * (scale > 0.0f ? scale : 1.0f);
    const DSFontGlyph *g = ds_font_glyph(font, 'M');
    if (!g)
        g = ds_font_glyph(font, '0');
    return g ? g->advance * scale : 14.0f * scale;
}
