/* Build glyph quads for solid, rainbow, and subtle shimmer text runs. */

/* Map hue in [0, 1) to packed RGB. */
static uint32_t rainbow_rgb(float hue) {
    hue = hue - floorf(hue);
    float r = fabsf(hue * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - fabsf(hue * 6.0f - 2.0f);
    float b = 2.0f - fabsf(hue * 6.0f - 4.0f);
    r = r < 0 ? 0 : (r > 1 ? 1 : r);
    g = g < 0 ? 0 : (g > 1 ? 1 : g);
    b = b < 0 ? 0 : (b > 1 ? 1 : b);
    return RGB(0,0,0) | ((uint32_t)(r * 255.0f) << 16) | ((uint32_t)(g * 255.0f) << 8) |
           (uint32_t)(b * 255.0f);
}

/* Shift base color gently in hue and brightness for subtle per-glyph shimmer. */
static uint32_t shimmer_rgb(uint32_t base, float w) {
    uint32_t a = (base >> 24) & 0xFFu;
    if (!a)
        a = 255u;
    float r = ((base >> 16) & 0xFFu) / 255.0f;
    float g = ((base >> 8) & 0xFFu) / 255.0f;
    float b = (base & 0xFFu) / 255.0f;
    float mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    float mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    float d = mx - mn;
    float h = 0.0f;
    if (d > 1e-5f) {
        if (mx == r)
            h = (g - b) / d + (g < b ? 6.0f : 0.0f);
        else if (mx == g)
            h = (b - r) / d + 2.0f;
        else
            h = (r - g) / d + 4.0f;
        h /= 6.0f;
    }
    float s = mx > 1e-5f ? d / mx : 0.0f;
    h = h + 0.032f * w;
    h = h - floorf(h);
    s = s - 0.14f * (w > 0.0f ? w : 0.0f);
    if (s < 0.0f)
        s = 0.0f;
    float v = mx + 0.13f * w;
    if (v < 0.0f)
        v = 0.0f;
    if (v > 1.0f)
        v = 1.0f;
    float hr = fabsf(h * 6.0f - 3.0f) - 1.0f;
    float hg = 2.0f - fabsf(h * 6.0f - 2.0f);
    float hb = 2.0f - fabsf(h * 6.0f - 4.0f);
    hr = hr < 0.0f ? 0.0f : (hr > 1.0f ? 1.0f : hr);
    hg = hg < 0.0f ? 0.0f : (hg > 1.0f ? 1.0f : hg);
    hb = hb < 0.0f ? 0.0f : (hb > 1.0f ? 1.0f : hb);
    r = v * (1.0f - s + s * hr);
    g = v * (1.0f - s + s * hg);
    b = v * (1.0f - s + s * hb);
    return (a << 24) | ((uint32_t)(r * 255.0f + 0.5f) << 16) | ((uint32_t)(g * 255.0f + 0.5f) << 8) |
           (uint32_t)(b * 255.0f + 0.5f);
}

static int geo_text_shimmer(const char *s, float x, float y, uint32_t base_c, float sc, float phase,
                            const DSFont *fnt) {
    if (!s || !fnt || !isfinite(x + y + sc + phase) || sc <= 0)
        return 1;
    float asc = ds_font_ascent(fnt), lb = 0;
    const DSFontGlyph *ref = ds_font_glyph(fnt, 'S');
    if (ref) {
        asc = ref->bearing_top;
        lb = ref->bearing_x;
    }
    float pen = x - lb * sc, base = y + asc * sc;
    int idx = 0;
    for (const char *cur = s; *cur;) {
        int cp = utf8_dec(&cur);
        if (cp == '\n') {
            pen = x - lb * sc;
            base += ds_font_lineh(fnt) * sc;
            idx = 0;
            continue;
        }
        const DSFontGlyph *g = ds_font_glyph(fnt, (uint32_t)cp);
        if (!g)
            continue;
        if (g->width > 0 && g->height > 0) {
            uint32_t c = pack_c(shimmer_rgb(base_c, sinf(phase * 3.0f - (float)idx * 0.45f)));
            GeoVert q[4] = {
                {pen + g->bearing_x * sc, base - g->bearing_top * sc, g->u0, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - g->bearing_top * sc, g->u1, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - (g->bearing_top - g->height) * sc, g->u1, g->v1, c},
                {pen + g->bearing_x * sc, base - (g->bearing_top - g->height) * sc, g->u0, g->v1, c},
            };
            if (!geo_quad(q))
                return 0;
        }
        pen += g->advance * sc;
        idx++;
    }
    return 1;
}

static int geo_text_rainbow(const char *s, float x, float y, float sc, float hue, const DSFont *fnt) {
    if (!s || !fnt || !isfinite(x + y + sc + hue) || sc <= 0)
        return 1;
    float asc = ds_font_ascent(fnt), lb = 0;
    const DSFontGlyph *ref = ds_font_glyph(fnt, 'S');
    if (ref) {
        asc = ref->bearing_top;
        lb = ref->bearing_x;
    }
    float pen = x - lb * sc, base = y + asc * sc;
    int idx = 0;
    for (const char *cur = s; *cur;) {
        int cp = utf8_dec(&cur);
        if (cp == '\n') {
            pen = x - lb * sc;
            base += ds_font_lineh(fnt) * sc;
            idx = 0;
            continue;
        }
        const DSFontGlyph *g = ds_font_glyph(fnt, (uint32_t)cp);
        if (!g)
            continue;
        if (g->width > 0 && g->height > 0) {
            uint32_t c = pack_c(rainbow_rgb(hue + (float)idx * 0.09f));
            GeoVert q[4] = {
                {pen + g->bearing_x * sc, base - g->bearing_top * sc, g->u0, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - g->bearing_top * sc, g->u1, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - (g->bearing_top - g->height) * sc, g->u1, g->v1, c},
                {pen + g->bearing_x * sc, base - (g->bearing_top - g->height) * sc, g->u0, g->v1, c},
            };
            if (!geo_quad(q))
                return 0;
        }
        pen += g->advance * sc;
        idx++;
    }
    return 1;
}

static int geo_text(const char *s, float x, float y, uint32_t c, float sc, const DSFont *fnt) {
    if (!s || !fnt || !isfinite(x + y + sc) || sc <= 0)
        return 1;
    float asc = ds_font_ascent(fnt), lb = 0;
    const DSFontGlyph *ref = ds_font_glyph(fnt, 'S');
    if (ref) {
        asc = ref->bearing_top;
        lb = ref->bearing_x;
    }
    float pen = x - lb * sc, base = y + asc * sc;
    for (const char *cur = s; *cur;) {
        int cp = utf8_dec(&cur);
        if (cp == '\n') {
            pen = x - lb * sc;
            base += ds_font_lineh(fnt) * sc;
            continue;
        }
        const DSFontGlyph *g = ds_font_glyph(fnt, (uint32_t)cp);
        if (!g)
            continue;
        if (g->width > 0 && g->height > 0) {
            GeoVert q[4] = {
                {pen + g->bearing_x * sc, base - g->bearing_top * sc, g->u0, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - g->bearing_top * sc, g->u1, g->v0, c},
                {pen + (g->bearing_x + g->width) * sc, base - (g->bearing_top - g->height) * sc, g->u1, g->v1, c},
                {pen + g->bearing_x * sc, base - (g->bearing_top - g->height) * sc, g->u0, g->v1, c},
            };
            if (!geo_quad(q))
                return 0;
        }
        pen += g->advance * sc;
    }
    return 1;
}
