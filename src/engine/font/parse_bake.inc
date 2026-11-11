#include <limits.h>
#ifndef DS_FONT_TYPEDEF
#define DS_FONT_TYPEDEF
typedef struct DSFont DSFont;
#endif
void ds_font_destroy(DSFont *font);
#define DS_FONT_ATLAS_W 1024
#define DS_FONT_ATLAS_H 2048
#define DS_FONT_SS 4
#define DS_FONT_MAX 640
#define ARG_WORDS 0x0001
#define ARG_XY 0x0002
#define HAVE_SCALE 0x0008
#define MORE 0x0020
#define HAVE_XY_SCALE 0x0040
#define HAVE_2X2 0x0080
#define TAG(a, b, c, d) ((uint32_t)(a) << 24 | (uint32_t)(b) << 16 | (uint32_t)(c) << 8 | (uint32_t)(d))
typedef struct {
    float x, y;
    int on_curve;
} DSPoint;
typedef struct {
    DSPoint *points;
    int pc, pp;
    int *ends;
    int cc, cp;
} DSOutline;
typedef struct {
    float x, y;
} DSFPoint;
typedef struct {
    DSFPoint *points;
    int count, cap;
} DSFC;
struct DSFont {
    uint8_t *data;
    size_t size;
    int upem, loc_format, ng, nhm, asc_u, desc_u, lg_u;
    uint32_t head, hhea, hmtx, maxp, loca, glyf, cmap, cmap4, cmap12;
    uint32_t hmtx_len, loca_len, glyf_len, cmap_len, cmap4_len, cmap12_len;
    float scale, ascent, line_h;
    int aw, ah;
    uint8_t *alpha;
    DSFontGlyph *glyphs;
    int gcount;

    int *hmap;
    int hcap, qmark;
};
static int in_r(const DSFont *f, size_t o, size_t l) {
    return f && o <= f->size && l <= f->size - o;
}
static uint16_t bu16(const DSFont *f, size_t o) {
    if (!in_r(f, o, 2))
        return 0;
    return (uint16_t)((f->data[o] << 8) | f->data[o + 1]);
}
static int16_t bs16(const DSFont *f, size_t o) {
    return (int16_t)bu16(f, o);
}
static uint32_t bu32(const DSFont *f, size_t o) {
    if (!in_r(f, o, 4))
        return 0;
    return (uint32_t)f->data[o] << 24 | (uint32_t)f->data[o + 1] << 16 | (uint32_t)f->data[o + 2] << 8 |
           (uint32_t)f->data[o + 3];
}
static int in_span(const DSFont *f, size_t start, size_t end, size_t o, size_t n) {
    return start <= end && o >= start && o <= end && n <= end - o && in_r(f, o, n);
}
static int tbound(const DSFont *f, uint32_t tag, uint32_t *off, uint32_t *len) {
    if (!f || !in_r(f, 0, 12))
        return 0;
    uint16_t n = bu16(f, 4);
    for (size_t i = 0; i < n; i++) {
        size_t r = 12 + i * 16;
        if (!in_r(f, r, 16))
            return 0;
        if (bu32(f, r) == tag) {
            uint32_t a = bu32(f, r + 8), l = bu32(f, r + 12);
            if (!in_r(f, a, l))
                return 0;
            if (off)
                *off = a;
            if (len)
                *len = l;
            return 1;
        }
    }
    return 0;
}
static void ol_init(DSOutline *o) {
    memset(o, 0, sizeof(*o));
}
static void ol_free(DSOutline *o) {
    if (!o)
        return;
    free(o->points);
    free(o->ends);
    memset(o, 0, sizeof(*o));
}
static int ol_rsrv_p(DSOutline *o, int n) {
    int need = o->pc + n, cap = o->pp ? o->pp : 32;
    while (cap < need) {
        if (cap > 1e6)
            return 0;
        cap *= 2;
    }
    DSPoint *p = (DSPoint *)realloc(o->points, (size_t)cap * sizeof(*p));
    if (!p)
        return 0;
    o->points = p;
    o->pp = cap;
    return 1;
}
static int ol_rsrv_c(DSOutline *o, int n) {
    int need = o->cc + n, cap = o->cp ? o->cp : 8;
    while (cap < need) cap *= 2;
    int *e = (int *)realloc(o->ends, (size_t)cap * sizeof(*e));
    if (!e)
        return 0;
    o->ends = e;
    o->cp = cap;
    return 1;
}
static int ol_add(DSOutline *o, const DSPoint *p, int n) {
    if (n <= 0 || !ol_rsrv_p(o, n) || !ol_rsrv_c(o, 1))
        return 0;
    memcpy(o->points + o->pc, p, (size_t)n * sizeof(*p));
    o->pc += n;
    o->ends[o->cc++] = o->pc - 1;
    return 1;
}
static int g_offs(const DSFont *f, int g, uint32_t *s, uint32_t *e) {
    if (!f || g < 0 || g >= f->ng || (f->loc_format != 0 && f->loc_format != 1))
        return 0;
    size_t unit = f->loc_format == 0 ? 2 : 4;
    size_t entry = (size_t)f->loca + (size_t)g * unit;
    size_t loca_end = (size_t)f->loca + f->loca_len;
    if (!in_span(f, f->loca, loca_end, entry, unit * 2))
        return 0;
    uint32_t a, b;
    if (f->loc_format == 0) {
        a = (uint32_t)bu16(f, entry) * 2;
        b = (uint32_t)bu16(f, entry + 2) * 2;
    } else {
        a = bu32(f, entry);
        b = bu32(f, entry + 4);
    }
    size_t glyf_end = (size_t)f->glyf + f->glyf_len;
    if (a > b || b > f->glyf_len || !in_span(f, f->glyf, glyf_end, (size_t)f->glyf + a, b - a))
        return 0;
    if (s)
        *s = a;
    if (e)
        *e = b;
    return 1;
}
static int read_outline(const DSFont *f, int g, int depth, float a, float b, float c, float d, float tx, float ty,
                        DSOutline *dst);
