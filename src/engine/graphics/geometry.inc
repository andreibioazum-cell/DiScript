#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct {
    float x, y, u, v;
    uint32_t c;
} GeoVert;

static GeoVert *geo_verts;
static size_t geo_vn, geo_vcap;
static uint32_t *geo_idx;
static size_t geo_in, geo_icap;

static int geo_push_vert(float x, float y, float u, float v, uint32_t c) {
    x += gfx_shake_dx;
    y += gfx_shake_dy;
    if (geo_vn >= 0x3FFFFFFFu)
        return 0;
    if (geo_vn == geo_vcap) {
        size_t cap = geo_vcap ? geo_vcap * 2 : 2048;
        if (cap > SIZE_MAX / sizeof(*geo_verts))
            return 0;
        GeoVert *nv = (GeoVert *)realloc(geo_verts, cap * sizeof(*nv));
        if (!nv)
            return 0;
        geo_verts = nv;
        geo_vcap = cap;
    }
    GeoVert *g = &geo_verts[geo_vn++];
    g->x = x;
    g->y = y;
    g->u = u;
    g->v = v;
    g->c = c;
    return 1;
}

static int geo_push_index(uint32_t i) {
    if (geo_in == geo_icap) {
        size_t cap = geo_icap ? geo_icap * 2 : 4096;
        if (cap > SIZE_MAX / sizeof(*geo_idx))
            return 0;
        uint32_t *ni = (uint32_t *)realloc(geo_idx, cap * sizeof(*ni));
        if (!ni)
            return 0;
        geo_idx = ni;
        geo_icap = cap;
    }
    geo_idx[geo_in++] = i;
    return 1;
}

static int geo_quad(const GeoVert q[4]) {
    uint32_t base = (uint32_t)geo_vn;
    for (int i = 0; i < 4; i++)
        if (!geo_push_vert(q[i].x, q[i].y, q[i].u, q[i].v, q[i].c))
            return 0;
    static const uint32_t order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++)
        if (!geo_push_index(base + order[i]))
            return 0;
    return 1;
}

static void geo_reset(void) {
    geo_vn = 0;
    geo_in = 0;
}

#define GEO_TRIG_SLOTS 12
typedef struct {
    int n, mode;
    float *cosv, *sinv;
} GeoTrig;
static GeoTrig geo_trig[GEO_TRIG_SLOTS];
static int geo_trig_next;

static const GeoTrig *geo_trig_table(int n, int mode) {
    if (n < 3 || n > 4096)
        return NULL;
    for (int i = 0; i < GEO_TRIG_SLOTS; i++) {
        GeoTrig *t = &geo_trig[i];
        if (t->cosv && t->n == n && t->mode == mode)
            return t;
    }
    GeoTrig *t = &geo_trig[geo_trig_next];
    geo_trig_next = (geo_trig_next + 1) % GEO_TRIG_SLOTS;
    free(t->cosv);
    free(t->sinv);
    t->cosv = NULL;
    t->sinv = NULL;
    t->n = 0;
    float *c = (float *)malloc((size_t)(n + 1) * sizeof(float));
    float *v = (float *)malloc((size_t)(n + 1) * sizeof(float));
    if (!c || !v) {
        free(c);
        free(v);
        return NULL;
    }
    for (int i = 0; i <= n; i++) {
        float a = mode == 0 ? (float)i * (float)(2.0 * M_PI) / (float)n : (float)((double)i / n * 2.0 * M_PI);
        c[i] = cosf(a);
        v[i] = sinf(a);
    }
    t->cosv = c;
    t->sinv = v;
    t->n = n;
    t->mode = mode;
    return t;
}

static void geo_trig_free(void) {
    for (int i = 0; i < GEO_TRIG_SLOTS; i++) {
        free(geo_trig[i].cosv);
        free(geo_trig[i].sinv);
        geo_trig[i].cosv = geo_trig[i].sinv = NULL;
        geo_trig[i].n = geo_trig[i].mode = 0;
    }
}

