// Simplex 2D/3D Noise (for love.math.noise) and 2D Arcade/Rigid-Body Physics Engine.

#include "cpp/ds_cpp.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t kPerm[256] = {
    151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,
    140,36,103,30,69,142,8,99,37,240,21,10,23,190,6,148,
    247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,
    57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
    74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,
    60,211,133,230,220,105,92,41,55,46,245,40,244,102,143,54,
    65,25,63,161,1,216,80,73,209,76,132,187,208,89,18,169,
    200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,64,
    52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,
    207,206,59,227,47,16,58,17,182,189,28,42,223,183,170,213,
    119,248,152,2,44,154,163,70,221,153,101,155,167,43,172,9,
    129,22,39,253,19,98,108,110,79,113,224,232,178,185,112,104,
    218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,
    81,51,145,235,249,14,239,107,49,192,214,31,181,199,106,157,
    184,84,204,176,115,121,50,45,127,4,150,254,138,236,205,93,
    222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

inline uint8_t perm_at(int i) { return kPerm[i & 255]; }

inline int fast_floor(double x) {
    int xi = static_cast<int>(x);
    return (x < xi) ? (xi - 1) : xi;
}

double grad2(int hash, double x, double y) {
    switch (hash & 7) {
        case 0: return  x + y;
        case 1: return -x + y;
        case 2: return  x - y;
        case 3: return -x - y;
        case 4: return  x * 1.41421356;
        case 5: return -x * 1.41421356;
        case 6: return  y * 1.41421356;
        default: return -y * 1.41421356;
    }
}

double grad3(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = h < 8 ? x : y;
    double v = h < 4 ? y : (h == 12 || h == 14 ? x : z);
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

enum ShapeKind : int {
    SHAPE_NONE = 0,
    SHAPE_CIRCLE = 1,
    SHAPE_BOX = 2
};

struct PhysBody {
    bool alive;
    int shape;
    float x, y;     // center position
    float vx, vy;
    float w, h;     // for circle: w = radius; for box: w, h = full width/height
    float inv_mass; // 0 for static
    float restitution;
    float friction;
};

}  // namespace

struct DSPhysWorld {
    float gx, gy;
    PhysBody *bodies;
    int count;
    int cap;
};

extern "C" double ds_simplex2d(double xin, double yin) {
    constexpr double F2 = 0.3660254037844386;  // 0.5*(sqrt(3)-1)
    constexpr double G2 = 0.21132486540518713; // (3-sqrt(3))/6

    double s = (xin + yin) * F2;
    int i = fast_floor(xin + s);
    int j = fast_floor(yin + s);
    double t = (i + j) * G2;
    double x0 = xin - (i - t);
    double y0 = yin - (j - t);

    int i1 = (x0 > y0) ? 1 : 0;
    int j1 = (x0 > y0) ? 0 : 1;

    double x1 = x0 - i1 + G2;
    double y1 = y0 - j1 + G2;
    double x2 = x0 - 1.0 + 2.0 * G2;
    double y2 = y0 - 1.0 + 2.0 * G2;

    int gi0 = perm_at(i + perm_at(j));
    int gi1 = perm_at(i + i1 + perm_at(j + j1));
    int gi2 = perm_at(i + 1 + perm_at(j + 1));

    double n0 = 0.0, n1 = 0.0, n2 = 0.0;
    double t0 = 0.5 - x0 * x0 - y0 * y0;
    if (t0 > 0.0) { t0 *= t0; n0 = t0 * t0 * grad2(gi0, x0, y0); }
    double t1 = 0.5 - x1 * x1 - y1 * y1;
    if (t1 > 0.0) { t1 *= t1; n1 = t1 * t1 * grad2(gi1, x1, y1); }
    double t2 = 0.5 - x2 * x2 - y2 * y2;
    if (t2 > 0.0) { t2 *= t2; n2 = t2 * t2 * grad2(gi2, x2, y2); }

    // Normalize into [0, 1] matching love.math.noise convention!
    double raw = 45.23 * (n0 + n1 + n2);
    double norm = 0.5 + 0.5 * raw;
    if (norm < 0.0) norm = 0.0;
    if (norm > 1.0) norm = 1.0;
    return norm;
}