static int read_simple(const DSFont *f, uint32_t off, uint32_t glyph_end, int nc, float a, float b, float c,
                       float d, float tx, float ty, DSOutline *dst) {
    if (nc == 0)
        return 1;
    if (nc < 0 || nc > 4096 || off > glyph_end)
        return 0;
    size_t start = (size_t)f->glyf + off;
    size_t end = (size_t)f->glyf + glyph_end;
    size_t ends_at = start + 10;
    size_t cur = ends_at + (size_t)nc * 2;
    if (!in_span(f, start, end, ends_at, (size_t)nc * 2 + 2))
        return 0;
    int *ends = (int *)malloc((size_t)nc * sizeof(*ends));
    if (!ends)
        return 0;
    int previous = -1;
    for (int i = 0; i < nc; i++) {
        ends[i] = (int)bu16(f, ends_at + (size_t)i * 2);
        if (ends[i] <= previous) {
            free(ends);
            return 0;
        }
        previous = ends[i];
    }
    int pc = ends[nc - 1] + 1;
    if (pc <= 0 || pc > 200000 || !in_span(f, start, end, cur, 2)) {
        free(ends);
        return 0;
    }
    int ilen = bu16(f, cur);
    cur += 2;
    if (!in_span(f, start, end, cur, (size_t)ilen)) {
        free(ends);
        return 0;
    }
    cur += (size_t)ilen;
    int *flags = (int *)malloc((size_t)pc * sizeof(*flags));
    DSPoint *pts = (DSPoint *)calloc((size_t)pc, sizeof(*pts));
    if (!flags || !pts) {
        free(ends);
        free(flags);
        free(pts);
        return 0;
    }
    int p = 0;
    while (p < pc) {
        if (!in_span(f, start, end, cur, 1))
            goto fail;
        uint8_t fl = f->data[cur++];
        flags[p++] = fl;
        if (fl & 8) {
            if (!in_span(f, start, end, cur, 1))
                goto fail;
            int rep = (int)f->data[cur++];
            if (rep > pc - p)
                goto fail;
            while (rep-- > 0)
                flags[p++] = fl;
        }
    }
    int64_t x = 0;
    for (p = 0; p < pc; p++) {
        int fl = flags[p], dlt = 0;
        if (fl & 2) {
            if (!in_span(f, start, end, cur, 1))
                goto fail;
            dlt = f->data[cur++];
            if (!(fl & 16))
                dlt = -dlt;
        } else if (!(fl & 16)) {
            if (!in_span(f, start, end, cur, 2))
                goto fail;
            dlt = bs16(f, cur);
            cur += 2;
        }
        x += dlt;
        pts[p].x = (float)x;
        pts[p].on_curve = (fl & 1) != 0;
    }
    int64_t y = 0;
    for (p = 0; p < pc; p++) {
        int fl = flags[p], dlt = 0;
        if (fl & 4) {
            if (!in_span(f, start, end, cur, 1))
                goto fail;
            dlt = f->data[cur++];
            if (!(fl & 32))
                dlt = -dlt;
        } else if (!(fl & 32)) {
            if (!in_span(f, start, end, cur, 2))
                goto fail;
            dlt = bs16(f, cur);
            cur += 2;
        }
        y += dlt;
        pts[p].y = (float)y;
    }
    int st = 0;
    for (int cn = 0; cn < nc; cn++) {
        int en = ends[cn];
        if (en < st || en >= pc)
            goto fail;
        int cnt = en - st + 1;
        DSPoint *tr = (DSPoint *)malloc((size_t)cnt * sizeof(*tr));
        if (!tr)
            goto fail;
        for (int i = 0; i < cnt; i++) {
            DSPoint in = pts[st + i];
            tr[i].x = a * in.x + c * in.y + tx;
            tr[i].y = b * in.x + d * in.y + ty;
            tr[i].on_curve = in.on_curve;
        }
        if (!ol_add(dst, tr, cnt)) {
            free(tr);
            goto fail;
        }
        free(tr);
        st = en + 1;
    }
    if (st != pc)
        goto fail;
    free(ends);
    free(flags);
    free(pts);
    return 1;
fail:
    free(ends);
    free(flags);
    free(pts);
    return 0;
}
static int read_composite(const DSFont *f, uint32_t off, uint32_t glyph_end, int depth, float a, float b, float c,
                          float d, float tx, float ty, DSOutline *dst) {
    size_t start = (size_t)f->glyf + off;
    size_t end = (size_t)f->glyf + glyph_end;
    size_t cur = start + 10;
    int flags = MORE;
    int components = 0;
    while (flags & MORE) {
        if (++components > 1024 || !in_span(f, start, end, cur, 4))
            return 0;
        flags = bu16(f, cur);
        int comp = bu16(f, cur + 2);
        cur += 4;
        int a1, a2;
        size_t args = (flags & ARG_WORDS) ? 4 : 2;
        if (!in_span(f, start, end, cur, args))
            return 0;
        if (flags & ARG_WORDS) {
            a1 = bs16(f, cur);
            a2 = bs16(f, cur + 2);
        } else {
            a1 = (int8_t)f->data[cur];
            a2 = (int8_t)f->data[cur + 1];
        }
        cur += args;
        float ca = 1, cb = 0, cc = 0, cd = 1, dx = 0, dy = 0;
        if (flags & ARG_XY) {
            dx = (float)a1;
            dy = (float)a2;
        }
        if (flags & HAVE_SCALE) {
            if (!in_span(f, start, end, cur, 2))
                return 0;
            int16_t scale = bs16(f, cur);
            ca = cd = (float)scale / 16384.0f;
            cur += 2;
        } else if (flags & HAVE_XY_SCALE) {
            if (!in_span(f, start, end, cur, 4))
                return 0;
            ca = (float)bs16(f, cur) / 16384.0f;
            cd = (float)bs16(f, cur + 2) / 16384.0f;
            cur += 4;
        } else if (flags & HAVE_2X2) {
            if (!in_span(f, start, end, cur, 8))
                return 0;
            ca = (float)bs16(f, cur) / 16384.0f;
            cb = (float)bs16(f, cur + 2) / 16384.0f;
            cc = (float)bs16(f, cur + 4) / 16384.0f;
            cd = (float)bs16(f, cur + 6) / 16384.0f;
            cur += 8;
        }
        float na = a * ca + c * cb, nb = b * ca + d * cb, nc = a * cc + c * cd, nd = b * cc + d * cd;
        float ntx = a * dx + c * dy + tx, nty = b * dx + d * dy + ty;
        if (!read_outline(f, comp, depth + 1, na, nb, nc, nd, ntx, nty, dst))
            return 0;
    }
    return 1;
}
static int read_outline(const DSFont *f, int g, int depth, float a, float b, float c, float d, float tx, float ty,
                        DSOutline *dst) {
    uint32_t s, e;
    if (depth > 16 || !g_offs(f, g, &s, &e))
        return 0;
    if (s == e)
        return 1;
    size_t start = (size_t)f->glyf + s;
    size_t end = (size_t)f->glyf + e;
    if (!in_span(f, start, end, start, 10))
        return 0;
    int16_t cc = bs16(f, start);
    if (cc >= 0)
        return read_simple(f, s, e, cc, a, b, c, d, tx, ty, dst);
    return read_composite(f, s, e, depth, a, b, c, d, tx, ty, dst);
}