static int geo_rect(float x, float y, float w, float h, uint32_t c) {
    if (!isfinite(x + y + w + h) || w <= 0 || h <= 0)
        return 1;
    GeoVert q[4] = {
        {x, y, 0, 0, c},
        {x + w, y, 1, 0, c},
        {x + w, y + h, 1, 1, c},
        {x, y + h, 0, 1, c},
    };
    return geo_quad(q);
}

static int geo_circle_segs(float r) {
    if (r < 0)
        r = -r;
    int n = (int)(r * 0.65f);
    if (n < 12)
        n = 12;
    if (n > 64)
        n = 64;
    return n;
}

static int geo_circle(float x, float y, float r, uint32_t c) {
    if (!isfinite(x + y + r) || r <= 0)
        return 1;
    int n = geo_circle_segs(r);
    uint32_t base = (uint32_t)geo_vn;
    if (!geo_push_vert(x, y, 0.5f, 0.5f, c))
        return 0;
    const GeoTrig *tt = geo_trig_table(n, 0);
    for (int i = 0; i < n; i++) {
        float ca, sa;
        if (tt) {
            ca = tt->cosv[i];
            sa = tt->sinv[i];
        } else {
            float a = (float)i * (float)(2.0 * M_PI) / (float)n;
            ca = cosf(a);
            sa = sinf(a);
        }
        if (!geo_push_vert(x + ca * r, y + sa * r, 0, 0, c))
            return 0;
    }
    for (int i = 1; i <= n; i++)
        if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
            !geo_push_index(base + (uint32_t)((i % n) + 1)))
            return 0;
    return 1;
}

static int geo_ellipse(float x, float y, float rx, float ry, uint32_t c) {
    if (!isfinite(x + y + rx + ry) || rx <= 0 || ry <= 0)
        return 1;
    int n = geo_circle_segs(rx > ry ? rx : ry);
    uint32_t base = (uint32_t)geo_vn;
    if (!geo_push_vert(x, y, 0.5f, 0.5f, c))
        return 0;
    const GeoTrig *tt = geo_trig_table(n, 0);
    for (int i = 0; i < n; i++) {
        float ca = tt ? tt->cosv[i] : cosf((float)i * (float)(2.0 * M_PI) / (float)n);
        float sa = tt ? tt->sinv[i] : sinf((float)i * (float)(2.0 * M_PI) / (float)n);
        if (!geo_push_vert(x + ca * rx, y + sa * ry, 0, 0, c))
            return 0;
    }
    for (int i = 1; i <= n; i++)
        if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
            !geo_push_index(base + (uint32_t)((i % n) + 1)))
            return 0;
    return 1;
}

static int geo_sector(float x, float y, float r, float start_deg, float sweep_deg, uint32_t c) {
    if (!isfinite(x + y + r + start_deg + sweep_deg) || r <= 0 || sweep_deg == 0)
        return 1;
    if (sweep_deg > 360)
        sweep_deg = 360;
    else if (sweep_deg < -360)
        sweep_deg = -360;
    int n = (int)ceilf(fabsf(sweep_deg) / 4.0f);
    if (n < 1)
        n = 1;
    if (n > 90)
        n = 90;
    uint32_t base = (uint32_t)geo_vn;
    if (!geo_push_vert(x, y, 0.5f, 0.5f, c))
        return 0;
    for (int i = 0; i <= n; i++) {
        float a = (start_deg + sweep_deg * ((float)i / (float)n)) * (float)(M_PI / 180.0);
        if (!geo_push_vert(x + cosf(a) * r, y + sinf(a) * r, 0, 0, c))
            return 0;
    }
    for (int i = 1; i <= n; i++)
        if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
            !geo_push_index(base + (uint32_t)(i + 1)))
            return 0;
    return 1;
}