extern "C" double ds_simplex3d(double xin, double yin, double zin) {
    constexpr double F3 = 1.0 / 3.0;
    constexpr double G3 = 1.0 / 6.0;
    double s = (xin + yin + zin) * F3;
    int i = fast_floor(xin + s);
    int j = fast_floor(yin + s);
    int k = fast_floor(zin + s);
    double t = (i + j + k) * G3;
    double x0 = xin - (i - t), y0 = yin - (j - t), z0 = zin - (k - t);

    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0)      { i1=1; j1=0; k1=0; i2=1; j2=1; k2=0; }
        else if (x0 >= z0) { i1=1; j1=0; k1=0; i2=1; j2=0; k2=1; }
        else               { i1=0; j1=0; k1=1; i2=1; j2=0; k2=1; }
    } else {
        if (y0 < z0)       { i1=0; j1=0; k1=1; i2=0; j2=1; k2=1; }
        else if (x0 < z0)  { i1=0; j1=1; k1=0; i2=0; j2=1; k2=1; }
        else               { i1=0; j1=1; k1=0; i2=1; j2=1; k2=0; }
    }

    double x1 = x0 - i1 + G3, y1 = y0 - j1 + G3, z1 = z0 - k1 + G3;
    double x2 = x0 - i2 + 2.0 * G3, y2 = y0 - j2 + 2.0 * G3, z2 = z0 - k2 + 2.0 * G3;
    double x3 = x0 - 1.0 + 3.0 * G3, y3 = y0 - 1.0 + 3.0 * G3, z3 = z0 - 1.0 + 3.0 * G3;

    int gi0 = perm_at(i + perm_at(j + perm_at(k)));
    int gi1 = perm_at(i + i1 + perm_at(j + j1 + perm_at(k + k1)));
    int gi2 = perm_at(i + i2 + perm_at(j + j2 + perm_at(k + k2)));
    int gi3 = perm_at(i + 1 + perm_at(j + 1 + perm_at(k + 1)));

    auto contrib = [](int gi, double x, double y, double z) -> double {
        double tt = 0.6 - x * x - y * y - z * z;
        if (tt <= 0.0) return 0.0;
        tt *= tt;
        return tt * tt * grad3(gi, x, y, z);
    };

    double raw = 32.0 * (contrib(gi0, x0, y0, z0) + contrib(gi1, x1, y1, z1) +
                         contrib(gi2, x2, y2, z2) + contrib(gi3, x3, y3, z3));
    double norm = 0.5 + 0.5 * raw;
    return norm < 0.0 ? 0.0 : (norm > 1.0 ? 1.0 : norm);
}

extern "C" double ds_fbm2d(double x, double y, int octaves) {
    if (octaves < 1) octaves = 1;
    if (octaves > 8) octaves = 8;
    double sum = 0.0, amp = 0.5, freq = 1.0, norm = 0.0;
    for (int i = 0; i < octaves; ++i) {
        sum += amp * ds_simplex2d(x * freq, y * freq);
        norm += amp;
        amp *= 0.5;
        freq *= 2.0;
    }
    return norm > 0.0 ? sum / norm : 0.0;
}

extern "C" DSPhysWorld *ds_phys_world_new(float gx, float gy) {
    auto *w = static_cast<DSPhysWorld *>(calloc(1, sizeof(DSPhysWorld)));
    if (!w) return nullptr;
    w->gx = gx;
    w->gy = gy;
    w->cap = 64;
    w->bodies = static_cast<PhysBody *>(calloc(static_cast<size_t>(w->cap), sizeof(PhysBody)));
    if (!w->bodies) {
        free(w);
        return nullptr;
    }
    return w;
}

extern "C" void ds_phys_world_free(DSPhysWorld *w) {
    if (!w) return;
    free(w->bodies);
    free(w);
}

extern "C" void ds_phys_set_gravity(DSPhysWorld *w, float gx, float gy) {
    if (!w) return;
    w->gx = gx;
    w->gy = gy;
}

