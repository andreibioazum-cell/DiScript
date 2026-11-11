#ifndef RUNTIME_H
#define RUNTIME_H

#include <math.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <android/asset_manager.h>
#include <android/log.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Buffer {
    uint32_t *pixels;
    int width;
    int height;
    int stride;
} Buffer;

extern int screen_w, screen_h;
extern double dt;
extern int mouse_clicked;
extern double ds_mouse_x, ds_mouse_y;

typedef struct {
    float x, y, dx, dy, ox, oy, r;
} Joy;
extern Joy joy;

/* ---- Logging & Console -------------------------------------------------- */
extern int ds_log_quiet;
void ds_log(const char *format, ...);
void ds_log_err(const char *format, ...);
void ds_console_log(int is_error, const char *format, ...);
void ds_console_raw(int is_error, const char *line);
void console_clear(void);
int console_count(void);
const char *console_line(int index);
int console_type(int index);

/* ---- Protected Execution & Timing --------------------------------------- */
typedef void (*DSProtectedFunction)(void *userdata);
int ds_call_protected(DSProtectedFunction function, void *userdata, const char *label);
void ds_runtime_error(const char *format, ...);
const char *ds_runtime_error_message(void);
int ds_game_has_error(void);
void ds_clear_runtime_error(void);
void ds_request_game_restart(void);
int ds_game_restart_requested(void);
void ds_clear_game_restart(void);

uint64_t ds_monotonic_ns(void);
double ds_time_sec(void);
long ds_wall_clock(void);

/* ---- Storage & SAF File Dialogs ----------------------------------------- */
void ds_set_storage_dirs(const char *internal_dir, const char *external_dir);
const char *ds_data_dir(void);
const char *ds_external_dir(void);

void ds_saf_request_open(void);
void ds_saf_request_save(const char *suggested_name);
/* Returns fd >= 0 when user picked/created a file; kind: 1=open, 2=save. */
int ds_saf_poll_fd(int *out_kind, char *out_name, size_t name_cap);

/* ---- Android Keyboard, Editor Event Stream & Clipboard ------------------ */
typedef enum {
    DS_ED_NONE = 0,
    DS_ED_TEXT = 1,
    DS_ED_BACKSPACE = 2,
    DS_ED_ENTER = 3,
    DS_ED_KEY = 4
} DSEditorEventKind;

/* Portable key codes forwarded from Android KeyEvent */
#define DS_KEY_DPAD_UP      19
#define DS_KEY_DPAD_DOWN    20
#define DS_KEY_DPAD_LEFT    21
#define DS_KEY_DPAD_RIGHT   22
#define DS_KEY_A            29
#define DS_KEY_C            31
#define DS_KEY_F            34
#define DS_KEY_R            46
#define DS_KEY_S            47
#define DS_KEY_V            50
#define DS_KEY_X            52
#define DS_KEY_Y            53
#define DS_KEY_Z            54
#define DS_KEY_TAB          61
#define DS_KEY_ENTER        66
#define DS_KEY_DEL          67
#define DS_KEY_PAGE_UP      92
#define DS_KEY_PAGE_DOWN    93
#define DS_KEY_ESCAPE       111
#define DS_KEY_FORWARD_DEL  112
#define DS_KEY_MOVE_HOME    122
#define DS_KEY_MOVE_END     123
#define DS_KEY_F5           135

#define DS_META_SHIFT       0x0001
#define DS_META_ALT         0x0002
#define DS_META_CTRL        0x1000

typedef struct {
    DSEditorEventKind kind;
    int count;
    int keycode;
    int meta;
    char text[128];
} DSEditorEvent;

void ds_set_activity(void *activity);
void keyboard_show(void);
void keyboard_hide(void);
int keyboard_visible(void);
int ds_keyboard_inset_px(void);

void ds_editor_push_text(const char *utf8);
void ds_editor_push_backspace(int count);
void ds_editor_push_enter(void);
void ds_editor_push_key(int keycode, int meta);
int ds_editor_poll_event(DSEditorEvent *out);
void ds_editor_clear_events(void);

void keyboard_tick(uint64_t now_ms);
void keyboard_cancel_repeat(void);
int keyboard_handle_key(int keycode, int action, int meta);

void ds_clipboard_set(const char *utf8);
void ds_clipboard_get(char *dst, size_t cap);

/* ---- Legacy String / Math Helpers --------------------------------------- */
char *ds_concat(const char *left, const char *right);
char *ds_num_to_string(double number);
void ds_string_pool_reset(void);
void ds_string_pool_tick(void);

typedef struct DSArray DSArray;
DSArray *arr_new(void);
void arr_push(DSArray *a, double v);
double arr_get(DSArray *a, double idx);
void arr_set(DSArray *a, double idx, double v);
double arr_len(DSArray *a);
void arr_clear(DSArray *a);
void arr_free(DSArray *a);