static int geo_ring(float x, float y, float r, float th, uint32_t c) {
    if (!isfinite(x + y + r + th) || r <= 0 || th <= 0)
        return 1;
    if (th >= r)
        return geo_circle(x, y, r, c);
    int n = geo_circle_segs(r);
    uint32_t base = (uint32_t)geo_vn;
    const GeoTrig *tt = geo_trig_table(n, 1);
    for (int i = 0; i <= n; i++) {
        float ca = tt ? tt->cosv[i] : cosf((float)((double)i / n * 2.0 * M_PI));
        float sa = tt ? tt->sinv[i] : sinf((float)((double)i / n * 2.0 * M_PI));
        if (!geo_push_vert(x + ca * (r - th), y + sa * (r - th), 0, 0, c))
            return 0;
        if (!geo_push_vert(x + ca * r, y + sa * r, 0, 0, c))
            return 0;
    }
    for (int i = 0; i < n; i++) {
        uint32_t b = base + (uint32_t)i * 2;
        if (!geo_push_index(b) || !geo_push_index(b + 1) || !geo_push_index(b + 2))
            return 0;
        if (!geo_push_index(b + 1) || !geo_push_index(b + 3) || !geo_push_index(b + 2))
            return 0;
    }
    return 1;
}

static int geo_line(float x1, float y1, float x2, float y2, float th, uint32_t c) {
    if (!isfinite(x1 + y1 + x2 + y2 + th) || th <= 0)
        return 1;
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float rad = th * 0.5f;
    if (len <= 0.0001f)
        return geo_circle(x1, y1, rad, c);
    float nx = -dy / len * rad, ny = dx / len * rad;
    GeoVert q[4] = {
        {x1 + nx, y1 + ny, 0, 0, c},
        {x2 + nx, y2 + ny, 1, 0, c},
        {x2 - nx, y2 - ny, 1, 1, c},
        {x1 - nx, y1 - ny, 0, 1, c},
    };
    if (!geo_quad(q))
        return 0;
    if (th >= 2.5f) {
        int n = geo_circle_segs(rad) / 2;
        if (n < 5)
            n = 5;
        for (int cap = 0; cap < 2; cap++) {
            float cx = cap ? x2 : x1, cy = cap ? y2 : y1;
            float dir = cap ? -1.0f : 1.0f;
            float base_ang = atan2f(dy, dx) + (float)M_PI * 0.5f;
            uint32_t base = (uint32_t)geo_vn;
            if (!geo_push_vert(cx, cy, 0, 0, c))
                return 0;
            for (int i = 0; i <= n; i++) {
                float a = base_ang + dir * (float)((double)i / n * M_PI);
                if (!geo_push_vert(cx + cosf(a) * rad, cy + sinf(a) * rad, 0, 0, c))
                    return 0;
            }
            for (int i = 1; i <= n; i++)
                if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
                    !geo_push_index(base + (uint32_t)(i + 1)))
                    return 0;
        }
    }
    return 1;
}