static int alloc_body_slot(DSPhysWorld *w) {
    for (int i = 0; i < w->count; ++i) {
        if (!w->bodies[i].alive) return i;
    }
    if (w->count == w->cap) {
        int nc = w->cap * 2;
        auto *nb = static_cast<PhysBody *>(realloc(w->bodies, static_cast<size_t>(nc) * sizeof(PhysBody)));
        if (!nb) return -1;
        memset(nb + w->cap, 0, static_cast<size_t>(nc - w->cap) * sizeof(PhysBody));
        w->bodies = nb;
        w->cap = nc;
    }
    return w->count++;
}

extern "C" int ds_phys_add_circle(DSPhysWorld *w, float x, float y, float radius, float mass, float restitution) {
    if (!w || radius <= 0.5f) return -1;
    int id = alloc_body_slot(w);
    if (id < 0) return -1;
    PhysBody &b = w->bodies[id];
    b.alive = true;
    b.shape = SHAPE_CIRCLE;
    b.x = x;
    b.y = y;
    b.vx = 0.0f;
    b.vy = 0.0f;
    b.w = radius;
    b.h = radius;
    b.inv_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
    b.restitution = restitution < 0.0f ? 0.0f : (restitution > 1.2f ? 1.2f : restitution);
    b.friction = 0.18f;
    return id;
}

extern "C" int ds_phys_add_box(DSPhysWorld *w, float x, float y, float width, float height, float mass, float restitution) {
    if (!w || width <= 1.0f || height <= 1.0f) return -1;
    int id = alloc_body_slot(w);
    if (id < 0) return -1;
    PhysBody &b = w->bodies[id];
    b.alive = true;
    b.shape = SHAPE_BOX;
    b.x = x;
    b.y = y;
    b.vx = 0.0f;
    b.vy = 0.0f;
    b.w = width;
    b.h = height;
    b.inv_mass = (mass > 0.0f) ? (1.0f / mass) : 0.0f;
    b.restitution = restitution < 0.0f ? 0.0f : (restitution > 1.2f ? 1.2f : restitution);
    b.friction = 0.24f;
    return id;
}

extern "C" int ds_phys_body_count(const DSPhysWorld *w) {
    return w ? w->count : 0;
}

extern "C" int ds_phys_get_body(const DSPhysWorld *w, int id, float *x, float *y, float *vx, float *vy,
                                float *w_or_r, float *h, int *shape) {
    if (!w || id < 0 || id >= w->count || !w->bodies[id].alive) return 0;
    const PhysBody &b = w->bodies[id];
    if (x) *x = b.x;
    if (y) *y = b.y;
    if (vx) *vx = b.vx;
    if (vy) *vy = b.vy;
    if (w_or_r) *w_or_r = b.w;
    if (h) *h = b.h;
    if (shape) *shape = b.shape;
    return 1;
}

extern "C" void ds_phys_set_pos(DSPhysWorld *w, int id, float x, float y) {
    if (!w || id < 0 || id >= w->count || !w->bodies[id].alive) return;
    w->bodies[id].x = x;
    w->bodies[id].y = y;
}

extern "C" void ds_phys_set_vel(DSPhysWorld *w, int id, float vx, float vy) {
    if (!w || id < 0 || id >= w->count || !w->bodies[id].alive) return;
    w->bodies[id].vx = vx;
    w->bodies[id].vy = vy;
}

extern "C" void ds_phys_apply_impulse(DSPhysWorld *w, int id, float ix, float iy) {
    if (!w || id < 0 || id >= w->count || !w->bodies[id].alive) return;
    PhysBody &b = w->bodies[id];
    if (b.inv_mass > 0.0f) {
        b.vx += ix * b.inv_mass;
        b.vy += iy * b.inv_mass;
    }
}

extern "C" void ds_phys_remove_body(DSPhysWorld *w, int id) {
    if (!w || id < 0 || id >= w->count) return;
    w->bodies[id].alive = false;
}

extern "C" void ds_phys_clear(DSPhysWorld *w) {
    if (!w) return;
    w->count = 0;
}

