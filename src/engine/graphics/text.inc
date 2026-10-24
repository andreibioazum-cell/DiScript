/* Build glyph quads for solid and rainbow text runs. */

/* Map hue in [0, 1) to packed RGB. */
static uint32_t rainbow_rgb(float hue) {
    hue = hue - floorf(hue);
    float r = fabsf(hue * 6.0f - 3.0f) - 1.0f;
    float g = 2.0f - fabsf(hue * 6.0f - 2.0f);
    float b = 2.0f - fabsf(hue * 6.0f - 4.0f);
    r = r < 0 ? 0 : (r > 1 ? 1 : r);
    g = g < 0 ? 0 : (g > 1 ? 1 : g);
    b = b < 0 ? 0 : (b > 1 ? 1 : b);
    return 0xFF000000u | ((uint32_t)(r * 255.0f) << 16) | ((uint32_t)(g * 255.0f) << 8) |
           (uint32_t)(b * 255.0f);
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
