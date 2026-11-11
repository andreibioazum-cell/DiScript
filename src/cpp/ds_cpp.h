#ifndef DISCRIPT_CPP_H
#define DISCRIPT_CPP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- UI Visual Effects & Color Math (ui_fx.cpp) ------------------------- */
void cb_button_gradient(float x, float y, float w, float h, float rad, uint32_t color);
void ds_panel_gradient(float x, float y, float w, float h, float rad, uint32_t top_col, uint32_t bot_col);
uint32_t ds_hsv_to_argb(float h, float s, float v, float a);
uint32_t ds_lerp_argb(uint32_t c0, uint32_t c1, float t);
float ds_smooth_damp(float current, float target, float *velocity, float smooth_time, float dt_sec);

/* ---- Native ARM64 / x86_64 Numeric JIT (native_jit.cpp) ----------------- */
typedef enum {
    DS_JOP_LOAD_ARG = 1,   /* dst_reg = args[imm] */
    DS_JOP_LOAD_CONST = 2, /* dst_reg = const_val */
    DS_JOP_MOV = 3,        /* dst_reg = src_a */
    DS_JOP_ADD = 4,        /* dst_reg = src_a + src_b */
    DS_JOP_SUB = 5,        /* dst_reg = src_a - src_b */
    DS_JOP_MUL = 6,        /* dst_reg = src_a * src_b */
    DS_JOP_DIV = 7,        /* dst_reg = src_a / src_b */
    DS_JOP_NEG = 8,        /* dst_reg = -src_a */
    DS_JOP_CMP_LT = 9,     /* dst_reg = (src_a < src_b) ? 1.0 : 0.0 */
    DS_JOP_CMP_LE = 10,    /* dst_reg = (src_a <= src_b) ? 1.0 : 0.0 */
    DS_JOP_CMP_GT = 11,    /* dst_reg = (src_a > src_b) ? 1.0 : 0.0 */
    DS_JOP_CMP_GE = 12,    /* dst_reg = (src_a >= src_b) ? 1.0 : 0.0 */
    DS_JOP_SELECT = 13,    /* dst_reg = (cond_reg != 0.0) ? src_a : src_b */
    DS_JOP_RET = 14        /* return src_a */
} DSJitOpKind;

typedef struct {
    uint8_t op;
    uint8_t dst;
    uint8_t a;
    uint8_t b;
    int32_t imm;
    double k;
} DSJitInst;

typedef struct DSJitFunc DSJitFunc;
typedef double (*DSJitEntryFn)(const double *slots);

int ds_jit_supported(void);
DSJitFunc *ds_jit_compile(const DSJitInst *insts, int count, int num_regs);
DSJitEntryFn ds_jit_entry(const DSJitFunc *fn);
size_t ds_jit_code_size(const DSJitFunc *fn);
void ds_jit_free(DSJitFunc *fn);

/* ---- Simplex Noise & 2D Physics World (love_noise_physics.cpp) ---------- */
double ds_simplex2d(double x, double y);
double ds_simplex3d(double x, double y, double z);
double ds_fbm2d(double x, double y, int octaves);

typedef struct DSPhysWorld DSPhysWorld;

DSPhysWorld *ds_phys_world_new(float gx, float gy);
void ds_phys_world_free(DSPhysWorld *w);
void ds_phys_set_gravity(DSPhysWorld *w, float gx, float gy);
int ds_phys_add_circle(DSPhysWorld *w, float x, float y, float radius, float mass, float restitution);
int ds_phys_add_box(DSPhysWorld *w, float x, float y, float width, float height, float mass, float restitution);
int ds_phys_body_count(const DSPhysWorld *w);
int ds_phys_get_body(const DSPhysWorld *w, int id, float *x, float *y, float *vx, float *vy, float *w_or_r, float *h, int *shape);
void ds_phys_set_pos(DSPhysWorld *w, int id, float x, float y);
void ds_phys_set_vel(DSPhysWorld *w, int id, float vx, float vy);
void ds_phys_apply_impulse(DSPhysWorld *w, int id, float ix, float iy);
void ds_phys_remove_body(DSPhysWorld *w, int id);
void ds_phys_clear(DSPhysWorld *w);
int ds_phys_step(DSPhysWorld *w, float dt_sec, int substeps);
int ds_phys_raycast(const DSPhysWorld *w, float x0, float y0, float dx, float dy, float max_dist, float *hit_t, int *hit_id);

/* ---- Context-Aware IDE Autocomplete (editor_assist.cpp) ----------------- */
typedef struct {
    char label[48];
    char insert_suffix[48];
    uint8_t kind; /* 0=keyword, 1=api/func, 2=module, 3=user_sym */
} DSCompletionItem;

int ds_editor_complete(const char *full_source, size_t source_len, size_t cursor_byte_offset,
                       DSCompletionItem *out_items, int max_items);

#ifdef __cplusplus
}
#endif

#endif