static bool resolve_pair(PhysBody &a, PhysBody &b) {
    if (a.inv_mass == 0.0f && b.inv_mass == 0.0f) return false;
    float nx = 0.0f, ny = 0.0f, pen = 0.0f;

    if (a.shape == SHAPE_CIRCLE && b.shape == SHAPE_CIRCLE) {
        float dx = b.x - a.x, dy = b.y - a.y;
        float rsum = a.w + b.w;
        float d2 = dx * dx + dy * dy;
        if (d2 >= rsum * rsum) return false;
        float d = sqrtf(d2);
        if (d > 1e-4f) { nx = dx / d; ny = dy / d; pen = rsum - d; }
        else { nx = 0.0f; ny = -1.0f; pen = rsum; }
    } else if (a.shape == SHAPE_BOX && b.shape == SHAPE_BOX) {
        float dx = b.x - a.x, dy = b.y - a.y;
        float ox = (a.w + b.w) * 0.5f - fabsf(dx);
        if (ox <= 0.0f) return false;
        float oy = (a.h + b.h) * 0.5f - fabsf(dy);
        if (oy <= 0.0f) return false;
        if (ox < oy) { nx = dx < 0.0f ? -1.0f : 1.0f; ny = 0.0f; pen = ox; }
        else         { nx = 0.0f; ny = dy < 0.0f ? -1.0f : 1.0f; pen = oy; }
    } else {
        bool a_circ = (a.shape == SHAPE_CIRCLE);
        const PhysBody &ci = a_circ ? a : b;
        const PhysBody &bx = a_circ ? b : a;
        float hw = bx.w * 0.5f, hh = bx.h * 0.5f;
        float rx = ci.x - bx.x, ry = ci.y - bx.y;
        float cx = rx < -hw ? -hw : (rx > hw ? hw : rx);
        float cy = ry < -hh ? -hh : (ry > hh ? hh : ry);
        bool inside = (rx == cx && ry == cy);
        if (inside) {
            float dx_edge = hw - fabsf(rx);
            float dy_edge = hh - fabsf(ry);
            if (dx_edge < dy_edge) { cx = rx < 0 ? -hw : hw; }
            else                   { cy = ry < 0 ? -hh : hh; }
        }
        float dx = rx - cx, dy = ry - cy;
        float d2 = dx * dx + dy * dy;
        if (!inside && d2 >= ci.w * ci.w) return false;
        float d = sqrtf(d2);
        if (inside) {
            if (d > 1e-4f) { nx = -dx / d; ny = -dy / d; pen = ci.w + d; }
            else           { nx = 0.0f; ny = -1.0f; pen = ci.w; }
        } else {
            if (d > 1e-4f) { nx = dx / d; ny = dy / d; pen = ci.w - d; }
            else           { nx = 0.0f; ny = -1.0f; pen = ci.w; }
        }
        // Normal currently points from bx -> ci; flip if a is circle (we need a -> b)
        if (a_circ) { nx = -nx; ny = -ny; }
    }

    float inv_sum = a.inv_mass + b.inv_mass;
    if (inv_sum <= 0.0f) return false;

    // Positional correction (Baumgarte slop)
    const float slop = 0.05f;
    const float pct = 0.82f;
    float corr = (pen > slop ? (pen - slop) : 0.0f) / inv_sum * pct;
    a.x -= nx * corr * a.inv_mass;
    a.y -= ny * corr * a.inv_mass;
    b.x += nx * corr * b.inv_mass;
    b.y += ny * corr * b.inv_mass;

    float rvx = b.vx - a.vx;
    float rvy = b.vy - a.vy;
    float vn = rvx * nx + rvy * ny;
    if (vn > 0.0f) return true;

    float e = a.restitution < b.restitution ? a.restitution : b.restitution;
    float j = -(1.0f + e) * vn / inv_sum;
    float ix = j * nx, iy = j * ny;
    a.vx -= ix * a.inv_mass;
    a.vy -= iy * a.inv_mass;
    b.vx += ix * b.inv_mass;
    b.vy += iy * b.inv_mass;

    // Tangent Coulomb friction
    rvx = b.vx - a.vx;
    rvy = b.vy - a.vy;
    float tx = rvx - (rvx * nx + rvy * ny) * nx;
    float ty = rvy - (rvx * nx + rvy * ny) * ny;
    float tlen = sqrtf(tx * tx + ty * ty);
    if (tlen > 1e-4f) {
        tx /= tlen;
        ty /= tlen;
        float jt = -(rvx * tx + rvy * ty) / inv_sum;
        float mu = sqrtf(a.friction * b.friction);
        if (fabsf(jt) > j * mu)
            jt = (jt < 0.0f ? -1.0f : 1.0f) * j * mu;
        a.vx -= jt * tx * a.inv_mass;
        a.vy -= jt * ty * a.inv_mass;
        b.vx += jt * tx * b.inv_mass;
        b.vy += jt * ty * b.inv_mass;
    }
    return true;
}

