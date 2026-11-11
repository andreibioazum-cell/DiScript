/* Software rasterizer matching Vulkan solid, image, and tint pipelines. */

static inline unsigned pc_div255(unsigned v) {
    unsigned t = v + 128u;
    return (t + (t >> 8)) >> 8;
}

static inline void pc_blend_pixel(uint32_t *dst, unsigned sr, unsigned sg, unsigned sb, unsigned sa) {
    unsigned dr, da, a256, ia, oa;
    if (sa == 0)
        return;
    if (sa >= 255) {
        *dst = (sr & 0xffu) | ((sg & 0xffu) << 8) | ((sb & 0xffu) << 16) | 0xff000000u;
        return;
    }
    dr = *dst;
    a256 = sa + (sa >> 7);
    ia = 256u - a256;
    sr = (sr * a256 + (dr & 0xffu) * ia) >> 8;
    sg = (sg * a256 + ((dr >> 8) & 0xffu) * ia) >> 8;
    sb = (sb * a256 + ((dr >> 16) & 0xffu) * ia) >> 8;
    da = (dr >> 24) & 0xffu;
    oa = sa + ((da * ia) >> 8);
    *dst = (sr & 0xffu) | ((sg & 0xffu) << 8) | ((sb & 0xffu) << 16) | ((oa & 0xffu) << 24);
}