static int geo_roundrect(float x, float y, float w, float h, float rad, uint32_t c) {
    if (!isfinite(x + y + w + h + rad) || w <= 0 || h <= 0)
        return 1;
    if (rad < 0)
        rad = 0;
    if (rad > w * 0.5f)
        rad = w * 0.5f;
    if (rad > h * 0.5f)
        rad = h * 0.5f;
    if (rad < 0.5f)
        return geo_rect(x, y, w, h, c);
    if (!geo_rect(x + rad, y, w - rad * 2, h, c))
        return 0;
    if (!geo_rect(x, y + rad, rad, h - rad * 2, c))
        return 0;
    if (!geo_rect(x + w - rad, y + rad, rad, h - rad * 2, c))
        return 0;
    int n = (int)ceilf(rad * 0.35f);
    if (n < 6)
        n = 6;
    if (n > 20)
        n = 20;
    static const float cxs[4] = {1, 0, 0, 1}, cys[4] = {1, 1, 0, 0};
    static const float a0s[4] = {0, (float)M_PI * 0.5f, (float)M_PI, (float)M_PI * 1.5f};
    for (int k = 0; k < 4; k++) {
        float cx = x + cxs[k] * (w - rad) + (cxs[k] ? 0 : rad);
        float cy = y + cys[k] * (h - rad) + (cys[k] ? 0 : rad);
        uint32_t base = (uint32_t)geo_vn;
        if (!geo_push_vert(cx, cy, 0, 0, c))
            return 0;
        for (int i = 0; i <= n; i++) {
            float a = a0s[k] + (float)((double)i / n * M_PI * 0.5);
            if (!geo_push_vert(cx + cosf(a) * rad, cy + sinf(a) * rad, 0, 0, c))
                return 0;
        }
        for (int i = 1; i <= n; i++)
            if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
                !geo_push_index(base + (uint32_t)(i + 1)))
                return 0;
    }
    return 1;
}

static int geo_rect_rot(float x, float y, float w, float h, float ang, uint32_t c) {
    if (!isfinite(x + y + w + h + ang) || w <= 0 || h <= 0)
        return 1;
    if (ang == 0.0f)
        return geo_rect(x, y, w, h, c);
    float hw = w * 0.5f, hh = h * 0.5f;
    float ca = cosf(ang), sa = sinf(ang);
    float cx = x + hw, cy = y + hh;
    static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
    GeoVert q[4];
    for (int i = 0; i < 4; i++) {
        float lx = sx[i] * hw, ly = sy[i] * hh;
        q[i].x = cx + ca * lx - sa * ly;
        q[i].y = cy + sa * lx + ca * ly;
        q[i].u = 0;
        q[i].v = 0;
        q[i].c = c;
    }
    return geo_quad(q);
}

static int geo_pt_in_tri(float px, float py, float ax, float ay, float bx, float by, float cx, float cy) {
    float c1 = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    float c2 = (cx - bx) * (py - by) - (cy - by) * (px - bx);
    float c3 = (ax - cx) * (py - cy) - (ay - cy) * (px - cx);
    return (c1 >= -1e-4f && c2 >= -1e-4f && c3 >= -1e-4f) || (c1 <= 1e-4f && c2 <= 1e-4f && c3 <= 1e-4f);
}

static int geo_poly(const float *xy, int count, uint32_t c) {
    if (!xy || count < 3)
        return 1;
    uint32_t base = (uint32_t)geo_vn;
    for (int i = 0; i < count; i++) {
        if (!isfinite(xy[i * 2] + xy[i * 2 + 1]))
            return 1;
        if (!geo_push_vert(xy[i * 2], xy[i * 2 + 1], 0, 0, c))
            return 0;
    }
    if (count == 3) {
        return geo_push_index(base) && geo_push_index(base + 1) && geo_push_index(base + 2);
    }
    if (count <= 64) {
        /* Ear-clipping triangulation supporting concave LÖVE2D polygons */
        int V[64];
        double area = 0.0;
        for (int i = 0; i < count; i++) {
            int j = (i + 1) % count;
            area += (double)xy[i * 2] * xy[j * 2 + 1] - (double)xy[j * 2] * xy[i * 2 + 1];
        }
        if (area >= 0.0) {
            for (int i = 0; i < count; i++) V[i] = i;
        } else {
            for (int i = 0; i < count; i++) V[i] = count - 1 - i;
        }
        int nv = count;
        int guard = nv * 3 + 4;
        int v = nv - 1;
        while (nv > 2 && guard-- > 0) {
            int u = v >= nv ? 0 : v;
            v = u + 1 >= nv ? 0 : u + 1;
            int w = v + 1 >= nv ? 0 : v + 1;
            float ax = xy[V[u] * 2], ay = xy[V[u] * 2 + 1];
            float bx = xy[V[v] * 2], by = xy[V[v] * 2 + 1];
            float cx = xy[V[w] * 2], cy = xy[V[w] * 2 + 1];
            float cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
            if (cross > 1e-5f) {
                int ear = 1;
                for (int p = 0; p < nv; p++) {
                    if (p == u || p == v || p == w)
                        continue;
                    if (geo_pt_in_tri(xy[V[p] * 2], xy[V[p] * 2 + 1], ax, ay, bx, by, cx, cy)) {
                        ear = 0;
                        break;
                    }
                }
                if (ear) {
                    if (!geo_push_index(base + (uint32_t)V[u]) ||
                        !geo_push_index(base + (uint32_t)V[v]) ||
                        !geo_push_index(base + (uint32_t)V[w]))
                        return 0;
                    for (int s = v; s + 1 < nv; s++) V[s] = V[s + 1];
                    nv--;
                    guard = nv * 3 + 4;
                }
            }
        }
        if (nv <= 2)
            return 1;
    }
    for (int i = 1; i + 1 < count; i++) {
        if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
            !geo_push_index(base + (uint32_t)(i + 1)))
            return 0;
    }
    return 1;
}