double clamp(double v, double lo, double hi);
double lerp(double a, double b, double t);
double dist(double x1, double y1, double x2, double y2);
double ds_mod(double a, double b);
double str_len(const char *s);
int str_eq(const char *a, const char *b);
double str_index_of(const char *hay, const char *needle);
int str_starts_with(const char *s, const char *pref);
const char *str_sub(const char *s, double start, double len);
const char *str_trim(const char *s);
const char *str_upper(const char *s);

/* ---- Vulkan Graphics & Font API ----------------------------------------- */
#define DS_FONT_ASSET "fonts/DejaVuSansMono.ttf"
#define DS_FONT_PIXEL_HEIGHT 28

int ds_graphics_init(AAssetManager *assets, ANativeWindow *window);
int ds_graphics_begin_frame(Buffer *buffer);
void ds_graphics_end_frame(void);
void ds_graphics_cancel_frame(void);
void ds_graphics_error_screen(const char *message);
void ds_graphics_window_lost(void);
void ds_graphics_shutdown(void);
const char *ds_graphics_failure(void);
int ds_graphics_show_failure(ANativeWindow *window, int attempts);
int ds_graphics_pixel_scale(void);
void ds_graphics_report_frame_interval(double sec);

void ds_set_asset_manager(AAssetManager *assets);
void ds_release_assets(void);
int ds_asset_list(const char *dir, char *out, int cap);
int ds_asset_extract(const char *name, void *buf, int cap);
int ds_asset_read(const char *name, void *buf, int cap);
int ds_asset_size(const char *name);
void ds_assets_pack_dir(const char *dir);
int ds_assets_reload_textures(void);

int png_load(const char *name);
int png_load_memory(const char *name, const void *data, int size);
int rgba_load_memory(const char *name, const uint32_t *rgba_pixels, int w, int h);
int png_reset(const char *name);
int tex_ready(const char *name);
int tex_size(const char *name, int *out_w, int *out_h);

void rect(float x, float y, float w, float h, uint32_t color);
void rect_line(float x, float y, float w, float h, float thickness, uint32_t color);
void roundrect(float x, float y, float w, float h, float r, uint32_t color);
void cb_button_gradient(float x, float y, float w, float h, float r, uint32_t color);
void ds_panel_gradient(float x, float y, float w, float h, float r, uint32_t top_col, uint32_t bot_col);
void rect_rot(float x, float y, float w, float h, float angle_deg, uint32_t color);
void circle(float x, float y, float r, uint32_t color);
void ring(float x, float y, float r, float thickness, uint32_t color);
void ellipse_fill(float x, float y, float rx, float ry, uint32_t color);
void ellipse_line(float x, float y, float rx, float ry, float thickness, uint32_t color);
void sector(float x, float y, float r, float start_deg, float sweep_deg, uint32_t color);
void arc_line(float x, float y, float r, float start_deg, float sweep_deg, float thickness, int closed_pie, uint32_t color);
void line(float x1, float y1, float x2, float y2, float thickness, uint32_t color);
void poly_fill(const float *xy, int point_count, uint32_t color);
void poly_line(const float *xy, int point_count, float thickness, int closed, uint32_t color);

void tex(float x, float y, const char *name, float angle_deg, float scale);
void tex_tint(float x, float y, const char *name, float angle_deg, float scale, uint32_t color);
void tex_alpha(float x, float y, const char *name, float angle_deg, float scale, double alpha);
void tex_round(float x, float y, const char *name, float scale, float radius);
void tex_ex(const char *name, float x, float y, float angle_rad, float sx, float sy, float ox, float oy, uint32_t tint);

void ds_gfx_gray(double on);
void ds_gfx_shake(double amp, double seconds);
void ds_gfx_scissor(double x, double y, double w, double h);

void text(const char *string, float x, float y, uint32_t color);
void text_scaled(const char *string, float x, float y, uint32_t color, float scale);
void text_n(const char *string, size_t byte_len, float x, float y, uint32_t color, float scale);
void text_rainbow(const char *string, float x, float y, float scale, float hue);
void text_shimmer(const char *string, float x, float y, uint32_t color, float scale, float phase);

float text_width(const char *string, float scale);
float text_width_n(const char *string, size_t byte_len, float scale);
float text_line_height(float scale);
float text_mono_advance(float scale);

/* ---- Audio & Procedural Synthesizer ------------------------------------- */
int ds_sound_init(AAssetManager *assets);
void ds_sound_shutdown(void);
void ds_sound_suspend(void);
void ds_sound_pause(void);
void ds_sound_resume(void);
void ds_sound_set_java_vm(void *vm);

int snd_load(const char *name);
int snd_play(const char *name);
int snd_loop(const char *name);
void snd_stop(const char *name);
void snd_stop_all(void);
int snd_playing(const char *name);
void snd_volume(const char *name, double volume);
int snd_register_pcm(const char *name, const int16_t *stereo_samples, size_t frames, int src_rate);
int snd_synth_tone(double freq_hz, double duration_sec, const char *wave, double volume);
int snd_synth_sfx(const char *preset, double volume);

#ifdef __cplusplus
}
#endif

#endif
