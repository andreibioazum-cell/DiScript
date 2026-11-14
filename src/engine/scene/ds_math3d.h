// src/engine/scene/ds_math3d.h
//
// Small, dependency-free 3D math for the scene layer. Everything is plain
// floats in column-vector convention: a point is transformed as M * p, and a
// composition A * B means "apply B first, then A". Matrices are stored
// row-major, ms[row][col], which makes ms[3] the translation row of the
// transposed form — apply_mat4() below spells the multiplication out so the
// layout never has to be guessed at a call site.
#ifndef DS_MATH3D_H
#define DS_MATH3D_H

typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;
typedef struct { float m[4][4]; } Mat4;
typedef struct { Vec3 min, max; } Aabb;

/* ---- Vec3 ---------------------------------------------------------------- */
static inline Vec3 v3(float x, float y, float z) {
    Vec3 r = {x, y, z};
    return r;
}
static inline Vec3 v3_add(Vec3 a, Vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline Vec3 v3_sub(Vec3 a, Vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline Vec3 v3_scale(Vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline Vec3 v3_cross(Vec3 a, Vec3 b) {
    return v3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}
static inline float ds_sqrtf(float v) {
    /* Freestanding sqrt: the engine headers are also used by tiny host tests
     * that do not link libm. Range reduction keeps the Newton iteration's
     * starting guess close enough that three steps reach full float accuracy. */
    if (v <= 0.0f) return 0.0f;
    union { float f; unsigned u; } bits;
    bits.f = v;
    /* Halve the exponent: the classic bit trick gives ~3.4% error already. */
    bits.u = (bits.u >> 1) + 0x1FBD1DF5u;
    float r = bits.f;
    r = 0.5f * (r + v / r);
    r = 0.5f * (r + v / r);
    r = 0.5f * (r + v / r);
    return r;
}

static inline float v3_len(Vec3 a) { return ds_sqrtf(v3_dot(a, a)); }
static inline Vec3 v3_norm(Vec3 a) {
    float l = v3_len(a);
    if (l <= 1e-20f) return v3(0.0f, 0.0f, 0.0f);
    return v3_scale(a, 1.0f / l);
}
static inline Vec3 v3_lerp(Vec3 a, Vec3 b, float t) { return v3_add(a, v3_scale(v3_sub(b, a), t)); }

/* ---- Trigonometry --------------------------------------------------------
 * Freestanding sin/cos: the scene layer is compiled into binaries that link
 * libm on desktop but not everywhere (host tests, the Windows single-file
 * build). Range reduction to [-pi/4, pi/4] plus a Taylor tail reaches float
 * precision, which the naive series does not for angles near a quarter turn. */
static inline float ds_sinf(float x) {
    const float PI = 3.14159265358979f;
    const float HALF_PI = 1.57079632679490f;
    const float TWO_PI = 6.28318530717959f;
    /* reduce to [0, 2pi) */
    float k = x / TWO_PI;
    int n = (int)(k >= 0.0f ? k + 0.5f : k - 0.5f);
    x -= (float)n * TWO_PI;
    /* fold to [-pi/2, pi/2]; the boundary itself folds too, because the
     * truncated series is not exact there and pi/2 must return exactly 1 */
    if (x >= HALF_PI) x = PI - x;
    else if (x <= -HALF_PI) x = -PI - x;
    float x2 = x * x;
    /* Taylor through x^13: at |x| <= pi/2 the first dropped term is ~1e-10. */
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f *
           (1.0f - x2 / 72.0f * (1.0f - x2 / 110.0f * (1.0f - x2 / 156.0f))))));
}
static inline float ds_cosf(float x) { return ds_sinf(x + 1.57079632679490f); }