static int geo_tex(float x, float y, float ang, float sc, float tw, float th_, uint32_t c) {
    if (!isfinite(ang))
        ang = 0.0f;
    if (!isfinite(x + y + sc) || sc <= 0 || tw <= 0 || th_ <= 0)
        return 1;
    float hw = tw * 0.5f * sc, hh = th_ * 0.5f * sc;
    float ca = ang == 0.0f ? 1.0f : cosf(ang), sa = ang == 0.0f ? 0.0f : sinf(ang);
    float cx = x + hw, cy = y + hh;
    static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
    static const float uu[4] = {0, 1, 1, 0}, vv[4] = {0, 0, 1, 1};
    GeoVert q[4];
    for (int i = 0; i < 4; i++) {
        float lx = sx[i] * hw, ly = sy[i] * hh;
        q[i].x = cx + ca * lx - sa * ly;
        q[i].y = cy + sa * lx + ca * ly;
        q[i].u = uu[i];
        q[i].v = vv[i];
        q[i].c = c;
    }
    return geo_quad(q);
}

static int geo_tex_ex(float x, float y, float ang_rad, float sx, float sy, float ox, float oy,
                      float tw, float th_, uint32_t c) {
    if (!isfinite(x + y + ang_rad + sx + sy + ox + oy) || tw <= 0 || th_ <= 0)
        return 1;
    float ca = ang_rad == 0.0f ? 1.0f : cosf(ang_rad);
    float sa = ang_rad == 0.0f ? 0.0f : sinf(ang_rad);
    float lx[4] = {-ox * sx, (tw - ox) * sx, (tw - ox) * sx, -ox * sx};
    float ly[4] = {-oy * sy, -oy * sy, (th_ - oy) * sy, (th_ - oy) * sy};
    static const float uu[4] = {0, 1, 1, 0}, vv[4] = {0, 0, 1, 1};
    GeoVert q[4];
    for (int i = 0; i < 4; i++) {
        q[i].x = x + ca * lx[i] - sa * ly[i];
        q[i].y = y + sa * lx[i] + ca * ly[i];
        q[i].u = uu[i];
        q[i].v = vv[i];
        q[i].c = c;
    }
    return geo_quad(q);
}