extern "C" int ds_phys_step(DSPhysWorld *w, float dt_sec, int substeps) {
    if (!w || dt_sec <= 0.0f) return 0;
    if (dt_sec > 0.05f) dt_sec = 0.05f;
    if (substeps < 1) substeps = 1;
    if (substeps > 8) substeps = 8;
    float h = dt_sec / static_cast<float>(substeps);
    int contacts = 0;

    for (int step = 0; step < substeps; ++step) {
        for (int i = 0; i < w->count; ++i) {
            PhysBody &b = w->bodies[i];
            if (!b.alive || b.inv_mass == 0.0f) continue;
            b.vx += w->gx * h;
            b.vy += w->gy * h;
            b.vx *= (1.0f - 0.08f * h);
            b.vy *= (1.0f - 0.08f * h);
            b.x += b.vx * h;
            b.y += b.vy * h;
        }
        for (int i = 0; i < w->count; ++i) {
            if (!w->bodies[i].alive) continue;
            for (int j = i + 1; j < w->count; ++j) {
                if (!w->bodies[j].alive) continue;
                if (resolve_pair(w->bodies[i], w->bodies[j]))
                    contacts++;
            }
        }
    }
    return contacts;
}

extern "C" int ds_phys_raycast(const DSPhysWorld *w, float x0, float y0, float dx, float dy, float max_dist,
                               float *hit_t, int *hit_id) {
    if (!w || max_dist <= 0.0f) return 0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len <= 1e-6f) return 0;
    dx /= len;
    dy /= len;
    float best = max_dist + 1.0f;
    int best_id = -1;

    for (int i = 0; i < w->count; ++i) {
        const PhysBody &b = w->bodies[i];
        if (!b.alive) continue;
        if (b.shape == SHAPE_CIRCLE) {
            float ocx = x0 - b.x, ocy = y0 - b.y;
            float hb = ocx * dx + ocy * dy;
            float c = ocx * ocx + ocy * ocy - b.w * b.w;
            float disc = hb * hb - c;
            if (disc >= 0.0f) {
                float t = -hb - sqrtf(disc);
                if (t >= 0.0f && t <= max_dist && t < best) {
                    best = t;
                    best_id = i;
                }
            }
        } else if (b.shape == SHAPE_BOX) {
            float minx = b.x - b.w * 0.5f, maxx = b.x + b.w * 0.5f;
            float miny = b.y - b.h * 0.5f, maxy = b.y + b.h * 0.5f;
            float tmin = 0.0f, tmax = max_dist;
            if (fabsf(dx) < 1e-6f) {
                if (x0 < minx || x0 > maxx) continue;
            } else {
                float t1 = (minx - x0) / dx, t2 = (maxx - x0) / dx;
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) continue;
            }
            if (fabsf(dy) < 1e-6f) {
                if (y0 < miny || y0 > maxy) continue;
            } else {
                float t1 = (miny - y0) / dy, t2 = (maxy - y0) / dy;
                if (t1 > t2) { float tmp = t1; t1 = t2; t2 = tmp; }
                if (t1 > tmin) tmin = t1;
                if (t2 < tmax) tmax = t2;
                if (tmin > tmax) continue;
            }
            if (tmin >= 0.0f && tmin < best) {
                best = tmin;
                best_id = i;
            }
        }
    }
    if (best_id < 0) return 0;
    if (hit_t) *hit_t = best;
    if (hit_id) *hit_id = best_id;
    return 1;
}