static float pc_edge(float ax, float ay, float bx, float by, float px, float py) {
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

static void pc_tri(int pipeline, const uint32_t *tex, int tw, int th, GeoVert a, GeoVert b, GeoVert c) {
    double area, inv, w0, w1, dw0dx, dw0dy, dw1dx, dw1dy;
    double u0, u1, u2, v0, v1, v2;
    float minxf, maxxf, minyf, maxyf, cr0, cg0, cb0, ca0, cr1, cg1, cb1, ca1, cr2, cg2, cb2, ca2;
    float drdx, dgdx, dbdx, dadx;
    int minx, maxx, miny, maxy, x, y;
    GeoVert t;
    int solid, opaque_col;

    area = (double)pc_edge(a.x, a.y, b.x, b.y, c.x, c.y);
    if (area > -1e-6 && area < 1e-6)
        return;
    if (area < 0) { /* normalize winding so "inside" is w >= 0 */
        t = b;
        b = c;
        c = t;
        area = -area;
    }
    inv = 1.0 / area;

    minxf = a.x < b.x ? (a.x < c.x ? a.x : c.x) : (b.x < c.x ? b.x : c.x);
    maxxf = a.x > b.x ? (a.x > c.x ? a.x : c.x) : (b.x > c.x ? b.x : c.x);
    minyf = a.y < b.y ? (a.y < c.y ? a.y : c.y) : (b.y < c.y ? b.y : c.y);
    maxyf = a.y > b.y ? (a.y > c.y ? a.y : c.y) : (b.y > c.y ? b.y : c.y);
    minx = (int)floorf(minxf);
    maxx = (int)ceilf(maxxf);
    miny = (int)floorf(minyf);
    maxy = (int)ceilf(maxyf);
    if (minx < pc_clip_x0) minx = pc_clip_x0;
    if (miny < pc_clip_y0) miny = pc_clip_y0;
    if (maxx > pc_clip_x1) maxx = pc_clip_x1;
    if (maxy > pc_clip_y1) maxy = pc_clip_y1;
    if (minx > maxx || miny > maxy)
        return;

    /* Incremental barycentric step deltas (w2 = 1 - w0 - w1). */
    dw0dx = -(double)(c.y - b.y) * inv;
    dw0dy = (double)(c.x - b.x) * inv;
    dw1dx = -(double)(a.y - c.y) * inv;
    dw1dy = (double)(a.x - c.x) * inv;

    cr0 = (float)(a.c & 0xff);
    cg0 = (float)((a.c >> 8) & 0xff);
    cb0 = (float)((a.c >> 16) & 0xff);
    ca0 = (float)((a.c >> 24) & 0xff);
    cr1 = (float)(b.c & 0xff);
    cg1 = (float)((b.c >> 8) & 0xff);
    cb1 = (float)((b.c >> 16) & 0xff);
    ca1 = (float)((b.c >> 24) & 0xff);
    cr2 = (float)(c.c & 0xff);
    cg2 = (float)((c.c >> 8) & 0xff);
    cb2 = (float)((c.c >> 16) & 0xff);
    ca2 = (float)((c.c >> 24) & 0xff);
    drdx = (float)(dw0dx * (cr0 - cr2) + dw1dx * (cr1 - cr2));
    dgdx = (float)(dw0dx * (cg0 - cg2) + dw1dx * (cg1 - cg2));
    dbdx = (float)(dw0dx * (cb0 - cb2) + dw1dx * (cb1 - cb2));
    dadx = (float)(dw0dx * (ca0 - ca2) + dw1dx * (ca1 - ca2));

    solid = pipeline == 0 && a.c == b.c && a.c == c.c;
    opaque_col = solid && (a.c >> 24) == 0xff;

    u0 = u1 = u2 = v0 = v1 = v2 = 0.0;
    if (pipeline != 0) {
        u0 = a.u;
        u1 = b.u;
        u2 = c.u;
        v0 = a.v;
        v1 = b.v;
        v2 = c.v;
    }

    w0 = (double)pc_edge(b.x, b.y, c.x, c.y, (float)minx + 0.5f, (float)miny + 0.5f) * inv;
    w1 = (double)pc_edge(c.x, c.y, a.x, a.y, (float)minx + 0.5f, (float)miny + 0.5f) * inv;

    for (y = miny; y <= maxy; y++) {
        uint32_t *row = pc_fb + (size_t)y * (size_t)pc_fb_w;
        double rw0 = w0, rw1 = w1;
        float r = (float)(rw0 * cr0 + rw1 * cr1 + (1.0 - rw0 - rw1) * cr2);
        float g = (float)(rw0 * cg0 + rw1 * cg1 + (1.0 - rw0 - rw1) * cg2);
        float bb = (float)(rw0 * cb0 + rw1 * cb1 + (1.0 - rw0 - rw1) * cb2);
        float al = (float)(rw0 * ca0 + rw1 * ca1 + (1.0 - rw0 - rw1) * ca2);
        float uu = 0.0f, vv = 0.0f;
        if (pipeline != 0) {
            uu = (float)(rw0 * u0 + rw1 * u1 + (1.0 - rw0 - rw1) * u2);
            vv = (float)(rw0 * v0 + rw1 * v1 + (1.0 - rw0 - rw1) * v2);
        }
        for (x = minx; x <= maxx; x++) {
            unsigned sa, sr, sg, sb;
            if (rw0 < -1e-6 || rw1 < -1e-6 || (1.0 - rw0 - rw1) < -1e-6) {
                rw0 += dw0dx;
                rw1 += dw1dx;
                r += drdx;
                g += dgdx;
                bb += dbdx;
                al += dadx;
                if (pipeline != 0) {
                    uu += (float)(dw0dx * (u0 - u2) + dw1dx * (u1 - u2));
                    vv += (float)(dw0dx * (v0 - v2) + dw1dx * (v1 - v2));
                }
                continue;
            }
            if (solid) {
                if (opaque_col) {
                    /* Vertex colors are already packed r | g<<8 | b<<16 | a<<24. */
                    row[x] = a.c | 0xff000000u;
                } else {
                    pc_blend_pixel(&row[x], (unsigned)(a.c & 0xff), (unsigned)((a.c >> 8) & 0xff),
                                   (unsigned)((a.c >> 16) & 0xff), (unsigned)((a.c >> 24) & 0xff));
                }
            } else {
                sr = (unsigned)(r + 0.5f);
                sg = (unsigned)(g + 0.5f);
                sb = (unsigned)(bb + 0.5f);
                sa = (unsigned)(al + 0.5f);
                if (pipeline != 0) {
                    unsigned txi = (unsigned)(uu * (float)tw);
                    unsigned tyi = (unsigned)(vv * (float)th);
                    uint32_t tc, ta;
                    if (txi >= (unsigned)tw)
                        txi = (unsigned)tw - 1;
                    if (tyi >= (unsigned)th)
                        tyi = (unsigned)th - 1;
                    tc = tex[(size_t)tyi * (size_t)tw + txi];
                    ta = (tc >> 24) & 0xff;
                    if (ta == 0) {
                        /* Skip transparent texels. */
                    } else if (pipeline == 1) {
                        sr = pc_div255(sr * (tc & 0xffu));
                        sg = pc_div255(sg * ((tc >> 8) & 0xffu));
                        sb = pc_div255(sb * ((tc >> 16) & 0xffu));
                        sa = pc_div255(sa * ta);
                        pc_blend_pixel(&row[x], sr, sg, sb, sa);
                    } else {
                        sa = pc_div255(sa * ta);
                        pc_blend_pixel(&row[x], sr, sg, sb, sa);
                    }
                } else {
                    pc_blend_pixel(&row[x], sr, sg, sb, sa);
                }
            }
            rw0 += dw0dx;
            rw1 += dw1dx;
            r += drdx;
            g += dgdx;
            bb += dbdx;
            al += dadx;
            if (pipeline != 0) {
                uu += (float)(dw0dx * (u0 - u2) + dw1dx * (u1 - u2));
                vv += (float)(dw0dx * (v0 - v2) + dw1dx * (v1 - v2));
            }
        }
        w0 += dw0dy;
        w1 += dw1dy;
    }
}

/* Fast-path axis-aligned uniform-color quads (solid, alpha, textured, tinted). */
static int pc_quad_aa(int pipeline, const uint32_t *tex, int tw, int th, GeoVert v0, GeoVert v1, GeoVert v2,
                      GeoVert v3) {
    float x0, x1, y0, y1;
    int minx, maxx, miny, maxy, x, y;
    uint32_t col = v0.c;
    unsigned sr, sg, sb, sa;
    if (v0.c != v1.c || v0.c != v2.c || v0.c != v3.c)
        return 0;
    if (fabsf(v0.y - v1.y) > 1e-4f || fabsf(v2.y - v3.y) > 1e-4f || fabsf(v0.x - v3.x) > 1e-4f ||
        fabsf(v1.x - v2.x) > 1e-4f)
        return 0;
    x0 = v0.x;
    x1 = v1.x;
    y0 = v0.y;
    y1 = v2.y;
    if (!(x1 > x0 && y1 > y0))
        return 0;
    if (pipeline != 0 && (fabsf(v0.v - v1.v) > 1e-4f || fabsf(v2.v - v3.v) > 1e-4f || fabsf(v0.u - v3.u) > 1e-4f ||
                          fabsf(v1.u - v2.u) > 1e-4f))
        return 0;
    minx = (int)ceilf(x0 - 0.5001f);
    maxx = (int)floorf(x1 - 0.4999f);
    miny = (int)ceilf(y0 - 0.5001f);
    maxy = (int)floorf(y1 - 0.4999f);
    if (minx < pc_clip_x0) minx = pc_clip_x0;
    if (miny < pc_clip_y0) miny = pc_clip_y0;
    if (maxx > pc_clip_x1) maxx = pc_clip_x1;
    if (maxy > pc_clip_y1) maxy = pc_clip_y1;
    if (minx > maxx || miny > maxy)
        return 1;
    sa = (col >> 24) & 0xffu;
    if (sa == 0)
        return 1;
    sr = col & 0xffu;
    sg = (col >> 8) & 0xffu;
    sb = (col >> 16) & 0xffu;
    if (pipeline == 0) {
        if (sa == 255) {
            uint32_t out = col | 0xff000000u;
            for (y = miny; y <= maxy; y++) {
                uint32_t *row = pc_fb + (size_t)y * (size_t)pc_fb_w;
                for (x = minx; x <= maxx; x++)
                    row[x] = out;
            }
        } else {
            unsigned a256 = sa + (sa >> 7), ia = 256u - a256;
            unsigned psr = sr * a256, psg = sg * a256, psb = sb * a256;
            for (y = miny; y <= maxy; y++) {
                uint32_t *row = pc_fb + (size_t)y * (size_t)pc_fb_w;
                for (x = minx; x <= maxx; x++) {
                    uint32_t d = row[x];
                    unsigned r = (psr + (d & 0xffu) * ia) >> 8;
                    unsigned g = (psg + ((d >> 8) & 0xffu) * ia) >> 8;
                    unsigned b = (psb + ((d >> 16) & 0xffu) * ia) >> 8;
                    unsigned oa = sa + ((((d >> 24) & 0xffu) * ia) >> 8);
                    row[x] = r | (g << 8) | (b << 16) | (oa << 24);
                }
            }
        }
        return 1;
    }
    if (!tex || tw <= 0 || th <= 0)
        return 1;
    {
        float du = (v1.u - v0.u) / (x1 - x0);
        float dv = (v2.v - v0.v) / (y1 - y0);
        float u_start = v0.u + ((float)minx + 0.5f - x0) * du;
        float v_cur = v0.v + ((float)miny + 0.5f - y0) * dv;
        int tx_start = (int)(u_start * (float)tw * 65536.0f);
        int dtx = (int)(du * (float)tw * 65536.0f);
        int white = (col == 0xffffffffu);
        for (y = miny; y <= maxy; y++, v_cur += dv) {
            int tyi = (int)(v_cur * (float)th);
            const uint32_t *trow;
            uint32_t *row = pc_fb + (size_t)y * (size_t)pc_fb_w;
            int txf = tx_start;
            if (tyi < 0)
                tyi = 0;
            else if (tyi >= th)
                tyi = th - 1;
            trow = tex + (size_t)tyi * (size_t)tw;
            if (pipeline == 1 && white) {
                for (x = minx; x <= maxx; x++, txf += dtx) {
                    int txi = txf >> 16;
                    uint32_t tc, ta;
                    if (txi < 0)
                        txi = 0;
                    else if (txi >= tw)
                        txi = tw - 1;
                    tc = trow[txi];
                    ta = (tc >> 24) & 0xffu;
                    if (ta == 255)
                        row[x] = tc;
                    else if (ta != 0)
                        pc_blend_pixel(&row[x], tc & 0xffu, (tc >> 8) & 0xffu, (tc >> 16) & 0xffu, ta);
                }
            } else {
                for (x = minx; x <= maxx; x++, txf += dtx) {
                    int txi = txf >> 16;
                    uint32_t tc;
                    unsigned ta;
                    if (txi < 0)
                        txi = 0;
                    else if (txi >= tw)
                        txi = tw - 1;
                    tc = trow[txi];
                    ta = (tc >> 24) & 0xffu;
                    if (ta == 0)
                        continue;
                    if (pipeline == 1)
                        pc_blend_pixel(&row[x], pc_div255(sr * (tc & 0xffu)), pc_div255(sg * ((tc >> 8) & 0xffu)),
                                       pc_div255(sb * ((tc >> 16) & 0xffu)), pc_div255(sa * ta));
                    else
                        pc_blend_pixel(&row[x], sr, sg, sb, pc_div255(sa * ta));
                }
            }
        }
    }
    return 1;
}

static void pc_draw_range(int pipeline, const uint32_t *tex, int tw, int th, size_t first, size_t count) {
    size_t i = first, end = first + count;
    while (i + 2 < end) {
        if (i + 5 < end && geo_idx[i] == geo_idx[i + 3] && geo_idx[i + 2] == geo_idx[i + 4] &&
            pc_quad_aa(pipeline, tex, tw, th, geo_verts[geo_idx[i]], geo_verts[geo_idx[i + 1]],
                       geo_verts[geo_idx[i + 2]], geo_verts[geo_idx[i + 5]])) {
            i += 6;
            continue;
        }
        pc_tri(pipeline, tex, tw, th, geo_verts[geo_idx[i]], geo_verts[geo_idx[i + 1]], geo_verts[geo_idx[i + 2]]);
        i += 3;
    }
}

static void pc_render_frame(void) {
    Buffer *b = cur_buf;
    size_t i;
    if (!b || b->width < 1 || b->height < 1)
        return;
    if (!pc_fb || pc_fb_w != b->width || pc_fb_h != b->height)
        if (!ds_vk_begin_frame_backend((uint32_t)b->width, (uint32_t)b->height, (uint32_t)b->width,
                                       (uint32_t)b->height))
            return;
    b->pixels = pc_fb;
    b->stride = pc_fb_w;
    pc_set_clip(0, 0, 0, 0);

    geo_reset();
    for (i = 0; i < cmd_n; i++) {
        DSCmd *c = &cmds[i];
        int pipeline = 0;
        const uint32_t *tex = NULL;
        int tw = 0, th = 0;
        size_t is = geo_in;
        if (c->t == DS_CMD_SCISSOR) {
            pc_set_clip(c->v.rc.x, c->v.rc.y, c->v.rc.w, c->v.rc.h);
            continue;
        }
        switch (c->t) {
        case DS_CMD_RECT:
            geo_rect(c->v.rc.x, c->v.rc.y, c->v.rc.w, c->v.rc.h, c->v.rc.c);
            break;
        case DS_CMD_ROUND:
            geo_roundrect(c->v.rr.x, c->v.rr.y, c->v.rr.w, c->v.rr.h, c->v.rr.r, c->v.rr.c);
            break;
        case DS_CMD_RECT_ROT:
            geo_rect_rot(c->v.rot.x, c->v.rot.y, c->v.rot.w, c->v.rot.h, c->v.rot.ang, c->v.rot.c);
            break;
        case DS_CMD_CIRCLE:
            geo_circle(c->v.ci.x, c->v.ci.y, c->v.ci.r, c->v.ci.c);
            break;
        case DS_CMD_ELLIPSE:
            geo_ellipse(c->v.el.x, c->v.el.y, c->v.el.rx, c->v.el.ry, c->v.el.c);
            break;
        case DS_CMD_POLY:
            geo_poly(c->v.poly.xy, c->v.poly.count, c->v.poly.c);
            break;
        case DS_CMD_SECTOR:
            geo_sector(c->v.sec.x, c->v.sec.y, c->v.sec.r, c->v.sec.start_deg,
                       c->v.sec.sweep_deg, c->v.sec.c);
            break;
        case DS_CMD_RING:
            geo_ring(c->v.rg.x, c->v.rg.y, c->v.rg.r, c->v.rg.th, c->v.rg.c);
            break;
        case DS_CMD_LINE:
            geo_line(c->v.ln.x1, c->v.ln.y1, c->v.ln.x2, c->v.ln.y2, c->v.ln.th, c->v.ln.c);
            break;
        case DS_CMD_TEX: {
            Texture *t = c->v.tx.tx;
            if (t && t->gpu.uploaded && t->pixels) {
                pipeline = 1;
                tex = t->pixels;
                tw = t->w;
                th = t->h;
                geo_tex(c->v.tx.x, c->v.tx.y, c->v.tx.a, c->v.tx.sc, (float)t->w, (float)t->h, 0xffffffffu);
            }
            break;
        }
        case DS_CMD_TEX_TINT: {
            Texture *t = c->v.tx2.tx;
            if (t && t->gpu.uploaded && t->pixels) {
                pipeline = 2;
                tex = t->pixels;
                tw = t->w;
                th = t->h;
                geo_tex(c->v.tx2.x, c->v.tx2.y, c->v.tx2.a, c->v.tx2.sc, (float)t->w, (float)t->h, c->v.tx2.c);
            }
            break;
        }
        case DS_CMD_TEX_FADE: {
            Texture *t = c->v.tx2.tx;
            if (t && t->gpu.uploaded && t->pixels) {
                pipeline = 1;
                tex = t->pixels;
                tw = t->w;
                th = t->h;
                geo_tex(c->v.tx2.x, c->v.tx2.y, c->v.tx2.a, c->v.tx2.sc, (float)t->w, (float)t->h, c->v.tx2.c);
            }
            break;
        }
        case DS_CMD_TEX_ROUND: {
            Texture *t = c->v.txr.tx;
            if (t && t->gpu.uploaded && t->pixels) {
                pipeline = 1;
                tex = t->pixels;
                tw = t->w;
                th = t->h;
                geo_tex_round(c->v.txr.x, c->v.txr.y, c->v.txr.sc, (float)t->w, (float)t->h, c->v.txr.r,
                              0xffffffffu);
            }
            break;
        }
        case DS_CMD_TEX_EX: {
            Texture *t = c->v.txe.tx;
            if (t && t->gpu.uploaded && t->pixels) {
                pipeline = 1;
                tex = t->pixels;
                tw = t->w;
                th = t->h;
                geo_tex_ex(c->v.txe.x, c->v.txe.y, c->v.txe.ang_rad, c->v.txe.sx, c->v.txe.sy,
                           c->v.txe.ox, c->v.txe.oy, (float)t->w, (float)t->h, c->v.txe.c);
            }
            break;
        }
        case DS_CMD_TEXT:
            if (font && pc_font_tex) {
                pipeline = 1;
                tex = pc_font_tex;
                tw = pc_font_w;
                th = pc_font_h;
                geo_text(c->v.tt.s, c->v.tt.x, c->v.tt.y, c->v.tt.c, c->v.tt.sc, font);
            }
            break;
        case DS_CMD_TEXT_RAINBOW:
            if (font && pc_font_tex) {
                pipeline = 1;
                tex = pc_font_tex;
                tw = pc_font_w;
                th = pc_font_h;
                if (c->v.tr.c)
                    geo_text_shimmer(c->v.tr.s, c->v.tr.x, c->v.tr.y, c->v.tr.c, c->v.tr.sc, c->v.tr.hue, font);
                else
                    geo_text_rainbow(c->v.tr.s, c->v.tr.x, c->v.tr.y, c->v.tr.sc, c->v.tr.hue, font);
            }
            break;
        case DS_CMD_SCISSOR:
            break;
        }
        pc_draw_range(pipeline, tex, tw, th, is, geo_in - is);
    }
}