static int geo_tex_round(float x, float y, float sc, float tw, float th_, float rad, uint32_t c) {
    if (!isfinite(x + y + sc) || sc <= 0 || tw <= 0 || th_ <= 0)
        return 1;
    float w = tw * sc, h = th_ * sc;
    if (!isfinite(w + h + rad) || w <= 0 || h <= 0)
        return 1;
    if (rad < 0)
        rad = 0;
    if (rad > w * 0.5f)
        rad = w * 0.5f;
    if (rad > h * 0.5f)
        rad = h * 0.5f;
#define GEO_TEX_RUV_X(px) (((px)-x) / w)
#define GEO_TEX_RUV_Y(py) (((py)-y) / h)
    if (rad < 0.5f) {
        GeoVert q[4] = {
            {x, y, 0, 0, c},
            {x + w, y, 1, 0, c},
            {x + w, y + h, 1, 1, c},
            {x, y + h, 0, 1, c},
        };
        return geo_quad(q);
    }
    GeoVert q1[4] = {
        {x + rad, y, GEO_TEX_RUV_X(x + rad), GEO_TEX_RUV_Y(y), c},
        {x + w - rad, y, GEO_TEX_RUV_X(x + w - rad), GEO_TEX_RUV_Y(y), c},
        {x + w - rad, y + h, GEO_TEX_RUV_X(x + w - rad), GEO_TEX_RUV_Y(y + h), c},
        {x + rad, y + h, GEO_TEX_RUV_X(x + rad), GEO_TEX_RUV_Y(y + h), c},
    };
    if (!geo_quad(q1))
        return 0;
    GeoVert q2[4] = {
        {x, y + rad, GEO_TEX_RUV_X(x), GEO_TEX_RUV_Y(y + rad), c},
        {x + rad, y + rad, GEO_TEX_RUV_X(x + rad), GEO_TEX_RUV_Y(y + rad), c},
        {x + rad, y + h - rad, GEO_TEX_RUV_X(x + rad), GEO_TEX_RUV_Y(y + h - rad), c},
        {x, y + h - rad, GEO_TEX_RUV_X(x), GEO_TEX_RUV_Y(y + h - rad), c},
    };
    if (!geo_quad(q2))
        return 0;
    GeoVert q3[4] = {
        {x + w - rad, y + rad, GEO_TEX_RUV_X(x + w - rad), GEO_TEX_RUV_Y(y + rad), c},
        {x + w, y + rad, GEO_TEX_RUV_X(x + w), GEO_TEX_RUV_Y(y + rad), c},
        {x + w, y + h - rad, GEO_TEX_RUV_X(x + w), GEO_TEX_RUV_Y(y + h - rad), c},
        {x + w - rad, y + h - rad, GEO_TEX_RUV_X(x + w - rad), GEO_TEX_RUV_Y(y + h - rad), c},
    };
    if (!geo_quad(q3))
        return 0;
    int n = (int)ceilf(rad * 0.35f);
    if (n < 6)
        n = 6;
    if (n > 20)
        n = 20;
    static const float cxs[4] = {1, 0, 0, 1}, cys[4] = {1, 1, 0, 0};
    static const float a0s[4] = {0, (float)M_PI * 0.5f, (float)M_PI, (float)M_PI * 1.5f};
    for (int k = 0; k < 4; k++) {
        float cx = x + cxs[k] * (w - rad) + (cxs[k] ? 0 : rad);
        float cy = y + cys[k] * (h - rad) + (cys[k] ? 0 : rad);
        uint32_t base = (uint32_t)geo_vn;
        if (!geo_push_vert(cx, cy, GEO_TEX_RUV_X(cx), GEO_TEX_RUV_Y(cy), c))
            return 0;
        for (int i = 0; i <= n; i++) {
            float a = a0s[k] + (float)((double)i / n * M_PI * 0.5);
            float px = cx + cosf(a) * rad, py = cy + sinf(a) * rad;
            if (!geo_push_vert(px, py, GEO_TEX_RUV_X(px), GEO_TEX_RUV_Y(py), c))
                return 0;
        }
        for (int i = 1; i <= n; i++)
            if (!geo_push_index(base) || !geo_push_index(base + (uint32_t)i) ||
                !geo_push_index(base + (uint32_t)(i + 1)))
                return 0;
    }
#undef GEO_TEX_RUV_X
#undef GEO_TEX_RUV_Y
    return 1;
}