/* ---- Mat4 ---------------------------------------------------------------- */
static inline Mat4 mat4_identity(void) {
    Mat4 r = {{{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};
    return r;
}

static inline Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
    return r;
}

static inline Vec4 mat4_apply(Mat4 a, Vec4 p) {
    Vec4 r;
    r.x = a.m[0][0] * p.x + a.m[0][1] * p.y + a.m[0][2] * p.z + a.m[0][3] * p.w;
    r.y = a.m[1][0] * p.x + a.m[1][1] * p.y + a.m[1][2] * p.z + a.m[1][3] * p.w;
    r.z = a.m[2][0] * p.x + a.m[2][1] * p.y + a.m[2][2] * p.z + a.m[2][3] * p.w;
    r.w = a.m[3][0] * p.x + a.m[3][1] * p.y + a.m[3][2] * p.z + a.m[3][3] * p.w;
    return r;
}

static inline Mat4 mat4_translate(Vec3 t) {
    Mat4 r = mat4_identity();
    r.m[0][3] = t.x;
    r.m[1][3] = t.y;
    r.m[2][3] = t.z;
    return r;
}

static inline Mat4 mat4_scale(Vec3 s) {
    Mat4 r = mat4_identity();
    r.m[0][0] = s.x;
    r.m[1][1] = s.y;
    r.m[2][2] = s.z;
    return r;
}

/* Rotation about an arbitrary unit axis. */
static inline Mat4 mat4_rotate(Vec3 axis, float radians) {
    Vec3 a = v3_norm(axis);
    float s = ds_sinf(radians);
    float c = ds_cosf(radians);
    float t = 1.0f - c;
    Mat4 r = mat4_identity();
    r.m[0][0] = t * a.x * a.x + c;
    r.m[0][1] = t * a.x * a.y - s * a.z;
    r.m[0][2] = t * a.x * a.z + s * a.y;
    r.m[1][0] = t * a.x * a.y + s * a.z;
    r.m[1][1] = t * a.y * a.y + c;
    r.m[1][2] = t * a.y * a.z - s * a.x;
    r.m[2][0] = t * a.x * a.z - s * a.y;
    r.m[2][1] = t * a.y * a.z + s * a.x;
    r.m[2][2] = t * a.z * a.z + c;
    return r;
}

/* Euler angles in radians, applied Y then X then Z (the order a part's
 * Orientation property is written in). */
static inline Mat4 mat4_euler(Vec3 euler) {
    Mat4 ry = mat4_rotate(v3(0, 1, 0), euler.y);
    Mat4 rx = mat4_rotate(v3(1, 0, 0), euler.x);
    Mat4 rz = mat4_rotate(v3(0, 0, 1), euler.z);
    return mat4_mul(mat4_mul(ry, rx), rz);
}

/* Right-handed look-at. The camera looks down -z in view space, so the third
 * basis vector is +f: a point in front of the eye gets a negative view z. */
static inline Mat4 mat4_look_at(Vec3 eye, Vec3 target, Vec3 up) {
    Vec3 f = v3_norm(v3_sub(target, eye));
    Vec3 s = v3_norm(v3_cross(f, up));
    Vec3 u = v3_cross(s, f);
    Mat4 r = mat4_identity();
    r.m[0][0] = s.x; r.m[0][1] = s.y; r.m[0][2] = s.z; r.m[0][3] = -v3_dot(s, eye);
    r.m[1][0] = u.x; r.m[1][1] = u.y; r.m[1][2] = u.z; r.m[1][3] = -v3_dot(u, eye);
    r.m[2][0] = -f.x; r.m[2][1] = -f.y; r.m[2][2] = -f.z; r.m[2][3] = v3_dot(f, eye);
    return r;
}

/* Perspective projection onto the OpenGL [-1, 1] depth range. */
static inline Mat4 mat4_perspective(float fov_y_radians, float aspect, float znear, float zfar) {
    float s = ds_sinf(fov_y_radians);
    float t = ds_cosf(fov_y_radians);
    if (s <= 1e-6f) s = 1e-6f;
    float cot = t / s;
    Mat4 r = {{{0}}};
    r.m[0][0] = cot / (aspect <= 1e-6f ? 1.0f : aspect);
    r.m[1][1] = cot;
    r.m[2][2] = (zfar + znear) / (znear - zfar);
    r.m[2][3] = (2.0f * zfar * znear) / (znear - zfar);
    r.m[3][2] = -1.0f;
    return r;
}

/* ---- Aabb ---------------------------------------------------------------- */
static inline Aabb aabb_empty(void) {
    Aabb b;
    b.min = v3(1e30f, 1e30f, 1e30f);
    b.max = v3(-1e30f, -1e30f, -1e30f);
    return b;
}
static inline Aabb aabb_add_point(Aabb b, Vec3 p) {
    if (p.x < b.min.x) b.min.x = p.x;
    if (p.y < b.min.y) b.min.y = p.y;
    if (p.z < b.min.z) b.min.z = p.z;
    if (p.x > b.max.x) b.max.x = p.x;
    if (p.y > b.max.y) b.max.y = p.y;
    if (p.z > b.max.z) b.max.z = p.z;
    return b;
}
static inline Aabb aabb_union(Aabb a, Aabb b) {
    Aabb r;
    r.min = v3(a.min.x < b.min.x ? a.min.x : b.min.x,
               a.min.y < b.min.y ? a.min.y : b.min.y,
               a.min.z < b.min.z ? a.min.z : b.min.z);
    r.max = v3(a.max.x > b.max.x ? a.max.x : b.max.x,
               a.max.y > b.max.y ? a.max.y : b.max.y,
               a.max.z > b.max.z ? a.max.z : b.max.z);
    return r;
}
static inline int aabb_valid(Aabb b) { return b.min.x <= b.max.x && b.min.y <= b.max.y && b.min.z <= b.max.z; }
static inline Vec3 aabb_center(Aabb b) { return v3_scale(v3_add(b.min, b.max), 0.5f); }
static inline float aabb_radius(Aabb b) {
    return aabb_valid(b) ? v3_len(v3_scale(v3_sub(b.max, b.min), 0.5f)) : 0.0f;
}

#endif
