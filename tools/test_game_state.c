#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/game.c"

/* Test-only views over fighter sprites. */
static const char *fighter_sprite(double cls, double punching, double skin) {
    double look = class_look(cls, skin);
    if (look_ready(look) != 1) {
        return "";
    }
    return look_tex(look, punching == 1);
}

static double class_hp_of(double cls) { return astra_ability_max_hp_for(cls, astra_ability_of(cls, class_skin_of(cls))); }

static double class_power_of(double cls) {
    return astra_ability_punch_damage_for(cls, class_level_of(cls), astra_ability_of(cls, class_skin_of(cls)));
}

static double class_visible_index(double cls) {
    double n = 0;
    double i = 0;
    while (i < class_count) {
        double c = class_at_order(i);
        if (class_visible(c) == 1) {
            if (c == cls) {
                return n;
            }
            n = n + 1;
        }
        i = i + 1;
    }
    return -1;
}

int screen_w = 320;
int screen_h = 180;
double dt = 1.0 / 60.0;
int mouse_clicked;
double ds_mouse_x;
double ds_mouse_y;
Joy joy;

/* --- admin panel test instrumentation -------------------------------- */
#define ADM_COLOR_MAX 1024
static int adm_color_n;
static uint32_t adm_colors[ADM_COLOR_MAX];
static int adm_text_n;
static char adm_text[64][80];
static int adm_rr_n;
static double adm_rr_x, adm_rr_y, adm_rr_w, adm_rr_h;
static double adm_last_circle_r;
static double adm_last_circle_x, adm_last_circle_y;
static void adm_record_color(uint32_t c) {
    if (adm_color_n < ADM_COLOR_MAX)
        adm_colors[adm_color_n++] = c;
}
static void adm_record_text(const char *s) {
    if (adm_text_n < 64 && s)
        snprintf(adm_text[adm_text_n++], sizeof(adm_text[0]), "%s", s);
}
#define ADM_RR_COLOR_MAX 512
static int adm_rr_color_n;
static uint32_t adm_rr_colors[ADM_RR_COLOR_MAX];
static void adm_record_rr_color(uint32_t c) {
    if (adm_rr_color_n < ADM_RR_COLOR_MAX)
        adm_rr_colors[adm_rr_color_n++] = c;
}
static void adm_reset_log(void) {
    adm_color_n = 0;
    adm_text_n = 0;
    adm_rr_n = 0;
    adm_rr_color_n = 0;
}
static int adm_has_text(const char *needle) {
    for (int i = 0; i < adm_text_n; i++)
        if (strstr(adm_text[i], needle))
            return 1;
    return 0;
}
/* The panel paints no half-transparent shapes: fills and outlines are either
 * fully opaque (alpha 0xFF) or the plain no-alpha colour the game uses. */
static int adm_all_flat(void) {
    for (int i = 0; i < adm_rr_color_n; i++) {
        uint32_t a = adm_rr_colors[i] >> 24;
        if (a != 0 && a != 0xFF)
            return 0;
    }
    return 1;
}
/* Enabled / selected buttons may only turn grey: nothing in the panel may be
 * painted with a white fill, and the fill count says how many buttons are on. */
static int adm_no_white_fill(void) {
    for (int i = 0; i < adm_rr_color_n; i++)
        if ((adm_rr_colors[i] & 0xFFFFFF) == 0xFFFFFF)
            return 0;
    return 1;
}
static int adm_fill_count(uint32_t c) {
    int n = 0;
    for (int i = 0; i < adm_rr_color_n; i++)
        if ((adm_rr_colors[i] & 0xFFFFFF) == c)
            n++;
    return n;
}
static int adm_all_greyscale(void) {
    for (int i = 0; i < adm_color_n; i++) {
        uint32_t c = adm_colors[i];
        if (((c >> 16) & 255) != ((c >> 8) & 255) || ((c >> 8) & 255) != (c & 255))
            return 0;
    }
    return 1;
}

/* The Players tab reads the ban list through these: a tiny in-memory list. */
static char stub_bans[8][24];
static int stub_ban_n;
static char stub_nicks[4][24];
static char stub_last_chat[128];
static uint32_t stub_circle_color;

static int ground_texture_missing;
static int ground_tiles;
static int ground_fills;
static uint32_t last_ground_fill;
static const char *last_ground;
static int lobby_playing;
static int winter_playing;
static int winter_loop_calls;
static int snowflake_draws;
static int snowflake_tinted;
static uint32_t snowflake_tint_color;
static double snowflake_tint_scale;
static double snowflake_centres[64];
static int snowflake_centre_n;
static int star_line_count;
static double star_max_y;
/* A stroke whose both ends sit on one circle around the FX centre is a piece
 * of a ring. The CodeCracker ult must not draw any of those. */
static int fx_orbit_lines;
static double fx_center_x = 200;
static double fx_center_y = 200;

struct DSArray {
    double *data;
    size_t length;
    size_t capacity;
};

DSArray *arr_new(void) {
    DSArray *array = calloc(1, sizeof(*array));
    assert(array);
    return array;
}

void arr_push(DSArray *array, double value) {
    if (array->length == array->capacity) {
        size_t capacity = array->capacity ? array->capacity * 2 : 8;
        double *data = realloc(array->data, capacity * sizeof(*data));
        assert(data);
        array->data = data;
        array->capacity = capacity;
    }
    array->data[array->length++] = value;
}

double arr_get(DSArray *array, double index) {
    size_t position = (size_t)index;
    return array && position < array->length ? array->data[position] : 0;
}

void arr_set(DSArray *array, double index, double value) {
    size_t position = (size_t)index;
    while (array->length <= position)
        arr_push(array, 0);
    array->data[position] = value;
}

double arr_len(DSArray *array) {
    return array ? (double)array->length : 0;
}

double clamp(double value, double low, double high) {
    return value < low ? low : value > high ? high : value;
}

void arr_clear(DSArray *array) {
    if (array)
        array->length = 0;
}

void arr_free(DSArray *array) {
    if (!array)
        return;
    free(array->data);
    free(array);
}

void ds_runtime_error(const char *format, ...) {
    (void)format;
    abort();
}

void ds_log(const char *format, ...) {
    (void)format;
}
void ds_gfx_gray(double on) {
    (void)on;
}
void ds_gfx_shake(double amp, double seconds) {
    (void)amp;
    (void)seconds;
}
static int stub_scissor_calls;
static int stub_scissor_active;
void ds_gfx_scissor(double x, double y, double w, double h) {
    (void)x;
    (void)y;
    stub_scissor_calls++;
    stub_scissor_active = (w > 0.0 && h > 0.0) ? 1 : 0;
}
double dist(double x, double y, double a, double b) {
    return hypot(x - a, y - b);
}
double ds_mod(double value, double divisor) {
    return (double)((int)value % (int)divisor);
}
/* Real frame length, the number the FPS counter reads from the engine. */
static double stub_frame_seconds;
double ds_frame_seconds(void) { return stub_frame_seconds; }
int str_eq(const char *a, const char *b) {
    return a && b && strcmp(a, b) == 0 ? 1 : 0;
}
double str_len(const char *s) {
    return s ? (double)strlen(s) : 0;
}
/* String join as in the runtime: two alternating buffers, so that
 * ds_concat(ds_concat(a, b), c) does not overwrite itself. */
char *ds_concat(const char *left, const char *right) {
    static char buffers[4][512];
    static int turn = 0;
    turn = (turn + 1) & 3;
    snprintf(buffers[turn], sizeof(buffers[turn]), "%s%s", left ? left : "", right ? right : "");
    return buffers[turn];
}
/* Keyboard stub with a real buffer: the admin nick field is driven through the
 * same calls the platform keyboard uses. */
static char stub_kb[128];
static int stub_kb_visible;
void keyboard_hide(void) { stub_kb_visible = 0; }
void keyboard_clear(void) { stub_kb[0] = 0; }
void keyboard_show(void) { stub_kb_visible = 1; }
int keyboard_visible(void) { return stub_kb_visible; }
void keyboard_get_text_to(char *dst, size_t cap) {
    if (dst && cap)
        snprintf(dst, cap, "%s", stub_kb);
}
void keyboard_type(const char *text) { snprintf(stub_kb, sizeof(stub_kb), "%s", text ? text : ""); }
void keyboard_backspace(void) {
    size_t len = strlen(stub_kb);
    if (len > 0)
        stub_kb[len - 1] = 0;
}
int keyboard_enter_pressed(void) { return 0; }
int keyboard_uses_editor(void) { return 0; }
int keyboard_handle_key(int keycode, int action, int meta) {
    (void)keycode;
    (void)action;
    (void)meta;
    return 0;
}
static void stub_kb_set(const char *text) { snprintf(stub_kb, sizeof(stub_kb), "%s", text ? text : ""); }
static double stub_net_slot = -1;
static double stub_online[4];
static double stub_chat_count;
static char stub_chat_keys[4][28];
static char stub_chat_uids[4][24];
static char stub_chat_texts[4][64];
static double stub_level[4];
static double stub_grab[4];
static double stub_gx[4];
static double stub_gy[4];
static double stub_gdx[4];
static double stub_gdy[4];
double net_slot(void) {
    return stub_net_slot;
}
static double stub_class[4];
static double stub_hp[4];
double net_player_online(double slot) {
    return stub_online[(int)slot];
}
double net_player_hp(double slot) {
    return stub_hp[(int)slot];
}
double net_player_class(double slot) {
    return stub_class[(int)slot];
}
double net_player_level(double slot) {
    return stub_level[(int)slot];
}
double net_player_grab(double slot) {
    return stub_grab[(int)slot];
}
double net_player_grab_x(double slot) {
    return stub_gx[(int)slot];
}
double net_player_grab_y(double slot) {
    return stub_gy[(int)slot];
}
double net_player_grab_dx(double slot) {
    return stub_gdx[(int)slot];
}
double net_player_grab_dy(double slot) {
    return stub_gdy[(int)slot];
}
static double stub_pub_hp = -1, stub_pub_alive = -1;
static int stub_pub_calls = 0;
void net_publish(double x, double y, double angle, double hp, double alive) {
    (void)x;
    (void)y;
    (void)angle;
    stub_pub_hp = hp;
    stub_pub_alive = alive;
    stub_pub_calls++;
}
void net_publish_block(double on) { (void)on; }
void net_publish_codecracker_super(double on) { (void)on; }
void net_publish_punch(double x, double y, double dx, double dy, double punch) {
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
    (void)punch;
}
void net_publish_turrets(double x1, double y1, double hp1, double x2, double y2, double hp2, double x3, double y3,
                         double hp3, double count) {
    (void)x1;
    (void)y1;
    (void)hp1;
    (void)x2;
    (void)y2;
    (void)hp2;
    (void)x3;
    (void)y3;
    (void)hp3;
    (void)count;
}
void net_publish_universe(double x, double y, double counter) {
    (void)x;
    (void)y;
    (void)counter;
}
void net_publish_dash(double x, double y, double dx, double dy, double dash) {
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
    (void)dash;
}
void net_publish_grab(double x, double y, double dx, double dy, double grab) {
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
    (void)grab;
}
static double stub_brick[4], stub_bkx[4], stub_bky[4], stub_bkdx[4], stub_bkdy[4];
void net_publish_brick(double x, double y, double dx, double dy, double shot) {
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
    (void)shot;
}
void net_publish_snow(double x, double y, double dx, double dy, double shot) {
    (void)x;
    (void)y;
    (void)dx;
    (void)dy;
    (void)shot;
}
void net_publish_thud(double seq) { (void)seq; }
static double stub_xball[4], stub_xbx[4], stub_xby[4], stub_xbdx[4], stub_xbdy[4];
void net_publish_xball(double x, double y, double dx, double dy, double shot) {
    (void)x; (void)y; (void)dx; (void)dy; (void)shot;
}
double net_player_xball(double slot) { return stub_xball[(int)slot]; }
double net_player_xball_x(double slot) { return stub_xbx[(int)slot]; }
double net_player_xball_y(double slot) { return stub_xby[(int)slot]; }
double net_player_xball_dx(double slot) { return stub_xbdx[(int)slot]; }
double net_player_xball_dy(double slot) { return stub_xbdy[(int)slot]; }
double net_player_brick(double slot) { return stub_brick[(int)slot]; }
double net_player_brick_x(double slot) { return stub_bkx[(int)slot]; }
double net_player_brick_y(double slot) { return stub_bky[(int)slot]; }
double net_player_brick_dx(double slot) { return stub_bkdx[(int)slot]; }
double net_player_brick_dy(double slot) { return stub_bkdy[(int)slot]; }
static double stub_universe[4], stub_ux[4], stub_uy[4];
static double stub_snow[4], stub_sx[4], stub_sy[4], stub_sdx[4], stub_sdy[4];
static double stub_dash[4], stub_thud[4], stub_cc_super[4];
static double stub_rx[4], stub_ry[4], stub_ra[4], stub_ralive[4], stub_rpunch[4], stub_rskin[4];
double net_player_universe(double slot) { return stub_universe[(int)slot]; }
double net_player_universe_x(double slot) { return stub_ux[(int)slot]; }
double net_player_universe_y(double slot) { return stub_uy[(int)slot]; }
double net_player_snow(double slot) { return stub_snow[(int)slot]; }
double net_player_snow_x(double slot) { return stub_sx[(int)slot]; }
double net_player_snow_y(double slot) { return stub_sy[(int)slot]; }
double net_player_snow_dx(double slot) { return stub_sdx[(int)slot]; }
double net_player_snow_dy(double slot) { return stub_sdy[(int)slot]; }
double net_player_dash(double slot) { return stub_dash[(int)slot]; }
double net_player_thud(double slot) { return stub_thud[(int)slot]; }
double net_player_x(double slot) { return stub_rx[(int)slot]; }
double net_player_y(double slot) { return stub_ry[(int)slot]; }
double net_player_angle(double slot) { return stub_ra[(int)slot]; }
double net_player_alive(double slot) { return stub_ralive[(int)slot]; }
double net_player_punch(double slot) { return stub_rpunch[(int)slot]; }
double net_player_skin(double slot) { return stub_rskin[(int)slot]; }
double net_player_punch_x(double slot) { return stub_rx[(int)slot]; }
double net_player_punch_y(double slot) { return stub_ry[(int)slot]; }
double net_player_punch_dx(double slot) { (void)slot; return 1; }
double net_player_punch_dy(double slot) { (void)slot; return 0; }
void net_mark_achievement_flag(double flag) {
    (void)flag;
}
void net_save_azum_revives(double value) {
    (void)value;
}
double stub_bpx = 0;
double net_load_bp_level(void) {
    return bp_claimed;
}
void net_set_bp_xp(double v) {
    if (v > stub_bpx)
        stub_bpx = v;
}
double net_load_bp_xp(void) {
    return stub_bpx;
}
void net_set_class(double value) {
    (void)value;
}
void net_set_level(double value) {
    (void)value;
}
void net_set_skin(double value) {
    (void)value;
}
void net_save_astra(double owned, double level, double unlocked) {
    (void)owned;
    (void)level;
    (void)unlocked;
}
static double stub_login_status;
static char stub_login_nick[24];
static char stub_login_pass[24];
const char *net_player_nick(double slot) {
    int i = (int)slot;
    return (i >= 0 && i < 4) ? stub_nicks[i] : "";
}
/* Extra account password: device-only hash, kept in a plain stub variable. */
static int stub_pin2;
double net_load_pin2(void) { return (double)stub_pin2; }
void net_save_pin2(int hash) { stub_pin2 = hash > 0 ? hash : 0; }

/* In-memory complaints so the owner screen can be exercised without network. */
static int stub_report_n;
static const char *stub_report_from[8], *stub_report_to[8], *stub_report_text[8], *stub_report_room[8];
double net_report_send(const char *target, const char *text, const char *room) {
    (void)text;
    (void)room;
    if (target && *target && stub_report_n < 8) {
        stub_report_from[stub_report_n] = "Tester";
        stub_report_to[stub_report_n] = target;
        stub_report_text[stub_report_n] = "rude";
        stub_report_room[stub_report_n] = "main";
        stub_report_n++;
    }
    return 1;
}
void net_reports_fetch(void) {}
double net_reports_status(void) { return 2; }
double net_reports_count(void) { return (double)stub_report_n; }
const char *net_report_from(double idx) { return idx >= 0 && idx < stub_report_n ? stub_report_from[(int)idx] : ""; }
const char *net_report_target(double idx) { return idx >= 0 && idx < stub_report_n ? stub_report_to[(int)idx] : ""; }
const char *net_report_text(double idx) { return idx >= 0 && idx < stub_report_n ? stub_report_text[(int)idx] : ""; }
const char *net_report_room(double idx) { return idx >= 0 && idx < stub_report_n ? stub_report_room[(int)idx] : ""; }
const char *net_report_key(double idx) { (void)idx; return "1_1"; }
void net_report_delete(double idx) {
    int i = (int)idx;
    if (i < 0 || i >= stub_report_n) return;
    while (i + 1 < stub_report_n) {
        stub_report_from[i] = stub_report_from[i + 1];
        stub_report_to[i] = stub_report_to[i + 1];
        stub_report_text[i] = stub_report_text[i + 1];
        stub_report_room[i] = stub_report_room[i + 1];
        i++;
    }
    stub_report_n--;
}

void net_ban_set(const char *nick, double banned) {
    int i, j = 0;
    if (!nick || !*nick)
        return;
    for (i = 0; i < stub_ban_n; i++) {
        if (strcmp(stub_bans[i], nick) == 0)
            continue;
        if (j != i)
            memcpy(stub_bans[j], stub_bans[i], sizeof(stub_bans[0]));
        j++;
    }
    stub_ban_n = j;
    if (banned >= 0.5 && stub_ban_n < 8) {
        memset(stub_bans[stub_ban_n], 0, sizeof(stub_bans[0]));
        snprintf(stub_bans[stub_ban_n], sizeof(stub_bans[0]), "%s", nick);
        stub_ban_n++;
    }
}
char *ds_num_to_string(double number) {
    static char buffers[4][24];
    static int turn = 0;
    turn = (turn + 1) & 3;
    snprintf(buffers[turn], sizeof(buffers[turn]), "%.0f", number);
    return buffers[turn];
}
void net_chat_send(const char *text) {
    snprintf(stub_last_chat, sizeof(stub_last_chat), "%s", text ? text : "");
}
double net_login_status(void) {
    return stub_login_status;
}
const char *net_login_nick(void) {
    return stub_login_nick;
}
const char *net_login_pass(void) {
    return stub_login_pass;
}
static char last_bye[256];
static int bye_calls;
void bye_notice_show(const char *text) {
    bye_calls++;
    snprintf(last_bye, sizeof(last_bye), "%s", text ? text : "");
}
static int fx_ring_count;
static int fx_big_circle_count;
static int wheel_outline_capture;
static int wheel_capture_line_n;
static uint32_t wheel_capture_line_color[16];
static float wheel_capture_line_width[16];
static int wheel_capture_ring_n;
static uint32_t wheel_capture_ring_color[4];
static float wheel_capture_ring_width[4];
static int emerald_tint_draws;
static uint32_t last_emerald_tint;
void ring(float x, float y, float r, float t, uint32_t color) {
    fx_ring_count++;
    (void)x;
    (void)y;
    (void)r;
    if (wheel_outline_capture && wheel_capture_ring_n < 4) {
        wheel_capture_ring_color[wheel_capture_ring_n] = color;
        wheel_capture_ring_width[wheel_capture_ring_n] = t;
        wheel_capture_ring_n++;
    }
    adm_record_color(color);
}
void tex_tint(float x, float y, const char *name, float angle, float scale, uint32_t color) {
    (void)y;
    (void)angle;
    (void)scale;
    ground_tiles++;
    last_ground = name;
    if (name && strcmp(name, "gift_emerald.png") == 0) {
        emerald_tint_draws++;
        last_emerald_tint = color;
    }
    if (name && SNOWFLAKE_TEX && strcmp(name, SNOWFLAKE_TEX) == 0) {
        snowflake_tinted++;
        snowflake_tint_color = color;
        snowflake_tint_scale = scale;
        snowflake_draws++;
        if (snowflake_centre_n < 64) {
            snowflake_centres[snowflake_centre_n++] = x + 26.0 * scale;
        }
    }
}
void net_mark_progress_synced(void) {}
void net_progress_flush(void) {}
double stub_last_bp = -1;
void net_save_progress_all(double a, double b, double c, double d, double e, double f, double g, double h, double i,
                           double j, double k, double l, double m, double n, double o, double p, double q, double r) {
    stub_last_bp = q; /* bp_level: claimed tiers of the battle pass */
    (void)a;
    (void)b;
    (void)c;
    (void)d;
    (void)e;
    (void)f;
    (void)g;
    (void)h;
    (void)i;
    (void)j;
    (void)k;
    (void)l;
    (void)m;
    (void)n;
    (void)o;
    (void)p;
    (void)r;
}

int snd_load(const char *name) {
    return name && *name;
}
char stub_last_snd[64] = "";
int snd_play(const char *name) {
    if (name) {
        snprintf(stub_last_snd, sizeof(stub_last_snd), "%s", name);
    }
    return 1;
}
static int seraphim_playing = 0;
static double seraphim_vol = 0;
int snd_loop(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0) {
        lobby_playing = 1;
    }
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0) {
        winter_playing = 1;
        winter_loop_calls++;
    }
    if (name && SERAPHIM_MUSIC && strcmp(name, SERAPHIM_MUSIC) == 0) {
        seraphim_playing = 1;
    }
    return 1;
}
int snd_playing(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0)
        return lobby_playing;
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0)
        return winter_playing;
    if (name && SERAPHIM_MUSIC && strcmp(name, SERAPHIM_MUSIC) == 0)
        return seraphim_playing;
    return 0;
}
void snd_stop(const char *name) {
    if (name && LOBBY_MUSIC && strcmp(name, LOBBY_MUSIC) == 0)
        lobby_playing = 0;
    if (name && WINTER_JINGLE && strcmp(name, WINTER_JINGLE) == 0)
        winter_playing = 0;
    if (name && SERAPHIM_MUSIC && strcmp(name, SERAPHIM_MUSIC) == 0)
        seraphim_playing = 0;
}
void snd_volume(const char *name, double volume) {
    if (name && SERAPHIM_MUSIC && strcmp(name, SERAPHIM_MUSIC) == 0)
        seraphim_vol = volume;
}

int tex_ready(const char *name) {
    return name && *name && !ground_texture_missing;
}

void rect(float x, float y, float width, float height, uint32_t color) {
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    last_ground_fill = color;
    ground_fills++;
    adm_record_color(color);
}

/* Added for the admin panel: the game only needs these on screen, the host
 * test just records what was drawn. */
static double stub_enemy_hp_fill_x;
void roundrect(float x, float y, float w, float h, float rad, uint32_t color) {
    (void)rad;
    adm_record_color(color);
    adm_record_rr_color(color);
    if ((color & 0xFFFFFFu) == 0xE94560u) {
        stub_enemy_hp_fill_x = x;
    }
    if (adm_rr_n == 0) {
        adm_rr_x = x;
        adm_rr_y = y;
        adm_rr_w = w;
        adm_rr_h = h;
    }
    adm_rr_n++;
}

static float stub_text_x;
static float stub_text_y;
static float stub_text_scale;
/* Position of the last drawn line that contains a watched word. */
static char stub_watch_name[32];
static float stub_watch_x;
static float stub_watch_y;
static int stub_watch_hit;
static void text_watch(const char *name) {
    snprintf(stub_watch_name, sizeof(stub_watch_name), "%s", name ? name : "");
    stub_watch_hit = 0;
}
void text_scaled(const char *s, float x, float y, uint32_t c, float scale) {
    stub_text_x = x;
    stub_text_y = y;
    stub_text_scale = scale;
    if (stub_watch_name[0] && s && strstr(s, stub_watch_name)) {
        stub_watch_x = x;
        stub_watch_y = y;
        stub_watch_hit = 1;
    }
    adm_record_text(s);
    adm_record_color(c);
}

/* Rainbow lettering runs through the engine's own command in the game, so the
 * host build only needs a stub that records the line. */
static int rainbow_text_n;
static char rainbow_text[16][48];
void text_rainbow(const char *s, float x, float y, float scale, float hue) {
    (void)x;
    (void)y;
    (void)scale;
    (void)hue;
    if (s && rainbow_text_n < 16)
        snprintf(rainbow_text[rainbow_text_n++], sizeof(rainbow_text[0]), "%s", s);
}

static int shimmer_text_n;
void text_shimmer(const char *s, float x, float y, uint32_t c, float scale, float phase) {
    (void)phase;
    shimmer_text_n++;
    text_scaled(s, x, y, c, scale);
}

int text_ink_width(const char *s) {
    return s ? (int)(9 * strlen(s)) : 0;
}

int text_ink_height(const char *s) {
    (void)s;
    return 40;
}

int text_ink_top(const char *s) {
    (void)s;
    return -34;
}

void line(float x1, float y1, float x2, float y2, float thickness, uint32_t color) {
    double d1 = hypot(x1 - fx_center_x, y1 - fx_center_y);
    double d2 = hypot(x2 - fx_center_x, y2 - fx_center_y);
    if (wheel_outline_capture && wheel_capture_line_n < 16) {
        wheel_capture_line_color[wheel_capture_line_n] = color;
        wheel_capture_line_width[wheel_capture_line_n] = thickness;
        wheel_capture_line_n++;
    }
    star_line_count++;
    if (y1 > star_max_y) star_max_y = y1;
    if (y2 > star_max_y) star_max_y = y2;
    if (d1 > 30 && d1 < 50 && fabs(d1 - d2) < 0.75) fx_orbit_lines++;
    adm_record_color(color);
}

static int candy_draws;
static double candy_alpha_drawn;
static double candy_scale_drawn;
static double candy_x_drawn;
static double candy_y_drawn;
static uint32_t candy_circle_color;
void sector(float x, float y, float r, float start_deg, float sweep_deg, uint32_t color) {
    (void)x;
    (void)y;
    (void)r;
    (void)start_deg;
    (void)sweep_deg;
    adm_record_color(color);
}

void circle(float x, float y, float r, uint32_t color) {
    if (r > 20) fx_big_circle_count++;
    candy_circle_color = color;
    candy_x_drawn = x;
    candy_y_drawn = y;
    stub_circle_color = color;
    adm_last_circle_r = r;
    adm_last_circle_x = x;
    adm_last_circle_y = y;
    adm_record_color(color);
}
/* The gradient buttons live in C++ (src/game/cpp/ui_fx.cpp); the state test
 * never draws them, so a stub keeps the host link self-contained. */
void cb_button_gradient(float x, float y, float w, float h, float rad, uint32_t color) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rad;
    (void)color;
}
void tex_alpha(float x, float y, const char *name, float angle, float scale, double alpha) {
    (void)angle;
    if (name && CANDY_TEX && strcmp(name, CANDY_TEX) == 0) {
        candy_draws++;
        candy_scale_drawn = scale;
        candy_alpha_drawn = alpha;
                /* tex_alpha() wants the top left corner, the centre is 25*scale right and down. */
        candy_x_drawn = x + 25.0 * scale;
        candy_y_drawn = y + 25.0 * scale;
    }
}

static int gift_tex_draws;
static const char *last_gift_tex;
static double last_tex_scale;
static double last_tex_x, last_tex_y;
void tex(float x, float y, const char *name, float angle, float scale) {
    (void)angle;
    ground_tiles++;
    last_ground = name;
    last_tex_x = x;
    last_tex_y = y;
    last_tex_scale = scale;
    if (name && strncmp(name, "gift_", 5) == 0) {
        gift_tex_draws++;
        last_gift_tex = name;
    }
    if (name && SNOWFLAKE_TEX && strcmp(name, SNOWFLAKE_TEX) == 0) {
        snowflake_draws++;
        if (snowflake_centre_n < 64) {
            /* tex() receives the top-left corner; the game sets hw = 26*scale, so the centre is 26*scale further right. */
            snowflake_centres[snowflake_centre_n++] = x + 26.0 * scale;
        }
    }
}

/* --- Engine API pulled in by the full touch dispatch: the release-tap test
 * reaches logic_touch -> touch_menu -> login -> progress_from_storage and
 * friends. These paths are not exercised here; the stubs only satisfy the
 * linker (with --gc-sections these used to be stripped as dead code). ----- */
double net_auth(const char *url, const char *nick, const char *pass) {
    (void)url;
    (void)nick;
    (void)pass;
    return 2;
}
/* Offline/online switch for every live Firebase-driven feature. */
static double stub_link = 1;
static double stub_azum_buff;
double net_azum_buff(void) { return stub_link == 1 ? stub_azum_buff : 0; }
static double stub_flash_sale;
static int stub_live_refreshes;
double net_flash_sale(void) { return stub_link == 1 ? stub_flash_sale : 0; }
static double stub_christmas;
double net_christmas(void) { return stub_link == 1 ? stub_christmas : 0; }
void net_live_flags_refresh(void) { stub_live_refreshes++; }
double net_flags_reachable(void) { return stub_link; }
static double stub_banner_ts;
double net_banner_ts(void) { return stub_banner_ts; }
void net_banner_clear(void) { stub_banner_ts = 0; }
static double stub_event_mode;
double net_event(void) { return stub_event_mode; }
void net_event_set(double mode) { stub_event_mode = mode; }
double net_load_cups(void) { return 0; }
double net_load_candies(void) { return 0; }
double net_load_class(void) { return 0; }
double net_load_azum(void) { return 0; }
double net_load_santa(void) { return 0; }
double net_load_ebuc(void) { return 0; }
double net_load_level(void) { return 0; }
double net_load_levels_unlocked(void) { return 0; }
double net_load_ordinary_level(void) { return 0; }
double net_load_ordinary_levels_unlocked(void) { return 0; }
double net_load_azum_level(void) { return 0; }
double net_load_azum_levels_unlocked(void) { return 0; }
double net_load_santa_level(void) { return 0; }
double net_load_santa_levels_unlocked(void) { return 0; }
double net_load_ebuc_level(void) { return 0; }
double net_load_ebuc_levels_unlocked(void) { return 0; }
double net_load_azum_skin(void) { return 0; }
static int stub_wheel_spins, stub_wheel_wins;
int net_load_wheel_spins(void) { return stub_wheel_spins; }
int net_load_wheel_wins(void) { return stub_wheel_wins; }
void net_save_wheel_progress(int spins, int wins) { stub_wheel_spins = spins; stub_wheel_wins = wins; }
double net_load_astra(void) { return 0; }
double net_load_astra_level(void) { return 0; }
double net_load_astra_levels_unlocked(void) { return 0; }
static double stub_last_codecracker;
double net_load_codecracker(void) { return 0; }
double net_load_codecracker_level(void) { return 0; }
double net_load_codecracker_levels_unlocked(void) { return 0; }
void net_save_codecracker(double owned, double level, double levels_unlocked) {
    stub_last_codecracker = owned;
    (void)level;
    (void)levels_unlocked;
}
double net_load_achievement_flags(void) { return 0; }
double net_load_azum_revives(void) { return 0; }
void net_connect(const char *url, const char *room) { (void)url; (void)room; }
void net_disconnect(void) {}
static int stub_promo2027_used;
double net_promo_2027_used(void) { return stub_promo2027_used; }
void net_promo_2027_mark_used(void) { stub_promo2027_used = 1; }
void net_leaderboard_fetch(const char *url) { (void)url; }
double net_load_language(void) { return 0; }
double net_load_hitboxes(void) { return 0; }
void net_save_settings(double language, double hitboxes) {
    (void)language;
    (void)hitboxes;
}
double net_load_music_volume(void) { return 0; }
void net_save_music_volume(double volume) { (void)volume; }
double net_load_winter_theme(void) { return 0; }
void net_save_winter_theme(double on) { (void)on; }
double net_load_new_theme(void) { return 1; }
void net_save_new_theme(double on) { (void)on; }
double net_load_theme_mode(void) { return 0; }
void net_save_theme_mode(double mode) { (void)mode; }
static double stub_theme_chosen_load;
static double stub_theme_chosen_saved;
double net_load_theme_chosen(void) { return stub_theme_chosen_load; }
void net_save_theme_chosen(double on) { stub_theme_chosen_saved = on; }
int net_fs_list(const char *dir, char *out, int cap) { (void)dir; if (cap) out[0] = 0; return 0; }
int net_fs_read(const char *path, void *buf, int cap) { (void)path; (void)buf; (void)cap; return 0; }
int net_fs_home(char *dst, size_t cap) { if (cap) dst[0] = 0; return 0; }
int net_fs_dir_readable(const char *dir) { (void)dir; return 0; }
int ds_pick_open_file(char *dst, int cap) { if (cap) dst[0] = 0; return 0; }
void ds_assets_pack_dir(const char *dir) { (void)dir; }
int ds_assets_reload_textures(void) { return 0; }
double net_load_parent_pin(void) { return 0; }
void net_save_parent_pin(int hash) { (void)hash; }
double net_load_parent_age(void) { return 0; }
void net_save_parent_age(int age) { (void)age; }
/* Texture packs touch the real file system; the state harness needs the calls
 * to exist but never reads a pack. */
int net_packs_root(char *dst, size_t cap) { if (cap) dst[0] = 0; return 0; }
int net_packs_mkdir(const char *rel) { (void)rel; return 0; }
int net_packs_exists(const char *rel) { (void)rel; return 0; }
int net_packs_write(const char *rel, const void *data, int len) { (void)rel; (void)data; (void)len; return 0; }
int net_packs_read(const char *rel, void *buf, int cap) { (void)rel; (void)buf; (void)cap; return 0; }
int net_packs_size(const char *rel) { (void)rel; return 0; }
int net_packs_list(const char *rel, char *out, int cap) { (void)rel; if (cap) out[0] = 0; return 0; }
int net_packs_remove(const char *rel) { (void)rel; return 0; }
void settings_mark_legal(void) {}
/* The game-state harness does not link the runtime pool tick; the promo
 * test only needs the calls to exist so it can prove the message text is
 * an owned copy, not a pooled pointer. */
void ds_string_pool_tick(void) {}
static double stub_legal_ts;
double settings_legal_ts(void) { return stub_legal_ts; }
/* Personal data consent and account deletion: consent starts granted, so old
 * lobby checks do not run into the question window. */
static double stub_pd_ver = LEGAL_DOCS_VERSION;
static int stub_pd_uploads;
static int stub_delete_calls;
static double stub_delete_status;
static int stub_logout_calls;
void settings_set_pd_consent(double version) { stub_pd_ver = version; }
double settings_pd_consent_version(void) { return stub_pd_ver; }
void net_pd_consent_upload(void) { stub_pd_uploads++; }
static char stub_delete_pass[64];
static int stub_pass_calls;
static double stub_pass_status;
static char stub_pass_old[64], stub_pass_new[64];
void net_delete_account(const char *password) {
    stub_delete_calls++;
    snprintf(stub_delete_pass, sizeof(stub_delete_pass), "%s", password ? password : "");
    stub_delete_status = 1;
}
void net_change_password(const char *old_pass, const char *new_pass) {
    stub_pass_calls++;
    snprintf(stub_pass_old, sizeof(stub_pass_old), "%s", old_pass ? old_pass : "");
    snprintf(stub_pass_new, sizeof(stub_pass_new), "%s", new_pass ? new_pass : "");
    stub_pass_status = 1;
}
double net_change_password_status(void) {
    double v = stub_pass_status;
    if (v >= 2) {
        stub_pass_status = 0;
    }
    return v;
}
double net_delete_account_status(void) {
    double v = stub_delete_status;
    if (v >= 2) {
        stub_delete_status = 0;
    }
    return v;
}
void net_logout(void) {
    stub_logout_calls++;
    stub_login_status = 0;
}
double net_is_banned(const char *nick) {
    (void)nick;
    return 0;
}
double net_chat_is_ban(const char *msg) {
    (void)msg;
    return 0;
}
double net_chat_is_unban(const char *msg) {
    (void)msg;
    return 0;
}
const char *net_chat_ban_target(const char *msg) {
    (void)msg;
    return "";
}
const char *net_chat_unban_target(const char *msg) {
    (void)msg;
    return "";
}
static int stub_admin_reset_calls;
static char stub_admin_reset_nick[24], stub_admin_reset_new[64];
static double stub_admin_reset_status;
void net_admin_reset_password(const char *nick, const char *new_pass) {
    stub_admin_reset_calls++;
    snprintf(stub_admin_reset_nick, sizeof(stub_admin_reset_nick), "%s", nick ? nick : "");
    snprintf(stub_admin_reset_new, sizeof(stub_admin_reset_new), "%s", new_pass ? new_pass : "");
    stub_admin_reset_status = 1;
}
double net_admin_reset_status(void) {
    double v = stub_admin_reset_status;
    if (v >= 2) stub_admin_reset_status = 0;
    return v;
}
static double stub_gift_counts[8];
double net_gift_count(double gift) {
    int i = (int)gift;
    return i >= 0 && i < 8 ? stub_gift_counts[i] : 0;
}
/* A local change of one counter: the rest of the collection stays as it is. */
void net_gift_count_add(double gift, double delta) {
    int i = (int)gift;
    if (i < 0 || i >= 8) return;
    stub_gift_counts[i] = stub_gift_counts[i] + delta;
    if (stub_gift_counts[i] < 0) stub_gift_counts[i] = 0;
}
double net_gift_types(void) { return NET_GIFT_TYPE_COUNT; }
void net_gift_refresh(void) {}
/* The action can be refused (another gift operation is already running). */
static double stub_gift_action_ok = 1;
double net_gift_action(double gift, const char *recipient, double grant) {
    (void)gift; (void)recipient; (void)grant;
    return stub_gift_action_ok;
}
/* Legacy gift action status; emerald color is fixed and has no profile hue. */
static double stub_gift_status;
double net_gift_action_status(void) {
    double v = stub_gift_status;
    if (v >= 2) stub_gift_status = 0;
    return v;
}
/* Balance the database holds after a buy/sell; -1 means "not a shop action". */
static double stub_gift_cups = -1;
double net_gift_cups(void) {
    double v = stub_gift_cups;
    stub_gift_cups = -1;
    return v;
}
double net_gift_price(double gift) {
    int i = (int)gift;
    return i == 0 ? 25 : i == 1 ? 100 : i == 2 ? 250 : i == 3 ? 400 : i == 4 ? 500 : i == 5 ? 555 : 0;
}
double net_gift_refund(double gift) { return (double)((int)net_gift_price(gift) / 2); }
double net_chat_is_passwd(const char *msg) {
    if (!msg || strncmp(msg, "passwd ", 7) != 0) return 0;
    const char *t = msg + 7;
    while (*t == ' ' || *t == '\t') t++;
    if (!*t) return 0;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    return *t ? 1 : 0;
}
static char stub_passwd_target[24], stub_passwd_new[64];
const char *net_chat_passwd_target(const char *msg) {
    const char *t = msg ? msg + 7 : "";
    size_t i = 0;
    while (*t == ' ' || *t == '\t') t++;
    while (t[i] && i < sizeof(stub_passwd_target) - 1 && t[i] != ' ' && t[i] != '\t') { stub_passwd_target[i] = t[i]; i++; }
    stub_passwd_target[i] = 0;
    return stub_passwd_target;
}
const char *net_chat_passwd_new(const char *msg) {
    const char *t = msg ? msg + 7 : "";
    size_t i = 0;
    while (*t == ' ' || *t == '\t') t++;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    while (t[i] && i < sizeof(stub_passwd_new) - 1 && t[i] != ' ' && t[i] != '\t') { stub_passwd_new[i] = t[i]; i++; }
    stub_passwd_new[i] = 0;
    return stub_passwd_new;
}
double net_chat_is_text_cmd(const char *msg) {
    (void)msg;
    return 0;
}
const char *net_chat_text_cmd_text(const char *msg) {
    (void)msg;
    return "";
}
const char *net_chat_text_cmd_color(const char *msg) {
    (void)msg;
    return "";
}
void net_banner_send(const char *text, const char *color) {
    (void)text;
    (void)color;
}
void net_chat_trim(double keep) { (void)keep; }
int png_load(const char *name) {
    (void)name;
    return 0;
}
/* The raid payload lives in the repo assets; the harness reads the real file
 * so the parser is exercised against the shipped container. */
int ds_asset_read(const char *name, void *buf, int cap) {
    char path[256];
    FILE *f;
    size_t got;
    if (!name || !buf || cap <= 0)
        return -1;
    snprintf(path, sizeof(path), "assets/%s", name);
    f = fopen(path, "rb");
    if (!f)
        return -1;
    got = fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return (int)got;
}
double str_index_of(const char *hay, const char *needle) {
    (void)hay;
    (void)needle;
    return -1;
}
const char *str_sub(const char *s, double start, double len) {
    (void)s;
    (void)start;
    (void)len;
    return "";
}
/* Faithful little versions (the runtime ones are not linked into this
 * harness): trim + uppercase over rotating static buffers. */
const char *str_trim(const char *s) {
    static char bufs[4][128];
    static int slot;
    if (s == NULL) s = "";
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) n--;
    if (n > 127) n = 127;
    char *out = bufs[slot];
    slot = (slot + 1) % 4;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}
const char *str_upper(const char *s) {
    static char bufs[4][128];
    static int slot;
    if (s == NULL) s = "";
    char *out = bufs[slot];
    slot = (slot + 1) % 4;
    size_t i = 0;
    while (s[i] != '\0' && i < 127) {
        char c = s[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        out[i] = c;
        i++;
    }
    out[i] = '\0';
    return out;
}

static void assert_clean(const char *in, const char *want) {
    char out[CF_OUT_MAX];
    chat_clean_into(out, sizeof(out), in);
    if (strcmp(out, want) != 0) {
        fprintf(stderr, "chat filter: '%s' -> '%s', want '%s'\n", in, out, want);
        assert(0);
    }
}

/* Drag the delete knob with finger id: from the knob by a fraction of the track. */
static void swipe_delete_knob(double frac, double id) {
    double kx = acct_knob_x() + acct_knob_w() / 2;
    double ky = acct_slider_y() + acct_slider_h() / 2;
    double span = acct_w() - acct_knob_w() - 8;
    logic_touch(kx, ky, 0, id);
    press_points_clear();
    logic_touch(kx + span * frac * 0.5, ky, 2, id);
    logic_touch(kx + span * frac, ky, 2, id);
    logic_touch(kx + span * frac, ky, 1, id);
}

/* Deleting: with no password and no full swipe nothing goes to the server. */
static void test_account_delete(void) {
    int calls = stub_delete_calls;
        /* Swipe without a password: ask for the password, the knob slides back. */
    swipe_delete_knob(1.2, 60);
    assert(stub_delete_calls == calls && str_eq(acct_msg, tr_acct_need_pass()) == 1);
        /* The password is there but the knob was let go halfway - no deletion. */
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(0) + login_fh / 2, 0, 61);
    press_points_clear();
    assert(keyboard_visible() == 1 && acct_field == 0);
    keyboard_type("secret1");
    acct_tick();
    assert(strcmp(acct_buf[0], "secret1") == 0 && strcmp(acct_mask(0), "*******") == 0);
    acct_knob = 0;
    swipe_delete_knob(0.5, 62);
    assert(stub_delete_calls == calls && acct_knob > 0.3);
    double k = acct_knob;
    dt = 0.05;
    acct_tick();
    assert(acct_knob < k); /* springs back */
    acct_knob = 0;
        /* A plain tap on the knob does nothing either. */
    logic_touch(acct_knob_x() + acct_knob_w() / 2, acct_slider_y() + acct_slider_h() / 2, 0, 63);
    press_points_clear();
    logic_touch(acct_knob_x() + acct_knob_w() / 2, acct_slider_y() + acct_slider_h() / 2, 1, 63);
    assert(stub_delete_calls == calls);
        /* Full swipe with a password: delete with that password, the field is cleared. */
    swipe_delete_knob(1.2, 64);
    assert(stub_delete_calls == calls + 1 && strcmp(stub_delete_pass, "secret1") == 0);
    assert(acct_busy == 1 && acct_buf[0][0] == 0);
        /* Wrong password: a message, the player can try again. */
    stub_delete_status = 4;
    acct_tick();
    assert(acct_busy == 0 && acct_done == 0 && str_eq(acct_msg, tr_acct_wrong()) == 1);
    acct_buf[0][0] = 's';
    acct_buf[0][1] = 0;
    acct_knob = 0;
    swipe_delete_knob(1.2, 65);
    assert(stub_delete_calls == calls + 2);
    stub_delete_status = 2;
    acct_tick();
    assert(acct_busy == 0 && acct_done == 1 && str_eq(acct_msg, tr_delete_done()) == 1);
}

/* Advance only the transition clock, without polling login or gameplay. */
static void finish_transition(void) {
    double saved_dt = dt;
    dt = 0.05;
    for (int step = 0; step < 100 && t_dir != 0; step++) update_transition();
    dt = saved_dt;
    assert(t_dir == 0 && t_fade == 0 && t_action == NULL);
}

/* Account screen from the lobby: menu, password change, sign out. */
static void test_account_screen(void) {
    char saved_nick[24];
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    stub_login_status = 2;
    game_state = ST_LOBBY;
    t_dir = 0;
    docs_open = 0;
    acct_open = 0;
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 70);
    press_points_clear();
    /* The account opens through the screen transition, like every other
     * screen: nothing yet, the account at the end of the fade. */
    assert(acct_open == 0 && t_dir == 1 && acct_pending == 1);
    {
        double saved_dt = dt;
        dt = 0.1;
        for (int step = 0; step < 60 && acct_open == 0; step++) update_transition();
        dt = saved_dt;
    }
    assert(acct_open == ACCT_MENU);
    t_dir = 0;
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(0) + btn_h / 2, 0, 71);
    press_points_clear();
    assert(acct_open == ACCT_MENU && t_dir == 1);
    finish_transition();
    assert(acct_open == ACCT_PASS);
        /* Checks before the server: empty fields, mismatch, same password. */
    logic_touch(acct_x() + acct_w() / 2, acct_below_fields() + btn_h / 2, 0, 72);
    press_points_clear();
    assert(str_eq(acct_msg, tr_acct_need_pass()) == 1 && stub_pass_calls == 0);
    snprintf(acct_buf[0], ACCT_BUF, "%s", "old1");
    snprintf(acct_buf[1], ACCT_BUF, "%s", "new1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "new2");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_mismatch()) == 1 && stub_pass_calls == 0);
    snprintf(acct_buf[1], ACCT_BUF, "%s", "old1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "old1");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_same()) == 1 && stub_pass_calls == 0);
        /* The admin password is set in the code: changing it would cost admin rights. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(acct_buf[1], ACCT_BUF, "%s", "new1");
    snprintf(acct_buf[2], ACCT_BUF, "%s", "new1");
    acct_pass_try();
    assert(str_eq(acct_msg, tr_acct_admin()) == 1 && stub_pass_calls == 0);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
        /* Keyboard input: Enter moves to the next field. */
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(1) + login_fh / 2, 0, 73);
    press_points_clear();
    assert(acct_field == 1 && keyboard_visible() == 1);
    keyboard_type("new1");
    acct_tick();
    logic_touch(acct_x() + acct_w() / 2, acct_field_y(2) + login_fh / 2, 0, 74);
    press_points_clear();
    keyboard_type("new1");
    acct_tick();
    assert(strcmp(acct_buf[1], "new1") == 0 && strcmp(acct_buf[2], "new1") == 0);
    logic_touch(acct_x() + acct_w() / 2, acct_below_fields() + btn_h / 2, 0, 75);
    press_points_clear();
    assert(stub_pass_calls == 1 && strcmp(stub_pass_old, "old1") == 0 && strcmp(stub_pass_new, "new1") == 0);
    assert(acct_busy == 2 && acct_buf[0][0] == 0 && acct_buf[1][0] == 0);
    stub_pass_status = 4;
    acct_tick();
    assert(acct_busy == 0 && str_eq(acct_msg, tr_acct_wrong()) == 1);
    acct_busy = 2;
    stub_pass_status = 2;
    acct_tick();
    assert(acct_busy == 0 && str_eq(acct_msg, tr_acct_changed()) == 1);
        /* Back goes to the account menu, once more closes it. */
    assert(logic_back() == 1 && t_dir == 1 && acct_open == ACCT_PASS);
    finish_transition();
    assert(acct_open == ACCT_MENU);
        /* Sign out. */
    int logouts = stub_logout_calls;
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(2) + btn_h / 2, 0, 76);
    press_points_clear();
    assert(acct_open == ACCT_MENU && stub_logout_calls == logouts && t_dir == 1);
    finish_transition();
    assert(acct_open == 0 && stub_logout_calls == logouts + 1);
    /* Sign-out zeroes the local progress: cups, candies, classes, levels. */
    cups = 500;
    candies = 90;
    set_class_owned(CLASS_AZUM, 1);
    set_class_progress(CLASS_AZUM, 2, 3);
    player_class = CLASS_AZUM;
    progress_reset_on_signout();
    assert(cups == 0 && candies == 0);
    assert(class_owned_of(CLASS_AZUM) == 0 && player_class == CLASS_ORDINARY);
    assert(class_level_of(CLASS_AZUM) == 0 && class_unlocked_of(CLASS_AZUM) == 0);
    assert(achievement_mask == 0 && azum_revive_count == 0);
        /* Without a login the Account row leads to the login screen. */
    stub_login_status = 0;
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 77);
    press_points_clear();
    assert(acct_open == 0 && t_target == ST_LOGIN);
    t_dir = 0;
    t_target = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    puts("account: menu, password change checks, sign-out, delete needs password + full swipe right");
}

static void test_lobby_ball(void) {
    int saved_w = screen_w, saved_h = screen_h;
    double saved_state = game_state, saved_dt = dt;
    screen_w = 720;
    screen_h = 1280;
    game_state = ST_LOBBY;
    acct_open = 0;
    docs_open = warn_open = studio_open = 0;
    dt = 1.0 / 60.0;
    lobby_ball_initialized = 0;
    lobby_ball_pointer = -1;
    lobby_ball_vx = lobby_ball_vy = 0;
    lobby_ball_tick();
    assert(lobby_ball_initialized == 1);
    assert(lobby_ball_ceiling() == 0 && fabs(lobby_ball_min_y() - lobby_ball_r) < 1e-9);
    assert(fabs(lobby_ball_y - (lobby_ball_floor() - lobby_ball_r)) < 1e-9);

    double x = lobby_ball_x, y = lobby_ball_y;
    assert(lobby_ball_touch(x, y, 0, 91) == 1 && lobby_ball_pointer == 91);
    assert(lobby_ball_touch(x, y, 1, 91) == 1);
    assert(lobby_ball_phrase == 0 && lobby_ball_speech_t > 0);

    x = lobby_ball_x;
    y = lobby_ball_y;
    assert(lobby_ball_touch(x, y, 0, 93) == 1);
    assert(lobby_ball_touch(x + 45, y - 35, 2, 93) == 1);
    assert(fabs(lobby_ball_x - (x + 45)) < 1e-9);
    assert(lobby_ball_touch(x + 45, y - 35, 1, 93) == 1);
    assert(lobby_ball_vx > 1000 && lobby_ball_vy < -1000);
    double before_y = lobby_ball_y;
    dt = 0.05;
    lobby_ball_tick();
    assert(lobby_ball_y < before_y);

    /* An upward full-force throw hits the visible ceiling and rebounds. */
    lobby_ball_x = lobby_ball_default_x();
    lobby_ball_y = lobby_ball_min_y() + 2;
    lobby_ball_vy = -2200;
    int ceiling_bounced = 0;
    for (int i = 0; i < 24; i++) {
        lobby_ball_tick();
        assert(lobby_ball_y >= lobby_ball_min_y() - 1e-9);
        if (lobby_ball_vy > 0) ceiling_bounced = 1;
    }
    assert(ceiling_bounced == 1);

    lobby_ball_x = lobby_ball_default_x();
    lobby_ball_y = lobby_ball_max_y() - 4;
    lobby_ball_vy = 150;
    lobby_ball_tick();
    assert(lobby_ball_y <= lobby_ball_max_y() && lobby_ball_vy < 0);
    lobby_ball_y = lobby_ball_min_y();
    assert(lobby_ball_bubble_y() < 0); /* hide it instead of overlapping at the very top */
    lobby_ball_y = lobby_ball_max_y();
    assert(lobby_ball_bubble_y() + 39 <= lobby_ball_y - lobby_ball_r + 1e-9);

    /* The engine routes full pointer gestures from lobby to the physics toy. */
    t_dir = 0;
    admin_panel_open = 0;
    docs_open = warn_open = studio_open = 0;
    acct_open = 0;
    press_points_clear();
    lobby_ball_x = lobby_ball_default_x();
    lobby_ball_y = lobby_ball_floor() - lobby_ball_r;
    lobby_ball_vx = lobby_ball_vy = 0;
    x = lobby_ball_x;
    y = lobby_ball_y;
    logic_touch(x, y, 0, 94);
    logic_touch(x + 35, y - 28, 2, 94);
    logic_touch(x + 35, y - 28, 1, 94);
    assert(lobby_ball_pointer < 0 && lobby_ball_vx > 0 && lobby_ball_vy < 0);
    press_points_clear();
    adm_reset_log();
    lobby_ball_draw();
    for (int ci = 0; ci < adm_color_n; ci++) {
        assert(adm_colors[ci] != 0x77FFFFFFu); /* no glossy white spot */
        assert(adm_colors[ci] != 0x55302650u); /* no floating shadow */
        assert(adm_colors[ci] != 0x665D5D5Du); /* no visible lobby rails */
        assert(adm_colors[ci] != 0xFFFFFFFFu); /* dark speech bubble, no white UI chip */
    }
    lobby_ball_initialized = 0;
    lobby_ball_pointer = -1;
    lobby_ball_vx = lobby_ball_vy = 0;
    dt = saved_dt;
    game_state = saved_state;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("lobby ball: velocity-based throw, fixed face, ceiling/floor/side rebounds OK");
}

/* Gifts: two columns, the shop on the left fits whole, the collection on the
 * right scrolls smoothly. A card is one PNG plus a caption in its rarity
 * colour; cards have no buttons - a tap selects, the actions sit below the
 * columns. */
static void test_gifts_page(void) {
    double saved_w = screen_w, saved_h = screen_h;
    double saved_lang = language;
    screen_w = 720;
    screen_h = 1280;
    language = 1;
    memset(stub_gift_counts, 0, sizeof(stub_gift_counts));
    acct_open = ACCT_GIFTS;
    acct_gifts_page();
    assert(acct_gift_choice < 0 && acct_gift_own_row < 0);
    assert(str_eq(acct_gift_name(0), "Какашка") == 1);
    assert(str_eq(acct_gift_name(4), "Изумруд") == 1);
    assert(net_gift_types() == NET_GIFT_TYPE_COUNT);
    assert(net_gift_price(0) == 25 && net_gift_price(3) == 400 && net_gift_price(4) == 500);
        /* Selling pays back half the price, buying always costs more. */
    assert(net_gift_refund(0) == 12 && net_gift_refund(1) == 50);
    assert(net_gift_refund(2) == 125 && net_gift_refund(3) == 200 && net_gift_refund(4) == 250);
        /* Stickers are real PNGs, one picture per gift. */
    assert(str_eq(gift_tex(0), "gift_poop.png") == 1);
    assert(str_eq(gift_tex(1), "gift_cake.png") == 1);
    assert(str_eq(gift_tex(2), "gift_bear.png") == 1);
    assert(str_eq(gift_tex(3), "gift_rocket.png") == 1);
    assert(str_eq(gift_tex(4), "gift_emerald.png") == 1);
        /* Rarity: common white, rare blue, epic purple, exclusive special. */
    assert(gift_rarity(0) == 0 && gift_rarity(1) == 1 && gift_rarity(2) == 2);
    assert(gift_rarity(3) == 2 && gift_rarity(4) == 3);
    assert(str_eq(gift_rarity_name(0), "ОБЫЧНЫЙ") == 1);
    assert(str_eq(gift_rarity_name(1), "РЕДКИЙ") == 1);
    assert(str_eq(gift_rarity_name(2), "ЭПИЧЕСКИЙ") == 1);
    assert(str_eq(gift_rarity_name(4), "ЭКСКЛЮЗИВНЫЙ") == 1);
    assert((gift_rarity_color(0) & 0xFFFFFFu) == 0xFFFFFFu);
    assert((gift_rarity_color(1) & 0xFFFFFFu) == 0x5CC8FFu);
    assert((gift_rarity_color(2) & 0xFFFFFFu) == 0xC08CFFu);
        /* The columns stand side by side and do not overlap. */
    assert(acct_gift_x(0) + acct_gift_col_w() < acct_gift_x(1));
    assert(acct_gift_x(1) + acct_gift_col_w() <= acct_x() + acct_w() + 1e-9);
        /* Four catalogue cards fit; the remaining rows require shop scrolling. */
    for (double i = 0; i < 4; i++) {
        double cy = acct_gift_view_top() + i * acct_gift_step();
        assert(cy >= acct_gift_view_top());
        assert(cy + acct_gift_card_h() <= acct_gift_view_bottom() + 1e-9);
        assert(hit_acct_gift(acct_gift_x(0) + 10, cy + 5, i) == 1);
    }
    assert(acct_gift_shop_scroll_needed() == 1 && acct_gift_shop_scroll_max() > 0);
        /* Empty collection: no rows and no scrolling. */
    assert(acct_gift_own_total() == 0 && acct_gift_scroll_max() == 0);
    gift_tex_draws = 0;
    last_gift_tex = "";
    draw_acct_gifts();
        /* The visible shop viewport draws four catalogue pictures; lower rows scroll in. */
    assert(gift_tex_draws == 4);
    assert(str_eq(last_gift_tex, "gift_rocket.png") == 1);
        /* An empty collection says "no gifts yet" under the MY GIFTS header, in
         * the right column, not in the middle of the screen. */
    text_watch("Пока нет подарков");
    draw_acct_gifts();
    assert(stub_watch_hit == 1);
    assert(stub_watch_x >= acct_gift_x(1) - 1);
    assert(stub_watch_x <= acct_gift_x(1) + acct_gift_col_w());
    assert(stub_watch_y < acct_gift_view_top() + 30);
    text_watch("");

        /* Two poops are two separate cards, the emerald is the third. */
    stub_gift_counts[0] = 2;
    stub_gift_counts[4] = 1;
    assert(acct_gift_own_total() == 3);
    assert(acct_gift_own_id(0) == 0 && acct_gift_own_id(1) == 0);
    assert(acct_gift_own_id(2) == 4 && acct_gift_own_id(3) == -1);
    assert(acct_gift_shop_rows() == NET_GIFT_TYPE_COUNT);
    acct_gift_choice = 0;
    acct_gift_is_owned = 0;
    acct_gift_own_row = -1;
    adm_reset_log();
    draw_acct_gifts();
    assert(adm_has_text("Осмотреть") == 0);
    acct_gift_choice = 4;
    acct_gift_is_owned = 1;
    acct_gift_own_row = 2;
    adm_reset_log();
    draw_acct_gifts();
    assert(adm_has_text("Осмотреть") == 1);
    assert(hit_acct_gift_inspect(acct_x() + 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_send(acct_gift_action_x(1) + 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_sell(acct_gift_action_x(2) + 10, acct_below_fields() + 5) == 1);
    acct_gift_choice = -1;
    acct_gift_is_owned = 0;
    acct_gift_own_row = -1;
    gift_tex_draws = 0;
    rainbow_text_n = 0;
    draw_acct_gifts();
        /* Four visible shop cards and three owned ones, each with its own picture. */
    assert(gift_tex_draws == 7);
        /* The emerald caption is white: the colour lives on the stone itself. */
    int rainbow_emerald = 0;
    for (int i = 0; i < rainbow_text_n; i++)
        if (str_eq(rainbow_text[i], "Изумруд") == 1 || str_eq(rainbow_text[i], "ЭКСКЛЮЗИВНЫЙ") == 1)
            rainbow_emerald = 1;
    assert(rainbow_emerald == 0);

        /* A shop card: selection and "Look | Buy" under the columns. */
    double y = acct_gift_view_top() + 1 * acct_gift_step() + acct_gift_card_h() / 2;
    assert(touch_acct_gift_list(acct_gift_x(0) + 20, y, 0, 11) == 1);
    assert(touch_acct_gift_list(acct_gift_x(0) + 20, y, 1, 11) == 1);
    assert(acct_gift_choice == 1 && acct_gift_is_owned == 0);
    assert(hit_acct_gift_inspect(acct_x() + 10, acct_below_fields() + 5) == 0);
    assert(hit_acct_gift_buy(acct_x() + 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_buy(acct_x() + acct_w() - 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_send(acct_x() + 10, acct_below_fields() + 5) == 0);
    assert(hit_acct_gift_sell(acct_x() + acct_w() - 10, acct_below_fields() + 5) == 0);

        /* A tap on an owned card only selects it: the buttons live in the footer. */
    double oy = acct_gift_view_top() + 6;
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, oy, 0, 12) == 1);
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, oy + 2, 1, 12) == 1);
    assert(acct_gift_own_sel() == 0 && acct_gift_sel_id() == 0 && acct_gift_picked() == 1);
    assert(acct_gift_drag < 0 && acct_gift_busy == 0);
    assert(hit_acct_gift_buy(acct_x() + 10, acct_below_fields() + 5) == 0);
    assert(hit_acct_gift_inspect(acct_x() + 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_send(acct_x() + acct_gift_inspect_w() + ACCT_GIFT_COL_GAP + 10, acct_below_fields() + 5) == 1);
    assert(hit_acct_gift_sell(acct_x() + 2 * (acct_gift_inspect_w() + ACCT_GIFT_COL_GAP) + 10, acct_below_fields() + 5) == 1);
    assert(acct_x() + 3 * acct_gift_inspect_w() + 2 * ACCT_GIFT_COL_GAP <= acct_x() + acct_w() + 1e-9);
        /* Sell in the footer sells the selected card. */
    acct_gift_sell();
    assert(acct_gift_busy == 1 && acct_gift_busy_mode == 3);
    acct_gift_busy = 0;
    acct_gift_busy_mode = 0;
    acct_msg = "";

        /* Buying and selling take the balance from the database: even if the
         * local cups somehow got zeroed, the server number is not overwritten
         * with a zero. */
    cups = 0;
    acct_gift_choice = 4;
    acct_gift_is_owned = 0;
    acct_gift_busy = 1;
    acct_gift_busy_mode = 2;
    stub_gift_cups = 900;
    stub_gift_status = 2;
    acct_poll_gifts();
    assert(acct_gift_busy == 0 && cups == 900);
    cups = 0;
    acct_gift_choice = 0;
    acct_gift_is_owned = 1;
    acct_gift_own_row = 0;
    acct_gift_busy = 1;
    acct_gift_busy_mode = 3;
    stub_gift_cups = 137;
    stub_gift_status = 2;
    acct_poll_gifts();
    assert(acct_gift_busy == 0 && cups == 137 && acct_gift_is_owned == 0);
    acct_msg = "";

        /* The admin buys for free: buying starts with zero cups, and the balance
         * after the purchase is the untouched server one, no price deducted. */
    {
        char saved_nick[24];
        snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
        assert(admin_playing() == 1);
        cups = 0;
        acct_gift_choice = 2; /* teddy, 250 cups */
        acct_gift_is_owned = 0;
        acct_gift_buy();
        assert(acct_gift_busy == 1 && acct_gift_busy_mode == 2);
        stub_gift_cups = 55; /* server balance unchanged */
        stub_gift_status = 2;
        acct_poll_gifts();
        assert(acct_gift_busy == 0 && cups == 55);
        acct_msg = "";
        net_gift_count_add(2, -1); /* revert cached teddy count */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    }

        /* Selling removes only the sticker that was sold: the second poop and the
         * emerald stay. The list used to be wiped whole, so it looked like the
         * emerald went with the poop. */
    stub_gift_counts[0] = 2;
    stub_gift_counts[4] = 1;
    acct_gift_choice = 0;
    acct_gift_is_owned = 1;
    acct_gift_own_row = 0;
    acct_gift_busy = 1;
    acct_gift_busy_mode = 3;
    stub_gift_cups = 137;
    stub_gift_status = 2;
    acct_poll_gifts();
    assert(stub_gift_counts[0] == 1 && stub_gift_counts[4] == 1);
    assert(acct_gift_own_total() == 2);
    assert(acct_gift_own_id(0) == 0 && acct_gift_own_id(1) == 4);
    acct_msg = "";

        /* A refused action (another one is running) must not leave the page stuck
         * on "Selling...". */
    stub_gift_action_ok = 0;
    stub_gift_counts[0] = 1;
    acct_gift_choice = 0;
    acct_gift_is_owned = 1;
    acct_gift_own_row = 0;
    acct_gift_sell();
    assert(acct_gift_busy == 0 && acct_gift_busy_mode == 3);
        /* The sticker of a refused request must not move into the next action:
         * otherwise selling the wrong sticker would pass silently. */
    assert(acct_gift_acted == -1);
    stub_gift_action_ok = 1;
    acct_msg = "";

        /* A purchase adds exactly one sticker and leaves the others alone. */
    acct_gift_choice = 4;
    acct_gift_is_owned = 0;
    acct_gift_busy = 1;
    acct_gift_busy_mode = 2;
    stub_gift_cups = 900;
    stub_gift_status = 2;
    acct_poll_gifts();
    assert(stub_gift_counts[4] == 2 && stub_gift_counts[0] == 1);
    acct_msg = "";

        /* While a request is in flight the selection can change: the sticker that
         * was pressed is the one applied. */
    acct_gift_choice = 3;
    acct_gift_is_owned = 0;
    acct_gift_buy();
    acct_gift_choice = 0; /* selection changed while request in flight */
    stub_gift_cups = 640;
    stub_gift_status = 2;
    acct_poll_gifts();
    assert(stub_gift_counts[3] == 1);
    assert(acct_gift_acted == -1);
    net_gift_count_add(3, -1);
    acct_msg = "";

    /* Both gift columns scroll independently; compact height makes the five-row
     * shop list scroll too, while the long collection keeps its inertia checks. */
    double saved_scroll_screen_h = screen_h;
    screen_h = 720;
    stub_gift_counts[0] = 15;
    assert(acct_gift_scroll_needed() == 1 && acct_gift_scroll_max() > 0);
    acct_gift_scroll = 0;
    acct_gift_vel = 0;
    double sy = acct_gift_view_top() + 40;
    assert(acct_gift_shop_scroll_needed() == 1);
    double shop_x = acct_gift_panel_x(0) + acct_gift_panel_w() - 3;
    assert(touch_acct_gift_list(shop_x, sy, 0, 20) == 1);
    assert(touch_acct_gift_list(shop_x, sy - 45, 2, 20) == 1);
    assert(fabs(acct_gift_shop_scroll - 45) < 1e-9 && acct_gift_scroll == 0);
    assert(touch_acct_gift_list(shop_x, sy - 45, 1, 20) == 1);
    acct_gift_shop_scroll = 0;
    acct_gift_shop_vel = 0;
    double sx = acct_gift_panel_x(1) + acct_gift_panel_w() - 3;
    /* Right-margin thumb swipe (outside center column) scrolls smoothly. */
    assert(touch_acct_gift_list(sx, sy, 0, 14) == 1);
    /* Secondary pointer cannot hijack an active collection drag. */
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 50, 0, 19) == 1);
    assert(acct_gift_drag == 14);
    assert(touch_acct_gift_list(sx, sy - 60, 2, 14) == 1);
    /* Duplicate zero-delta move does not wipe velocity. */
    assert(touch_acct_gift_list(sx, sy - 60, 2, 14) == 1);
    assert(fabs(acct_gift_scroll - 60) < 1e-9 && acct_gift_shop_scroll == 0);
    assert(touch_acct_gift_list(sx, sy - 60, 1, 14) == 1);
    assert(acct_gift_drag < 0);
    assert(acct_gift_vel > 100 && fabs(acct_gift_vel) <= 2200 + 1e-9);
    assert(fabs(acct_gift_scroll - 60) < 1e-9);
    double before = acct_gift_scroll;
    acct_gift_scroll_tick();
    double moved = acct_gift_scroll - before;
    assert(moved > 0 && moved <= 2200 * dt + 1e-9);
    assert(fabs(fmod(acct_gift_scroll, acct_gift_step())) > 1e-6);
    /* Thumb-lift centroid roll in opposite direction never reverses flick. */
    acct_gift_scroll = 0;
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 160, 0, 17) == 1);
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 110, 2, 17) == 1);
    acct_gift_scroll_tick();
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 55, 2, 17) == 1);
    acct_gift_scroll_tick();
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 72, 2, 17) == 1);
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy + 72, 1, 17) == 1);
    assert(acct_gift_vel > 200);
    /* Holding finger still before release stops velocity cleanly at zero. */
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy, 0, 16) == 1);
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy - 80, 2, 16) == 1);
    for (int i = 0; i < 4; i++) acct_gift_scroll_tick();
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, sy - 80, 1, 16) == 1);
    assert(acct_gift_vel == 0);
    /* Scrolling never leaves the list. */
    acct_gift_scroll = 0;
    acct_gift_vel = -500;
    for (int i = 0; i < 200; i++) acct_gift_scroll_tick();
    assert(acct_gift_scroll == 0);
    acct_gift_scroll = acct_gift_scroll_max();
    acct_gift_vel = 500;
    for (int i = 0; i < 200; i++) acct_gift_scroll_tick();
    assert(fabs(acct_gift_scroll - acct_gift_scroll_max()) < 1e-9);
    /* After scrolling a tap picks the row under the finger. */
    acct_gift_scroll = acct_gift_step() * 3;
    double ty = acct_gift_view_top() + acct_gift_step() / 2;
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, ty, 0, 15) == 1);
    assert(touch_acct_gift_list(acct_gift_x(1) + 20, ty, 1, 15) == 1);
    assert(acct_gift_own_sel() == 3 && acct_gift_sel_id() == 0);
    screen_h = saved_scroll_screen_h;

    stub_scissor_calls = 0;
    stub_scissor_active = 0;
    draw_acct_gifts();
    assert(stub_scissor_calls >= 2 && stub_scissor_active == 0);
    acct_open = 0;
    acct_gift_choice = -1;
    acct_gift_own_row = -1;
    acct_gift_is_owned = 0;
    acct_gift_scroll = 0;
    acct_gift_vel = 0;
    acct_gift_drag = -1;
    acct_gift_busy = 0;
    memset(stub_gift_counts, 0, sizeof(stub_gift_counts));
    language = saved_lang;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("gifts: PNG stickers, rarity colours, footer actions and a soft scroll");
}

/* Legal part: chat filter, consent at login, the lobby question, the
 * documents window over the warning and account deletion. */
static void test_legal(void) {
    double saved_lang = language;
    double saved_w = screen_w, saved_h = screen_h;
    screen_w = 1280;
    screen_h = 720;
    language = 1;

        /* Swearing, its latin lookalikes, noisy spellings and links become asterisks. */
    assert_clean("привет хуй", "привет ***");
    assert_clean("Пиздец!", "***");
    assert_clean("cyka blyat", "*** ***");
    assert_clean("huy", "***");
    assert_clean("х.у.й", "***");
    assert_clean("xyй", "***");
    assert_clean("заебал уже", "*** уже");
    assert_clean("fuck you", "*** you");
    assert_clean("заходи на site.ru", "заходи на ***");
    assert_clean("https://x.com/a", "***");
    assert_clean("t.me/chan", "***");
        /* Ordinary words that look like banned ones are left alone. */
    assert_clean("себе хлеб небо тебе веб вебинар психуй застрахуй", "себе хлеб небо тебе веб вебинар психуй застрахуй");
    assert_clean("спидран мандарин сукно употреблять упорный оскорблять", "спидран мандарин сукно употреблять упорный оскорблять");
    assert_clean("ebuC web ebay hello pizza 12:00 1.5", "ebuC web ebay hello pizza 12:00 1.5");
    assert_clean("ebat", "***");
    assert(nick_blocked("huy228") == 1);
    assert(nick_blocked("Dimasi4ek229") == 0);
    assert(nick_blocked("ebuc_fan") == 0);
    assert(nick_blocked("tester") == 0);

        /* Login: the consent box must be ticked, and it starts unticked. */
    stub_pd_ver = 0;
    stub_login_status = 0;
    game_state = ST_LOGIN;
    t_dir = 0;
    warn_open = 0;
    studio_open = 0;
    login_open();
    assert(login_consent == 0);
    login_nick = "tester";
    login_pass = "secret1";
    login_do();
    assert(login_status == 8 && str_eq(login_error_text(), tr_consent_needed()) == 1);
    login_nick = "huy";
    login_do();
    assert(login_status == 9);
    login_nick = "tester";
    login_status = 8;
    logic_touch(login_consent_x() + 10, login_consent_y() + 10, 0, 40);
    press_points_clear();
    assert(login_consent == 1 && login_status == 0);
    int uploads = stub_pd_uploads;
    login_do();
    assert(login_status == 0 && stub_pd_ver == LEGAL_DOCS_VERSION && stub_pd_uploads == uploads + 1);
    t_dir = 0;
    t_target = 0;
        /* The link under the box opens the documents and the nick is kept. */
    logic_touch(center_x(login_more_w()) + login_more_w() / 2, login_more_y() + btn_h * 0.4, 0, 41);
    press_points_clear();
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && docs_page == LEGAL_PAGE_CONSENT - 2 && str_eq(login_nick, "tester") == 1);
    docs_close();
    login_open();
    /* The checkbox resets on every entry, even after consent was given. */
    assert(login_consent == 0);

        /* A player who signed in before consent existed is asked in the lobby, once. */
    stub_pd_ver = 0;
    stub_login_status = 2;
    game_state = ST_LOBBY;
    docs_asked = 0;
    docs_tick();
    assert(docs_open == 1 && docs_ask == 1 && docs_page == LEGAL_PAGE_CONSENT);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_row_y() + btn_h / 2, 0, 42);
    press_points_clear();
    assert(docs_open == 1 && t_dir == 1);
    finish_transition();
    assert(docs_open == 0 && stub_pd_ver == LEGAL_DOCS_VERSION);
    docs_tick();
    assert(docs_open == 0);
        /* Refusal: the player signs out. */
    stub_pd_ver = 0;
    docs_show(LEGAL_PAGE_CONSENT, 1);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_refuse_y() + btn_h / 2, 0, 43);
    press_points_clear();
    finish_transition();
    assert(docs_open == 0 && stub_logout_calls == 1 && stub_login_status == 0);
        /* Online does not open without consent - the question is shown instead. */
    stub_login_status = 2;
    open_online();
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && docs_ask == 1 && t_dir == 0);
    docs_close();
    stub_pd_ver = LEGAL_DOCS_VERSION;

        /* Paging; the delete button opens the account screen over the documents. */
    docs_show(0, 0);
    assert(hit_docs_prev(docs_prev_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2) == 0);
    logic_touch(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0, 44);
    press_points_clear();
    assert(docs_page == 0 && t_dir == 1);
    finish_transition();
    assert(docs_page == 1);
    docs_show(LEGAL_PAGES - 1, 0);   /* the license page is last now */
    assert(hit_docs_next(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2) == 0);
    docs_show(LEGAL_PAGE_RIGHTS, 0);
    logic_touch(docs_mid_x() + docs_mid_w() / 2, docs_row_y() + btn_h / 2, 0, 45);
    press_points_clear();
    finish_transition();
    assert(acct_open == ACCT_DELETE && docs_open == 1 && stub_delete_calls == 0);
    test_account_delete();
        /* Back closes the account screen and leaves the documents open. */
    assert(logic_back() == 1 && t_dir == 1 && acct_open == ACCT_DELETE);
    finish_transition();
    assert(acct_open == 0 && docs_open == 1);
    docs_close();
        /* Without a login there is no delete button. */
    stub_login_status = 0;
    docs_show(LEGAL_PAGE_RIGHTS, 0);
    assert(docs_has_mid() == 0);
    docs_close();

        /* Start-up warning: Documents open a window on top of it. */
    warn_open = 1;
    warn_closing = 0;
    logic_touch(warn_docs_x() + warn_docs_w() / 2, warn_btn_y() + btn_h / 2, 0, 47);
    press_points_clear();
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && warn_open == 1);
    assert(logic_back() == 1 && docs_open == 1);
    finish_transition();
    assert(docs_open == 0);
    warn_open = 0;

    test_account_screen();
    stub_login_status = 2;

        /* Character descriptions are original phrases and are not translated; HP is kept. */
    language = 1;
    assert(str_eq(tr_class_ordinary_desc(), "I was the first") == 1);
    assert(strstr(tr_achievement_last_hp_desc(), "HP") != NULL);

    game_state = ST_LOBBY;
    language = saved_lang;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("legal: chat filter, sign-in consent, lobby consent ask, documents window, account deletion");
}

/* Game start: the epilepsy warning, then the lobby right away. The old
 * "tap to start" screen is gone, so the lobby is live from the first touch. */
static void test_start_gate(void) {
    game_reset();
    game_state = ST_LOBBY;
    dt = 0.1;

        /* Every launch starts with the warning and nothing covers it. */
    stub_legal_ts = 0;
    start_gate_init();
    assert(warn_open == 1 && warn_closing == 0 && warn_a == 1);
        /* A touch on the warning does not fall through to the lobby. */
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(1) + btn_h / 2, 0, 71);
    assert(t_dir == 0);
        /* The accept button waits until the warning is read, then slides it away
         * and the lobby opens without a curtain. */
    int i = 0;
    while (i < 40 && warn_ready < 1) {
        update_warning();
        i = i + 1;
    }
    assert(warn_ready >= 1);
    legal_accept();
    while (i < 120 && warn_open == 1) {
        update_warning();
        i = i + 1;
    }
    assert(warn_open == 0 && warn_a == 0 && game_state == ST_LOBBY);
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(1) + btn_h / 2, 0, 72);
    assert(t_dir == 1 && t_target == ST_CLASSES);
    t_dir = 0;
    t_target = 0;
    t_fade = 0;
    press_points_clear();
    stub_legal_ts = 0;
    puts("start gate: epilepsy warning only, the lobby is live right after it");
}

/* Admin panel: closing it any way also hides the keyboard. It used to stay
 * on top of the lobby and eat touches after the panel closed. */
static void test_admin_keyboard(void) {
    double saved_stat = game_state;
    double saved_open = admin_panel_open;
    double saved_dir = t_dir;
    t_dir = 0;
    game_state = ST_LOBBY;

        /* The Android back button. */
    admin_panel_open = 1;
    admin_tab = 1;
    keyboard_show();
    assert(keyboard_visible() == 1);
    logic_back();
    assert(admin_panel_open == 0 && keyboard_visible() == 0);

        /* The cross in the panel header. */
    admin_panel_open = 1;
    keyboard_show();
    admin_panel_close();
    assert(admin_panel_open == 0 && keyboard_visible() == 0);

        /* Switching to the Players tab must not leave the gift field keyboard up. */
    admin_panel_open = 1;
    admin_tab = 1;
    keyboard_show();
    admin_panel_press(admin_panel_x() + 4, admin_content_top() + 4);
    assert(admin_tab == 0 && keyboard_visible() == 0);

    admin_panel_open = saved_open;
    game_state = saved_stat;
    t_dir = saved_dir;
    keyboard_hide();
    puts("admin panel: every way of closing it also hides the keyboard");
}

/* Promo 2027: the success text is a private copy, not a pointer into the
 * string pool, so after a few pool ticks it does not turn into garbage. */
static void test_promo_message(void) {
    game_state = ST_PROMO;
    language = 0;
    stub_login_status = 2;
    stub_promo2027_used = 0;
    promo_focus = 0;
    promo_input = "2027";
    double cups_before = cups;
    promo_submit();
    assert(cups == cups_before + 27 && stub_promo2027_used == 1);
    assert(promo_msg_t > 0);
    for (int i = 0; i < 4; i++) {
        ds_string_pool_tick();
        (void)ds_concat("frame noise ", ds_num_to_string((double)i));
    }
    assert(str_eq(promo_msg_text, "+27 cups!") == 1);
    /* Second submit says "already redeemed" and also survives the pool. */
    promo_submit();
    for (int i = 0; i < 4; i++)
        ds_string_pool_tick();
    assert(str_eq(promo_msg_text, "Promo code already redeemed") == 1);
    promo_msg_t = 0;
    stub_login_status = 0;
    game_state = ST_LOBBY;
    puts("promo: 2027 message survives the string pool, no garbage or ? glyph");
}

static void test_azum_buff(void) {
    game_reset();
    assert(azum_buff_active == 0);
    assert(strcmp(tr_class_azum(), "Азум") == 0);
    player_class = CLASS_AZUM;
    reset_battle();
    enemy_class = CLASS_AZUM;
    enemy->max_hp = player_max_hp_for(CLASS_AZUM);
    enemy->hp = enemy->max_hp / 2;
    player->hp = player->max_hp / 2;
    stub_azum_buff = 1;
    update_azum_buff();
    assert(fabs(player->max_hp - azum_hp * 1.10 * round_hp_mult()) < 1e-9);
    assert(fabs(player->hp / player->max_hp - 0.5) < 1e-9);
    assert(fabs(enemy->max_hp - azum_hp * 1.10 * round_hp_mult()) < 1e-9);
    assert(fabs(enemy->hp / enemy->max_hp - 0.5) < 1e-9);
    assert(fabs(class_punch_damage_for(CLASS_AZUM, 0) - punch_damage * 1.10) < 1e-9);
    assert(fabs(azum_dash_damage_for() - azum_dash_damage * 1.10) < 1e-9);
    assert(fabs(enemy_azum_dash_damage_for() - enemy_dash_damage * 1.10) < 1e-9);
    assert(player_max_hp_for(CLASS_ORDINARY) == 20 * round_hp_mult());
    assert(class_punch_damage_for(CLASS_SANTA, 0) == santa_damage);
    assert(strcmp(tr_class_azum(), "@z&m") == 0);
    assert(strcmp(tr_class_azum_desc(), "Суббота-17") == 0);
    language = 0;
    assert(strcmp(tr_class_azum_desc(), "Saturday-17") == 0);
    for (int i = 0; i < 2; i++) {
        player->hp = 0;
        enemy->hp = 0;
        assert(try_revive() == 1 && enemy_try_revive() == 1);
        assert(player->hp == player->max_hp && enemy->hp == enemy->max_hp);
    }
    player->hp = 0;
    enemy->hp = 0;
    assert(try_revive() == 0 && enemy_try_revive() == 0);
    assert(azum_revived == 2 && enemy_revived == 2);
    stub_azum_buff = 0;
    update_azum_buff();
    assert(fabs(player->max_hp - azum_hp * round_hp_mult()) < 1e-9);
    assert(fabs(enemy->max_hp - azum_hp * round_hp_mult()) < 1e-9);
    assert(strcmp(tr_class_azum(), "Azum") == 0);
    assert(strcmp(tr_class_azum_desc(), "Lived once, buried twice...") == 0);
    assert(class_punch_damage_for(CLASS_AZUM, 0) == punch_damage);
    assert(azum_dash_damage_for() == azum_dash_damage);
    /* Disabling the flag never grants another revive after two were spent. */
    assert(try_revive() == 0 && enemy_try_revive() == 0);
    azum_revived = 0;
    enemy_revived = 0;
    assert(try_revive() == 1 && enemy_try_revive() == 1);
    player->hp = 0;
    enemy->hp = 0;
    assert(try_revive() == 0 && enemy_try_revive() == 0);

    /* FlashSale: off keeps full prices, on cuts EVERY price by 10%
     * (rounded to whole units), and buying charges the discounted price. */
    assert(flash_sale_active == 0);
    assert(class_cost_of(CLASS_AZUM) == azum_cost);
    assert(level_cost(1) == level_base_cost);
    stub_flash_sale = 1;
    update_flash_sale();
    assert(flash_sale_active == 1);
    assert(class_cost_of(CLASS_AZUM) == 77);   /* 85 - 10% -> 76.5 -> 77 */
    assert(class_cost_of(CLASS_SANTA) == 113); /* 125 candies - 10% -> 112.5 -> 113 */
    assert(class_cost_of(CLASS_ASTRA) == 158); /* 175 - 10% -> 157.5 -> 158 */
    assert(level_cost(1) == 45);              /* 50 - 10% */
    assert(level_cost(3) == 81);              /* 90 - 10% */
    assert(class_cost_of(CLASS_ORDINARY) == 0);
    cups = 77;
    pick_class(CLASS_AZUM);
    assert(class_owned_of(CLASS_AZUM) == 1 && cups == 0);
    stub_flash_sale = 0;
    update_flash_sale();
    assert(flash_sale_active == 0 && class_cost_of(CLASS_EBUC) == ebuc_cost);
    assert(level_cost(1) == level_base_cost);
    puts("flash sale: -10% on all prices and purchase OK");
}

/* CodeCracker ult: nothing circles the cube - no disc, no ring, no arcs, no
 * rim of dots; the sparks are smooth. */
static void test_codecracker_fx(void) {
    fx_ring_count = 0;
    fx_big_circle_count = 0;
    fx_orbit_lines = 0;
    fx_center_x = 200;
    fx_center_y = 200;
    draw_codecracker_fx_at(200, 200, 0.3, 0.7);
    assert(fx_ring_count == 0);
    assert(fx_big_circle_count == 0);
        /* No stroke runs around the cube: the ring of arcs starts exactly like that.
         * The sparks fly backwards and never form such an outline. */
    assert(fx_orbit_lines == 0);

    /* A smooth star needs many more strokes than its diameter in pixels:
     * the old pixel step produced at most ~2*r strokes. */
    double r = stat_star_r();
    star_line_count = 0;
    draw_star(0, 0, r, 0xFFF7C948);
    assert(star_line_count > (int)(3 * r));
    puts("codecracker fx: no halo circle in the ult; stars drawn smooth, not pixelated");
}

/* Battle FPS is counted from the real frame length, not from the clamped dt. */
static void test_fps_meter(void) {
    int i;
    fps_acc = 0;
    fps_frames = 0;
    fps_shown = 0;
    stub_frame_seconds = 1.0 / 30.0;
    i = 0;
    while (i < 16) {
        draw_fps_meter();
        i = i + 1;
    }
    assert(fps_shown == 30);

        /* Bottom right, same small font: the caption sits in the corner. */
    int saved_w = screen_w;
    int saved_h = screen_h;
    screen_w = 1280;
    screen_h = 720;
    fps_acc = 0;
    fps_frames = 0;
    fps_shown = 0;
    stub_frame_seconds = 1.0 / 60.0;
    i = 0;
    while (i < 31) {
        draw_fps_meter();
        i = i + 1;
    }
    assert(fps_shown == 60);
    assert(stub_text_x > screen_w * 0.9f && stub_text_y > screen_h * 0.9f);
    assert(stub_text_scale == (float)FPS_SCALE);
    screen_w = saved_w;
    screen_h = saved_h;

        /* 8 fps: dt is clamped to 0.1 s, which would read as 10. */
    fps_acc = 0;
    fps_frames = 0;
    fps_shown = 0;
    stub_frame_seconds = 1.0 / 8.0;
    i = 0;
    while (i < 5) {
        draw_fps_meter();
        i = i + 1;
    }
    assert(fps_shown == 8);
        /* A minimised app is not "0 FPS": the window simply starts over. */
    fps_acc = 0;
    fps_frames = 0;
    fps_shown = 0;
    stub_frame_seconds = 30.0;
    draw_fps_meter();
    assert(fps_shown == 0);
    stub_frame_seconds = 0;
    puts("fps meter: real frame times, 8 fps stays 8, a pause is not a slow frame");
}

static int stub_roomlist_status;
static int stub_roomlist_count;
static char stub_room_names[32][25];
static int stub_prep_status;
static char stub_prep_name[25];
static char stub_prep_pass[64];
static int stub_prep_create;
static int stub_prep_calls;
void net_roomlist_fetch(const char *url) { (void)url; stub_roomlist_status = 2; }
double net_roomlist_status(void) { return stub_roomlist_status; }
double net_roomlist_count(void) { return stub_roomlist_count; }
const char *net_roomlist_name(double idx) {
    int i = (int)idx;
    return (i >= 0 && i < stub_roomlist_count) ? stub_room_names[i] : "";
}
void net_room_prepare(const char *url, const char *name, const char *pass, double create) {
    (void)url;
    snprintf(stub_prep_name, sizeof(stub_prep_name), "%s", name ? name : "");
    snprintf(stub_prep_pass, sizeof(stub_prep_pass), "%s", pass ? pass : "");
    stub_prep_create = create ? 1 : 0;
    stub_prep_calls++;
    stub_prep_status = 1;
}
double net_room_prep_status(void) { return stub_prep_status; }
static char stub_current_room[25];
const char *net_current_room(void) { return stub_current_room; }
void net_room_prep_reset(void) { stub_prep_status = 0; }

/* The mov eax, 0x229 achievement and the password rooms screen. */

static void test_dice_alone_and_bubbles(void) {
    /* Dice event: prizes, admin-only cubes, throw + collect. */
    assert(dice_prize_of(DICE_CUBE_ORDINARY) == 2);
    assert(dice_prize_of(DICE_CUBE_LEGENDARY) == 95);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "x");
    stub_login_status = 2;
    astra_ability_refresh();
    assert(dev_access() == 0);
    srand(12345);
    double legendary = 0;
    double saw_santa = 0;
    double saw_cc = 0;
    double i = 0;
    while (i < 30000) {
        double f = dice_roll_cube();
        /* Every class rolls for everyone, admins included or not. */
        if (f == DICE_CUBE_SANTA) saw_santa = 1;
        if (f == DICE_CUBE_CODECRACKER) saw_cc = 1;
        if (f == DICE_CUBE_LEGENDARY) legendary = legendary + 1;
        i = i + 1;
    }
    assert(legendary > 0 && legendary < 80); /* 0.08% of 30000 = ~24 */
    assert(saw_santa == 1 && saw_cc == 1);

    screen_w = 800;
    screen_h = 600;
    event_mode = 4;
    dice_event_reset();
    dt = 0.7;
    update_dice_event(); /* first cube is thrown */
    assert(arr_get(dice_cubes, 0) == 1);
    double cube = arr_get(dice_cubes, 1);
    arr_set(dice_cubes, 2, dice_flight); /* landed */
    player->x = 300;
    player->y = 300;
    player->size = 50;
    arr_set(dice_cubes, 5, 300);
    arr_set(dice_cubes, 6, 300);
    double candies_before = candies;
    update_dice_event();
    assert(candies == candies_before + dice_prize_of(cube));
    assert(arr_get(dice_cubes, 0) == 0);
    event_mode = 0;
    dt = 1.0; /* the cube round survives a short blink, then stops */
    update_dice_event();
    assert(dice_active == 0);

    /* Alone in a room: 40 s of waiting, then out to the room list. */
    stub_net_slot = 0;
    double s = 0;
    while (s < 4) { stub_online[(int)s] = 0; s = s + 1; }
    s = 0;
    while (s < max_players * remote_fields) { arr_set(remotes, s, 0); s = s + 1; }
    online_ready = 1;
    finished = 0;
    game_state = ST_ONLINE;
    match_seen_foe = 0;
    foes_gone_t = 0;
    alone_t = 0;
    dt = 1;
    t_dir = 0;
    i = 0;
    while (i < 39) { check_online_finish(); i = i + 1; }
    assert(alone_t == 39 && t_dir == 0); /* still waiting, not kicked */
    check_online_finish();
    assert(t_target == ST_ROOMS && t_dir != 0); /* 40 s: out */
    t_dir = 0;
    alone_t = 5;
    arr_set(remotes, remote_fields + 5, 1); /* someone joins alive */
    check_online_finish();
    assert(alone_t == 0);
    arr_set(remotes, remote_fields + 5, 0);
    online_ready = 0;
    game_state = ST_LOBBY;

    /* Admin passwd command: resets a normal account, never an admin. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "test");
    stub_login_status = 2;
    stub_admin_reset_calls = 0;
    assert(admin_handle("passwd bob secret1") == 1);
    assert(stub_admin_reset_calls == 1);
    assert(strcmp(stub_admin_reset_nick, "bob") == 0);
    assert(strcmp(stub_admin_reset_new, "secret1") == 0);
    assert(admin_handle("passwd Dimasi4ek229 secret1") == 1);
    assert(stub_admin_reset_calls == 1); /* protected admin account */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "x");

    /* Chat bubbles: a second message must replace the bubble even though
     * net_chat_key() reuses one static buffer (the old aliasing bug). */
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "%s", "bob");
    stub_chat_count = 1;
    snprintf(stub_chat_keys[0], sizeof(stub_chat_keys[0]), "%s", "k1");
    snprintf(stub_chat_uids[0], sizeof(stub_chat_uids[0]), "%s", "bob");
    snprintf(stub_chat_texts[0], sizeof(stub_chat_texts[0]), "%s", "first");
    chat_bubbles_reset();
    dt = 0.05;
    chat_bubbles_watch();
    assert(strcmp(bubble_msg(1), "first") == 0);
    stub_chat_count = 2;
    snprintf(stub_chat_keys[1], sizeof(stub_chat_keys[1]), "%s", "k2");
    snprintf(stub_chat_uids[1], sizeof(stub_chat_uids[1]), "%s", "bob");
    snprintf(stub_chat_texts[1], sizeof(stub_chat_texts[1]), "%s", "second");
    chat_bubbles_watch();
    assert(strcmp(bubble_msg(1), "second") == 0);
    stub_chat_count = 0;
    puts("dice event + alone wait + chat bubbles: OK");
}

/* Events: 2 - plates, 3 - Santa, 4 - cube. "event 3" used to start the same
 * plates, and the database rule allowed only 0..3, so the cube write was
 * rejected and the poller kept returning the previous event. */
static void test_battle_events(void) {
    double saved_w = screen_w, saved_h = screen_h;
    screen_w = 1280;
    screen_h = 720;
    game_reset();
    stub_net_slot = 0;
    player_class = CLASS_ORDINARY;
    reset_battle();

        /* Azum: the full 20 HP pool for both the player and the bot. */
    assert(fabs(azum_hp - 20.0) < 1e-9);
    assert(fabs(player_max_hp_for(CLASS_AZUM) - 20.0 * round_hp_mult()) < 1e-9);

        /* Admin commands pick three DIFFERENT events. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "test");
    stub_login_status = 2;
    stub_event_mode = 0;
    assert(admin_handle("event 2") == 1 && stub_event_mode == 2);
    assert(admin_handle("event 3") == 1 && stub_event_mode == 3);
    assert(admin_handle("event 4") == 1 && stub_event_mode == 4);
    assert(admin_handle("event 5") == 1 && stub_event_mode == 5);
    assert(admin_handle("event 6") == 1 && stub_event_mode == 6);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "x");
    stub_login_status = 2;
    stub_event_mode = 0;

        /* Event 3: Santa shows up at once, no plates charge. */
    event_mode = 3;
    plates_event_reset();
    plates_prev_mode = 0;
    plates_phase = 0;
    dt = 0.05;
    tick_plates_event();
    assert(plates_phase == 3);
    assert(plates_thrown == 0);
    for (int i = 0; i < 40; i++) tick_plates_event();
    assert(plates_thrown > 0);
    draw_event_plates(); /* no plates drawn in mode 3 */
    draw_event_candies();
    draw_event_santa();
    draw_plates_hud();
        /* A round ends once: while the event stays on it does not repeat. */
    plates_santa_gone = 1;
    for (int i = 0; i < plates_candy_max; i++) arr_set(plates_candies, i * plates_candy_fields, 0);
    tick_plates_event();
    assert(plates_finished == 1 && plates_phase == 4);
    for (int i = 0; i < 10; i++) tick_plates_event();
    assert(plates_phase == 4);
        /* Off and on again - a new round from the start. */
    event_mode = 0;
    tick_plates_event();
    event_mode = 3;
    tick_plates_event();
    assert(plates_phase == 3);

        /* Event 4: plates do not tick under it, and the cube survives a short
         * flicker of the event value (the poller still sees the old event). */
    event_mode = 4;
    tick_plates_event();
    assert(plates_phase == 0 && plates_prev_mode == 4);
    dice_event_reset();
    dt = 0.1;
    update_dice_event();
    assert(dice_active == 1);
    event_mode = 0;
    for (int i = 0; i < 5; i++) update_dice_event();
    assert(dice_active == 1);
    dt = 1.0;
    update_dice_event();
    assert(dice_active == 0);

        /* Event 5: finite Santa-style round; every cane travels horizontally right. */
    event_mode = 5;
    game_state = ST_LOBBY;
    dt = 0.1;
    /* The Columns figure towers over a fighter, and the banner sits below the HUD. */
    assert(columns_size == 260);
    assert(banner_y == 154);
    columns_phase = 0;
    columns_shot_active = 0;
    player->x = screen_w * 0.8;
    player->y = screen_h * 0.9;
    for (int i = 0; i < 8; i++) update_columns_event();
    assert(columns_phase == 1 && columns_shot_active == 1);
    assert(fabs(columns_shot_y - screen_h / 2) < 1e-9);
    double columns_x_before = columns_shot_x;
    update_columns_event();
    assert(columns_shot_x > columns_x_before);
    assert(fabs(columns_shot_y - screen_h / 2) < 1e-9);
    draw_columns_event();
    draw_columns_hud();
    for (int i = 0; i < 180; i++) update_columns_event();
    assert(columns_thrown == columns_shot_total && columns_phase == 3);
    event_mode = 0;
    update_columns_event();
    assert(columns_phase == 0 && columns_shot_active == 0);

        /* Event 2: both plates taken -> charge -> Santa and candies. */
    event_mode = 2;
    plates_prev_mode = 4;
    dt = 0.05;
    tick_plates_event();
    assert(plates_phase == 0);
    player->x = plate_ax();
    player->y = plate_ay();
    arr_set(remotes, remote_fields + 1, plate_bx());
    arr_set(remotes, remote_fields + 2, plate_by());
    arr_set(remotes, remote_fields + 5, 1);
    tick_plates_event();
    assert(plates_phase == 1);
    for (int i = 0; i < 41; i++) tick_plates_event();
    assert(plates_phase == 2);
    for (int i = 0; i < 20; i++) tick_plates_event();
    assert(plates_phase == 3);
    for (int i = 0; i < 10; i++) tick_plates_event();
    assert(plates_thrown > 0);
    arr_set(remotes, remote_fields + 5, 0);
    event_mode = 0;
    tick_plates_event();
    assert(plates_prev_mode == 0 && plates_phase == 0);

    /* Event 6: any fighter on either plate weakens all hits to one third. */
    game_state = ST_ONLINE;
    event_mode = 6;
    player->hp = player->max_hp = 20;
    player->x = screen_w * 0.5;
    player->y = screen_h * 0.8;
    arr_set(remotes, remote_fields + 1, plate_ax());
    arr_set(remotes, remote_fields + 2, plate_ay());
    arr_set(remotes, remote_fields + 5, 1);
    assert(weak_hit_plate_active() == 1 && fabs(weak_hit_damage_mult() - 1.0 / 3.0) < 1e-9);
    take_damage(3);
    assert(fabs(player->hp - 19.0) < 1e-9);
    arr_set(remotes, remote_fields + 5, 0);
    assert(weak_hit_plate_active() == 0 && weak_hit_damage_mult() == 1);
    event_mode = 0;
    game_state = ST_LOBBY;
    stub_net_slot = -1;
    game_state = ST_LOBBY;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("events: plates 2, Santa 3, cube 4, Columns 5, weak-hit plates 6, Azum HP -2%");
}

static void test_hacker_event(void) {
    double saved_w = screen_w, saved_h = screen_h;
    double candy_before;
    double bx, by, bw, bh, px, py;
    double cx, cy, dx, dy;
    int i, n;
    screen_w = 1280;
    screen_h = 720;
    game_reset();
    reset_battle();
    game_state = ST_ONLINE;
    stub_net_slot = 0;
    dt = 0.05;
    event_mode = 0;
    update_hacker_event();
    assert(hack_active == 0);

    /* The raid starts from the admin chat command like every other event. */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "test");
    stub_login_status = 2;
    stub_event_mode = 0;
    assert(admin_handle("event 7") == 1 && stub_event_mode == 7);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    stub_event_mode = 0;
    /* And locally from the admin cheat switch (admin account only). */
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    cheat_hack = 1;
    assert(admin_cheat_hack() == 1 && hack_enabled() == 1);
    cheat_hack = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    assert(hack_enabled() == 0);

    /* Boot: payload of this build's platform parses, junk table is known. */
    event_mode = HACK_EVENT;
    update_hacker_event();
    assert(hack_active == 1 && hack_glitch > 0);
#ifdef CB_PC
    assert(hack_platform == HACK_PLAT_PC);
#else
    assert(hack_platform == HACK_PLAT_PHONE);
#endif
    assert(hack_payload_ok == 1 && hack_payload_bytes == 27);
    assert(hack_cmd_n == 4 && hack_btn_total == 14 && hack_cane_total == 10 && hack_cane_candy == 3);
    assert(str_len(hack_junk_label(0)) > 0 && str_len(hack_junk_label(9)) > 0);
    draw_hacker_event();
    draw_hacker_world();

    /* 0..14 s: glitch ends, all four commands fire, one cloud survives. */
    for (i = 0; i < 280; i++) {
        update_hacker_event();
        if (i % 9 == 0) {
            draw_hacker_event();
            draw_hacker_world();
        }
    }
    assert(fabs(hack_t - 14.0) < 0.06);
    assert(hack_glitch <= 0 && hack_cmd_idx == 4 && hack_cloud_on == 1);
    assert(hack_stub_disco_t > 0 && hack_stub_plates_t > 0);
    /* Typed live: the greeting is complete, Azum has not started laughing. */
    assert(hack_typed_chars(1) == utf8_len(hack_line_text(1)));
    assert(hack_typed_chars(6) == 0);
    assert(hack_btn_made == 0);

    /* 40 s: the menu junk avalanches, falls and lands as solid cover. */
    for (i = 0; i < 620; i++) {
        update_hacker_event();
    }
    assert(hack_btn_made >= 3);
    for (i = 0; i < 60; i++) {
        update_hacker_event();
    }
    assert(arr_get(hack_buttons, HB_LAND) == 1);
    bx = arr_get(hack_buttons, HB_X);
    by = arr_get(hack_buttons, HB_Y);
    bw = arr_get(hack_buttons, HB_W);
    bh = arr_get(hack_buttons, HB_H);
    /* A fighter inside a landed button is pushed out of it. */
    px = bx + bw / 2;
    py = by + bh / 2;
    hack_button_push(&px, &py, player_half());
    assert(hit_rect(px, py, bx, by, bw, bh) == 0);
    /* Junk clicks with its native sound and does nothing else. */
    assert(hack_touch(bx + bw / 2, by + bh / 2, 0) == 1);
    assert(arr_get(hack_buttons, HB_PRESS) > 0);
    draw_hacker_event();
    draw_hacker_world();

    /* 75 s: Azum's ban wipes the terminals red. */
    for (i = 0; i < 560; i++) {
        update_hacker_event();
        if (i % 11 == 0) {
            draw_hacker_event();
        }
    }
    assert(hack_ban_t > 0 && hack_t >= HACK_BAN_AT);
    draw_hacker_event();

    /* 80 s: everything explodes, the cane rain slides at exactly 45 degrees. */
    for (i = 0; i < 100; i++) {
        update_hacker_event();
    }
    assert(hack_btn_made == 0 && hack_cloud_on == 0 && hack_cane_made >= 2);
    assert(hack_touch(screen_w / 2, screen_h / 2, 0) == 0);
    cx = arr_get(hack_canes, HC_X);
    cy = arr_get(hack_canes, HC_Y);
    update_hacker_event();
    if (arr_get(hack_canes, HC_LAND) == 0) {
        dx = fabs(arr_get(hack_canes, HC_X) - cx);
        dy = arr_get(hack_canes, HC_Y) - cy;
        assert(fabs(dy - dx) < 1e-6);
    }
    draw_hacker_world();

    /* Collect all ten canes: each one pays candies on the spot. */
    candy_before = candies;
    n = 0;
    while (hack_cane_got < hack_cane_total && n < 3000) {
        int slot = -1;
        for (i = 0; i < HACK_CANE_MAX; i++) {
            if (arr_get(hack_canes, i * HC_FIELDS + HC_ON) == 1 &&
                arr_get(hack_canes, i * HC_FIELDS + HC_POP) == 0) {
                slot = i;
                break;
            }
        }
        if (slot >= 0) {
            player->x = arr_get(hack_canes, slot * HC_FIELDS + HC_X);
            player->y = arr_get(hack_canes, slot * HC_FIELDS + HC_Y);
        }
        update_hacker_event();
        n++;
    }
    assert(hack_cane_got == 10);
    assert(candies - candy_before >= 10 * hack_cane_candy);
    /* The clean victory closes the round once the last cane is gone. */
    for (i = 0; i < 200 && finished == 0; i++) {
        update_hacker_event();
    }
    assert(finished == 1);
    draw_hacker_event();
    draw_hacker_world();

    /* Event off: the whole scene rewinds for the next round. */
    event_mode = 0;
    update_hacker_event();
    assert(hack_active == 0 && hack_t == 0 && hack_cane_got == 0);
    game_state = ST_LOBBY;
    stub_net_slot = -1;
    screen_w = saved_w;
    screen_h = saved_h;
    puts("events: H4c4k0r raid 7 payload, junk cover, ban, 45-degree cane rain");
}

static void test_eax229_and_rooms(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    assert(achievement_count() == 6);
    assert(achievement_bit(5) == ACH_BUK_GIFT);
    assert(strcmp(tr_achievement_eax229(), "mov eax, 0x229") == 0);
    assert(strcmp(tr_achievement_eax229_desc(), "???") == 0);
    language = 1;
    assert(strcmp(tr_class_ebuc(), "буК") == 0);
    assert(strcmp(tr_achievement_buk(), "Подарок буКа") == 0);
    assert(strcmp(tr_skin_elf_hint(), "Подарок буКа") == 0);
    assert(strcmp(tr_skins_note_for(CLASS_EBUC), "Скин «Эльф»: достижение «Подарок буКа»") == 0);
    assert(strcmp(bp_reward_text(4), "+45 кубков") == 0);
    language = 0;
    assert(strcmp(tr_class_ebuc(), "ebuC") == 0);
    assert(strcmp(tr_achievement_buk(), "ebuC's gift") == 0);
    assert(strcmp(tr_skin_elf_hint(), "ebuC's gift") == 0);
    assert(strcmp(bp_reward_text(4), "+45 cups") == 0);
    language = 1;

    /* ACH_BUK_GIFT: granted in an online Ded Moroz event as Santa, unlocks Elf ebuC. */
    achievement_mask = 0;
    bp_claimed = 0;
    assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
    game_state = ST_ONLINE;
    online_ready = 1;
    event_mode = 3;
    player_class = CLASS_SANTA;
    check_buk_elf_achievement();
    assert(has_achievement(ACH_BUK_GIFT) == 1);
    assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 1);
    online_ready = 0;
    event_mode = 0;
    player_class = CLASS_ORDINARY;
    game_state = ST_LOBBY;

    /* Aiming the magic ball follows the joystick directly and keeps direction on release. */
    player->x = 400;
    player->y = 360;
    player->angle = 0;
    joy.x = 130;
    joy.y = 570;
    joy.r = 70;
    drop_joy();
    super_cd = 0;
    finished = 0;
    player_block = 0;
    game_state = ST_SOLO;
    player_class = CLASS_EBUC;
    player->x = 400;
    player->y = 360;
    player->angle = atan2(-0.8, 0.6);
    ball_reset();
    ball_mode = 1;
    ball_fire();
    assert(ball.active == 1);
    assert(fabs(ball.vx - 0.6 * BALL_SPEED) < 1e-6 && fabs(ball.vy + 0.8 * BALL_SPEED) < 1e-6);
    ball_reset();

    /* Solo enemies now deal full class damage instead of the old 20% cut. */
    assert(fabs(enemy_dash_damage - 2.8) < 1e-9);
    assert(fabs(enemy_damage_mult_for(CLASS_AZUM) - 1.00) < 1e-9);
    assert(fabs(enemy_damage_mult_for(CLASS_ORDINARY) - 1.00) < 1e-9);
    assert(fabs(enemy_damage_mult_for(CLASS_EBUC) - 1.00) < 1e-9);
    assert(fabs(enemy_damage_mult_for(CLASS_SANTA) - 1.00) < 1e-9);
    assert(fabs(enemy_damage_mult_for(CLASS_ASTRA) - 1.00) < 1e-9);
    assert(bot_speed < joy_speed && enemy_cooldown_min >= 0.20);

    /* ebuC ball bounce scaling: 0.15 base dmg, +2.8 on first bounce, x1.28 gain and x1.04 speed per bounce. */
    assert(fabs(ball_damage_for(0) - 0.15) < 1e-9);
    for (int b = 0; b < 10; b++) {
        assert(ball_damage_for(b + 1) > ball_damage_for(b));
    }
    assert(fabs(ball_damage_for(1) - 2.95) < 1e-9);
    assert(fabs(ball_damage_for(2) - 6.534) < 1e-9);
    for (int b = 1; b < 10; b++) {
        double prev_gain = ball_damage_for(b) - ball_damage_for(b - 1);
        double next_gain = ball_damage_for(b + 1) - ball_damage_for(b);
        assert(fabs(next_gain - prev_gain * 1.28) < 1e-9);
    }
    assert(fabs(ball_damage_for(4) - 16.9935456) < 1e-9);
    assert(fabs(ball_damage_for(10) - (0.15 + 2.8 * (pow(1.28, 10) - 1.0) / 0.28)) < 1e-9);
    assert(fabs(station_beam_pull - 26.0) < 1e-9 && fabs(station_beam_min - 54.0) < 1e-9);
    {
        /* ebuC's dispenser beam renders before ebuC's body layer in Solo. */
        int beam_idx = -1, body_idx = -1;
        player_class = CLASS_EBUC;
        reset_battle();
        game_state = ST_SOLO;
        finished = 0;
        player->hp = player->max_hp = 24;
        player->x = 300;
        player->y = 400;
        player_block = 1;
        arr_set(turret_x, 0, 500);
        arr_set(turret_y, 0, 400);
        arr_set(turret_hp, 0, 10);
        arr_set(turret_maxhp, 0, 10);
        last_turret_slot = 0;
        adm_reset_log();
        draw_game();
        for (int ci = 0; ci < adm_color_n; ci++) {
            uint32_t c = adm_colors[ci];
            if ((c & 0x00FFFFFFu) == 0x00C8FAD6u && beam_idx < 0)
                beam_idx = ci;
            if (c == 0x2E4FC3F7u && body_idx < 0)
                body_idx = ci;
        }
        assert(beam_idx >= 0 && body_idx > beam_idx);
        player_block = 0;
        arr_set(turret_hp, 0, 0);
        player_class = CLASS_ORDINARY;
    }
    {
        game_state = ST_SOLO;
        finished = 0;
        enemy_class = CLASS_ORDINARY;
        enemy->max_hp = 20;
        enemy->hp = 20;
        enemy->size = 25;
        enemy->x = 460;
        enemy->y = 360;
        enemy_revive_prot = 0;
        player->x = 400;
        player->y = 360;
        player->angle = 0;
        super_cd = 0;
        ball_reset();
        ball_fire();
        for (int f = 0; f < 30 && ball.active == 1; f++)
            ball_tick();
        assert(ball.active == 0 && ball.bounces == 0);
        assert(fabs(enemy->hp - 19.85) < 1e-6);

        /* Wall bounce: speed multiplies by 1.04 on each bounce, damage grows. */
        enemy->x = 900;
        enemy->y = 600;
        enemy->hp = 20;
        player->x = 80;
        player->y = 80;
        player->angle = atan2(-0.6, -0.8);
        super_cd = 0;
        ball_reset();
        ball_fire();
        for (int f = 0; f < 120 && ball.bounces < 2; f++)
            ball_tick();
        assert(ball.bounces == 2);
        assert(fabs(hypot(ball.vx, ball.vy) - BALL_SPEED * 1.04 * 1.04) < 1e-6);
        enemy->x = ball.x;
        enemy->y = ball.y;
        ball_tick();
        assert(ball.active == 0 && fabs(enemy->hp - (20.0 - ball_damage_for(2))) < 1e-6);

        /* Expiry burst damages enemies inside BALL_BOOM_RADIUS (150) while sparing distant ones. */
        assert(fabs(BALL_BOOM_RADIUS - 150.0) < 1e-6 && BALL_BOOM_RADIUS > super_radius * 1.14);
        ball_reset();
        ball.active = 1;
        ball.x = 400;
        ball.y = 360;
        ball.vx = 0;
        ball.vy = 0;
        ball.bounces = 4;
        ball.t = BALL_MAX_TIME + 0.01;
        enemy->hp = 20;
        enemy->x = 400 + 145; /* outside BALL_RADIUS and old 130, inside 150 + cube half-width */
        enemy->y = 360;
        ball_tick();
        assert(ball.active == 0 && ball.boom_t > 0);
        assert(fabs(enemy->hp - (20.0 - ball_damage_for(4))) < 1e-6);

        ball_reset();
        ball.active = 1;
        ball.x = 400;
        ball.y = 360;
        ball.bounces = 4;
        ball.t = BALL_MAX_TIME + 0.01;
        enemy->hp = 20;
        enemy->x = 800; /* outside BALL_BOOM_RADIUS */
        enemy->y = 360;
        ball_tick();
        assert(ball.active == 0 && fabs(enemy->hp - 20.0) < 1e-6);

        /* Online remote brick: same +4% speed per bounce, bounce damage, and expiry blast. */
        game_state = ST_ONLINE;
        stub_net_slot = 0;
        stub_online[1] = 1;
        player_class = CLASS_ORDINARY;
        player->max_hp = 20;
        player->hp = 20;
        player->size = 25;
        player->x = 900;
        player->y = 600;
        player_block = 0;
        revive_prot = 0;
        remote_bricks_reset();
        stub_bkx[1] = 80.0 / screen_w;
        stub_bky[1] = 80.0 / screen_h;
        stub_bkdx[1] = -0.8;
        stub_bkdy[1] = -0.6;
        stub_brick[1] = 1;
        for (int f = 0; f < 120 && rb_bounces[1] < 2; f++)
            remote_bricks_tick();
        assert(rb_bounces[1] == 2);
        assert(fabs(hypot(rb_vx[1], rb_vy[1]) - BALL_SPEED * 1.04 * 1.04) < 1e-6);
        player->x = rb_x[1];
        player->y = rb_y[1];
        remote_bricks_tick();
        assert(rb_on[1] == 0 && fabs(player->hp - (20.0 - ball_damage_for(2))) < 1e-6);

        /* Online expiry blast: clips player within BALL_BOOM_RADIUS when rb_t expires. */
        player->hp = 20;
        rb_on[1] = 1;
        rb_x[1] = 400;
        rb_y[1] = 360;
        rb_vx[1] = 0;
        rb_vy[1] = 0;
        rb_bounces[1] = 3;
        rb_t[1] = BALL_MAX_TIME + 0.01;
        player->x = 400 + 145;
        player->y = 360;
        remote_bricks_tick();
        assert(rb_on[1] == 0 && rb_boom_t[1] > 0);
        assert(fabs(player->hp - (20.0 - ball_damage_for(3))) < 1e-6);
        stub_online[1] = 0;
        stub_brick[1] = 0;
        game_state = ST_LOBBY;
        ball_reset();
    }

    /* Dimasi4ek229 himself never gets the achievement for meeting himself
     * (even if ACH_EAX229 was previously in achievement_mask), sees 5 cards,
     * and has no scrolling on the Achievements screen. */
    achievement_mask = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "dimasi4ek229");
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 0);
    achievement_mask = ACH_EAX229;
    assert(has_achievement(ACH_EAX229) == 0);
    assert(ach_visible_count() == 5 && ach_scroll_needed() == 0);
    {
        int saved_w = screen_w, saved_h = screen_h;
        screen_w = 1280;
        screen_h = 720;
        language = 1;
        game_state = ST_ACHIEVEMENTS;
        t_dir = 0;
        warn_open = 0;
        studio_open = 0;
        acct_open = 0;
        docs_open = 0;
        ach_open();
        adm_reset_log();
        draw_achievements();
        assert(adm_has_text("mov eax, 0x229") == 0);
        assert(adm_has_text("Подарок буКа") == 1);
        logic_touch(ach_x() + 80, ach_view_top() + 260, 0, 91);
        logic_touch(ach_x() + 80, ach_view_top() + 120, 2, 91);
        logic_touch(ach_x() + 80, ach_view_top() + 120, 1, 91);
        assert(ach_scroll == 0 && ach_scroll_needed() == 0);
        screen_w = saved_w;
        screen_h = saved_h;
    }

    /* Anyone else: "mov eax, 0x229" IS in their achievements list (locked
     * before meeting Dimasi4ek229, unlocked after), so all 6 cards and vertical
     * drag scrolling are present even before unlocking it. */
    achievement_mask = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    game_state = ST_LOBBY;
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 0);
    assert(ach_visible_count() == 6);
    {
        int saved_w = screen_w, saved_h = screen_h;
        double mid;
        screen_w = 1280;
        screen_h = 720;
        language = 1;
        game_state = ST_ACHIEVEMENTS;
        t_dir = 0;
        ach_open();
        assert(ach_scroll_needed() == 1 && ach_scroll_max() > 80);
        adm_reset_log();
        draw_achievements();
        assert(adm_has_text("mov eax, 0x229") == 1);
        /* Drag upward from bottom margin on 1080p phone: full-screen scroll works! */
        screen_w = 2400;
        screen_h = 1080;
        logic_touch(screen_w - 60, screen_h - 40, 0, 92);
        /* Secondary touch does not hijack active drag or trigger Back. */
        logic_touch(screen_margin + 10, back_y + 10, 0, 93);
        assert(t_dir == 0 && ach_drag_id == 92);
        logic_touch(screen_w - 60, screen_h - 100, 2, 92);
        ach_scroll_tick();
        logic_touch(screen_w - 60, screen_h - 155, 2, 92);
        ach_scroll_tick();
        /* Opposite thumb-lift roll + duplicate zero-delta move still flicks forward! */
        logic_touch(screen_w - 60, screen_h - 140, 2, 92);
        logic_touch(screen_w - 60, screen_h - 140, 2, 92);
        assert(press_point_n == 0);
        logic_touch(screen_w - 60, screen_h - 140, 1, 92);
        mid = ach_scroll;
        assert(mid > 80 && ach_vel > 200);
        ach_scroll_tick();
        assert(ach_scroll > mid);
        ach_scroll_by(9999);
        assert(fabs(ach_scroll - ach_scroll_max()) < 1e-6);
        stub_scissor_calls = 0;
        stub_scissor_active = 0;
        adm_reset_log();
        draw_achievements();
        assert(stub_scissor_calls >= 2 && stub_scissor_active == 0);
        assert(adm_has_text("Подарок буКа") == 1);
        screen_w = saved_w;
        screen_h = saved_h;
    }
    game_state = ST_ONLINE;
    stub_net_slot = 0;
    stub_online[1] = 1;
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "%s", "Dimasi4ek229");
    check_meet_eax229();
    assert(has_achievement(ACH_EAX229) == 1);
    assert(ach_visible_count() == 6 && ach_scroll_allowed() == 1);
    stub_online[1] = 0;
    stub_nicks[1][0] = 0;
    game_state = ST_LOBBY;

    /* Rooms screen: join an open room, a password room, and create one. */
    achievement_mask = 0;
    game_state = ST_ROOMS;
    t_dir = 0;
    warn_open = 0;
    studio_open = 0;
    snprintf(stub_room_names[0], sizeof(stub_room_names[0]), "%s", "main");
    snprintf(stub_room_names[1], sizeof(stub_room_names[1]), "%s", "closed-club");
    stub_roomlist_count = 2;
    stub_roomlist_status = 2;
    rooms_open();
    assert(rooms_mode == 0);
    logic_touch(rooms_x() + rooms_w() / 2, rooms_row_y(0) + rooms_row_h() / 2, 0, 81);
    press_points_clear();
    assert(stub_prep_calls == 1 && stub_prep_create == 0 && strcmp(stub_prep_name, "main") == 0);
    stub_prep_status = 2; /* open room: allowed in */
    update_rooms();
    assert(t_dir == 1 && t_target == ST_ONLINE && strcmp(ROOM, "main") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_ROOMS;
    rooms_open();
    logic_touch(rooms_x() + rooms_w() / 2, rooms_row_y(1) + rooms_row_h() / 2, 0, 82);
    press_points_clear();
    stub_prep_status = 3; /* password required */
    update_rooms();
    assert(rooms_mode == 2 && str_len(rooms_msg) > 0);
    rooms_pass = "s3cret";
    rooms_submit_join();
    assert(stub_prep_create == 0 && strcmp(stub_prep_pass, "s3cret") == 0);
    stub_prep_status = 2;
    update_rooms();
    assert(t_target == ST_ONLINE && strcmp(ROOM, "closed-club") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_ROOMS;
    rooms_open();
    rooms_mode = 1;
    rooms_name = "my-room";
    rooms_pass = "";
    rooms_submit_create();
    assert(stub_prep_create == 1 && strcmp(stub_prep_name, "my-room") == 0);
    stub_prep_status = 2;
    update_rooms();
    assert(t_target == ST_ONLINE && strcmp(ROOM, "my-room") == 0);
    t_dir = 0;
    t_target = 0;
    game_state = ST_LOBBY;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");

    /* Chat header: after creating a room, the chat title is that room. */
    snprintf(stub_current_room, sizeof(stub_current_room), "%s", "my-room");
    language = 0;
    game_state = ST_ONLINE;
    chat_open = 1;
    adm_reset_log();
    draw_chat();
    assert(adm_has_text("Room: my-room") == 1);
    chat_open = 0;
    game_state = ST_LOBBY;
    puts("eax229 + rooms: meet-admin achievement, room list, password join, create");
}

/* Drawing the complete UI retains every screen through draw_screen(). */
void text(const char *s, float x, float y, uint32_t color) { text_scaled(s, x, y, color, 1); }
void tex_round(float x, float y, const char *name, float scale, float radius) {
    (void)radius;
    tex(x, y, name, 0, scale);
}
void rect_rot(float x, float y, float w, float h, float angle, uint32_t color) {
    (void)angle;
    rect(x, y, w, h, color);
}
double lerp(double a, double b, double t) { return a + (b - a) * t; }
int str_starts_with(const char *s, const char *pref) { return strncmp(s, pref, strlen(pref)) == 0; }
double net_chat_count(void) { return stub_chat_count; }
const char *net_chat_uid(double idx) { return stub_chat_uids[(int)idx]; }
const char *net_chat_text(double idx) { return stub_chat_texts[(int)idx]; }
/* Like the real one: ONE shared static buffer for every call. */
const char *net_chat_key(double idx) {
    static char ret[28];
    snprintf(ret, sizeof(ret), "%s", stub_chat_keys[(int)idx]);
    return ret;
}
double net_status(void) { return net_st_playing; }
double net_leaderboard_status(void) { return 0; }
double net_leaderboard_count(void) { return 0; }
const char *net_leaderboard_nick(double idx) { (void)idx; return ""; }
double net_leaderboard_cups(double idx) { (void)idx; return 0; }
double net_player_codecracker_super(double slot) { return stub_cc_super[(int)slot]; }
double net_player_block(double slot) { (void)slot; return 0; }
const char *net_banner_text(void) { return ""; }
const char *net_banner_color(void) { return "white"; }

static void test_class_stars(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    assert(class_imbalance_of(CLASS_ORDINARY) == 1);
    assert(class_imbalance_of(CLASS_SANTA) == 2);
    assert(class_imbalance_of(CLASS_AZUM) == 3);
    assert(class_imbalance_of(CLASS_EBUC) == 3);
    assert(class_imbalance_of(CLASS_ASTRA) == 4);
    assert(class_imbalance_of(CLASS_CODECRACKER) == 5);
    double r = stat_star_r();
    star_line_count = 0;
    draw_star(0, 0, r * 1.1, 0xFFF7C948);
    int filled = star_line_count;
    star_line_count = 0;
    draw_star(0, 0, r, 0xFF0B0B12);
    int empty = star_line_count;
    for (int i = 0; i < 6; i++) {
        player_class = class_at_order(i);
        int rating = (int)class_imbalance_of(player_class);
        star_line_count = 0;
        star_max_y = 0;
        adm_reset_log();
        draw_class_stats();
        assert(star_line_count == rating * filled + (5 - rating) * empty);
        assert(star_max_y < stat_motto_y());
        assert(adm_has_text(class_desc_of(player_class)));
    }
    puts("classes: all five rating stars restored above the description");
}

static void assert_curtain_on_top(void) {
    double fade = t_fade;
    t_fade = 1;
    adm_reset_log();
    logic_draw();
    assert(adm_color_n > 0 && adm_color_n < ADM_COLOR_MAX);
    assert(adm_colors[adm_color_n - 1] == 0xFF000000);
    t_fade = fade;
}

static void test_overlay_transitions(void) {
    game_reset();
    screen_w = 1280;
    screen_h = 720;
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
    warn_open = 0;
    studio_open = 0;
    /* The account stays behind the curtain, including opening from lobby. */
    logic_touch(menu_x() + menu_btn_w() / 2, menu_row(3) + btn_h / 2, 0, 90);
    assert(t_dir == 1 && acct_open == 0);
    finish_transition();
    assert(acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(0) + btn_h / 2, 0, 91);
    assert(t_dir == 1 && acct_open == ACCT_MENU);
    double generation = ui_nav_gen;
    logic_draw();
    update_button_press();
    press_overlay = 3;
    assert(button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h) > 0);
    press_overlay = 0;
    assert(ui_nav_gen == generation);
    /* A second tap or Back cannot overwrite a pending navigation action. */
    assert(logic_back() == 1 && acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(3) + btn_h / 2, 0, 92);
    assert(acct_open == ACCT_MENU);
    assert_curtain_on_top();
    finish_transition();
    assert(acct_open == ACCT_PASS);
    assert(logic_back() == 1 && acct_open == ACCT_PASS);
    finish_transition();
    assert(acct_open == ACCT_MENU);
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(3) + btn_h / 2, 0, 93);
    assert(acct_open == ACCT_MENU && t_dir == 1);
    finish_transition();
    assert(acct_open == ACCT_DELETE);
    assert(logic_back() == 1);
    finish_transition();
    /* Switch-account used to hide the old window before any fade. */
    logic_touch(acct_x() + acct_w() / 2, acct_menu_y(1) + btn_h / 2, 0, 94);
    assert(acct_open == ACCT_MENU && t_dir == 1);
    assert_curtain_on_top();
    finish_transition();
    assert(acct_open == 0 && game_state == ST_LOGIN);
    /* Opening, paging and closing documents share that very same curtain. */
    docs_transition(0, 0);
    assert(docs_open == 0 && t_dir == 1);
    finish_transition();
    assert(docs_open == 1 && game_state == ST_LOGIN);
    touch_docs(docs_next_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0);
    assert(docs_page == 0 && t_dir == 1);
    assert_curtain_on_top();
    finish_transition();
    assert(docs_page == 1);
    touch_docs(docs_prev_x() + docs_nav_w() / 2, docs_row_y() + btn_h / 2, 0);
    finish_transition();
    assert(docs_page == 0);
    assert(logic_back() == 1 && docs_open == 1);
    finish_transition();
    assert(docs_open == 0 && game_state == ST_LOGIN);
    /* A covered documents button may not claim an account's press. */
    docs_show(0, 0);
    acct_show(ACCT_MENU);
    press_points_clear();
    press_points_add(acct_x() + acct_w() / 2, acct_menu_y(0) + btn_h / 2, 7);
    press_overlay = 2;
    assert(button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h) == 0);
    /* The finger is still waiting: the covered button took nothing. */
    assert(press_point_n == 1);
    press_overlay = 3;
    (void)button_press_k(acct_x(), acct_menu_y(0), acct_w(), btn_h);
    assert(press_point_n == 0 && press_phase[0] == 1);
    press_overlay = 0;
    press_points_clear();
    acct_close();
    docs_close();
    /* Reset must discard callbacks as well as the transition clock. */
    docs_transition(0, 0);
    assert(t_action != NULL);
    game_reset();
    assert(t_action == NULL && t_dir == 0);
    puts("navigation: deferred overlays, outgoing press, topmost curtain and input lock OK");
}


/* Christmas snowballs: ebuC's exact projectile but flat damage, a 1 s
 * cooldown, a full-size pad, free aiming, and the Sanch / admin rules. */
static void test_christmas_snowballs(void) {
    double saved_login = stub_login_status;
    char saved_nick[sizeof(stub_login_nick)];
    double saved_state = game_state;
    double saved_finished = finished;
    double saved_cls = player_class;
    double saved_slot = stub_net_slot;
    double saved_hp = player->hp;
    double saved_px = player->x, saved_py = player->y, saved_pa = player->angle;
    double saved_ehp = enemy->hp, saved_ex = enemy->x, saved_ey = enemy->y;
    double saved_esize = enemy->size;
    double saved_bounces = xmas_balls[0].bounces;
    double saved_theme_mode = theme_mode, saved_theme_chosen = theme_chosen;
    int i, k;
    (void)saved_bounces;
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    /* Exactly ebuC's projectile, except a bounce never adds damage and the
     * hit uses its own reduced split damage. */
    assert(XMAS_RADIUS == BALL_RADIUS && XMAS_SPEED == BALL_SPEED);
    assert(XMAS_MAX_BOUNCES == BALL_MAX_BOUNCES && XMAS_MAX_TIME == BALL_MAX_TIME);
    assert(XMAS_BOOM_RADIUS == BALL_BOOM_RADIUS);
    assert(XMAS_DAMAGE == 0.2 && XMAS_COOLDOWN == 1.0);
    assert(christmas_pad_r() == battle_pad_r());   /* as big as block/ability */
    assert(XMAS_PUSH > 0 && XMAS_PUSH < 12.0);

    /* Flag off, guest: no snowballs. */
    stub_login_status = 0;
    theme_mode = 0;
    theme_chosen = 0;
    stub_christmas = 0;
    assert(christmas_snowballs_on() == 0 && christmas_pad_hit(10, 10) == 0);
    assert(winter_theme_on() == 0);

    /* Christmas: 1 enables New Year visuals, candies, snowballs and Battle Pass for everyone. */
    stub_christmas = 1;
    assert(christmas_snowballs_on() == 1 && christmas_pad_hit(christmas_pad_x(), christmas_pad_y()) == 1);
    assert(active_theme_mode() == 1 && winter_theme_on() == 1 && christmas_active() == 1);
    assert(candies_on() == 1 && bpass_allowed() == 1);
    /* A saved visual choice still works without disabling the global event. */
    theme_chosen = 1;
    theme_mode = 0;
    assert(winter_theme_on() == 0 && christmas_active() == 0);
    assert(christmas_snowballs_on() == 1 && candies_on() == 1 && bpass_allowed() == 1);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "Player2");
    assert(seasonal_content_allowed() == 1 && candies_on() == 1 && bpass_allowed() == 1);

    game_state = ST_SOLO;
    finished = 0;
    player_class = CLASS_ORDINARY;
    player->hp = 20;
    player->x = 60;
    player->y = 90;
    player->angle = 0;
    enemy->hp = 0;          /* keep the enemy out of the flight path */
    enemy->x = -500;
    enemy->y = -500;
    enemy->size = 25 * enemy_scale;
    player_stun = 0;
    player_freeze = 0;
    player_block = 0;
    revive_prot = 0;
    christmas_reset();

    christmas_throw();
    assert(xmas_balls[0].active == 1);
    assert(xmas_cd > 0.99 && xmas_cd < 1.01);
    snowflake_draws = snowflake_tinted = 0;
    snowflake_tint_scale = 0;
    adm_reset_log();
    draw_christmas_snowballs();
    assert(snowflake_draws == 1 && snowflake_tinted == 1);
    assert((snowflake_tint_color & 0xFFFFFFu) == 0xFFFFFFu && fabs(snowflake_tint_scale - 0.72) < 1e-6);
    for (int c = 0; c < adm_color_n; c++) assert(adm_colors[c] != 0xFFFF2D2Du);
    /* A second throw is blocked by the cooldown and the live snowball. */
    christmas_throw();
    assert(xmas_balls[0].active == 1);

    /* ebuC's bounce limit, then the snowball bursts. */
    k = 0;
    while (k < 600 && xmas_balls[0].active == 1) {
        christmas_tick();
        assert(xmas_balls[0].bounces <= XMAS_MAX_BOUNCES + 2);
        k++;
    }
    assert(xmas_balls[0].active == 0 && k < 600);
    assert(xmas_balls[0].bounces <= XMAS_MAX_BOUNCES + 2);

    /* Cooldown expires in one second; the burst deals flat base damage. */
    k = 0;
    while (k < 70) {
        christmas_tick();
        k++;
    }
    assert(xmas_cd == 0);
    enemy->hp = 20;
    enemy->x = player->x + 45;
    enemy->y = player->y;
    enemy->angle = pi;
    christmas_throw();
    k = 0;
    while (k < 20 && xmas_balls[0].active == 1) {
        christmas_tick();
        k++;
    }
    assert(fabs(enemy->hp - (20.0 - XMAS_DAMAGE)) < 1e-9);
    assert(enemy->x > player->x + 45);   /* knocked away, but only slightly */

    /* Free aim: tapping the top of the big pad throws straight up, even
     * though the fighter is facing right. */
    k = 0;
    while (k < 70) { christmas_tick(); k++; }
    xmas_cd = 0;
    enemy->hp = 0;
    enemy->x = -500;
    enemy->y = -500;
    player->angle = 0;
    christmas_pad_throw(christmas_pad_x(), christmas_pad_y() - christmas_pad_r());
    assert(xmas_balls[0].active == 1 && xmas_balls[0].vy < 0 && fabs(xmas_balls[0].vx) < 1e-6);
    christmas_reset();

    /* The global New Year event now includes every account, including Sanch. */
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "Sanch");
    assert(christmas_snowballs_on() == 1 && winter_theme_on() == 0);
    assert(christmas_pad_hit(christmas_pad_x(), christmas_pad_y()) == 1);
    assert(bpass_allowed() == 1 && candies_on() == 1 && bpass_rewards_allowed() == 1);
    christmas_reset();
    {
        double shot_before = xmas_balls[0].shot;
        christmas_throw();
        assert(xmas_balls[0].active == 1 && xmas_balls[0].shot == shot_before + 1);
    }

    /* Admins retain their New Year preview even with the global flag off. */
    theme_chosen = 0;
    stub_christmas = 0;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    assert(christmas_snowballs_on() == 1 && winter_theme_on() == 1 && christmas_active() == 1);
    assert(bpass_allowed() == 1 && bpass_rewards_allowed() == 1);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "qwertyuiopaj1234");
    assert(christmas_snowballs_on() == 1 && winter_theme_on() == 1);
    assert(admin_modifiers_allowed() == 1 && admin_gifts_allowed() == 0);

    /* A remote shot is simulated on the other clients with ebuC's limits. */
    stub_login_status = 0;
    stub_christmas = 1;
    game_state = ST_ONLINE;
    stub_net_slot = 0;
    for (i = 0; i < 4; i++) {
        stub_online[i] = 0;
        stub_xball[i] = 0;
        arr_set(remotes, i * remote_fields + 5, 0);
    }
    player->x = 5;
    player->y = 5;
    christmas_reset();
    stub_online[1] = 1;
    stub_xball[1] = 1;
    stub_xbx[1] = 0.5;
    stub_xby[1] = 0.5;
    stub_xbdx[1] = 1;
    stub_xbdy[1] = 0;
    christmas_tick();
    assert(xmas_remote[1].on == 1);
    k = 0;
    while (k < 600 && xmas_remote[1].on == 1) {
        christmas_tick();
        assert(xmas_remote[1].bounces <= XMAS_MAX_BOUNCES + 2);
        k++;
    }
    assert(xmas_remote[1].on == 0);

    /* Restore everything the rest of the suite may rely on. */
    for (i = 0; i < 4; i++) {
        stub_online[i] = 0;
        stub_xball[i] = 0;
        stub_xbdx[i] = 0;
        stub_xbdy[i] = 0;
    }
    christmas_reset();
    stub_christmas = 0;
    stub_login_status = saved_login;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    stub_net_slot = saved_slot;
    game_state = saved_state;
    finished = saved_finished;
    player_class = saved_cls;
    player->hp = saved_hp;
    player->x = saved_px;
    player->y = saved_py;
    player->angle = saved_pa;
    theme_mode = saved_theme_mode;
    theme_chosen = saved_theme_chosen;
    enemy->hp = saved_ehp;
    enemy->x = saved_ex;
    enemy->y = saved_ey;
    enemy->size = saved_esize;
    puts("christmas: ebuC physics, flat damage, full snowflake, big pad, global event OK");
}


/* With the Internet switched off every live server feature falls back to the
 * ordinary game: no New Year, no AzumBuff, no FlashSale, no event, no banner. */
static void test_offline_ordinary(void) {
    double saved_link = stub_link, saved_login = stub_login_status;
    double saved_xmas = stub_christmas, saved_ab = stub_azum_buff, saved_fs = stub_flash_sale;
    double saved_ev = stub_event_mode, saved_state = game_state;
    double saved_cls = player_class, saved_ecls = enemy_class;
    double saved_max = player->max_hp, saved_hp = player->hp;
    char saved_nick[sizeof(stub_login_nick)];
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    player_class = CLASS_ORDINARY;
    enemy_class = CLASS_ORDINARY;
    game_state = ST_LOBBY;
    stub_link = 1;
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    stub_christmas = 1;
    stub_azum_buff = 1;
    stub_flash_sale = 1;
    stub_event_mode = 3;
    azum_buff_active = 0;
    flash_sale_active = 0;
    assert(christmas_admin_allowed() == 1);
    assert(christmas_snowballs_on() == 1 && winter_theme_on() == 1);
    update_azum_buff();
    update_flash_sale();
    assert(azum_buff_active == 1 && flash_sale_active == 1);
    update_event();
    assert(event_mode == 3);

    /* One failed poll is enough: offline means the ordinary game. */
    stub_link = 0;
    assert(net_flags_reachable() == 0);
    assert(christmas_admin_allowed() == 0);
    assert(christmas_snowballs_on() == 0 && winter_theme_on() == 0 && christmas_active() == 0);
    update_azum_buff();
    update_flash_sale();
    assert(azum_buff_active == 0 && flash_sale_active == 0);
    update_event();
    assert(event_mode == 0);
    banner_t = 5;
    game_state = ST_ONLINE;
    update_banner();
    assert(banner_t == 0);

    stub_link = saved_link;
    stub_login_status = saved_login;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    stub_christmas = saved_xmas;
    stub_azum_buff = saved_ab;
    stub_flash_sale = saved_fs;
    stub_event_mode = saved_ev;
    azum_buff_active = 0;
    flash_sale_active = 0;
    event_mode = 0;
    game_state = saved_state;
    player_class = saved_cls;
    enemy_class = saved_ecls;
    player->max_hp = saved_max;
    player->hp = saved_hp;
    puts("offline: ordinary theme, no Christmas/buffs/sale/event/banner OK");
}
/* An explicit ordinary or Halloween theme wipes every Christmas visual from an
 * admin account. A guest who picks the New Year theme while it is locked
 * keeps the ordinary look and sees the grey button in the cycle. */
static void test_admin_theme_clears_christmas(void) {
    double saved_link = stub_link, saved_login = stub_login_status;
    double saved_xmas = stub_christmas, saved_mode = theme_mode;
    double saved_chosen = theme_chosen, saved_candy = candy_enabled;
    double saved_lang = language, saved_state = game_state;
    double saved_snow_tex = snow_tex_ok, saved_jingle = christmas_jingle_ok;
    double saved_jingle_tried = christmas_jingle_tried;
    char saved_nick[sizeof(stub_login_nick)];
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    stub_link = 1;
    stub_christmas = 0;   /* flag off: only the admin privilege is left */
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    astra_ability_refresh();
    candy_enabled = 1;
    language = 1;
    snow_tex_ok = 1;
    christmas_jingle_ok = 1;
    christmas_jingle_tried = 1;
    theme_chosen = 0;
    theme_mode = 0;
    game_state = ST_LOBBY;

    /* No explicit choice yet: the holiday admin keeps the New Year look. */
    assert(active_theme_mode() == 1 && winter_theme_on() == 1);
    assert(christmas_active() == 1 && christmas_menu_active() == 1);
    assert(christmas_music_active() == 1 && christmas_snowballs_on() == 1);
    assert(candies_on() == 1);
    apply_theme();
    assert((uint32_t)ui_purple == 0x3E7FB5u);
    adm_reset_log();
    draw_lobby();
    assert(adm_has_text(tr_christmas_banner()) == 1);
    assert(adm_has_text(tr_candies()) == 1);
    game_state = ST_SOLO;
    draw_arena_background();
    assert(strcmp(last_ground, SNOW_TEX) == 0 && snow_active() == 1);

    /* Ordinary theme chosen by the admin: nothing Christmas is left. */
    theme_chosen = 1;
    theme_mode = 0;
    apply_theme();
    assert(active_theme_mode() == 0 && winter_theme_on() == 0);
    assert(christmas_active() == 0 && christmas_menu_active() == 0);
    assert(christmas_music_active() == 0 && christmas_snowballs_on() == 0);
    assert(candies_on() == 0 && snow_active() == 0);
    assert(christmas_snow_under_notice_active() == 0);
    assert(strcmp(arena_ground_tex(), GRASS) == 0);
    assert(strcmp(tr_theme_label(), "Тема: Обычная") == 0);
    assert((uint32_t)ui_purple == 0x5F10A0u);
    game_state = ST_LOBBY;
    adm_reset_log();
    draw_lobby();
    assert(adm_has_text(tr_christmas_banner()) == 0);
    assert(adm_has_text(tr_candies()) == 0);
    assert(bpass_allowed() == 1);   /* earned progression is not deleted */

    /* A legacy saved Halloween value is clamped back to the ordinary theme on
     * load and in the cycle (progress_rewards, menu_input); clamp it the same
     * way before the theme is applied. */
    theme_mode = 2;
    if (theme_mode > 1) {
        theme_mode = 0;
    }
    apply_theme();
    assert(active_theme_mode() == 0);
    assert(strcmp(tr_theme_label(), "Тема: Обычная") == 0);

    /* New Year theme brings the admin's Christmas back. */
    theme_mode = 1;
    apply_theme();
    assert(active_theme_mode() == 1 && winter_theme_on() == 1);
    assert(christmas_active() == 1 && christmas_snowballs_on() == 1);
    assert(candies_on() == 1 && (uint32_t)ui_purple == 0x3E7FB5u);

    /* A guest selects New Year while it is locked: the button is grey and
     * selected, but the game still draws the ordinary theme. */
    stub_login_status = 0;
    astra_ability_refresh();
    theme_chosen = 1;
    theme_mode = 1;
    apply_theme();
    assert(active_theme_mode() == 1 && winter_theme_on() == 0);
    assert(christmas_active() == 0 && strcmp(arena_ground_tex(), GRASS) == 0);
    assert(strcmp(tr_theme_label(), "Тема: Новогодняя (недоступна)") == 0);
    assert((uint32_t)ui_purple == 0x5F10A0u);
    game_state = ST_MODES;
    adm_reset_log();
    draw_modes();
    assert(adm_has_text("недоступна") == 1);
    {
        int grey = 0, i = 0;
        while (i < adm_color_n) {
            if ((adm_colors[i] & 0xFFFFFFu) == 0xB4B4BCu) {
                grey = 1;
            }
            i = i + 1;
        }
        assert(grey == 1);   /* disabled button text colour */
    }
    /* The flag unlocks the very same selection without another press. */
    stub_christmas = 1;
    assert(winter_theme_on() == 1 && christmas_active() == 1);
    apply_theme();
    assert((uint32_t)ui_purple == 0x3E7FB5u);

    stub_link = saved_link;
    stub_login_status = saved_login;
    stub_christmas = saved_xmas;
    theme_mode = saved_mode;
    theme_chosen = saved_chosen;
    candy_enabled = saved_candy;
    language = saved_lang;
    game_state = saved_state;
    snow_tex_ok = saved_snow_tex;
    christmas_jingle_ok = saved_jingle;
    christmas_jingle_tried = saved_jingle_tried;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    astra_ability_refresh();
    apply_theme();
    puts("Темы: кнопка на «Играть», серая зима без флага, у админа обычная/Новый год без новогоднего контента - OK");
}

/* The admin auto-block guards against enemy punches and answers when safe. */
static void test_admin_auto_block(void) {
    double saved_login = stub_login_status, saved_state = game_state, saved_finished = finished;
    double saved_block = player_block, saved_cd = punch_cd, saved_left = punch_left;
    double saved_px = player->x, saved_py = player->y, saved_pa = player->angle;
    double saved_ex = enemy->x, saved_ey = enemy->y, saved_ehp = enemy->hp;
    char saved_nick[sizeof(stub_login_nick)];
    int i;
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    assert(admin_modifiers_allowed() == 1);
    game_state = ST_SOLO;
    finished = 0;
    player_class = CLASS_ORDINARY;
    player->hp = player->max_hp = 20;
    player->x = 200;
    player->y = 200;
    enemy->hp = enemy->max_hp = 20;
    enemy->x = 240;
    enemy->y = 200;
    enemy->state = 2;
    enemy_dash_active = 0;
    player_block = 0;
    punch_cd = 0;
    punch_left = 0;
    punch->active = 0;
    player_stun = 0;
    player_freeze = 0;
    dash_active = 0;
    astra_state = 0;
    pgrab_active = 0;
    gift->active = 0;
    universe_active = 0;
    boom_t = 0;

    cheat_autoblock = 0;
    autoblock_tick();
    assert(player_block == 0);           /* off: manual control untouched */

    cheat_autoblock = 1;
    assert(admin_cheat_autoblock() == 1);
    autoblock_tick();
    assert(player_block == 1);           /* guard while the enemy punch is active */

    enemy->state = 0;
    for (i = 0; i < 14; i++) {
        autoblock_tick();
    }
    assert(player_block == 0);
    assert(punch->active == 1);          /* punched back as soon as possible */
    assert(fabs(player->angle) < 1e-9); /* facing the foe */

    cheat_autoblock = 0;
    autoblock_tick();

    stub_login_status = saved_login;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    game_state = saved_state;
    finished = saved_finished;
    player_block = saved_block;
    punch_cd = saved_cd;
    punch_left = saved_left;
    punch->active = 0;
    player->x = saved_px;
    player->y = saved_py;
    player->angle = saved_pa;
    enemy->x = saved_ex;
    enemy->y = saved_ey;
    enemy->hp = saved_ehp;
    puts("admin: auto block guards against active punches and auto punches when safe OK");
}


/* Astra: the auto-block raises her PURPLE parry shield, and a successful
 * parry answers with the strike (the class ability), not with a punch. */
static void test_admin_auto_block_astra(void) {
    double saved_state = game_state, saved_finished = finished;
    double saved_cls = player_class, saved_login = stub_login_status;
    double saved_px = player->x, saved_py = player->y, saved_pa = player->angle;
    char saved_nick[sizeof(stub_login_nick)];
    int i;
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    game_state = ST_SOLO;
    finished = 0;
    dt = 1.0 / 60.0;
    player_class = CLASS_ASTRA;
    player->hp = player->max_hp = 20;
    player->x = 200;
    player->y = 200;
    player->angle = 0;
    enemy->hp = enemy->max_hp = 20;
    enemy->x = 240;
    enemy->y = 200;
    enemy->state = 2;
    enemy_dash_active = 0;
    cheat_autoblock = 1;
    block_success_t = 0;
    player_block = 0;
    astra_parry_t = 0;
    astra_parry_blocked = 0;
    astra_strike_unlock_t = 0;
    astra_grab_ready_t = 0;
    astra_cd = 0;
    astra_state = 0;
    astra_t = 0;
    queued_super = 0;
    queued_punch = 0;
    punch_cd = 0;
    punch_left = 0;
    punch->active = 0;
    player_stun = 0;
    player_freeze = 0;
    pgrab_active = 0;
    dash_active = 0;
    codecracker_super_t = 0;
    ball.active = 0;
    gift->active = 0;
    universe_active = 0;
    boom_t = 0;
    arr_clear(station_boom_ts);
    for (i = 0; i < turret_max; i++) {
        arr_set(turret_spawn_t, i, 0);
    }

    autoblock_tick();
    assert(astra_parry_t > 0);           /* the purple shield is up */
    assert(player_block == 0);           /* no ordinary blue block */

    /* The enemy's hit lands on the parry - that is a successful block. */
    astra_on_parry_block();
    assert(block_success_t > 0 && astra_parry_blocked == 1);
    assert(astra_strike_unlock_t > 0);

    for (i = 0; i < 180 && astra_state == 0; i++) {
        tick_astra();
        autoblock_tick();
        flush_queued_action();
    }
    assert(astra_state == 1);            /* the strike (ability) was fired */
    assert(punch->active == 0);          /* not an ordinary punch */
    assert(player_block == 0);
    autoblock_tick();
    assert(autoblock_skill_pending == 0);

    cheat_autoblock = 0;
    autoblock_tick();
    stub_login_status = saved_login;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    game_state = saved_state;
    finished = saved_finished;
    player_class = saved_cls;
    cheat_autoblock = 0;
    astra_state = 0;
    astra_parry_t = 0;
    astra_grab_ready_t = 0;
    astra_strike_unlock_t = 0;
    player_block = 0;
    queued_super = 0;
    punch->active = 0;
    player->x = saved_px;
    player->y = saved_py;
    player->angle = saved_pa;
    puts("admin: auto block uses Astra's purple parry and strikes after a block OK");
}

/* Other classes: after a successful blue block the cheat answers with their
 * super (Azum's dash here), not with a normal punch. */
static void test_admin_auto_block_skill(void) {
    double saved_state = game_state, saved_finished = finished;
    double saved_cls = player_class, saved_login = stub_login_status;
    double saved_hp = player->hp, saved_px = player->x, saved_pa = player->angle;
    char saved_nick[sizeof(stub_login_nick)];
    int i;
    snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);

    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    game_state = ST_SOLO;
    finished = 0;
    dt = 1.0 / 60.0;
    player_class = CLASS_AZUM;
    player->hp = player->max_hp = 20;
    player->x = 200;
    player->y = 200;
    player->angle = 0;
    enemy->hp = enemy->max_hp = 20;
    enemy->x = 245;
    enemy->y = 200;
    enemy->state = 2;
    enemy_dash_active = 0;
    cheat_autoblock = 1;
    azum_revived = 0;
    block_success_t = 0;
    player_block = 0;
    punch_cd = 0;
    punch_left = 0;
    punch->active = 0;
    dash_active = 0;
    dash_cd = 0;
    super_cd = 0;
    queued_super = 0;
    queued_punch = 0;
    player_stun = 0;
    player_freeze = 0;
    pgrab_active = 0;
    astra_state = 0;
    gift->active = 0;
    universe_active = 0;
    boom_t = 0;
    ball.active = 0;
    codecracker_super_t = 0;
    arr_clear(station_boom_ts);
    for (i = 0; i < turret_max; i++) {
        arr_set(turret_spawn_t, i, 0);
    }

    autoblock_tick();
    assert(player_block == 1);

    /* A frontal hit really lands on the raised shield. */
    take_damage_at(3.0, enemy->x, enemy->y);
    assert(block_success_t > 0);
    assert(player->hp < 20);             /* guard only softened the damage */

    autoblock_tick();                    /* consume the pulse, queue the ability */
    assert(player_block == 0 && queued_super == 1);
    flush_queued_action();
    assert(dash_active == 1);            /* the class ability, not a punch */
    assert(punch->active == 0);

    cheat_autoblock = 0;
    autoblock_tick();
    stub_login_status = saved_login;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    game_state = saved_state;
    finished = saved_finished;
    player_class = saved_cls;
    cheat_autoblock = 0;
    player_block = 0;
    dash_active = 0;
    queued_super = 0;
    punch->active = 0;
    player->hp = saved_hp;
    player->x = saved_px;
    player->angle = saved_pa;
    puts("admin: auto block answers a successful guard with the class ability OK");
}


int main(void) {
    state_create();
    state_ready = 1;
    assert(player && enemy && punch && gift && enemy_gift);
    assert(ST_LOBBY == 0 && ST_SOLO == 1 && ST_ONLINE == 5);
    /* fighters are born with the bare placeholder pool; reset_battle applies
     * the class pool and the round multiplier before any fight starts. */
    assert(player->hp == class_base_hp_for(CLASS_ORDINARY) &&
           enemy->hp == class_base_hp_for(CLASS_ORDINARY));
    assert(class_level_tbl && remote_punches && plates_candies && flake_cache);

    /* The game starts in Russian when the phone language is unknown (168-FZ). */
    assert(language == 1);
    assert(strcmp(tr_play(), "Играть") == 0);
    language = 0;
    assert(strcmp(tr_play(), "Play") == 0);
    language = 1;
    assert(strcmp(tr_play(), "Играть") == 0);
    language = 0;

    assert(class_count == 6 && CLASS_ASTRA == 4 && CLASS_CODECRACKER == 6);
    assert(winter_theme == 0 && winter_theme_on() == 0); /* the normal theme is the default */
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 1), tr_class_level_effect(CLASS_ASTRA, 1)) == 0);
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 2), tr_class_level_effect(CLASS_ASTRA, 2)) == 0);
    assert(strcmp(tr_class_level_effect(CLASS_ASTRA, 3), tr_class_level_effect(CLASS_ASTRA, 3)) == 0);
    assert(punch_forward_offset == 19 && astra_grab_forward_offset == 19);
    assert(fabs(ASTRA_PARRY_TIME - 0.7) < 1e-9);
    assert(fabs(ASTRA_PARRY_SPEED_K - 0.90) < 1e-9);
    assert(fabs(ASTRA_STRIKE_UNLOCK_DELAY - 0.42) < 1e-9);
    assert(fabs(ASTRA_STRIKE_WINDOW - 0.85) < 1e-9);
    assert(fabs(ASTRA_PULLBACK_TIME) < 1e-9);
    assert(fabs(ASTRA_PULLBACK_DIST) < 1e-9);
    assert(fabs(ASTRA_DASH_TIME - 0.44) < 1e-9);
    assert(fabs(ASTRA_STRIKE_DELAY) < 1e-9);
    assert(fabs(astra_ability_lunge_dist - 245.0) < 1e-9);
    assert(astra_ability_lunge_dist < azum_dash_speed * azum_dash_time);
    assert(fabs(PUNCH_WINDUP) < 1e-9);          /* no attack windups */
    assert(fabs(punch_cooldown - 0.45) < 1e-9);
    assert(fabs(ASTRA_STRIKE_STUN - 0.65) < 1e-9);
    assert(fabs(ASTRA_STRIKE_DAMAGE - 4.5) < 1e-9);
    assert(class_cost_of(CLASS_AZUM) == 85);
    assert(class_cost_of(CLASS_SANTA) == 125);
    assert(class_cost_of(CLASS_EBUC) == 155);
    assert(class_cost_of(CLASS_ASTRA) == 175);
    assert(class_has_super(CLASS_ASTRA) == 1);
    assert(astra_cd_for(0) == 6);
    assert(astra_cd_for(1) < astra_cd_for(0));
    assert(astra_zone_reach(2) > astra_zone_reach(1));
    assert(astra_strike_damage_for(3) > astra_strike_damage_for(2));
    assert(strcmp(WINTER_JINGLE, "winter_jingle.wav") == 0);
    assert(strcmp(SERAPHIM_MUSIC, "duel_theme.wav") == 0);
    assert(strcmp(fighter_sprite(CLASS_ORDINARY, 0, SKIN_NORMAL), ORDINARY_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ORDINARY, 1, SKIN_NORMAL), PUNCH_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ASTRA, 0, SKIN_NORMAL), ORDINARY_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ASTRA, 0, SKIN_SPECIAL), ASTRA_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_ASTRA, 1, SKIN_SPECIAL), ASTRA_PUNCH_TEX) == 0);
    FighterSpritePair astra_pair = look_sprite_pair(LOOK_ASTRA);
    assert(strcmp(astra_pair.idle, ASTRA_TEX) == 0 && strcmp(astra_pair.punch, ASTRA_PUNCH_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ASTRA, SKIN_NORMAL), ASTRA_CARD_TEX) == 0);
    assert(class_has_super(CLASS_ASTRA) == 1 && player_max_hp_for(CLASS_ASTRA) == astra_hp * round_hp_mult());
    /* Base HP pools: Ordinary/Santa 20, Azum 20, Astra 20 (*0.95 = 19), ebuC 24. */
    assert(player_max_hp_for(CLASS_ORDINARY) == 20 * round_hp_mult() &&
           player_max_hp_for(CLASS_SANTA) == 20 * round_hp_mult());
    assert(fabs(azum_hp - 20.0) < 1e-9 && astra_hp == 20 && ebuc_hp == 24 && enemy_base_hp == 20);
    assert(enemy_damage_mult_for(CLASS_ORDINARY) == 1.0); /* enemies deal full class damage now */
    assert(fabs(class_hp_of(CLASS_ASTRA) - 19.0) < 1e-9);
    assert(max_level == 4 && level_base_cost == 50 && level_cost_step == 20 && level_cost(1) == 50 &&
           level_cost(2) == 70 && level_cost(3) == 90 && level_cost(4) == 110);
    assert(punch_damage == 1 && santa_damage == 0.25 && ebuc_damage == 1.0);
    assert(ebuc_universe_damage >= ebuc_hp);
    azum_tex_ok = azum_punch_tex_ok = 1;
    azum_zombie_tex_ok = azum_zombie_punch_tex_ok = 1;
    assert(strcmp(fighter_sprite(CLASS_AZUM, 0, SKIN_NORMAL), AZUM_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 1, SKIN_NORMAL), AZUM_PUNCH_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 0, SKIN_ZOMBIE), AZUM_ZOMBIE_TEX) == 0);
    assert(strcmp(fighter_sprite(CLASS_AZUM, 1, SKIN_ZOMBIE), AZUM_ZOMBIE_PUNCH_TEX) == 0);
    FighterSpritePair azum_pair = look_sprite_pair(LOOK_AZUM);
    FighterSpritePair zombie_pair = look_sprite_pair(LOOK_ZOMBIE);
    assert(strcmp(azum_pair.idle, AZUM_TEX) == 0 && strcmp(azum_pair.punch, AZUM_PUNCH_TEX) == 0);
    assert(strcmp(zombie_pair.idle, AZUM_ZOMBIE_TEX) == 0 &&
           strcmp(zombie_pair.punch, AZUM_ZOMBIE_PUNCH_TEX) == 0);
    resolve_fighter_look(FLOOK_ME, CLASS_ORDINARY, SKIN_NORMAL);
    assert(fighter_look_of(FLOOK_ME) == LOOK_ORDINARY);
    assert(strcmp(look_tex(fighter_look_of(FLOOK_ME), fighter_pose_of(FLOOK_ME, 1)), PUNCH_TEX) == 0);
    resolve_fighter_look(FLOOK_ME, CLASS_AZUM, SKIN_NORMAL);
    assert(fighter_look_of(FLOOK_ME) == LOOK_AZUM);
    assert(strcmp(look_tex(fighter_look_of(FLOOK_ME), fighter_pose_of(FLOOK_ME, 1)), AZUM_PUNCH_TEX) == 0);
    assert(remote_fields == 12);

    assert(rects_overlap(0, 0, 1, 0, 10, 10, 15, 0, 1, 0, 10, 10) == 1);
    assert(rects_overlap(0, 0, 1, 0, 10, 10, 25, 0, 1, 0, 10, 10) == 0);
    assert(circle_hits_box(0, 0, 5, 8, 0, 0, 4) == 1);
    assert(circle_hits_box(0, 0, 2, 8, 0, 0, 4) == 0);

    /* The virtual stick now gives full directional speed beyond its small
     * deadzone: near-center and rim positions travel the same distance. */
    joy.x = 120;
    joy.y = 90;
    joy.r = 70;
    joy_id = -1;
    player_class = CLASS_ORDINARY;
    player->x = 160;
    player->y = 90;
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    joy_touch(joy.x + 14, joy.y, 0, 7);
    assert(fabs(joy.dx - 1) < 1e-9 && fabs(joy.dy) < 1e-9);
    move_player();
    double near_stick_move = player->x - 160;
        /* Acceleration: the first frame from a standstill moves less than a steady walk. */
    double first_step = near_stick_move;
    player->x = 160;
    move_player();
    double second_step = player->x - 160;
    assert(second_step > first_step);
    player->x = 160;
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    joy_touch(joy.x + joy.r, joy.y, 2, 7);
    assert(fabs(joy.dx - 1) < 1e-9 && fabs(joy.dy) < 1e-9);
    move_player();
    assert(fabs((player->x - 160) - near_stick_move) < 1e-9);
    player->x = 160;
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    joy_touch(joy.x + 5, joy.y, 2, 7);
    assert(joy.dx == 0 && joy.dy == 0);
    move_player();
    assert(fabs(player->x - 160) < 1e-9);
    joy_touch(joy.x + 5, joy.y, 1, 7);

        /* Braking: a released stick does not stop the cube in the same frame, it
         * fades the speed over walk_stop_time. */
    {
        joy_touch(joy.x + joy.r, joy.y, 0, 9);
        double frames_to_stop = 0;
        player->x = 160;
        player_vx = 0;
        player_vy = 0;
        player_stop_from = 0;
        if (joy_id < 0) joy_touch(joy.x + joy.r, joy.y, 0, 9);
        while (frames_to_stop < 60) {
            move_player();
            frames_to_stop = frames_to_stop + 1;
            if (frames_to_stop > 30) break;
            if (player_vx >= joy_speed - 0.01) break;
        }
        assert(fabs(player_vx - joy_speed) < 0.5);
        joy_touch(joy.x + joy.r, joy.y, 1, 9);
        assert(joy.dx == 0 && joy.dy == 0);
        player_stop_from = 0;
        frames_to_stop = 0;
        player->x = 160;
        while (frames_to_stop < 60 && fabs(player_vx) > 0.001) {
            move_player();
            frames_to_stop = frames_to_stop + 1;
        }
                /* at dt = 1/60 this is 5 frames, about 0.08 s */
        assert(frames_to_stop >= 4 && frames_to_stop <= 8);
        move_player();
        assert(fabs(player_vx) < 1e-9);
    }

    player->x = 400;
    player->y = 400;
    enemy->x = 399;
    enemy->y = 400;
    enemy->angle = 0.75;
    assert(punch_hits_enemy(player->x, player->y, 1, 0, punch_reach, punch_width) == 0);
    assert(astra_zone_hits_enemy(player->x, player->y, 1, 0, 0) == 0);
    enemy->x = 470;
    assert(punch_hits_enemy(player->x, player->y, 1, 0, punch_reach, punch_width) == 1);
    assert(astra_zone_hits_enemy(player->x, player->y, 1, 0, 0) == 1);

    winter_theme = 0;
    snow_tex_ok = 1;
    game_state = ST_SOLO;
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles > 0 && ground_fills == 1);
    assert(last_ground_fill == (uint32_t)grass_ground_rgb);
    assert(strcmp(last_ground, GRASS) == 0);

    ground_texture_missing = 1;
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles == 0 && ground_fills == 1);
    ground_texture_missing = 0;

    /* A saved winter setting stays private to signed-in CCP moderators. */
    winter_theme = 1;
    stub_login_status = 0;
    astra_ability_ok = 0;
    assert(dev_access() == 0 && seasonal_content_allowed() == 0 && winter_theme_on() == 0 && candies_on() == 0);
    assert(class_visible(CLASS_SANTA) == 0);
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(strcmp(last_ground, GRASS) == 0);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    astra_ability_refresh();
    assert(dev_access() == 1 && seasonal_content_allowed() == 1 && winter_theme_on() == 1 && candies_on() == 1);
    assert(class_visible(CLASS_SANTA) == 1);
    ground_tiles = ground_fills = 0;
    draw_arena_background();
    assert(ground_tiles > 0 && ground_fills == 1);
    assert(strcmp(last_ground, SNOW_TEX) == 0);
    assert(snow_active() == 1);
    assert(christmas_active() == 1);
    assert(christmas_menu_active() == 0);
    /* Developer access stays on through the winter/candy sections below;
     * the admin-login section further down refreshes it from credentials. */

    /* The lobby gradient is one smooth wash in every theme: hundreds of ~2 px
     * bands instead of the old thirty 14 px strips. */
    double saved_w = screen_w;
    double saved_h = screen_h;
    double saved_winter = winter_theme;
    screen_w = 1080;
    screen_h = 2400;
    winter_theme = 0;
    ground_tiles = ground_fills = 0;
    screen_fill();
    assert(ground_fills >= 1 + bg_gradient_h / 2);
    winter_theme = 1;
    ground_tiles = ground_fills = 0;
    screen_fill();
    assert(ground_fills >= 1 + bg_gradient_h / 2);
    winter_theme = saved_winter;
    screen_w = saved_w;
    screen_h = saved_h;

    game_state = ST_LOBBY;
    warn_open = 1;
    studio_open = 0;
    christmas_jingle_ok = 1;
    assert(snow_active() == 0);
    /* The startup notice and transitions do not pause the snow clock. */
    assert(christmas_menu_active() == 1);
    assert(christmas_music_active() == 1);
    christmas_snow_t = 0;
    update_christmas_snow();
    assert(christmas_snow_t > 0);
    t_fade = 0.5;
    t_dir = 1;
    update_christmas_snow();
    t_fade = 0;
    t_dir = 0;
    assert(christmas_snow_t >= 2 * dt);
    assert(christmas_menu_flakes == 22);

    /* Startup notice open and fully opaque: the epilepsy warning keeps every
     * flake under its panel, and nothing is ever drawn above its text. */
    snowflake_draws = 0;
    snowflake_tinted = 0;
    warn_open = 1;
    warn_a = 1;
    assert(christmas_snow_under_notice_active() == 0 && christmas_snow_over_notice_active() == 0);
    draw_christmas_under_notices();
    assert(snowflake_draws == 0);
    draw_christmas_over_notices();
    assert(snowflake_draws == 0);

    /* The panel is fading: the lobby - and its snow - shows through beneath it,
     * still never on top of the legal text, and still in the lobby layout. */
    warn_a = 0.4;
    assert(christmas_snow_under_notice_active() == 1 && christmas_snow_over_notice_active() == 0);
    snowflake_draws = 0;
    snowflake_tinted = 0;
    draw_christmas_under_notices();
    /* The bundled flake texture is blue; the lobby snow has to be tinted white
     * so the winter menu shows white flakes instead of blue blobs. */
    assert(snowflake_draws == (int)christmas_menu_flakes && snowflake_tinted == (int)christmas_menu_flakes);
    /* Menu snow is tinted opaque white; alpha 0 is the engine's opaque form. */
    assert((snowflake_tint_color & 0xFFFFFFu) == 0xFFFFFFu);
    /* The over-pass is inactive while the warning still shows, so the count
     * does not change. */
    draw_christmas_over_notices();
    assert(snowflake_draws == (int)christmas_menu_flakes);
    warn_a = 1;

    /* Snow falls across the full width in both passes - the button column
     * included, since the snow is drawn behind the buttons now. Closing the
     * splash must not move any flake: the over-pass at zero opacity has to
     * match the lobby layout exactly. */
    {
        int saved_w = screen_w;
        int saved_h = screen_h;
        double saved_t = christmas_snow_t;
        double lo = 0;
        double hi = 0;
        double lobby_x[64];
        int centre_over = 0;
        int centre_lobby = 0;
        int handoff_mismatch = 0;
        int k = 0;

        assert(christmas_menu_flakes <= 64);
        screen_w = 1080;
        screen_h = 2400;
        lo = menu_x() - 30;
        hi = menu_x() + btn_w + 30;
        for (k = 0; k < 200; k++) {
            int j = 0;
            christmas_snow_t = 1.0 + k * 0.173;

            studio_open = 0;
            studio_a = 0;
            warn_open = 0;
            warn_a = 0;
            snowflake_centre_n = 0;
            draw_christmas_under_notices();
            assert(snowflake_centre_n == (int)christmas_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                lobby_x[j] = snowflake_centres[j];
                if (lobby_x[j] >= lo && lobby_x[j] <= hi) {
                    centre_lobby++;
                }
            }

            studio_open = 1;
            studio_a = 0;
            snowflake_centre_n = 0;
            draw_christmas_over_notices();
            assert(snowflake_centre_n == (int)christmas_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                double d = snowflake_centres[j] - lobby_x[j];
                if (d > 1e-3 || d < -1e-3) {
                    handoff_mismatch++;
                }
            }

            studio_a = 1;
            snowflake_centre_n = 0;
            draw_christmas_over_notices();
            assert(snowflake_centre_n == (int)christmas_menu_flakes);
            for (j = 0; j < snowflake_centre_n; j++) {
                if (snowflake_centres[j] >= lo && snowflake_centres[j] <= hi) {
                    centre_over++;
                }
            }
        }
        /* The button column is about 31% of a 1080 px screen; flakes reach it
         * in both passes, so require at least 10% of draws there. */
        assert(centre_lobby >= 200 * (int)christmas_menu_flakes / 10);
        assert(handoff_mismatch == 0);
        assert(centre_over >= 200 * (int)christmas_menu_flakes / 10);

        screen_w = saved_w;
        screen_h = saved_h;
        christmas_snow_t = saved_t;
        studio_open = 0;
        studio_a = 0;
        warn_open = 1;
        warn_a = 1;
    }

    /* A transition curtain over the studio splash: no flakes are drawn above
     * black, and the epilepsy warning is never snowed over either. */
    studio_open = 1;
    studio_a = 1;
    t_fade = 0.5;
    t_dir = 1;
    snowflake_draws = 0;
    assert(christmas_snow_over_notice_active() == 0);
    assert(christmas_snow_under_notice_active() == 0);
    draw_christmas_over_notices();
    assert(snowflake_draws == 0);
    studio_open = 0;
    studio_a = 0;
    warn_open = 1;
    warn_a = 1;
    snowflake_draws = 0;
    draw_christmas_under_notices();
    assert(snowflake_draws == 0);
    t_fade = 0;
    t_dir = 0;

    /* Notice closed: flakes draw beneath the curtain and keep drawing through a fade. */
    warn_open = 0;
    assert(christmas_snow_under_notice_active() == 1 && christmas_snow_over_notice_active() == 0);
    snowflake_draws = 0;
    draw_christmas_under_notices();
    assert(snowflake_draws == (int)christmas_menu_flakes);
    t_fade = 0.5;
    t_dir = 1;
    snowflake_draws = 0;
    draw_christmas_under_notices();
    assert(snowflake_draws == (int)christmas_menu_flakes);
    t_fade = 0;
    t_dir = 0;

    /* The studio splash is an opaque notice too. */
    studio_open = 1;
    assert(christmas_snow_under_notice_active() == 0 && christmas_snow_over_notice_active() == 1);
    studio_open = 0;

    t_fade = 0;
    t_dir = 0;
    studio_open = 0;
    warn_open = 1;
    /* The jingle must take over even while the startup notice is visible; do
     * not let either lobby loop block it. */
    music_ok = 1;
    music_level = 1;
    lobby_playing = 1;
    winter_playing = 0;
    winter_loop_calls = 0;
    update_christmas_jingle();
    assert(lobby_playing == 0 && winter_playing == 1);
    assert(winter_loop_calls == 1 && music_level == 0);
    update_music();
    assert(lobby_playing == 0);
    /* Regular players see no winter visuals at all, even with the old
     * winter_theme setting left on their device. */
    winter_theme = 0;
    stub_login_status = 0;
    update_christmas_jingle();
    assert(winter_playing == 0);
    stub_login_status = 2;
    /* Azum vs Astra battle music: starts immediately in Solo and Online, cuts
     * lobby music cleanly, reaches battle volume, and stops outside the matchup. */
    seraphim_ok = 0;
    seraphim_playing = 0;
    seraphim_vol = 0;
    lobby_playing = 1;
    game_state = ST_SOLO;
    player_class = CLASS_AZUM;
    enemy_class = CLASS_ASTRA;
    assert(seraphim_should_play() == 1);
    update_seraphim_music();
    assert(seraphim_ok == 1 && seraphim_playing == 1 && lobby_playing == 0 && seraphim_vol > 0.2);
    for (int f = 0; f < 20; f++)
        update_seraphim_music();
    assert(seraphim_vol > 0.8);
    player_class = CLASS_ASTRA;
    enemy_class = CLASS_AZUM;
    assert(seraphim_should_play() == 1);
    enemy_class = CLASS_ORDINARY;
    assert(seraphim_should_play() == 0);
    for (int f = 0; f < 80; f++)
        update_seraphim_music();
    assert(seraphim_playing == 0);
    game_state = ST_ONLINE;
    player_class = CLASS_AZUM;
    arr_set(remotes, 1 * remote_fields, 1);
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ASTRA);
    assert(seraphim_should_play() == 1);
    update_seraphim_music();
    assert(seraphim_playing == 1);
    arr_set(remotes, 1 * remote_fields, 0);
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ORDINARY);
    game_state = ST_LOBBY;
    for (int f = 0; f < 80; f++)
        update_seraphim_music();
    assert(seraphim_playing == 0);
    winter_theme = 1;
    warn_open = 0;

    /* Candies appear by fading in only: same place, same size, alpha 0 -> 1. */
    {
        double cx = 0;
        double cy = 0;
        double x0 = 0;
        double y0 = 0;
        candy_enabled = 1;
        arr_set(candy_pick, 0, 0);
        cx = arr_get(candy_x, 0);
        cy = arr_get(candy_y, 0);
        candy_tex_ok = 1;
        arr_set(candy_t, 0, 0);
        candy_draws = 0;
        draw_candies();
        assert(candy_draws == 0);

        arr_set(candy_t, 0, candy_pop_time * 0.05);
        candy_alpha_drawn = -1;
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_draws == 1);
                /* scale arrives as a float, so compare with a tolerance */
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0 && candy_alpha_drawn < 0.2);
        x0 = candy_x_drawn;
        y0 = candy_y_drawn;
        assert(fabs(x0 - cx) < 1e-3 && fabs(y0 - cy) < 1e-3);

        arr_set(candy_t, 0, candy_pop_time * 0.5);
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0.4 && candy_alpha_drawn < 1);
                /* No growth and no fly-in: same place as at the start of the appearance. */
        assert(fabs(candy_x_drawn - x0) < 1e-3 && fabs(candy_y_drawn - y0) < 1e-3);

        arr_set(candy_t, 0, candy_pop_time);
        candy_scale_drawn = -1;
        draw_candies();
        assert(candy_scale_drawn > candy_scale - 1e-5 && candy_scale_drawn < candy_scale + 1e-5);
        assert(candy_alpha_drawn > 0.99);
        assert(fabs(candy_x_drawn - x0) < 1e-3 && fabs(candy_y_drawn - y0) < 1e-3);

                /* No texture - the same alpha channel in the circle colour. */
        candy_tex_ok = 0;
        arr_set(candy_t, 0, candy_pop_time * 0.05);
        candy_circle_color = 0;
        draw_candies();
        assert(((candy_circle_color >> 24) & 0xff) > 0 && ((candy_circle_color >> 24) & 0xff) < 60);
        assert((candy_circle_color & 0x00FFFFFF) == 0x00FF4081);
        assert(fabs(candy_x_drawn - cx) < 1e-3 && fabs(candy_y_drawn - cy) < 1e-3);
        arr_set(candy_t, 0, candy_pop_time);
        candy_circle_color = 0;
        draw_candies();
        assert(((candy_circle_color >> 24) & 0xff) > 250);
        candy_tex_ok = 0;
    }

    /* Punch boxes stay close to the body: 86 px long and 86 px wide. */
    assert(punch_reach == 86 && punch_width == 86);
    assert(enemy_punch_reach == 86 && enemy_punch_width == 86);

    /* Flawless victory requires full health and no incoming hit. */
    {
        game_state = ST_SOLO;
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_ORDINARY;
        player->max_hp = 10;
        player->hp = 10;
        player_was_hit = 0;
        enemy->hp = 0;
        finish_game(1);
        assert(has_achievement(ACH_FIRST_WIN) == 1);
        assert(has_achievement(ACH_LAST_HP) == 1);

        /* The former one-HP condition no longer qualifies. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player->hp = 1;
        player_was_hit = 0;
        finish_game(1);
        assert(has_achievement(ACH_LAST_HP) == 0);

        /* Being healed back to full does not erase damage taken earlier. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player->hp = 10;
        player_was_hit = 0;
        assert(take_damage(1) == 0);
        assert(player_was_hit == 1 && player->hp < player->max_hp);
        player->hp = player->max_hp;
        finish_game(1);
        assert(player->hp == player->max_hp);
        assert(has_achievement(ACH_LAST_HP) == 0);

        /* ebuC's turret shield absorbs damage, which still disqualifies ACH_LAST_HP. */
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_EBUC;
        player->max_hp = ebuc_hp;
        player->hp = player->max_hp;
        player_was_hit = 0;
        arr_clear(turret_x);
        arr_clear(turret_y);
        arr_clear(turret_hp);
        arr_clear(turret_maxhp);
        arr_clear(turret_spawn_t);
        arr_push(turret_x, player->x);
        arr_push(turret_y, player->y);
        arr_push(turret_hp, 5);
        arr_push(turret_maxhp, 5);
        arr_push(turret_spawn_t, 0);
        assert(take_damage(1) == 0);
        assert(player->hp == player->max_hp);
        assert(arr_get(turret_hp, 0) == 4);
        assert(player_was_hit == 1);
        finish_game(1);
        assert(has_achievement(ACH_LAST_HP) == 0);

        assert(achievement_count() == 6);
        assert(achievement_bit(3) == ACH_LAST_HP);
        assert(achievement_bit(5) == ACH_BUK_GIFT);
        finished = 0;
        cups_awarded = 0;
        achievement_mask = 0;
        player_class = CLASS_ORDINARY;
        player->max_hp = 10;
        player->hp = 10;
        player_was_hit = 0;
        enemy->hp = 10;
        arr_clear(turret_x);
        arr_clear(turret_y);
        arr_clear(turret_hp);
        arr_clear(turret_maxhp);
        arr_clear(turret_spawn_t);
    }

    /* Leaving the game (game_bye, called when the activity pauses or stops):
     * half of the exits stay silent, the rest say one of the three farewells,
     * and the nick line needs a logged-in account. */
    {
        int i = 0;
        int silent = 0;
        int sorry = 0;
        int cubes = 0;
        int nick_line = 0;
        int empty_nick = 0;
        int other = 0;
        game_state = ST_LOBBY;
        warn_open = 0;
        studio_open = 0;
        t_dir = 0;
        t_fade = 0;
        language = 1;
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "Дима");
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 0) {
                silent++;
                continue;
            }
            assert(bye_calls == 1);
            if (strcmp(last_bye, "Нам жаль что вы вышли, удачи!") == 0) {
                sorry++;
            } else if (strcmp(last_bye, "Пока, кубы будут вас ждать!") == 0) {
                cubes++;
            } else if (strcmp(last_bye, "Пока, Дима, удачи!") == 0) {
                nick_line++;
            } else {
                other++;
            }
        }
        assert(other == 0);
        /* A 50/50 roll over 4000 exits, with a wide band for the random draw. */
        assert(silent > 1700 && silent < 2300);
        assert(sorry > 400 && cubes > 400 && nick_line > 400);

                /* Without an account the nick line is never picked: no "Bye, , good luck!". */
        stub_login_nick[0] = 0;
        empty_nick = 0;
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 1 && strstr(last_bye, ", , ") != NULL) {
                empty_nick++;
            }
        }
        assert(empty_nick == 0);

        /* The English wording follows the language setting. */
        language = 0;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "Dima");
        other = 0;
        for (i = 0; i < 4000; i++) {
            bye_calls = 0;
            last_bye[0] = 0;
            game_bye();
            if (bye_calls == 0) {
                continue;
            }
            if (strcmp(last_bye, "Sorry that you left, good luck!") != 0 &&
                strcmp(last_bye, "Bye, the cubes will be waiting for you!") != 0 &&
                strcmp(last_bye, "Bye, Dima, good luck!") != 0) {
                other++;
            }
        }
        assert(other == 0);

        /* Back in the lobby closes the game, but the farewell itself is said by
         * the lifecycle hook (game_bye): one goodbye per exit, whichever way the
         * player leaves - back button, swipe-up gesture, swipe out of recents.
         * An ordinary back from a sub-screen only returns to the lobby. */
        language = 1;
        game_state = ST_LOBBY;
        bye_calls = 0;
        assert(logic_back() == 0);
        assert(bye_calls == 0);
        game_state = ST_SETTINGS;
        assert(logic_back() == 1);
        assert(bye_calls == 0);
        t_dir = 0;
        t_target = ST_LOBBY;
        game_state = ST_LOBBY;
    }

    /* The ordinary Astra class now always uses the fast ability kit. */
    assert(astra_ability_of(CLASS_ASTRA, SKIN_NORMAL) == 1);
    assert(astra_ability_of(CLASS_ASTRA, SKIN_SPECIAL) == 1);
    assert(astra_ability_of(CLASS_AZUM, SKIN_SPECIAL) == 0);
    assert(admin_slot("DIMASI4EK229") == 1);
    assert(admin_slot("QWERTYUIOPAJ1234") == 0);
    assert(admin_slot("qwertyuiopaj1234") == 0);
    assert(admin_slot("regular_player") == 0);
    assert(admin_slot("Efy") == 0 && admin_slot("efy") == 0);
    /* Special nicks carry plain RGB colours; alpha 0 is the engine's opaque. */
    assert(((uint32_t)admin_color_of("EFY", 0xFFFFFFFFu) & 0xFFFFFFu) == 0xFFD54Au);
    assert(((uint32_t)admin_head_color_of("efy", 0xFFFFFFFFu) & 0xFFFFFFu) == 0xB388FFu);
    assert(nick_shimmers("eFy") == 1);
    assert(efy_gradient_color(0, 0, 0) != efy_gradient_color(1, 0, 2));
    {
        double saved_efy_shimmer = gift_shimmer_t;
        gift_shimmer_t = 0;
        adm_reset_log();
        draw_styled_nick("Efy", 10, 10, 0xFFFFFFFFu, 0.6);
        int e_nick_gold = 0, e_nick_purple = 0, e_nick_white = 0;
        for (int ci = 0; ci < adm_color_n; ci++) {
            /* Gradient glyphs are plain RGB; alpha 0 is the engine's opaque. */
            uint32_t rgb = adm_colors[ci] & 0xFFFFFFu;
            if (rgb == 0xFFC733u) e_nick_gold = 1;
            if (rgb == 0xFFFFFFu) e_nick_white = 1;
            if (((rgb >> 8) & 255u) < 128u && (rgb & 255u) > 192u) e_nick_purple = 1;
        }
        assert(e_nick_gold == 1 && e_nick_purple == 1 && e_nick_white == 0);
        adm_reset_log();
        draw_overhead_nick("Efy", 100, 100, 0xFFFFFFFFu);
        assert(adm_color_n == 3 && adm_colors[0] != 0xFFFFFFFFu && adm_colors[2] != 0xFFFFFFFFu);
        gift_shimmer_t = saved_efy_shimmer;
    }
        /* Dev rights come from the profile (nick), not the password: the password
         * can be changed as often as you like, the right stays; only nick and login
         * matter. */
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "test");
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1);
    assert(astra_ability_is_dev() == 1);
    assert(acct_is_admin_nick() == 1);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "completely-different");
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1 && astra_ability_is_dev() == 1);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "regular_player");
    astra_ability_refresh();
    assert(astra_ability_allowed() == 0 && astra_ability_is_dev() == 0);
    assert(acct_is_admin_nick() == 0);
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "qwertyuiopaj1234");
    astra_ability_refresh();
    assert(astra_ability_allowed() == 0 && astra_ability_is_dev() == 0 && is_admin() == 0);
    assert(acct_is_admin_nick() == 0);
    stub_login_status = 0;
    astra_ability_refresh();
    assert(astra_ability_allowed() == 0 && astra_ability_is_dev() == 0);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    astra_ability_refresh();
    assert(astra_ability_allowed() == 1);

    player_class = CLASS_ASTRA;
    player_level = 0;
    set_class_owned(CLASS_ASTRA, 1);
    assert(astra_ability_on() == 1 && astra_ability_local() == 1);
    assert(class_skin_of(CLASS_ASTRA) == SKIN_NORMAL && current_skin() == SKIN_NORMAL);
    assert(class_owned_of(CLASS_ASTRA) == 1 && class_visible(CLASS_ASTRA) == 1);
    assert(strcmp(class_name_of(CLASS_ASTRA), tr_class_astra()) == 0);
    assert(fabs(astra_ability_max_hp_for(CLASS_ASTRA, 1) - astra_hp * 0.95 * round_hp_mult()) < 1e-9);
    assert(astra_ability_max_hp_for(CLASS_ASTRA, 1) < astra_hp * round_hp_mult());
    assert(astra_ability_max_hp_for(CLASS_AZUM, 1) == azum_hp * round_hp_mult());
    assert(fabs(astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1) - punch_damage * 0.93) < 1e-9);
    assert(astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1) <
           astra_ability_punch_damage_for(CLASS_ASTRA, 0, 0));
    assert(fabs(astra_ability_punch_damage_net(CLASS_ASTRA, 0, SKIN_NORMAL) -
                astra_ability_punch_damage_for(CLASS_ASTRA, 0, 1)) < 1e-9);
    assert(fabs(class_hp_of(CLASS_ASTRA) - astra_hp * 0.95 * round_hp_mult()) < 1e-9);
    assert(fabs(class_power_of(CLASS_ASTRA) - punch_damage * 0.93) < 1e-9);
    classes_scroll = 0;
    codecracker_refresh();
    assert(class_visible(CLASS_CODECRACKER) == 1 && astra_ability_allowed() == 1);
    assert(player_max_hp_for(CLASS_CODECRACKER) == 12 * round_hp_mult() && codecracker_hp == 12);
    assert(codecracker_speed_mult == 3 && codecracker_touch_damage == 1.55 && codecracker_touch_heal == 0.529);
    assert(codecracker_super_time == 7 && codecracker_super_absorb == 0.5);
    /* The overclock recharges in 4 seconds. */
    assert(codecracker_super_cd == 4);
    assert(class_has_super(CLASS_CODECRACKER) == 1 && codecracker_super_active() == 0);
    /* The overclock ultimate: 7 seconds of triple speed and half damage,
     * then it is just an ordinary cube with a small HP pool again. */
    player_class = CLASS_CODECRACKER;
    codecracker_super_t = codecracker_super_time;
    assert(codecracker_super_active() == 1);
    /* The dash (overclock) cooldown starts when the dash ends, not when
     * it starts: mid-dash there is no cooldown ticking yet, the frame the
     * timer runs out the full cooldown lands on super_cd. */
    {
        double keep_dt = dt;
        double keep_cd = super_cd;
        dt = 0.05;
        super_cd = 0;
        codecracker_super_t = 1;
        codecracker_tick_super_timer();
        assert(codecracker_super_t > 0 && super_cd == 0);
        codecracker_super_t = 0.03;
        codecracker_tick_super_timer();
        assert(codecracker_super_t == 0 && super_cd == codecracker_super_cd_for(player_level));
        super_cd = keep_cd;
        codecracker_super_t = codecracker_super_time;
        dt = keep_dt;
    }
    assert(fabs(player_walk_speed() - joy_speed * codecracker_speed_mult * admin_cheat_speed_mult()) < 1e-6);
    {
        double hp_before = player->hp;
        player->max_hp = codecracker_hp;
        player->hp = codecracker_hp;
        take_damage(2);
        assert(fabs(player->hp - (codecracker_hp - 1)) < 1e-9); /* half absorbed */
        player->hp = hp_before;
    }
    codecracker_super_t = 0;
    assert(fabs(player_walk_speed() - joy_speed * admin_cheat_speed_mult()) < 1e-6);
    /* The CodeCracker has a level branch of its own: the ordinary cube keeps
     * its own levels, and the two never move together. */
    {
        double ordinary_lv = class_level_of(CLASS_ORDINARY);
        double ordinary_un = class_unlocked_of(CLASS_ORDINARY);
        set_class_progress(CLASS_ORDINARY, 1, 2);
        set_class_progress(CLASS_CODECRACKER, 3, 3);
        assert(class_level_of(CLASS_ORDINARY) == 1 && class_unlocked_of(CLASS_ORDINARY) == 2);
        assert(class_level_of(CLASS_CODECRACKER) == 3 && class_unlocked_of(CLASS_CODECRACKER) == 3);
        player_class = CLASS_CODECRACKER;
        sync_selected_class();
        assert(player_level == 3 && levels_unlocked == 3);
        player_class = CLASS_ORDINARY;
        sync_selected_class();
        assert(player_level == 1 && levels_unlocked == 2);
        set_class_progress(CLASS_ORDINARY, ordinary_lv, ordinary_un);
        set_class_progress(CLASS_CODECRACKER, 0, 0);
    }
    /* Every CodeCracker level gives a bonus of his own: level 1 - a harder
     * bite, level 2 - the overclock comes back sooner (4 s -> 3 s), level 3 -
     * thicker overclock armour (half of the damage -> 60%). */
    {
        assert(fabs(codecracker_bite_damage(0) - codecracker_touch_damage) < 1e-9);
        assert(fabs(codecracker_bite_damage(1) - codecracker_touch_damage * 1.10) < 1e-9);
        assert(fabs(codecracker_bite_damage(3) - codecracker_bite_damage(1)) < 1e-9);
        assert(codecracker_super_cd_for(0) == codecracker_super_cd);
        assert(codecracker_super_cd_for(1) == codecracker_super_cd);
        assert(codecracker_super_cd_for(2) == 3 && codecracker_super_cd_for(3) == 3);
        assert(codecracker_absorb_for(0) == codecracker_super_absorb);
        assert(codecracker_absorb_for(2) == codecracker_super_absorb);
        assert(codecracker_absorb_for(3) == 0.6);
        /* The levels screen tells the CodeCracker his own bonuses, not the
         * ordinary cube's ones. */
        int saved_language = language;
        language = 0;
        assert(strcmp(tr_class_level_effect(CLASS_CODECRACKER, 1), "Overclock: contact damage +10%") == 0);
        language = 1;
        assert(strcmp(tr_class_level_effect(CLASS_CODECRACKER, 1), "Разгон: урон касанием +10%") == 0);
        language = saved_language;
        assert(strcmp(tr_class_level_effect(CLASS_CODECRACKER, 1),
                      tr_class_level_effect(CLASS_ORDINARY, 1)) != 0);
        assert(strcmp(tr_class_level_effect(CLASS_CODECRACKER, 2),
                      tr_class_level_effect(CLASS_ORDINARY, 2)) != 0);
        assert(strcmp(tr_class_level_effect(CLASS_CODECRACKER, 3),
                      tr_class_level_effect(CLASS_ORDINARY, 3)) != 0);
        /* A level 3 CodeCracker really takes 60% of the damage instead of
         * half of it while the overclock runs. */
        player_class = CLASS_CODECRACKER;
        player_level = 3;
        codecracker_super_t = codecracker_super_time;
        player->max_hp = codecracker_hp;
        player->hp = codecracker_hp;
        take_damage(2);
        assert(fabs(player->hp - (codecracker_hp - 0.8)) < 1e-9);
        player_level = 0;
        player->hp = codecracker_hp;
        take_damage(2);
        assert(fabs(player->hp - (codecracker_hp - 1)) < 1e-9);
        codecracker_super_t = 0;
    }
    /* Ability sounds: every ability plays its own effect, and the zombie
     * Azum skin swaps both of its sounds for the custom zombie ones. */
    {
        player_class = CLASS_AZUM;
        azum_skin = SKIN_NORMAL;
        start_punch_now();
        assert(strcmp(stub_last_snd, "punch.wav") == 0);
        azum_skin = SKIN_ZOMBIE;
        start_punch_now();
        assert(strcmp(stub_last_snd, "zombie_punch.wav") == 0);
        start_dash_now();
        assert(strcmp(stub_last_snd, "zombie_dash.wav") == 0);
        azum_skin = SKIN_NORMAL;
        start_dash_now();
        assert(strcmp(stub_last_snd, "dash.wav") == 0);
        player_class = CLASS_CODECRACKER;
        punch_left = 0;
        punch_cd = 0;
        super_cd = 0;
        codecracker_super_t = 0;
        fire_super();
        assert(strcmp(stub_last_snd, "overclock.wav") == 0);
        codecracker_super_t = 0;
        player_class = CLASS_SANTA;
        punch_left = 0;
        punch_cd = 0;
        super_cd = 0;
        fire_super();
        assert(strcmp(stub_last_snd, "gift.wav") == 0);
        assert(gift->active == 1); /* Santa launches immediately */
        gift->active = 0; /* the immediate Santa cast finishes before testing ebuC's station */
        player_class = CLASS_EBUC;
        punch_left = 0;
        punch_cd = 0;
        super_cd = 0;
        fire_super();
        assert(strcmp(stub_last_snd, "station.wav") == 0);
        assert(turret_count_alive() > 0); /* ebuC's station appears immediately */
        player_class = CLASS_ASTRA;
        player->hp = 18;
        finished = 0;
        stub_last_snd[0] = 0;
        start_grab_now();
        assert(strcmp(stub_last_snd, "punch.wav") == 0);
        astra_state = 0;
        astra_punch_t = 0;
        player_class = CLASS_ORDINARY;
        super_cd = 0;
    }
    /* The result window: in the old theme both victory and defeat are
     * rose (its button colour), in the new theme both stay purple. */
    {
        new_theme = 0;
        apply_theme();
        finished = 2;
        assert((uint32_t)result_face_color() == 0xD4508Au);
        finished = 1;
        assert((uint32_t)result_face_color() == 0xD4508Au);
        new_theme = 1;
        apply_theme();
        finished = 2;
        assert((uint32_t)result_face_color() == 0x5F10A0u);
        finished = 1;
        assert((uint32_t)result_face_color() == 0x5F10A0u);
        finished = 0;
    }
    /* Emerald tint cycles through rainbow hues and the renderer uses that color. */
    {
        double saved_shimmer = gift_shimmer_t;
        gift_shimmer_t = 0;
        uint32_t c0 = gift_emerald_tint();
        gift_shimmer_t = 1.25;
        uint32_t c1 = gift_emerald_tint();
        gift_shimmer_t = 2.50;
        uint32_t c2 = gift_emerald_tint();
        assert(gift_rarity(4) == 3 && strcmp(gift_tex(4), "gift_emerald.png") == 0);
        assert(c0 != c1 && c1 != c2 && c0 != c2);
        emerald_tint_draws = 0;
        gift_icon(4, 8, 9, 48);
        assert(emerald_tint_draws == 1 && last_emerald_tint == c2);
        gift_shimmer_t = saved_shimmer;
    }
    /* CCP gift tab has 8 rows (tabs + 5 gifts + nick + grant) without any hue picker. */
    {
        int saved_w = screen_w, saved_h = screen_h;
        screen_w = 1280;
        screen_h = 720;
        admin_tab = 1;
        admin_gift_choice = 4;
        assert(fabs(admin_content_h() - (9 * admin_row_h() + 8 * admin_gap())) < 1e-9);
        snprintf(admin_gift_target, sizeof(admin_gift_target), "%s", "tester");
        admin_gift_busy = 0;
        double grant_y = admin_content_top() + 8 * (admin_row_h() + admin_gap()) + admin_row_h() / 2;
        admin_panel_press(admin_panel_x() + admin_pad() + 20, grant_y);
        assert(admin_gift_busy == 1);
        admin_gift_busy = 0;
        admin_gift_target[0] = 0;
        admin_tab = 0;
        screen_w = saved_w;
        screen_h = saved_h;
    }
    player_class = CLASS_ORDINARY;
    assert(class_owned_of(CLASS_CODECRACKER) == 0 && class_visible(CLASS_CODECRACKER) == 1);
    assert(class_base_cost_of(CLASS_CODECRACKER) == 550 && class_cost_of(CLASS_CODECRACKER) == 550);
    assert(strcmp(class_desc_of(CLASS_CODECRACKER), "MOL Edits...") == 0);
    /* Admin gets shop classes without spending cups; persistence still saves ownership. */
    cups = 100;
    stub_last_codecracker = 0;
    pick_class(CLASS_CODECRACKER);
    assert(cups == 100 && class_owned_of(CLASS_CODECRACKER) == 1 && player_class == CLASS_CODECRACKER);
    assert(stub_last_codecracker == 1);
    player_class = CLASS_ORDINARY;
    assert(class_owned_of(CLASS_CODECRACKER) == 1 && class_visible(CLASS_CODECRACKER) == 1);
    assert(visible_class_at(5) == CLASS_CODECRACKER);
    screen_w = 320;
    assert(classes_page_size() == 1 && classes_scroll_needed() == 1);
    assert(classes_scroll == 0 && visible_class_at(5) == CLASS_CODECRACKER);
    classes_scroll_by(100000);
    assert(classes_scroll == classes_scroll_max() && classes_scroll > 0);
    classes_scroll_by(-100000);
    assert(classes_scroll == 0);
    screen_w = 600;
    assert(classes_page_size() == 3 && classes_scroll_needed() == 1);
    assert(visible_class_at(5) == CLASS_CODECRACKER);
    classes_scroll = 0;
    screen_w = 900;
    assert(classes_page_size() == 5 && classes_scroll_needed() == 1);
    double swipe_y = classes_top() + class_card_h / 2;
    double swipe_x = classes_card_x(0) + classes_card_w() / 2;
    double selected_before_swipe = player_class;
    assert(classes_swipe_zone(swipe_x, swipe_y) == 1);
    touch_classes(swipe_x, swipe_y, 0, 7);
    touch_classes(swipe_x - 100, swipe_y, 2, 7);
    touch_classes(swipe_x - 100, swipe_y, 1, 7);
    assert(classes_scroll > 0 && player_class == selected_before_swipe);
    double scroll_after_drag = classes_scroll;
    swipe_x = classes_card_x(1) + classes_card_w() / 2;
    touch_classes(swipe_x, swipe_y, 0, 7);
    touch_classes(swipe_x + 100, swipe_y, 2, 7);
    touch_classes(swipe_x + 100, swipe_y, 1, 7);
    assert(classes_scroll < scroll_after_drag && player_class == selected_before_swipe);
    double had_astra_access = astra_ability_ok;
    astra_ability_ok = 0;
    assert(classes_visible_count() == 5 && classes_scroll_needed() == 0); /* Santa is developer-only; CodeCracker is public */
    assert(classes_swipe_zone(swipe_x, swipe_y) == 0);
    astra_ability_ok = had_astra_access;
    codecracker_refresh();
    classes_scroll = 0;
    screen_w = 320;
    player_class = CLASS_ASTRA;
    progress_normalize();
    assert(fabs(astra_strike_stun_for(0) - ASTRA_STRIKE_STUN) < 1e-9);
    assert(fabs(astra_strike_damage_for(0) - ASTRA_STRIKE_DAMAGE) < 1e-9);
    assert(astra_strike_damage_for(3) > astra_strike_damage_for(0));
    assert(astra_strike_stun_for(3) > astra_strike_stun_for(0));
    /* Astra counter-strike does a fast tiny backward recoil, then a long forward dash. */
    screen_w = 1080;
    player->angle = 0;
    player->size = 45;
    player->x = 300;
    player->y = 90;
    astra_punch_t = 0;
    start_grab_now();
    assert(astra_punch_t > 0);
    assert(fabs(player->x - 300) < 1e-9);
    assert(fabs(astra_ability_lunge_tx - (300 + astra_ability_lunge_dist)) < 1e-9);
    assert(fabs(astra_ability_lunge_ty - 90) < 1e-9);
    astra_ability_lunge_tick();
    assert(player->x > 300 && player->x < astra_ability_lunge_tx); /* forward dash begins immediately */
    int lunge_frames = 1;
    double min_x = player->x;
    while (lunge_frames < 120 && astra_ability_lunge_active() == 1) {
        astra_ability_lunge_tick();
        if (player->x < min_x)
            min_x = player->x;
        lunge_frames = lunge_frames + 1;
    }
    assert(min_x >= 300 - 1e-9);
    assert(fabs(player->x - astra_ability_lunge_tx) < 1e-9);
    assert(fabs(player->y - astra_ability_lunge_ty) < 1e-9);
    assert(lunge_frames * dt >= astra_ability_lunge_time - 1e-9);
    astra_state = 0;
    astra_cd = 0;
    /* Running does NOT fly farther; walking is locked and the dash starts without a windup. */
    {
        double saved_jdx = joy.dx;
        double saved_jdy = joy.dy;
        double saved_sh = screen_h;
        screen_h = 720;
        joy.dx = 1;
        joy.dy = 0;
        player_vx = 220;
        player_vy = 0;
        player->x = 300;
        player->y = 200;
        player->angle = 0;
        start_grab_now();
        assert(player_vx == 0 && player_vy == 0 && astra_locked() == 1);
        assert(fabs(astra_ability_lunge_tx - (300 + astra_ability_lunge_dist)) < 1e-9);
        /* Direction is locked at cast time, with no windup in which to retarget. */
        joy.dx = 0;
        joy.dy = 1;
        move_player();
        assert(player_vx == 0 && player_vy == 0);
        astra_ability_lunge_tick();
        assert(fabs(player->angle) < 1e-6);
        assert(fabs(astra_ability_lunge_tx - (300 + astra_ability_lunge_dist)) < 1e-6);
        assert(fabs(astra_ability_lunge_ty - 200) < 1e-6);
        int k = 0;
        while (k < 120 && astra_ability_lunge_active() == 1) {
            astra_ability_lunge_tick();
            k = k + 1;
        }
        assert(fabs(player->x - (300 + astra_ability_lunge_dist)) < 1e-6);
        assert(fabs(player->y - 200) < 1e-6);
        joy.dx = saved_jdx;
        joy.dy = saved_jdy;
        screen_h = saved_sh;
        player->angle = 0;
        astra_state = 0;
        astra_cd = 0;
    }
    screen_w = 320;
    /* Astra ult-strike hits along the forward dash right after the 0.2s recoil. */
    screen_w = 1080;
    player->x = 200;
    player->y = 90;
    player->angle = 0;
    player->hp = 10;
    enemy->hp = enemy->max_hp = 10;
    enemy->stun = 0;
    enemy_revive_prot = 0;
    enemy->y = player->y;
    game_state = ST_SOLO;
    {
        double saved_dt = dt;
        dt = 0.02;
        start_grab_now();
        assert(player_punching_now() == 1);
        enemy->x = 200 + 170; /* Reached by the 185 px forward dash! */
        for (int f = 0; f < 50 && astra_caught == 0; f++) {
            tick_astra();
            tick_astra_fades();
        }
        assert(astra_caught == 1 && astra_hits_done == 1 && astra_box_a > 0);
        assert(fabs(enemy->stun - ASTRA_STRIKE_STUN) < 1e-9);
        assert(fabs(enemy->hp - (10.0 - ASTRA_STRIKE_DAMAGE)) < 1e-6);
        astra_release(1);
        dt = saved_dt;
    }
    screen_w = 320;
    enemy->stun = 0;
    astra_cd = 0;
    double saved_astra_owned = class_owned_of(CLASS_ASTRA);
    double saved_astra_skin = astra_skin;
    double saved_astra_cups = cups;
    double saved_flash_sale = flash_sale_active;
    double saved_skin_test_login_status = stub_login_status;
    char saved_skin_test_nick[sizeof(stub_login_nick)];
    snprintf(saved_skin_test_nick, sizeof(saved_skin_test_nick), "%s", stub_login_nick);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "astra_skin_test");
    set_class_owned(CLASS_ASTRA, 0);
    astra_skin = SKIN_NORMAL;
    player_class = CLASS_ORDINARY;
    cups = astra_cost;
    flash_sale_active = 0;
    assert(skin_unlocked_for(CLASS_ASTRA, SKIN_SPECIAL) == 0);
    pick_class(CLASS_ASTRA);
    assert(class_owned_of(CLASS_ASTRA) == 1 && astra_skin == SKIN_SPECIAL);
    assert(player_class == CLASS_ASTRA && cups == 0);
    set_class_owned(CLASS_ASTRA, saved_astra_owned);
    astra_skin = saved_astra_skin;
    cups = saved_astra_cups;
    flash_sale_active = saved_flash_sale;
    stub_login_status = saved_skin_test_login_status;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_skin_test_nick);
    player_class = CLASS_ASTRA;
    assert(class_has_skins(CLASS_ASTRA) == 1 && class_has_skins(CLASS_ASTRA) == 1);
    assert(skins_btn_visible() == 1 && current_skin() == SKIN_NORMAL);
    assert(skin_unlocked_for(CLASS_ASTRA, SKIN_SPECIAL) == 1);
    pick_skin(SKIN_SPECIAL);
    assert(current_skin() == SKIN_SPECIAL && class_skin_of(CLASS_ASTRA) == SKIN_SPECIAL);
    assert(skin_flags() == 4);
    assert(strcmp(class_card_tex(CLASS_ASTRA, SKIN_SPECIAL), ASTRA_XASTRA_CARD_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ASTRA, SKIN_NORMAL), ASTRA_CARD_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ORDINARY, SKIN_SPECIAL), ORDINARY_CARD_TEX) == 0);
    assert(strcmp(class_card_tex(CLASS_ORDINARY, SKIN_NORMAL), ORDINARY_CARD_TEX) == 0);
    assert(strcmp(skin_name_for(CLASS_ASTRA, SKIN_SPECIAL), "Xastra") == 0);
    assert(strcmp(skin_name_for(CLASS_ORDINARY, SKIN_SPECIAL), "Золотой") == 0);
    assert(strcmp(class_card_tex(CLASS_CODECRACKER, SKIN_NORMAL), CODECRACKER_CARD_TEX) == 0);
    assert(strcmp(dice_cube_tex(DICE_CUBE_CODECRACKER), CODECRACKER_CARD_TEX) == 0);
    {
        int saved_tiles = ground_tiles;
        const char *saved_ground = last_ground;
        double saved_scale = last_tex_scale, saved_x = last_tex_x, saved_y = last_tex_y;
        draw_class_render(CLASS_ORDINARY, SKIN_NORMAL, 100, 200, 100);
        assert(last_ground && strcmp(last_ground, ORDINARY_CARD_TEX) == 0);
        assert(fabs(last_tex_scale - 100.0 / CARD_RENDER_PX) < 1e-9);
        assert(fabs(last_tex_x - 100) < 1e-9 && fabs(last_tex_y - 200) < 1e-9);
        draw_class_render(CLASS_CODECRACKER, SKIN_NORMAL, 100, 200, 100);
        assert(last_ground && strcmp(last_ground, CODECRACKER_CARD_TEX) == 0);
        assert(fabs(last_tex_scale - 100.0 / CARD_RENDER_PX) < 1e-9);
        assert(fabs(last_tex_x - 100) < 1e-9 && fabs(last_tex_y - 200) < 1e-9);
        ground_tiles = saved_tiles;
        last_ground = saved_ground;
        last_tex_scale = saved_scale;
        last_tex_x = saved_x;
        last_tex_y = saved_y;
    }
    assert(fabs(class_hp_of(CLASS_ASTRA) - astra_hp * 0.95 * round_hp_mult()) < 1e-9);
    /* The card keeps only its description; no rating graphic is drawn. */
    /* No developer account still hides only developer-only Santa; CodeCracker
     * is public in the shop for 475 cups with "MOL Edits..." description. */
    stub_login_nick[0] = 'x';
    stub_login_nick[1] = 0;
    astra_ability_refresh();
    codecracker_refresh();
    set_class_owned(CLASS_CODECRACKER, 0);
    assert(astra_ability_allowed() == 0 && class_visible(CLASS_CODECRACKER) == 1);
    assert(class_visible(CLASS_ASTRA) == 1);
    assert(class_owned_of(CLASS_CODECRACKER) == 0 && class_visible(CLASS_CODECRACKER) == 1);
    assert(class_visible_index(CLASS_ASTRA) >= 0 && classes_visible_count() == 5);
    assert(class_visible_index(CLASS_CODECRACKER) == 4);
    assert(class_cost_of(CLASS_CODECRACKER) == 550);
    assert(strcmp(class_desc_of(CLASS_CODECRACKER), "MOL Edits...") == 0);
    cups = 549;
    pick_class(CLASS_CODECRACKER);
    assert(cups == 549 && class_owned_of(CLASS_CODECRACKER) == 0 && player_class == CLASS_ASTRA);
    cups = 575;
    stub_last_codecracker = 0;
    pick_class(CLASS_CODECRACKER);
    assert(cups == 25 && class_owned_of(CLASS_CODECRACKER) == 1 && player_class == CLASS_CODECRACKER);
    assert(stub_last_codecracker == 1 && codecracker_on() == 1);
    set_class_owned(CLASS_CODECRACKER, 0);
    player_class = CLASS_ORDINARY;
    assert(class_skin_of(CLASS_ASTRA) == SKIN_SPECIAL && current_skin() == SKIN_NORMAL);
    /* A guest cannot use stored seasonal rewards, but the primary CCP admin can. */
    candies = 37;
    set_class_owned(CLASS_SANTA, 1);
    player_class = CLASS_SANTA;
    progress_normalize();
    assert(player_class == CLASS_ORDINARY);
    assert(candies == 37 && class_owned_of(CLASS_SANTA) == 1);
    double saved_season_status = stub_login_status;
    char saved_season_nick[sizeof(stub_login_nick)];
    snprintf(saved_season_nick, sizeof(saved_season_nick), "%s", stub_login_nick);
    stub_login_status = 0;
    astra_ability_refresh();
    assert(class_visible(CLASS_SANTA) == 0 && candies_on() == 0);
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    astra_ability_refresh();
    assert(class_visible(CLASS_SANTA) == 1 && candies_on() == 1 && candies == 37);
    select_class(CLASS_SANTA);
    assert(player_class == CLASS_SANTA);
    stub_login_status = 0;
    astra_ability_refresh();
    player_class = CLASS_ORDINARY;
    progress_normalize();
    assert(player_class == CLASS_ORDINARY);
    stub_login_status = saved_season_status;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_season_nick);
    astra_ability_refresh();
    player->angle = 0;
    player->x = 150;
    player->y = 90;
    double plain_x = player->x;
    start_grab_now();
    assert(fabs(player->x - plain_x) < 1e-9);
    astra_state = 0;
    astra_cd = 0;
    player_class = CLASS_ORDINARY;

    stub_net_slot = -1;
    online_ready = 0;
    screen_w = 1080;
    screen_h = 1920;

    player_class = CLASS_ASTRA;
    player_level = 0;
    game_state = ST_SOLO;
    finished = 0;
    player->x = 400;
    player->y = 400;
    player->angle = 0;
    player->hp = player->max_hp = 10;
    enemy_class = CLASS_ORDINARY;
    enemy->x = 470;
    enemy->y = 400;
    enemy->hp = enemy->max_hp = 10;
    enemy->stun = 0;
    enemy_throw_t = 0;
    astra_cd = 0;
    double saved_dt_astra = dt;
    dt = 0.02;
    start_grab_now();
    assert(player_punching_now() == 1);
    /* Strike has no pre-hit delay: the first update applies the hit. */
    tick_astra();
    assert(player_punching_now() == 1 && fabs(enemy->stun - ASTRA_STRIKE_STUN) < 1e-9);
    for (int frame = 0; frame < 60 && astra_state != 0; frame++)
        tick_astra();
    assert(fabs(enemy->hp - (10.0 - ASTRA_STRIKE_DAMAGE)) < 1e-6);
    assert(enemy_throw_t == 0);
    enemy->stun = 0;

    astra_state = 0;
    astra_punch_t = 0;
    player_class = CLASS_ORDINARY;
    game_state = ST_SOLO;
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    player_stun = 0;
    enemy_class = CLASS_ASTRA;
    enemy_level = 0;
    enemy->x = 400;
    enemy->y = 400;
    enemy->angle = 0;
    enemy->hp = enemy->max_hp = 10;
    pgrab_active = 0;
    pthrow_t = 0;
    enemy_start_grab();
    assert(enemy_grab_punching() == 1);
    tick_enemy_grab();
    tick_astra_fades();
    assert(enemy_grab_punching() == 1);
    assert(pgrab_active == 0 && pthrow_t == 0);
    assert(fabs(player_stun - ASTRA_STRIKE_STUN) < 1e-9);
    for (int frame = 0; frame < 60 && egrab_state != 0; frame++)
        tick_enemy_grab();
    assert(fabs(player->hp - (10.0 - ASTRA_STRIKE_DAMAGE * ENEMY_OTHER_DAMAGE_MULT)) < 1e-6);
    assert(pgrab_active == 0 && pthrow_t == 0);
    player_stun = 0;

    egrab_state = 0;
    egrab_punch_t = 0;
    pgrab_active = 0;
    pthrow_t = 0;
    game_state = ST_ONLINE;
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    player_stun = 0;
    stub_net_slot = 0;
    stub_online[1] = 1;
    stub_level[1] = 0;
    stub_gx[1] = 400.0 / screen_w;
    stub_gy[1] = 400.0 / screen_h;
    stub_gdx[1] = 1;
    stub_gdy[1] = 0;
    stub_grab[1] = 1;
    update_remote_grabs();
    tick_astra_fades();
    assert(remote_grab_punching(1) == 1);
    assert(pgrab_active == 0 && pthrow_t == 0);
    assert(fabs(player_stun - ASTRA_STRIKE_STUN) < 1e-9);
    for (int frame = 0; frame < 60; frame++)
        update_remote_grabs();
    assert(fabs(player->hp - (10.0 - ASTRA_STRIKE_DAMAGE)) < 1e-6);
    assert(pgrab_active == 0 && pthrow_t == 0);
    player_stun = 0;

    /* Level 3 remote Astra strike deals +20% damage with Lv3 stun and no grab/throw. */
    finished = 0;
    player->x = 470;
    player->y = 400;
    player->hp = player->max_hp = 10;
    stub_level[1] = 3;
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ASTRA);
    arr_set(remotes, 1 * remote_fields + 11, SKIN_SPECIAL);
    stub_grab[1] = 2;
    for (int frame = 0; frame < 10; frame++)
        update_remote_grabs();
    assert(pgrab_active == 0 && pthrow_t == 0 && fabs(player_stun - astra_strike_stun_for(3)) < 1e-9);
    dt = saved_dt_astra;
    for (int frame = 0; frame < 60; frame++)
        update_remote_grabs();
    assert(fabs(player->hp - (10.0 - astra_strike_damage_for(3))) < 1e-6);
    player_stun = 0;
    stub_level[1] = 0;
    stub_grab[1] = 0;
    arr_set(remotes, 1 * remote_fields + 10, CLASS_ORDINARY);
    arr_set(remotes, 1 * remote_fields + 11, SKIN_NORMAL);
    stub_net_slot = -1;

    player->hp = 1;
    game_state = ST_ONLINE;
    cups = 999;
    game_reset();
    assert(player->hp == class_base_hp_for(CLASS_ORDINARY));
    assert(game_state == ST_LOBBY);
    assert(cups == 0);

    /* --- admin panel -------------------------------------------------- */
    stub_login_status = 2;
    snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
    snprintf(stub_login_pass, sizeof(stub_login_pass), "%s", "test");
    assert(is_admin() == 1);
    screen_w = 1080;
    screen_h = 2400;
    game_state = ST_LOBBY;
    warn_open = 1; /* the epilepsy warning is up... */
    t_dir = 1;     /* ...and a screen transition is running */
    t_fade = 0.5;
    admin_ball_x = 0;
    admin_ball_y = 0;
    admin_panel_open = 0;
    stub_online[1] = 1;
    snprintf(stub_nicks[1], sizeof(stub_nicks[1]), "remote_guy");
    stub_online[2] = 1;
    snprintf(stub_nicks[2], sizeof(stub_nicks[2]), "qwertyuiopaj1234");
    stub_ban_n = 1;
    snprintf(stub_bans[0], sizeof(stub_bans[0]), "old_cheater");
    stub_last_chat[0] = 0;

    /* The orb is drawn even while the warning and the curtain are up, and it
     * is the last shape drawn, so nothing can cover it. */
    adm_reset_log();
    admin_draw();
    assert(admin_ball_x > 0 && admin_ball_y > 0);
    assert(fabs(adm_last_circle_x - admin_ball_x) < 0.001);
    assert(fabs(adm_last_circle_y - admin_ball_y) < 0.001);
    assert(fabs(adm_last_circle_r - admin_ball_r()) < 0.001);
    /* The menu button got smaller, wears the short panel name and is a black
     * disc with a white rim. */
    assert(admin_ball_r() >= 15 && admin_ball_r() <= 26);
    assert(strcmp(admin_orb_label(), "CCP") == 0);
    assert(strcmp(tr_admin_title(), "CONTENT CREATOR PANEL") == 0);
    /* Plain opaque black disc; the rim and the "CCP" are plain white. */
    assert((stub_circle_color & 0xFFFFFF) == 0);
    for (int i = 0; i < adm_color_n; i++) {
        uint32_t c = adm_colors[i];
        assert(((c >> 16) & 255) == ((c >> 8) & 255) && ((c >> 8) & 255) == (c & 255));
    }

    /* Tapping the orb opens the panel; it draws over the warning and the
     * transition instead of hiding itself. */
    assert(admin_touch(admin_ball_x, admin_ball_y, 0, 1) == 1);
    assert(admin_touch(admin_ball_x, admin_ball_y, 1, 1) == 1);
    assert(admin_panel_open == 1);
    adm_reset_log();
    admin_draw();
    assert(adm_rr_n > 0);
    assert(fabs(adm_rr_x - admin_panel_x()) < 0.001 && fabs(adm_rr_y - admin_panel_y()) < 0.001);
    assert(fabs(adm_rr_w - admin_panel_w()) < 0.001 && fabs(adm_rr_h - admin_panel_h()) < 0.001);
    assert(adm_all_greyscale() == 1);
    /* Flat and opaque: no button in the panel fades, glows or uses alpha. */
    assert(adm_all_flat() == 1);
    /* Compact, but a fifth wider than the old panel: no more full-screen panel. */
    assert(admin_panel_w() < screen_w * 0.75 && admin_panel_h() < screen_h * 0.55);
    /* The panel calls itself a content creator panel and the orb wears the
     * short name; the admin nick and the old "label: on/off" status line under
     * the panel are gone. */
    /* The title, close button, orb, two tabs and all cheat labels. */
    assert(adm_text_n == 5 + ADMIN_CHEATS);
    {
        double saved_lang_ab = language;
        language = 1;
        assert(strcmp(admin_cheat_label(ADMIN_CHEATS - 2), "Авто-блок") == 0);
        assert(strcmp(admin_cheat_label(ADMIN_CHEATS - 1), "Взлом H4c4k0r") == 0);
        language = 0;
        assert(strcmp(admin_cheat_label(ADMIN_CHEATS - 2), "Auto block") == 0);
        assert(strcmp(admin_cheat_label(ADMIN_CHEATS - 1), "H4c4k0r raid") == 0);
        language = saved_lang_ab;
    }
    assert(adm_has_text(tr_admin_title()) == 1);
    assert(adm_has_text(admin_orb_label()) == 1);
    assert(adm_has_text(language == 1 ? "Читы" : "Cheats") == 1);
    assert(adm_has_text(language == 1 ? "Подарки" : "Gifts") == 1);
    assert(adm_has_text("ADMIN") == 0);
    assert(adm_has_text(ADMIN_NICK) == 0);
    assert(adm_has_text(": ") == 0);
    assert(adm_has_text("Шедевро") == 0);
    for (int i = 0; i < ADMIN_CHEATS; i++)
        assert(adm_has_text(admin_cheat_label(i)) == 1);

        /* The panel has separate cheat and gift tabs. */
    adm_reset_log();
    admin_draw();
    assert(adm_all_greyscale() == 1);
    assert(adm_all_flat() == 1);
    for (int i = 0; i < ADMIN_CHEATS; i++)
        assert(adm_has_text(admin_cheat_label(i)) == 1);
    /* The cheat grid starts below the tabs and remains inside the panel. */
    assert(fabs(admin_grid_top() - admin_content_top() - admin_row_h() - admin_gap()) < 0.001);
    assert(admin_grid_top() + 4 * admin_cheat_row_h() + 3 * admin_gap() <=
               admin_panel_y() + admin_panel_h() - admin_pad() + 0.01);

        /* The keyboard writes into the field's own buffer: two fields do not have
         * to share one string. */
    stub_kb_set("typed_nick");
    {
        char fa[32];
        char fb[32];
        keyboard_get_text_to(fa, sizeof(fa));
        keyboard_get_text_to(fb, sizeof(fb));
        assert(strcmp(fa, "typed_nick") == 0 && strcmp(fb, "typed_nick") == 0);
        assert(&fa[0] != &fb[0]);
    }
    stub_kb_set("");

    /* The compact layout has to hold on every screen size. */
    double sizes[4][2] = {{360, 640}, {720, 1280}, {1080, 2400}, {1440, 3200}};
    for (int k = 0; k < 4; k++) {
        screen_w = sizes[k][0];
        screen_h = sizes[k][1];
        assert(admin_panel_w() <= screen_w - 16);
        assert(admin_panel_h() <= screen_h - 16);
        assert(admin_panel_x() >= 0 && admin_panel_y() >= 0);
        assert(admin_panel_w() <= screen_w - 16 && admin_panel_h() <= screen_h - 16);
        assert(admin_panel_w() < screen_w * 0.75 && admin_panel_h() < screen_h * 0.55);
        /* the panel is a fifth wider than the old one: 20% up */
        assert(admin_panel_w() >= screen_w * 0.58);
        assert(admin_content_top() < admin_panel_y() + admin_panel_h() - admin_pad());
        /* seven cheat buttons always fit between the header and the bottom edge */
        assert(admin_grid_top() + 4 * admin_cheat_row_h() + 3 * admin_gap() <=
               admin_panel_y() + admin_panel_h() - admin_pad() + 0.01);
        assert(admin_cheat_row_h() >= 26);
        adm_reset_log();
        admin_draw();
        assert(adm_all_greyscale() == 1);
        assert(adm_all_flat() == 1);
    }
    screen_w = 1080;
    screen_h = 2400;

    /* Any cheat switched on stains the battle: the win pays no Cups, and the
     * mark stays even when the switch goes straight back off. */
    net_ban_set("typed_cheater", 0);
    assert(cheat_god == 0 && cheat_stain == 0);
    admin_press(0); /* god mode on */
    assert(cheat_god == 1 && cheat_stain == 1);
    /* The enabled switch - like the selected tab - only turns grey: the panel
     * never paints a white button, and the fill count says what is on. */
    adm_reset_log();
    admin_draw();
    assert(adm_no_white_fill() == 1);
    assert(adm_fill_count(0x333333) == 2); /* selected tab and enabled cheat */
    admin_press(0); /* ...and straight back off */
    assert(cheat_god == 0 && cheat_stain == 1);
    adm_reset_log();
    admin_draw();
    assert(adm_no_white_fill() == 1);
    assert(adm_fill_count(0x333333) == 1); /* only selected tab remains grey */
    assert(adm_has_text(admin_cheat_label(0)) == 1);
    assert(admin_cups_allowed() == 0);
    cups = 0;
    cup_win = 50;
    cup_lose = 3;
    player->hp = player->max_hp;
    player_was_hit = 0;
    award_battle_result(1);
    assert(cups == 0);
    cups = 30;
    award_battle_result(2);
    assert(cups == 30); /* stained fight: loss also takes no Cups */
    finished = 1;
    adm_reset_log();
    draw_result(tr_victory());
    assert(adm_has_text(tr_cups_cheated()) == 1);
    finished = 2;
    adm_reset_log();
    draw_result(tr_defeat());
    assert(adm_has_text(tr_cups_admin()) == 1);
    finished = 0;

    /* A fresh battle with every switch off awards and deducts Cups normally,
     * even for the admin account. */
    admin_battle_stain_reset();
    assert(admin_cups_allowed() == 1);
    cups = 0;
    award_battle_result(1);
    assert(cups == 50);
    cups = 30;
    award_battle_result(2);
    assert(cups == 27);
    finished = 1;
    adm_reset_log();
    draw_result(tr_victory());
    assert(adm_has_text(tr_cups_reward(50)) == 1);
    finished = 2;
    adm_reset_log();
    draw_result(tr_defeat());
    assert(adm_has_text(tr_cups_lose(-3)) == 1);
    finished = 0;
    cup_lose = 1;

    /* Online battles never move Cups: neither a win nor a loss touches the
     * ladder, and the result card says so instead of printing a number. */
    game_state = ST_ONLINE;
    cups = 40;
    award_battle_result(1);
    assert(cups == 40);
    award_battle_result(2);
    assert(cups == 40);
    finished = 1;
    adm_reset_log();
    draw_result(tr_victory());
    assert(adm_has_text(tr_cups_online()) == 1);
    finished = 2;
    adm_reset_log();
    draw_result(tr_defeat());
    assert(adm_has_text(tr_cups_online()) == 1);
    finished = 0;
    game_state = ST_LOBBY;
    event_mode = 6;
    game_state = ST_ONLINE;
    cups = 10;
    award_battle_result(1);
    assert(cups == 45); /* The event winner gets 35 cups. */
    finished = 1;
    adm_reset_log();
    draw_result(tr_victory());
    assert(adm_has_text("Победа: +35 кубков") == 1);
    finished = 0;
    event_mode = 0;
    game_state = ST_LOBBY;

    /* Every single one of the 8 admin panel switches blocks both cup awards
     * and cup deductions while enabled. */
    for (int i = 0; i < ADMIN_CHEATS; i++) {
        cheat_stain = 0;
        admin_press_cheat(i);
        admin_battle_stain_reset();
        assert(admin_any_cheat_on() == 1 && admin_cups_allowed() == 0);
        cups = 10;
        award_battle_result(1);
        assert(cups == 10);
        award_battle_result(2);
        assert(cups == 10);
        admin_press_cheat(i);
        admin_battle_stain_reset();
        assert(admin_any_cheat_on() == 0 && admin_cups_allowed() == 1);
    }
    cups = 0;

    /* Cheat 6 (ebuC's bounces) draws predicted bounce segments. */
    {
        int calls_off, calls_on, red_hit = 0, red_miss = 0;
        language = 1;
        assert(strcmp(admin_cheat_label(6), "Отскоки буКа") == 0);
        language = 0;
        assert(strcmp(admin_cheat_label(6), "ebuC's bounces") == 0);
        language = 1;
        game_state = ST_SOLO;
        finished = 0;
        player_class = CLASS_EBUC;
        player->x = 300;
        player->y = 500;
        player->angle = 0; /* horizontal right */
        enemy->hp = 20;
        enemy->x = 700;
        enemy->y = 100; /* far above the horizontal y=500 bounce line */
        ball_reset();
        ball_mode = 1;
        adm_reset_log();
        draw_ball();
        calls_off = adm_color_n;
        admin_press_cheat(6);
        assert(admin_cheat_ball_path() == 1);
        adm_reset_log();
        draw_ball();
        calls_on = adm_color_n;
        assert(calls_on >= calls_off + 25);
        for (int ci = 0; ci < adm_color_n; ci++) {
            if ((adm_colors[ci] & 0x00FFFFFFu) == 0x00FF2D2Du)
                red_miss++;
        }
        assert(red_miss == 0);

        enemy->x = 700;
        enemy->y = 500; /* directly on the bounce line -> touching segment turns red */
        adm_reset_log();
        draw_ball();
        for (int ci = 0; ci < adm_color_n; ci++) {
            if ((adm_colors[ci] & 0x00FFFFFFu) == 0x00FF2D2Du)
                red_hit++;
        }
        assert(red_hit >= 2);

        admin_press_cheat(6);
        assert(admin_cheat_ball_path() == 0);
        cheat_stain = 0;
        ball_reset();
        player_class = CLASS_ORDINARY;
    }

    net_ban_set("remote_guy", 0);
    net_ban_set("old_cheater", 0);
    stub_online[1] = 0;
    stub_online[2] = 0;
    stub_nicks[1][0] = 0;
    stub_nicks[2][0] = 0;
    game_state = ST_LOBBY;
    warn_open = 0;
    t_dir = 0;
    admin_panel_close();
    admin_panel_open = 0;
    screen_w = 320;
    screen_h = 180;

    /* Buttons fire on the press itself: the click, the squeeze and the action
     * all happen on touch down, and the squeeze springs back on its own - the
     * finger never has to lift, stay put or be let go gently. Nothing is
     * fired twice by a drag, and the shop follows the same rule for its pill,
     * while the card body waits for the release so a swipe can still page. */
    {
        int saved_h = screen_h;
        screen_h = 2400;
        warn_open = 0;
        studio_open = 0;
        game_state = ST_LOBBY;
        t_dir = 0;
        t_target = 0;

        /* Press the Play row: it goes on the touch itself. */
        double tx = menu_x() + btn_w / 2;
        double ty = menu_row(0) + btn_h / 2;
        logic_touch(tx, ty, 0, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        /* The squeeze is claimed by the button the draw passes under it. */
        assert(press_point_n == 1);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_point_n == 0 && press_phase[0] == 1);
        update_button_press();
        assert(press_k[0] > 0 && press_k[0] < 1); /* easing in */
        assert(button_press_k(menu_x(), menu_row(0), btn_w, btn_h) > 0);
        int n = 0;
        while (n < 3) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k[0] > 0.95); /* fully squeezed and darkened */
        /* The finger is still down - and the button springs back anyway. */
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k[0] == 0 && press_phase[0] == 0);
        assert(button_press_k(menu_x(), menu_row(0), btn_w, btn_h) == 0);
        /* Letting go changes nothing: the row already fired. */
        logic_touch(tx + 4, ty, 2, 1);
        logic_touch(tx + 4, ty, 1, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        t_dir = 0;

        /* A drag off a pressed button does not fire it a second time, and a
         * release in the middle of nowhere fires nothing at all. */
        t_target = 0;
        logic_touch(tx, ty, 0, 1);
        assert(t_dir == 1 && t_target == ST_MODES);
        logic_touch(tx + 200, ty, 2, 1);
        logic_touch(tx + 200, ty, 1, 1);
        assert(t_target == ST_MODES);
        t_dir = 0;
        t_target = 0;
        logic_touch(menu_x() + btn_w / 2, menu_row(0) - 200, 0, 1);
        assert(t_dir == 0);
        logic_touch(menu_x() + btn_w / 2, menu_row(0) - 200, 1, 1);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 0);

        /* Helpy replaces the old left-side cube and opens the lobby dialogue. */
        language = 1;
        helpy_npc_tex_ok = 1;
        draw_lobby_npc_sprite();
        double expected_helpy_draw_sz = 96;
        if (expected_helpy_draw_sz > screen_w - 20) expected_helpy_draw_sz = screen_w - 20;
        if (expected_helpy_draw_sz > screen_h - dial_panel_h - 28)
            expected_helpy_draw_sz = screen_h - dial_panel_h - 28;
        assert(helpy_npc_draw_sz == expected_helpy_draw_sz);
        assert(helpy_npc_x == 20);
        logic_touch(helpy_npc_x + helpy_npc_draw_sz / 2, helpy_npc_y + helpy_npc_draw_sz / 2, 0, 8);
        press_points_drop(8);
        assert(cube_npc_dialogue_active == 1 && cube_npc_speaker == 1);
        assert(strcmp(cube_npc_dialogue_text(), "Привет! Я Хелпи.") == 0);
        /* Tapping outside advances Helpy and never triggers the menu underneath. */
        double ch0 = cube_npc_dialogue_chars;
        logic_touch(tx, ty, 0, 8);
        press_points_drop(8);
        assert(t_dir == 0);
        assert(cube_npc_dialogue_active == 1 && cube_npc_dialogue_chars > ch0);
        n = 0;
        while (cube_npc_dialogue_active == 1 && n < 20) {
            logic_touch(tx, ty, 0, 8);
            press_points_drop(8);
            n = n + 1;
        }
        assert(cube_npc_dialogue_active == 0 && t_dir == 0);
        cube_npc_dialogue_line = 0;
        cube_npc_dialogue_chars = 0;
        language = 0;

        /* The shop buttons fire on the press; a drag no longer cancels them. */
        game_state = ST_CLASSES;
        player_class = CLASS_ORDINARY;
        double lx = levels_btn_x() + class_action_w() / 2;
        double ly = class_actions_y() + btn_h / 2;
        logic_touch(lx, ly, 0, 2);
        assert(t_dir == 1 && t_target == ST_LEVELS);
        t_dir = 0;
        t_target = 0;
        logic_touch(lx, ly, 0, 2);
        logic_touch(lx + 200, ly, 2, 2);
        logic_touch(lx + 200, ly, 1, 2);
        assert(t_dir == 1 && t_target == ST_LEVELS);
        t_dir = 0;

        /* One shop card, sold once: the pill buys on the press, the rest of
         * the card on the release, and a swipe pages instead of buying.
         * The shop pays cups like an ordinary player: the admin nick from the
         * panel block above buys free, so hand the shop to a tester. */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "tester");
        cups = 1000;
        classes_scroll = 0; /* the second card: Azum, 65 cups */
        double v0 = 1;
        double cls = visible_class_at(v0);
        assert(cls != CLASS_ORDINARY && class_owned_of(cls) == 0);
        double card_x = classes_card_x(v0) + classes_card_w() / 2;
        double card_y = classes_top() + class_card_h / 2;
        double pill_x = classes_card_x(v0) + class_pick_inset() + class_pick_w() / 2;
        double pill_y = class_pick_y() + class_pick_h / 2;

        /* The pill is a button: it buys the moment the finger lands. */
        logic_touch(pill_x, pill_y, 0, 3);
        assert(class_owned_of(cls) == 1 && player_class == cls && cups == 1000 - class_cost_of(cls));
        /* The squeeze of a press on the pill belongs to the whole card. */
        assert(press_point_n == 1);
        (void)button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h);
        assert(press_point_n == 0 && press_phase[0] == 1);
        n = 0;
        while (n < 3) {
            update_button_press();
            n = n + 1;
        }
        assert(button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h) > 0.8);
        assert(button_press_k(classes_card_x(v0) + class_pick_inset(), class_pick_y(), class_pick_w(),
                              class_pick_h) == 0);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_k[0] == 0 && press_phase[0] == 0);

        /* The card body takes the press as feedback only: nothing is bought
         * until the finger lifts on the same card. */
        set_class_owned(cls, 0);
        player_class = CLASS_ORDINARY;
        cups = 1000;
        logic_touch(card_x, card_y, 0, 4);
        assert(class_owned_of(cls) == 0 && cups == 1000);
        (void)button_press_k(classes_card_x(v0), classes_top(), classes_card_w(), class_card_h);
        assert(press_phase[0] == 1); /* the card itself is what darkens */
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(class_owned_of(cls) == 0);
        logic_touch(card_x, card_y, 1, 4);
        assert(class_owned_of(cls) == 1 && player_class == cls && cups == 1000 - class_cost_of(cls));

        /* A sideways drag over a card scrolls the strip instead of buying -
         * provided the strip overflows at all in this test's screen size. */
        set_class_owned(cls, 0);
        player_class = CLASS_ORDINARY;
        cups = 1000;
        classes_scroll = 0;
        logic_touch(card_x, card_y, 0, 5);
        logic_touch(card_x - classes_swipe_threshold() - 10, card_y, 2, 5);
        assert(classes_scroll == classes_scroll_needed() * (classes_swipe_threshold() + 10) ||
               classes_scroll == classes_scroll_max());
        logic_touch(card_x - classes_swipe_threshold() - 10, card_y, 1, 5);
        assert(class_owned_of(cls) == 0 && cups == 1000);
        classes_scroll = 0;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);

        /* Button hitboxes are exactly the drawn shape, and the squeeze is
         * claimed by the very same test: a press either fires and squeezes,
         * or does neither. */
        game_state = ST_LOBBY;
        press_points_clear();
        press_pulses_clear();
        t_dir = 0;
        t_target = 0;
        /* Just beside the button (the old invisible touch margin): nothing. */
        logic_touch(menu_x() - 6, menu_row(0) + btn_h / 2, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 0);
        press_points_clear();
        logic_touch(menu_x() + btn_w / 2, menu_row(0) + btn_h + 4, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 0);
        press_points_clear();
        /* The transparent rounded corner is not part of the button either. */
        logic_touch(menu_x() + 2, menu_row(0) + 2, 0, 6);
        assert(t_dir == 0);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 0);
        press_points_clear();
        /* The very edge of the visible shape fires - and squeezes. */
        logic_touch(menu_x() + btn_radius, menu_row(0) + 1, 0, 6);
        assert(t_dir == 1 && t_target == ST_MODES);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 1);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        t_dir = 0;
        t_target = 0;
        /* The shape test itself: rounded corners and round pads. */
        assert(hit_round(3, 3, 0, 0, 100, 40, 20) == 0);
        assert(hit_round(20, 1, 0, 0, 100, 40, 20) == 1);
        assert(hit_round(50, 20, 0, 0, 100, 40, 20) == 1);
        assert(hit_round(101, 20, 0, 0, 100, 40, 20) == 0);
        assert(hit_round(110, 110, 0, 0, 120, 120, 60) == 0);
        assert(hit_round(60 + 59, 60, 0, 0, 120, 120, 60) == 1);

        /* A quick second tap while the first squeeze is still springing back
         * gets its own squeeze instead of firing silently. */
        logic_touch(menu_x() + btn_w / 2, menu_row(0) + btn_h / 2, 0, 7);
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        n = 0;
        while (press_phase[0] != 2 && n < 30) {
            update_button_press();
            n = n + 1;
        }
        update_button_press();
        assert(press_phase[0] == 2);
        t_dir = 0;
        press_points_add(menu_x() + btn_w / 2, menu_row(0) + btn_h / 2, 7); /* the second tap */
        (void)button_press_k(menu_x(), menu_row(0), btn_w, btn_h);
        assert(press_phase[0] == 1 && press_point_n == 0);
        n = 0;
        while (n < 30) {
            update_button_press();
            n = n + 1;
        }
        assert(press_phase[0] == 0);

        /* A touch that lands mid-transition is ignored, so it squeezes nothing. */
        t_dir = 1;
        logic_touch(menu_x() + btn_w / 2, menu_row(1) + btn_h / 2, 0, 8);
        assert(press_point_n == 0);
        t_dir = 0;
        /* Another finger sliding does not drag the press point away. */
        logic_touch(menu_x() + btn_w / 2, menu_row(1) + btn_h / 2, 0, 9);
        t_dir = 0;
        t_target = 0;
        logic_touch(5, 5, 2, 10);
        {
            int pi = 0;
            while (pi < (int)press_point_n && press_point_id[pi] != 9)
                pi = pi + 1;
            assert(pi < (int)press_point_n);
            assert(press_point_x[pi] == menu_x() + btn_w / 2 && press_point_y[pi] == menu_row(1) + btn_h / 2);
        }
        press_points_clear();

        game_state = ST_LOBBY;
        screen_h = saved_h;
    }

    /* The Play screen row 4 now cycles three themes: ordinary -> New Year
     * -> Halloween. The New Year one is grey/unavailable until the holiday flag
     * is on, but stays in the cycle and keeps the ordinary look. */
    {
        double tx = menu_x() + menu_btn_w() / 2;
        double ty = modes_row(4) + btn_h / 2;
        double saved_lang_theme = language;
        double saved_login_theme = stub_login_status;
        double saved_mode_theme = theme_mode;
        double saved_chosen_theme = theme_chosen;
        char saved_nick_theme[sizeof(stub_login_nick)];
        snprintf(saved_nick_theme, sizeof(saved_nick_theme), "%s", stub_login_nick);
        language = 0;
        stub_login_status = 0;
        stub_christmas = 0;
        theme_mode = 0;
        theme_chosen = 0;
        stub_theme_chosen_saved = -1;
        assert(strcmp(tr_theme_label(), "Theme: Ordinary") == 0);
        game_state = ST_MODES;
        t_dir = 0;
        t_target = 0;
        logic_touch(tx, ty, 0, 20);
        assert(theme_mode == 1 && theme_chosen == 1 && t_dir == 0);
        assert(stub_theme_chosen_saved == 1);   /* the choice is persisted */
        assert(winter_theme_on() == 0);         /* locked while the flag is off */
        assert(strcmp(tr_theme_label(), "Theme: New Year (locked)") == 0);
        assert((uint32_t)ui_purple == 0x5F10A0u);        /* grey button, ordinary palette */
        press_points_clear();
        logic_touch(tx, ty, 0, 21);
        assert(theme_mode == 0);   /* only ordinary and New Year remain */
        assert(strcmp(tr_theme_label(), "Theme: Ordinary") == 0);
        assert((uint32_t)ui_purple == 0x5F10A0u);
        press_points_clear();
        press_points_clear();
        /* With the holiday flag on, the New Year theme unlocks. */
        stub_christmas = 1;
        theme_mode = 1;
        assert(winter_theme_on() == 1 && christmas_active() == 1);
        assert(strcmp(tr_theme_label(), "Theme: New Year") == 0);
        stub_christmas = 0;
        apply_theme();
        t_dir = 0;
        language = saved_lang_theme;
        stub_login_status = saved_login_theme;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick_theme);
        theme_mode = saved_mode_theme;
        theme_chosen = saved_chosen_theme;
        /* Settings row 4 is no longer a theme: it opens the documents. */
        game_state = ST_SETTINGS;
        logic_touch(menu_x() + menu_btn_w() / 2, settings_row_y(4) + settings_btn_h() / 2, 0, 23);
        assert(t_dir == 1 && docs_open == 0);
        finish_transition();
        assert(docs_open == 1 && docs_page == 0 && docs_ask == 0);
        assert(logic_back() == 1 && docs_open == 1);
        finish_transition();
        assert(docs_open == 0 && game_state == ST_SETTINGS);
        press_points_clear();

        /* The promo code sits in the bottom-right corner of the modes screen
         * (on a landscape phone-sized screen, as the game runs). */
        double saved_mw = screen_w;
        double saved_mh = screen_h;
        screen_w = 1280;
        screen_h = 720;
        game_state = ST_MODES;
        assert(promo_corner_x() + promo_corner_w() == screen_w - screen_margin);
        assert(promo_corner_y() + btn_h == screen_h - screen_margin);
        assert(promo_corner_x() > menu_x() + menu_btn_w());
        logic_touch(promo_corner_x() + promo_corner_w() / 2, promo_corner_y() + btn_h / 2, 0, 23);
        assert(t_dir == 1 && t_target == ST_PROMO);
        t_dir = 0;
        t_target = 0;

                /* Promo 2027: +27 cups, once; a junk code gives nothing. */
        {
            stub_login_status = 2;
            game_state = ST_PROMO;
            double cups_before = cups;
            promo_focus = 0;
            promo_input = "2027";
            promo_submit();
            assert(cups == cups_before + 27 && stub_promo2027_used == 1);
            promo_submit();
            assert(cups == cups_before + 27);
            promo_input = "1234";
            promo_submit();
            assert(cups == cups_before + 27);
            stub_login_status = 0;
            game_state = ST_MODES;
        }
        press_points_clear();
        /* Row 4 belongs to everyone now. A holiday admin starts on New Year,
         * but an explicit ordinary theme removes every Christmas
         * visual: snow, banner, jingle, candies and snowballs. */
        {
            double saved_mode2 = theme_mode, saved_chosen2 = theme_chosen;
            double saved_link2 = stub_link, saved_xmas2 = stub_christmas;
            double saved_login2 = stub_login_status, saved_candy2 = candy_enabled;
            double saved_lang2 = language, saved_state2 = game_state;
            char saved_nick2[sizeof(stub_login_nick)];
            snprintf(saved_nick2, sizeof(saved_nick2), "%s", stub_login_nick);
            stub_link = 1;
            stub_christmas = 0;
            stub_login_status = 2;
            candy_enabled = 1;
            language = 0;
            game_state = ST_MODES;
            snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
            astra_ability_refresh();
            theme_mode = 0;
            theme_chosen = 0;
            assert(active_theme_mode() == 1);
            assert(winter_theme_on() == 1 && christmas_active() == 1);
            assert(christmas_snowballs_on() == 1 && candies_on() == 1);
            assert(strcmp(tr_theme_label(), "Theme: New Year") == 0);
            t_dir = 0;
            press_points_clear();
            logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 24);
            assert(theme_mode == 0 && theme_chosen == 1 && t_dir == 0);
            assert(strcmp(tr_theme_label(), "Theme: Ordinary") == 0);
            assert(winter_theme_on() == 0 && christmas_active() == 0);
            assert(christmas_snowballs_on() == 0 && candies_on() == 0);
            press_points_clear();
            logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 25);
            assert(theme_mode == 1 && strcmp(tr_theme_label(), "Theme: New Year") == 0);
            assert(winter_theme_on() == 1 && christmas_active() == 1);
            assert(christmas_snowballs_on() == 1 && candies_on() == 1);
            press_points_clear();
            logic_touch(menu_x() + menu_btn_w() / 2, modes_row(4) + btn_h / 2, 0, 26);
            assert(theme_mode == 0);
            assert(winter_theme_on() == 0 && christmas_active() == 0);
            assert(christmas_snowballs_on() == 0 && candies_on() == 0);
            assert(bpass_allowed() == 1);       /* earned progression stays */
            theme_mode = saved_mode2;
            theme_chosen = saved_chosen2;
            stub_link = saved_link2;
            stub_christmas = saved_xmas2;
            stub_login_status = saved_login2;
            candy_enabled = saved_candy2;
            language = saved_lang2;
            game_state = saved_state2;
            snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick2);
            astra_ability_refresh();
            press_points_clear();
        }
        screen_w = saved_mw;
        screen_h = saved_mh;
        game_state = ST_LOBBY;
    }

    test_legal();
    test_start_gate();
    test_promo_message();
    test_azum_buff();
    test_class_stars();
    test_codecracker_fx();
    test_fps_meter();
    test_eax229_and_rooms();
    test_dice_alone_and_bubbles();
    test_battle_events();
    test_hacker_event();
    test_overlay_transitions();
    test_gifts_page();
    test_admin_keyboard();

    /* Christmas theme and Battle Pass are available to all CCP moderators;
     * only the primary admin has the wider CCP/Gifts panel. */
    {
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "qwertyuiopaj1234");
        astra_ability_refresh();
        assert(is_admin() == 0 && admin_modifiers_allowed() == 1);
        assert(seasonal_content_allowed() == 1 && bpass_allowed() == 1 && bpass_rewards_allowed() == 1);
        bp_xp = 0;
        bp_claimed = 0;
        candies = 1000;
        cups = 0;
        stub_bpx = 0;
        award_battle_result(1);
        assert(bp_xp == 10 && stub_bpx == 10);
        double qwerty_candies_before_claim = candies;
        bp_claim(1);
        assert(bp_claimed == 1 && fabs(candies - (qwerty_candies_before_claim - 12)) < 1e-9);
        bp_xp = 300;
        bp_claimed = 7;
        progress_normalize();
        assert(class_owned_of(CLASS_SANTA) == 1 && class_visible(CLASS_SANTA) == 1);
        set_class_owned(CLASS_SANTA, 0);
        player_class = CLASS_ORDINARY;
        bp_xp = 0;
        bp_claimed = 0;
        candies = 1000;
        cups = 0;
        stub_bpx = 0;

        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
        astra_ability_refresh();
        assert(is_admin() == 1);
        bp_xp = 0;
        bp_claimed = 0;
        candies = 1000;
        cups = 0;
        stub_bpx = 0;
        award_battle_result(1);
        assert(bp_xp == 10 && stub_bpx == 10);
        double cups_after_win = cups;
        double candies_after_win = candies;
        assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        bp_claim(2); /* out of order: nothing happens */
        assert(bp_claimed == 0 && candies == candies_after_win);
        bp_claim(1);
        assert(bp_claimed == 1 && cups == cups_after_win + 10 && candies == candies_after_win - 12);
        assert(stub_last_bp == 1); /* claimed tier rides the cloud save (field "bp") */
        bp_xp = 300;
        bp_claim(2);
        bp_claim(3);
        assert(bp_claimed == 3 && skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        double cups_before_t4 = cups;
        bp_claim(4);
        assert(bp_claimed == 4 && fabs(cups - (cups_before_t4 + 45)) < 1e-9);
        assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        assert(class_owned_of(CLASS_SANTA) == 0);
        bp_claim(5);
        bp_claim(6);
        bp_claim(7);
        assert(bp_claimed == 7 && class_owned_of(CLASS_SANTA) == 1);
        assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0);
        assert(stub_last_bp == 7);
        assert(class_visible(CLASS_SANTA) == 1);
        bp_xp = 0;
        bp_claimed = 0;
        stub_bpx = 0;
    }

    /* Combat update:
     * 1) Frontal block absorbs 75% of incoming damage (block_guard = 0.25).
     * 2) Astra's super starts as an instant-on-press 0.4 s purple shield that
     *    blocks 100% of damage; blocking >=1 hit unlocks Grab for 0.8 s, and
     *    only then can Grab fire (otherwise Grab disappears back to Block + CD).
     * 3) Enemy punch switches sprite and hitbox immediately, with no pre-hit delay. */
    {
        assert(fabs(block_guard - 0.25) < 1e-9);
        dt = 1.0 / 60.0;

        /* 1) Normal frontal block cuts 75% of damage for player and bot. */
        game_state = ST_SOLO;
        finished = 0;
        cheat_god = 0;
        cheat_nocd = 0;
        cheat_damage = 0;
        player_class = CLASS_ORDINARY;
        player_level = 0;
        player->max_hp = 20;
        player->hp = 20;
        player->x = 400;
        player->y = 400;
        player->angle = 0;
        player_block = 1;
        take_damage_at(4.0, 500, 400);
        assert(fabs(player->hp - 19.0) < 1e-9);
        player_block = 0;

        enemy_class = CLASS_ORDINARY;
        enemy_revive_prot = 0;
        enemy->max_hp = 20;
        enemy->hp = 20;
        enemy->x = 500;
        enemy->y = 400;
        enemy->angle = pi;
        enemy_block_left = 0.5;
        enemy_apply_damage_at(4.0, 400, 400);
        assert(fabs(enemy->hp - 19.0) < 1e-9);
        enemy_block_left = 0;

        /* 2a) Astra super activates immediately on press as a 0.4 s purple shield
         *     with zero slowdown (normal walk speed and normal punch allowed);
         *     expiring unhit starts cooldown without unlocking Strike. */
        screen_w = 1080;
        screen_h = 1920;
        atk_x = screen_w - 132;
        atk_y = screen_h - 138;
        atk2_x = screen_w - 132;
        atk2_y = screen_h - 330;
        blk_x = screen_w - 258;
        blk_y = screen_h - 234;
        blk_id = -1;
        player_block = 0;
        player_class = CLASS_ASTRA;
        player_level = 0;
        player->max_hp = 18;
        player->hp = 18;
        astra_state = 0;
        astra_cd = 0;
        astra_parry_t = 0;
        astra_parry_blocked = 0;
        astra_grab_ready_t = 0;
        player_stun = 0;
        player_freeze = 1.5;
        player_freeze_slow = 0.12;
        pgrab_active = 0;
        language = 1;
        adm_reset_log();
        draw_controls();
        assert(adm_has_text("Блок") == 1 && adm_has_text("Захват") == 0);

        touch_game(atk2_x, atk2_y, 0, 7); /* press only - not released! */
        assert(astra_state == 0);
        assert(astra_parry_active() == 1 && fabs(astra_parry_t - 0.7) < 1e-9);
        assert(player_freeze == 0 && fabs(player_walk_speed() - joy_speed * 0.90) < 1e-9);
        player_block = 1;
        assert(fabs(player_walk_speed() - joy_speed * 0.90) < 1e-9);
        player_block = 0;
        punch->active = 0;
        punch_left = 0;
        punch_cd = 0;
        fire_punch();
        assert(punch->active == 1);
        /* Pressing punch again immediately during cooldown does NOT queue instant follow-up. */
        fire_punch();
        assert(queued_punch == 0);
        punch->active = 0;
        punch_left = 0;
        punch_cd = 0;
        assert(astra_grab_ready_t == 0);
        stub_circle_color = 0;
        draw_purple_guard(player->x, player->y, astra_parry_active(), player_scale);
        assert(stub_circle_color == 0x38B35CFF);
        assert(fabs(adm_last_circle_r - 25 * cube_scale * player_scale * 1.75 * 0.90) < 1e-4);
        /* Dash activates attack sprite for both player and bot Azum. */
        dash_active = 1;
        assert(player_punching_now() == 1);
        dash_active = 0;
        enemy_dash_active = 1;
        assert(enemy_punching_now() == 1);
        enemy_dash_active = 0;

        /* Bot Astra also keeps acting while its purple block slows walk by 10%. */
        enemy_class = CLASS_ASTRA;
        enemy_start_astra_parry();
        assert(egrab_parry_t > 0 && enemy_tick_class_abilities() == 0);
        egrab_parry_t = 0;

        for (int f = 0; f < 44; f++)
            tick_astra();
        assert(astra_parry_t == 0 && astra_grab_ready_t == 0 && astra_state == 0);
        assert(fabs(astra_cd - astra_grab_miss_cd) < 0.05);
        touch_game(atk2_x, atk2_y, 1, 7);

        /* 2b) Blocking >=1 hit absorbs 100% of damage + stun, waits 0.42 s before
         *     Strike can be pressed, then keeps it open for 0.85 s. */
        astra_cd = 0;
        player->hp = 19;
        touch_game(atk2_x, atk2_y, 0, 7);
        touch_game(atk2_x, atk2_y, 1, 7);
        assert(astra_parry_active() == 1);
        take_damage_at(6.5, 500, 400);
        stun_player(0.55);
        assert(fabs(player->hp - 19.0) < 1e-9 && player_stun == 0);
        assert(fabs(astra_strike_unlock_t - 0.42) < 1e-9 && astra_grab_ready_t == 0);
        /* Pressing immediately during the 0.42 s pause cannot cast Strike yet. */
        touch_game(atk2_x, atk2_y, 0, 8);
        touch_game(atk2_x, atk2_y, 1, 8);
        assert(astra_state == 0 && astra_grab_ready_t == 0 && astra_strike_unlock_t > 0);
        int unlock_ticks = 0;
        while (astra_strike_unlock_t > 0 && unlock_ticks < 40) {
            tick_astra();
            unlock_ticks++;
        }
        assert(unlock_ticks >= 25 && unlock_ticks <= 26);
        assert(fabs(astra_grab_ready_t - 0.85) < 1e-9);
        adm_reset_log();
        draw_controls();
        assert(adm_has_text("Захват") == 0);
        assert(strcmp(tr_super_astra(), "Удар") == 0);

        for (int f = 0; f < 50; f++)
            tick_astra();
        assert(astra_grab_ready_t > 0 && astra_cd == 0);
        tick_astra();
        tick_astra();
        assert(astra_grab_ready_t == 0 && astra_state == 0);
        assert(fabs(astra_cd - astra_grab_miss_cd) < 0.05);

        /* 2c) Pressing after the 0.42 s unlock pause performs pullback + 245 px forward dash + stun. */
        astra_cd = 0;
        player->x = 400;
        player->y = 400;
        player->angle = 0;
        arr_set(fighter_swings, FLOOK_ME, 1);
        enemy_class = CLASS_ORDINARY;
        enemy->x = 470;
        enemy->y = 400;
        enemy->hp = enemy->max_hp = 20;
        enemy->stun = 0;
        touch_game(atk2_x, atk2_y, 0, 7);
        touch_game(atk2_x, atk2_y, 1, 7);
        take_damage_at(3.0, 500, 400);
        assert(fabs(astra_strike_unlock_t - 0.42) < 1e-9 && fabs(player->hp - 19.0) < 1e-9);
        while (astra_strike_unlock_t > 0)
            tick_astra();
        assert(fabs(astra_grab_ready_t - 0.85) < 1e-9);
        touch_game(atk2_x, atk2_y, 0, 8);
        touch_game(atk2_x, atk2_y, 1, 8);
        assert(astra_state == 1 && astra_grab_ready_t == 0 && astra_parry_t == 0);
        assert(fabs(astra_ability_lunge_tx - (400.0 + astra_ability_lunge_dist)) < 1e-6 && player_punching_now() == 1);
        {
            double saved_dt2 = dt;
            dt = 0.02;
            int ab_frames = (int)(ASTRA_STRIKE_DELAY / dt + 0.999) - 1;
            for (int f = 0; f < ab_frames; f++)
                tick_astra();
            assert(enemy->stun == 0);
            tick_astra();
            assert(player_punching_now() == 1 && fighter_pose_of(FLOOK_ME, player_punching_now()) == 1);
            assert(fabs(enemy->stun - ASTRA_STRIKE_STUN) < 1e-9 && fabs(enemy->hp - (20.0 - ASTRA_STRIKE_DAMAGE)) < 1e-6);
            for (int f = 0; f < 60 && astra_state != 0; f++)
                tick_astra();
            assert(astra_state == 0 && fabs(astra_cd - astra_cd_for(0)) < 1e-9);
            for (int f = 0; f < 60 && player_punching_now() == 1; f++)
                tick_astra();
            assert(player_punching_now() == 0);
            dt = saved_dt2;
        }
        /* Enemy HP bar fills left-to-right like player's HP bar. */
        enemy->hp = 10;
        enemy->max_hp = 20;
        stub_enemy_hp_fill_x = -1;
        draw_game();
        assert(fabs(stub_enemy_hp_fill_x - (screen_w - screen_w / 4 - 20 + 4)) < 0.05);
        enemy->stun = 0;
        astra_cd = 0;

        /* 3) Enemy punch shows punch sprite and activates its hitbox immediately. */
        player_class = CLASS_ORDINARY;
        player->x = 460;
        player->y = 400;
        player->hp = 20;
        player->max_hp = 20;
        revive_prot = 0;
        enemy_class = CLASS_ORDINARY;
        enemy->x = 400;
        enemy->y = 400;
        enemy->angle = 0;
        enemy->hp = 20;
        enemy->max_hp = 20;
        enemy->stun = 0;
        enemy->state = 0;
        enemy_punch_a = 0;
        assert(start_enemy_attack() == 1);
        assert(enemy->state == 2 && enemy_punching_now() == 1);
        update_enemy();
        tick_hitbox_fades();
        assert(enemy_punch_a > 0);
        update_enemy();
        assert(player->hp < 20.0);
        enemy->state = 0;
        enemy_punch_a = 0;
        game_state = ST_LOBBY;
    }

    /* CCP modifiers remain separate from full admin rights and gift access. */
    {
        assert(admin_slot("Dimasi4ek229") == 1 && admin_slot("dimasi4ek229") == 1);
        assert(admin_slot("Sanch") == 0 && admin_slot("sanch") == 0 && admin_slot("SANCH") == 0);
        assert(admin_slot("qwertyuiopaj1234") == 0 && admin_slot("QWERTYUIOPAJ1234") == 0);
        assert(strcmp(admin_nick_of(1), "Dimasi4ek229") == 0);
        assert(((uint32_t)admin_color_of("Sanch", 0xFFFFFFFF) & 0xFFFFFFu) == 0xFFD54Au);
        assert(((uint32_t)admin_head_color_of("Sanch", 0xFFFFFFFF) & 0xFFFFFFu) == 0xFFDD33u);
        assert(((uint32_t)admin_color_of("qwertyuiopaj1234", 0xFFFFFFFF) & 0xFFFFFFu) == 0x4FC3F7u);
        assert(((uint32_t)admin_head_color_of("qwertyuiopaj1234", 0xFFFFFFFF) & 0xFFFFFFu) == 0x33A8FFu);
        assert(((uint32_t)admin_color_of("Dimasi4ek229", 0xFFFFFFFF) & 0xFFFFFFu) == 0xFF4444u);
        assert((uint32_t)admin_color_of("Player2", 0xFFFFFFFF) == 0xFFFFFFFFu);
        assert(nick_shimmers("Dimasi4ek229") == 1 && nick_shimmers("Sanch") == 1 &&
               nick_shimmers("qwertyuiopaj1234") == 1 && nick_shimmers("Player2") == 0);

        /* qwertyuiopaj1234: sky-blue shimmering nick and modifier-only CCP (no gift management). */
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "qwertyuiopaj1234");
        astra_ability_refresh();
        assert(is_admin() == 0 && admin_playing() == 0);
        winter_theme = 1;
        candy_enabled = 1;
        assert(dev_access() == 0 && seasonal_content_allowed() == 1);
        assert(winter_theme_on() == 1 && bpass_allowed() == 1 && bpass_rewards_allowed() == 1);
        assert(candies_on() == 1 && admin_modifiers_allowed() == 1 && admin_gifts_allowed() == 0);
        shimmer_text_n = 0;
        draw_lobby_nick();
        assert(shimmer_text_n == 1);
        adm_reset_log();
        language = 1;
        game_state = ST_LOBBY;
        admin_panel_open = 1;
        admin_tab = 1;
        admin_panel_press(admin_panel_x() + admin_panel_w() * 0.75, admin_content_top() + 8);
        assert(admin_tab == 0);
        adm_reset_log();
        admin_draw();
        assert(adm_has_text("CCP") == 1 && adm_has_text("Модификаторы") == 1);
        assert(adm_has_text("Подарки") == 0 && adm_has_text("Выдать подарок") == 0);
        admin_panel_open = 0;

        /* Without the global flag, Sanch has ordinary visuals and no event,
         * while earned rewards remain. The live Christmas flag then grants
         * candies and Battle Pass to him like every other online player. */
        double saved_global_xmas = stub_christmas;
        stub_christmas = 0;
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "Sanch");
        astra_ability_refresh();
        winter_theme = 1;
        candy_enabled = 1;
        bp_claimed = 7;
        bp_xp = 300;
        candies = 77;
        set_class_owned(CLASS_SANTA, 1);
        player_class = CLASS_SANTA;
        assert(is_admin() == 0 && admin_playing() == 0);
        assert(dev_access() == 0 && sanch_no_christmas() == 1 && sanch_rewards_kept() == 1);
        assert(seasonal_content_allowed() == 0 && winter_theme_on() == 0 && christmas_active() == 0);
        assert(christmas_snowballs_on() == 0 && christmas_pad_hit(10, 10) == 0);
        assert(bpass_allowed() == 0 && bpass_rewards_allowed() == 1 && candies_on() == 0);
        assert(admin_modifiers_allowed() == 1 && admin_gifts_allowed() == 0);

        double saved_theme_mode_sanch = theme_mode;
        double saved_theme_chosen_sanch = theme_chosen;
        stub_christmas = 1;
        theme_chosen = 1;
        theme_mode = 0; /* chosen ordinary visuals do not hide the live event */
        assert(seasonal_content_allowed() == 1 && winter_theme_on() == 0);
        assert(bpass_allowed() == 1 && bpass_rewards_allowed() == 1 && candies_on() == 1);
        assert(christmas_snowballs_on() == 1);
        stub_christmas = 0;
        theme_mode = saved_theme_mode_sanch;
        theme_chosen = saved_theme_chosen_sanch;

        progress_normalize();
        assert(fabs(candies - 77.0) < 1e-9);
        assert(class_owned_of(CLASS_SANTA) == 1 && class_visible(CLASS_SANTA) == 1 && player_class == CLASS_SANTA);
        /* A hidden Battle Pass cannot be claimed or spend his candies. */
        bp_claim(1);
        assert(bp_claimed == 7 && fabs(candies - 77.0) < 1e-9);
        adm_reset_log();
        language = 1;
        draw_lobby_money();
        assert(candies_on() == 0 && fabs(candies - 77.0) < 1e-9);
        /* Elf ebuC still needs the online achievement. */
        achievement_mask = 0;
        ebuc_skin = SKIN_SPECIAL;
        progress_normalize();
        assert(skin_unlocked_for(CLASS_EBUC, SKIN_SPECIAL) == 0 && ebuc_skin == SKIN_NORMAL);

        /* Efy gets the moderator CCP without gift management or primary admin rights. */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "Efy");
        astra_ability_refresh();
        assert(is_admin() == 0 && admin_playing() == 0 && admin_slot("Efy") == 0);
        assert(admin_modifiers_allowed() == 1 && admin_gifts_allowed() == 0);
        admin_panel_open = 1;
        admin_tab = 1;
        admin_panel_press(admin_panel_x() + admin_panel_w() * 0.75, admin_content_top() + 8);
        assert(admin_tab == 0);
        adm_reset_log();
        admin_draw();
        assert(adm_has_text("CCP") == 1 && adm_has_text("Подарки") == 0 && adm_has_text("Выдать подарок") == 0);
        admin_panel_open = 0;

        /* Sanch sees CCP with regular modifiers only and no gift management tab. */
        adm_reset_log();
        game_state = ST_LOBBY;
        admin_panel_open = 1;
        admin_tab = 1;
        admin_panel_press(admin_panel_x() + admin_panel_w() * 0.75, admin_content_top() + 8);
        assert(admin_tab == 0);
        adm_reset_log();
        language = 1;
        admin_draw();
        assert(adm_has_text("CCP") == 1 && adm_has_text("Модификаторы") == 1);
        assert(adm_has_text("Подарки") == 0 && adm_has_text("Выдать подарок") == 0);
        cheat_god = 1;
        cheat_damage = 1;
        assert(admin_cheat_god() == 1 && fabs(admin_cheat_damage_mult() - 5.0) < 1e-9);
        cheat_god = 0;
        cheat_damage = 0;
        admin_panel_open = 0;
        bp_claimed = 0;
        bp_xp = 0;
        winter_theme = 0;
        player_class = CLASS_ORDINARY;
        set_class_owned(CLASS_SANTA, 0);

        stub_christmas = saved_global_xmas;

        /* Dimasi4ek229 retains full CCP (both Cheats & Gifts), winter theme, and BP (Tier 6 = +100 cups). */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "Dimasi4ek229");
        astra_ability_refresh();
        winter_theme = 1;
        assert(is_admin() == 1 && dev_access() == 1 && winter_theme_on() == 1);
        assert(bpass_allowed() == 1 && candies_on() == 1 && admin_gifts_allowed() == 1);
        adm_reset_log();
        language = 1;
        admin_panel_open = 1;
        admin_draw();
        assert(adm_has_text("CCP") == 1 && adm_has_text("Читы") == 1 && adm_has_text("Подарки") == 1);
        admin_panel_open = 0;
        winter_theme = 0;

        candies = 500;
        cups = 100;
        bp_xp = 300;
        for (int t = 1; t <= 7; t++) {
            language = 0;
            assert(strstr(bp_reward_text(t), "cand") == NULL);
            language = 1;
            assert(strstr(bp_reward_text(t), "конфет") == NULL);
        }
        for (int t = 1; t <= 5; t++)
            bp_claim(t);
        double cups_before_t6 = cups;
        double candies_before_t6 = candies;
        bp_claim(6);
        assert(bp_claimed == 6 && fabs(cups - (cups_before_t6 + 100)) < 1e-9);
        assert(fabs(candies - (candies_before_t6 - 95)) < 1e-9);
        bp_claimed = 0;
        bp_xp = 0;

        stub_frame_seconds = 0.05;
        gift_shimmer_t = 0;
        uint32_t hue0 = gift_emerald_tint();
        for (int f = 0; f < 16; f++) gift_shimmer_tick();
        uint32_t hue1 = gift_emerald_tint();
        for (int f = 0; f < 16; f++) gift_shimmer_tick();
        uint32_t hue2 = gift_emerald_tint();
        assert(gift_shimmer_t > 0 && hue0 != hue1 && hue1 != hue2 && hue0 != hue2);
        stub_frame_seconds = 0;
    }

    /* Combat & UI regressions: single-collapse remote Universe, join baselining,
     * try_revive debuff clear + tiny-screen guard, revive_prot guards, and rooms_open reset. */
    {
        /* 1) Remote Universe collapses once per cast counter increment (no infinite loop). */
        screen_w = 1280;
        screen_h = 720;
        dt = 0.1;
        finished = 0;
        game_state = ST_ONLINE;
        stub_net_slot = 0;
        stub_online[2] = 1;
        stub_level[2] = 0;
        stub_universe[2] = 1;
        stub_ux[2] = 0.5;
        stub_uy[2] = 0.5;
        player_class = CLASS_ORDINARY;
        player->max_hp = 100;
        player->hp = 100;
        player_block = 0;
        revive_prot = 0;
        arr_set(remote_universe, 2 * universe_fields, 0);
        arr_set(remote_universe, 2 * universe_fields + 1, 0);
        for (int f = 0; f < 40; f++)
            update_remote_universe();
        double hp_after_one = player->hp;
        assert(hp_after_one < 100.0 && hp_after_one > 0.0);
        for (int f = 0; f < 40; f++)
            update_remote_universe();
        assert(fabs(player->hp - hp_after_one) < 1e-9);

        /* 2) Joining mid-match baselines remote counters so pre-join abilities do not replay. */
        stub_universe[3] = 7;
        stub_brick[3] = 4;
        stub_online[3] = 1;
        stub_ralive[3] = 1;
        stub_hp[3] = 20;
        stub_class[3] = CLASS_EBUC;
        arr_set(remotes, 3 * remote_fields, 0);
        remote_bricks_reset();
        read_remotes();
        remote_bricks_tick();
        assert(rb_on[3] == 0);
        for (int f = 0; f < 40; f++)
            update_remote_universe();
        assert(fabs(player->hp - hp_after_one) < 1e-9);
        stub_online[2] = 0;
        stub_online[3] = 0;

        /* 3) rb_burst and online CodeCracker bite respect revive_prot invulnerability. */
        player->x = 400;
        player->y = 300;
        player->hp = 20;
        player->max_hp = 20;
        revive_prot = 1.0;
        rb_bounces[1] = 3;
        rb_burst(1, 400, 300);
        assert(fabs(player->hp - 20.0) < 1e-9);

        stub_online[1] = 1;
        stub_ralive[1] = 1;
        stub_hp[1] = 12;
        stub_class[1] = CLASS_CODECRACKER;
        stub_cc_super[1] = 1;
        stub_rx[1] = 400.0 / screen_w;
        stub_ry[1] = 300.0 / screen_h;
        arr_set(remotes, 1 * remote_fields, 0);
        read_remotes();
        arr_set(codecracker_touch_cds, max_players + 1, 0);
        codecracker_tick_online();
        assert(fabs(player->hp - 20.0) < 1e-9);
        stub_online[1] = 0;
        stub_cc_super[1] = 0;
        revive_prot = 0;

        /* 4) try_revive clears stun/freeze/poison and handles tiny screens safely. */
        game_state = ST_SOLO;
        player_class = CLASS_AZUM;
        azum_revived = 0;
        player->max_hp = 20;
        player->hp = 0;
        player_stun = 1.5;
        player_freeze = 2.0;
        player_poison = 3.0;
        screen_w = 120;
        screen_h = 100;
        player->x = 60;
        player->y = 50;
        assert(try_revive() == 1);
        assert(fabs(player->hp - 20.0) < 1e-9);
        assert(player_stun == 0 && player_freeze == 0 && player_poison == 0);
        assert(player->x >= 0 && player->x <= screen_w && player->y >= 0 && player->y <= screen_h);
        screen_w = 1280;
        screen_h = 720;

        /* 5) rooms_open clears stale create/join input buffers. */
        snprintf(rooms_name_buf, sizeof(rooms_name_buf), "%s", "stale-room");
        snprintf(rooms_pass_buf, sizeof(rooms_pass_buf), "%s", "stale-pass");
        rooms_name = rooms_name_buf;
        rooms_pass = rooms_pass_buf;
        rooms_open();
        assert(rooms_name_buf[0] == 0 && rooms_pass_buf[0] == 0);
        assert(str_len(rooms_name) == 0 && str_len(rooms_pass) == 0);
        game_state = ST_LOBBY;
        dt = 1.0 / 60.0;
    }

    /* Bot ebuC mode switching and Santa alternating straight / zigzag (-10% size) snowflakes. */
    {
        /* 1) Bot ebuC alternates between dispenser turret (mode 0) and bouncing projectile (mode 1). */
        screen_w = 1280;
        screen_h = 720;
        dt = 0.05;
        finished = 0;
        game_state = ST_SOLO;
        enemy_reset_class_state();
        enemy_class = CLASS_EBUC;
        enemy->x = 300;
        enemy->y = 360;
        enemy->hp = 20;
        enemy->max_hp = 20;
        enemy->state = 0;
        player->x = 700;
        player->y = 360;
        player->hp = 20;
        player->max_hp = 20;
        player_block = 0;
        revive_prot = 0;
        assert(enemy_ball_mode == 0 && enemy_turret_count_alive() == 0);

        /* First cast: places turret and switches enemy_ball_mode to 1. */
        enemy_super_cd = 0;
        assert(enemy_tick_class_abilities() == 1);
        assert(enemy_super_kind == 0 && enemy_ball_mode == 1);
        assert(enemy_turret_count_alive() == 1 && enemy_ball_active() == 0);

        /* Second cast: fires bouncing projectile and switches enemy_ball_mode back to 0. */
        enemy_super_cd = 0;
        assert(enemy_tick_class_abilities() == 1);
        assert(enemy_super_kind == 0 && enemy_ball_mode == 0);
        assert(enemy_ball_active() == 1);
        for (int f = 0; f < 40 && enemy_ball_active() == 1; f++)
            tick_enemy_class();
        assert(enemy_ball_active() == 0 && player->hp < 20.0);

        /* When all 3 turret slots are full, bot ebuC forces projectile mode even if mode was 0. */
        enemy_place_turret();
        enemy_place_turret();
        assert(enemy_turret_count_alive() == 3);
        enemy_ball_mode = 0;
        enemy_super_cd = 0;
        assert(enemy_tick_class_abilities() == 1);
        assert(enemy_super_kind == 0 && enemy_ball_active() == 1);
        enemy_reset_class_state();

        /* 2) Santa alternates: cast 1 normal straight, cast 2 10%-smaller left-right zigzag, cast 3 straight, cast 4 zigzag. */
        assert(snow_is_zigzag(1) == 0 && fabs(snow_scale_for(1) - 1.0) < 1e-9 && fabs(snow_radius_for(1) - 24.0) < 1e-9);
        assert(snow_is_zigzag(2) == 1 && fabs(snow_scale_for(2) - 0.9) < 1e-9 && fabs(snow_radius_for(2) - 21.6) < 1e-9);
        assert(snow_is_zigzag(3) == 0 && fabs(snow_scale_for(3) - 1.0) < 1e-9);
        assert(snow_is_zigzag(4) == 1 && fabs(snow_scale_for(4) - 0.9) < 1e-9);
        /* Zigzag amplitude and frequency are both about 1.5x the old values. */
        assert(fabs(snow_zigzag_lateral((0.20 / 1.5) * 0.25) - 48.0) < 1e-9);
        assert(fabs(snow_zigzag_lateral((0.20 / 1.5) * 0.75) + 48.0) < 1e-9);
        assert(fabs(snow_zigzag_lateral(0.20 / 1.5)) < 1e-9);

        player_class = CLASS_SANTA;
        player->x = 200;
        player->y = 360;
        player->angle = 0;
        enemy->x = 1100;
        enemy->y = 360;
        enemy->hp = 20;
        gift->active = 0;
        gift->shot = 0;
        dt = 0.025;

        /* Cast 1: straight normal snowflake along +X (y stays at 360). */
        start_super_effect_now();
        assert(gift->active == 1 && gift->shot == 1);
        for (int f = 0; f < 10; f++) {
            update_gift();
            assert(fabs(gift->y - 360.0) < 1e-9);
        }
        assert(gift->x > 200.0);
        gift->active = 0;

        /* Cast 2: 10%-smaller zigzag snowflake moves forward along +X while oscillating both +Y and -Y. */
        start_super_effect_now();
        assert(gift->active == 1 && gift->shot == 2);
        double min_y = 360.0, max_y = 360.0, prev_x = gift->x;
        for (int f = 0; f < 12; f++) {
            update_gift();
            assert(gift->x > prev_x);
            prev_x = gift->x;
            if (gift->y < min_y) min_y = gift->y;
            if (gift->y > max_y) max_y = gift->y;
        }
        assert(max_y > 375.0 && min_y < 345.0);
        gift->active = 0;

        /* Bot Santa and online remote Santa also alternate straight (shot 1) then zigzag (shot 2). */
        enemy_class = CLASS_SANTA;
        enemy->x = 900;
        enemy->y = 360;
        player->x = 200;
        player->y = 360;
        enemy_gift->shot = 0;
        enemy_start_snow();
        assert(enemy_gift->shot == 1);
        tick_enemy_gift();
        assert(fabs(enemy_gift->y - 360.0) < 1e-9);
        enemy_gift->active = 0;
        enemy_start_snow();
        assert(enemy_gift->shot == 2);
        tick_enemy_gift();
        tick_enemy_gift();
        assert(fabs(enemy_gift->y - 360.0) > 5.0 && enemy_gift->x < 900.0);
        enemy_gift->active = 0;

        game_state = ST_ONLINE;
        stub_net_slot = 0;
        stub_online[1] = 1;
        stub_snow[1] = 2;
        stub_sx[1] = 300.0 / screen_w;
        stub_sy[1] = 360.0 / screen_h;
        stub_sdx[1] = 1.0;
        stub_sdy[1] = 0.0;
        player->x = 1100;
        player->y = 360;
        arr_set(remote_snow, 1 * snow_fields, 1);
        arr_set(remote_snow, 1 * snow_fields + 1, 0);
        update_remote_snow();
        update_remote_snow();
        assert(fabs(arr_get(remote_snow, 1 * snow_fields + 4) - 360.0) > 5.0);
        assert(arr_get(remote_snow, 1 * snow_fields + 3) > 300.0);
        arr_set(remote_snow, 1 * snow_fields + 1, 0);
        stub_online[1] = 0;
        stub_snow[1] = 0;
        game_state = ST_LOBBY;
        dt = 1.0 / 60.0;
    }

    /* ebuC's projectile bursts on online remote contact, side-hand touch, and slow frames,
     * remote dead-reckoning decays cleanly on stop, and online defeat publishes alive == 0. */
    {
        screen_w = 1280;
        screen_h = 720;
        dt = 1.0 / 60.0;
        reset_battle();

        /* 1. Online shooter's projectile bursts on living remote fighter instead of flying through. */
        game_state = ST_ONLINE;
        stub_net_slot = 0;
        player_class = CLASS_EBUC;
        ball_mode = 1;
        player->x = 300;
        player->y = 360;
        player->angle = 0;
        super_cd = 0;
        arr_set(remotes, 1 * remote_fields, 1);
        arr_set(remotes, 1 * remote_fields + 1, 480);
        arr_set(remotes, 1 * remote_fields + 2, 360);
        arr_set(remotes, 1 * remote_fields + 3, 0);
        arr_set(remotes, 1 * remote_fields + 4, 20);
        arr_set(remotes, 1 * remote_fields + 5, 1);
        ball_fire();
        assert(ball.active == 1);
        for (int f = 0; f < 30 && ball.active == 1; f++)
            ball_tick();
        assert(ball.active == 0 && ball.boom_t > 0);
        assert(ball.boom_x < 480);

        /* 2. Grazing visible side hands of a fighter bursts the projectile in Solo and Online. */
        game_state = ST_SOLO;
        ball_reset();
        enemy->hp = 20;
        enemy->x = 500;
        enemy->y = 360;
        enemy->angle = 0; /* Facing right: side hands extend vertically to +-23*cube_scale*enemy_scale (~58 px). */
        ball.active = 1;
        ball.x = 475;
        ball.y = 360 + 74; /* Center offset 74 px: misses 40.3+24=64.3 old box, touches 58+24=82 visible hand! */
        ball.vx = BALL_SPEED;
        ball.vy = 0;
        ball.bounces = 0;
        ball.t = 0;
        dt = 0.08; /* Slow frame spike: swept segment prevents tunneling past thin 4.5-texel hand! */
        ball_tick();
        assert(ball.active == 0 && ball.boom_t > 0);
        assert(enemy->hp < 20.0);
        dt = 1.0 / 60.0;

        /* 3. Remote dead-reckoning has zero join velocity spike and settles onto nx,ny when stopped. */
        game_state = ST_ONLINE;
        reset_remote_smooth();
        arr_set(remotes, 1 * remote_fields, 1);
        arr_set(remotes, 1 * remote_fields + 1, 600);
        arr_set(remotes, 1 * remote_fields + 2, 360);
        arr_set(remotes, 1 * remote_fields + 5, 1);
        arr_set(remotes, 1 * remote_fields + 7, 600);
        arr_set(remotes, 1 * remote_fields + 8, 360);
        smooth_remotes();
        assert(fabs(arr_get(remotes, 1 * remote_fields + 1) - 600.0) < 1e-6);
        /* Snapshot moves right by 18 px after 3 frames (360 px/s). */
        for (int f = 0; f < 3; f++)
            smooth_remotes();
        arr_set(remotes, 1 * remote_fields + 7, 618);
        for (int f = 0; f < 4; f++)
            smooth_remotes();
        assert(arr_get(remotes, 1 * remote_fields + 1) > 618.0);
        /* Remote stops moving: smoothed position settles back onto 618 instead of staying offset. */
        for (int f = 0; f < 45; f++)
            smooth_remotes();
        assert(fabs(arr_get(remotes, 1 * remote_fields + 1) - 618.0) < 0.5);

        /* 4. Online defeat immediately publishes hp == 0 and alive == 0. */
        stub_net_slot = 0;
        stub_pub_calls = 0;
        stub_pub_hp = 20;
        stub_pub_alive = 1;
        player_class = CLASS_ORDINARY;
        player->hp = 0;
        finish_game(2);
        assert(stub_pub_calls > 0 && stub_pub_hp == 0 && stub_pub_alive == 0);

        arr_set(remotes, 1 * remote_fields, 0);
        arr_set(remotes, 1 * remote_fields + 5, 0);
        stub_net_slot = -1;
        finished = 0;
        game_state = ST_LOBBY;
    }

    /* Drag-over does not squeeze buttons; normal punches hit after 0.2 s for player & remote. */
    {
        /* 1. Dragging/moving over a button (action == 2) never triggers press squeeze. */
        game_state = ST_LOBBY;
        t_dir = 0;
        press_points_clear();
        press_pulses_clear();
        logic_touch(menu_x() + btn_w / 2, menu_row(0) + btn_h / 2, 2, 11);
        assert(press_point_n == 0);
        assert(button_press_k(menu_x(), menu_row(0), btn_w, btn_h) == 0);
        assert(press_phase[0] == 0);

        /* 2. Player normal punch: pose immediate, hitbox/damage after 0.2 s. */
        game_state = ST_SOLO;
        finished = 0;
        dt = 0.02;
        player_class = CLASS_ORDINARY;
        player_level = 0;
        player->x = 400;
        player->y = 400;
        player->angle = 0;
        player->hp = player->max_hp = 20;
        enemy_class = CLASS_ORDINARY;
        enemy->x = 460;
        enemy->y = 400;
        enemy->hp = enemy->max_hp = 20;
        enemy->stun = 99; /* keep enemy stationary while checking immediate punch damage */
        punch->active = 0;
        punch_left = 0;
        punch_cd = 0;
        aim_a = 0;
        fire_punch();
        assert(punch->active == 1 && player_punching_now() == 1);
        update_game();
        assert(enemy->hp < 20.0);
        enemy->stun = 0;
        punch->active = 0;
        punch_left = 0;
        punch_cd = 0;

        /* 3. Online remote normal punch: pose and hitbox are immediate. */
        game_state = ST_ONLINE;
        stub_net_slot = 0;
        finished = 0;
        player->x = 460;
        player->y = 400;
        player->hp = player->max_hp = 20;
        revive_prot = 0;
        arr_set(remotes, 1 * remote_fields, 1);
        arr_set(remotes, 1 * remote_fields + 1, 400);
        arr_set(remotes, 1 * remote_fields + 2, 400);
        arr_set(remotes, 1 * remote_fields + 3, 0);
        arr_set(remotes, 1 * remote_fields + 4, 20);
        arr_set(remotes, 1 * remote_fields + 5, 1);
        arr_set(remotes, 1 * remote_fields + 10, CLASS_ORDINARY);
        arr_set(remote_punches, 1 * punch_fields, 1);
        arr_set(remote_punches, 1 * punch_fields + 1, 400);
        arr_set(remote_punches, 1 * punch_fields + 2, 400);
        arr_set(remote_punches, 1 * punch_fields + 3, 1);
        arr_set(remote_punches, 1 * punch_fields + 4, 0);
        arr_set(remote_punches, 1 * punch_fields + 5, 0);
        tick_remote_punches();
        update_remote_punches();
        assert(player->hp < 20.0 && arr_get(remote_punches, 1 * punch_fields) == 2);
        clear_remote_punch(1);
        arr_set(remotes, 1 * remote_fields, 0);
        arr_set(remotes, 1 * remote_fields + 5, 0);
        stub_net_slot = -1;
        dt = 1.0 / 60.0;
        game_state = ST_LOBBY;
    }

    /* Regression suite: room list scroll + search, Event Cube/Santa scale & nicks,
     * Elf ebuC custom dispenser + monochrome white beam, and 0-cup purchase guards. */
    {
        /* 1. ST_ROOMS vertical scrolling, inertia, wheel, and case-insensitive search. */
        screen_w = 1280;
        screen_h = 720;
        t_dir = 0;
        acct_open = 0;
        docs_open = 0;
        warn_open = 0;
        studio_open = 0;
        admin_panel_open = 0;
        language = 1;
        stub_roomlist_count = 20;
        for (int i = 0; i < 20; i++) {
            snprintf(stub_room_names[i], sizeof(stub_room_names[i]), i % 5 == 0 ? "Alpha-%02d" : "Room-%02d", i);
        }
        game_state = ST_ROOMS;
        rooms_open();
        assert(rooms_filtered_count() == 20 && rooms_scroll_needed() == 1 && rooms_scroll_max() > 0);
        int pre_calls = stub_prep_calls;
        int sc_before = stub_scissor_calls;
        draw_rooms();
        assert(stub_scissor_calls >= sc_before + 2 && stub_scissor_active == 0);
        double sx = rooms_x() + rooms_w() / 2;
        double sy = rooms_list_y() + 180;
        logic_touch(sx, sy, 0, 201);
        logic_touch(sx, sy - 90, 2, 201);
        update_rooms();
        logic_touch(sx, sy - 160, 2, 201);
        logic_touch(sx, sy - 160, 1, 201);
        assert(stub_prep_calls == pre_calls && rooms_scroll > 100 && rooms_vel > 0);
        double scrolled = rooms_scroll;
        update_rooms();
        assert(rooms_scroll > scrolled);
        rooms_vel = 0;
        rooms_scroll_by(5000);
        assert(fabs(rooms_scroll - rooms_scroll_max()) < 1e-9);
        /* Tap a visible bottom row after scrolling to join it. */
        double last_ry = rooms_row_y(19) + rooms_row_h() / 2;
        assert(last_ry >= rooms_list_y() && last_ry <= rooms_list_bottom());
        logic_touch(sx, last_ry, 0, 202);
        logic_touch(sx, last_ry, 1, 202);
        assert(stub_prep_calls == pre_calls + 1 && strcmp(stub_prep_name, "Room-19") == 0);
        rooms_waiting = 0;
        stub_prep_status = 0;
        /* Search filter + clear button. */
        logic_touch(rooms_x() + 40, rooms_search_y() + rooms_search_h() / 2, 0, 203);
        assert(rooms_field == 3 && keyboard_visible() == 1);
        stub_kb_set("alpha");
        update_rooms();
        assert(rooms_filtered_count() == 4 && rooms_scroll_needed() == 0 && rooms_scroll == 0);
        assert(strcmp(rooms_filtered_name(0), "Alpha-00") == 0 && strcmp(rooms_filtered_name(3), "Alpha-15") == 0);
        stub_kb_set("no-such-room");
        update_rooms();
        assert(rooms_filtered_count() == 0);
        adm_reset_log();
        draw_rooms();
        assert(adm_has_text("Комнаты не найдены") == 1);
        /* Clear search button restores full list. */
        logic_touch(rooms_x() + rooms_w() - rooms_search_h() / 2, rooms_search_y() + rooms_search_h() / 2, 0, 204);
        assert(str_len(rooms_search) == 0 && rooms_filtered_count() == 20);
        keyboard_hide();
        keyboard_clear();

        /* 2. Event 4 Cube & Event 3 Santa exact player scale and overhead nicknames. */
        game_state = ST_ONLINE;
        event_mode = 4;
        event_t = 1.0;
        dice_event_reset();
        adm_reset_log();
        draw_dice_event();
        assert(last_ground && strcmp(last_ground, ORDINARY_TEX) == 0);
        assert(fabs(last_tex_scale - cube_scale * player_scale) < 1e-6);
        assert(adm_has_text("Кубик") == 1);
        event_mode = 3;
        santa_event_begin();
        plates_santa_t = 1.0;
        adm_reset_log();
        draw_event_santa();
        assert(last_ground && strcmp(last_ground, SANTA_TEX) == 0);
        assert(fabs(last_tex_scale - cube_scale * player_scale) < 1e-6);
        assert(adm_has_text("Дед Мороз") == 1);
        event_mode = 0;
        plates_phase = 0;

        /* 3. Elf ebuC custom dispenser sprite + pure monochrome white beam (local and remote). */
        assert(strcmp(EBUC_ELF_DESPENSER_TEX, "ebuc_elf_dispenser.png") == 0);
        finished = 0;
        player_class = CLASS_EBUC;
        ebuc_skin = SKIN_SPECIAL;
        player->hp = 20;
        player->x = 500;
        player->y = 300;
        arr_set(turret_x, 0, 340);
        arr_set(turret_y, 0, 300);
        arr_set(turret_hp, 0, 10);
        arr_set(turret_maxhp, 0, 10);
        arr_set(turret_spawn_t, 0, 0);
        last_turret_slot = 0;
        adm_reset_log();
        draw_station();
        assert(last_ground && strcmp(last_ground, EBUC_ELF_DESPENSER_TEX) == 0);
        assert(adm_color_n >= 2);
        assert((adm_colors[0] & 0xFFFFFFu) == 0xFFFFFFu && (adm_colors[1] & 0xFFFFFFu) == 0xFFFFFFu);
        /* Normal ebuC still uses regular dispenser + green beam. */
        ebuc_skin = SKIN_NORMAL;
        adm_reset_log();
        draw_station();
        assert(last_ground && strcmp(last_ground, DESPENSER_TEX) == 0);
        assert(adm_color_n >= 2 && (adm_colors[0] & 0xFFFFFFu) != 0xFFFFFFu);
        arr_set(turret_hp, 0, 0);
        /* Remote Online Elf ebuC also renders custom dispenser + monochrome white beam. */
        stub_net_slot = 0;
        for (int k = 0; k < (int)(max_players * station_fields); k++) {
            arr_set(remote_station, k, 0);
        }
        arr_set(remotes, 1 * remote_fields, 1);
        arr_set(remotes, 1 * remote_fields + 1, 520);
        arr_set(remotes, 1 * remote_fields + 2, 320);
        arr_set(remotes, 1 * remote_fields + 4, 20);
        arr_set(remotes, 1 * remote_fields + 5, 1);
        arr_set(remotes, 1 * remote_fields + 10, CLASS_EBUC);
        arr_set(remotes, 1 * remote_fields + 11, SKIN_SPECIAL);
        arr_set(remote_station, 1 * station_fields, 1);
        arr_set(remote_station, 1 * station_fields + 1, 1);
        arr_set(remote_station, 1 * station_fields + 2, 360);
        arr_set(remote_station, 1 * station_fields + 3, 320);
        arr_set(remote_station, 1 * station_fields + 4, 10);
        adm_reset_log();
        draw_remote_station();
        assert(last_ground && strcmp(last_ground, EBUC_ELF_DESPENSER_TEX) == 0);
        assert(adm_color_n >= 2);
        assert((adm_colors[0] & 0xFFFFFFu) == 0xFFFFFFu && (adm_colors[1] & 0xFFFFFFu) == 0xFFFFFFu);
        arr_set(remote_station, 1 * station_fields + 1, 0);
        arr_set(remotes, 1 * remote_fields, 0);
        arr_set(remotes, 1 * remote_fields + 5, 0);
        stub_net_slot = -1;

        /* 4. 0-cup / negative / NaN / fractional balance purchase rejection. */
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "ZeroCupFriend");
        assert(admin_playing() == 0);
        set_class_owned(CLASS_AZUM, 0);
        set_class_owned(CLASS_EBUC, 0);
        set_class_owned(CLASS_ASTRA, 0);
        set_class_owned(CLASS_CODECRACKER, 0);
        player_class = CLASS_ORDINARY;
        set_class_progress(CLASS_ORDINARY, 0, 0);
        double bad_balances[5] = {0.0, -50.0, 0.0 / 0.0, 49.99, 84.99};
        for (int bi = 0; bi < 5; bi++) {
            cups = bad_balances[bi];
            candies = 0;
            pick_class(CLASS_AZUM);
            pick_class(CLASS_EBUC);
            pick_class(CLASS_ASTRA);
            pick_class(CLASS_CODECRACKER);
            assert(class_owned_of(CLASS_AZUM) == 0 && class_owned_of(CLASS_EBUC) == 0);
            assert(class_owned_of(CLASS_ASTRA) == 0 && class_owned_of(CLASS_CODECRACKER) == 0);
            assert(player_class == CLASS_ORDINARY);
            if (!(cups >= level_cost(1))) {
                assert(buy_level(CLASS_ORDINARY, 1) == 0 && class_unlocked_of(CLASS_ORDINARY) == 0);
            }
        }
        /* Corrupt non-1 value in class_owned_tbl does not count as owned. */
        set_class_owned(CLASS_AZUM, 2);
        assert(class_owned_of(CLASS_AZUM) == 0);
        game_state = ST_LOBBY;
    }

    test_christmas_snowballs();
    test_offline_ordinary();
    test_admin_theme_clears_christmas();
    test_lobby_ball();
    test_admin_auto_block();
    test_admin_auto_block_astra();
    test_admin_auto_block_skill();
    ordinary_gold_owned = 1;
    ordinary_gold_skin = SKIN_SPECIAL;
    assert(class_look(CLASS_ORDINARY, ordinary_gold_skin) == LOOK_GOLD);
    assert(strcmp(look_idle_tex(LOOK_GOLD), ORDINARY_GOLD_TEX) == 0);
    assert(strcmp(look_punch_tex(LOOK_GOLD), ORDINARY_GOLD_PUNCH_TEX) == 0);
    assert(look_ready(LOOK_GOLD) == 1);
    assert((int)skin_flags() == 48);
    ordinary_gold_owned = 0;
    ordinary_gold_skin = SKIN_NORMAL;
    {
        double saved_login = stub_login_status;
        char saved_nick[sizeof(stub_login_nick)];
        snprintf(saved_nick, sizeof(saved_nick), "%s", stub_login_nick);
        stub_login_status = 2;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "ordinary_player");
        stub_wheel_spins = 2;
        stub_wheel_wins = 4;
        wheel_award_win();
        assert(stub_wheel_spins == 2 && stub_wheel_wins == 5); /* every win is stored */
        wheel_award_win();
        assert(stub_wheel_spins == 3 && stub_wheel_wins == 0); /* One spin for six wins. */
        /* The wheel screen itself stays admin-only. */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
        int saved_language = language;
        language = 0;
        adm_reset_log();
        draw_lobby();
        assert(adm_has_text("Fortune wheel") == 1);
        /* The wheel button is public now: every player sees the spin count. */
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", "ordinary_player");
        adm_reset_log();
        draw_lobby();
        assert(adm_has_text("Fortune wheel") == 1);
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", ADMIN_NICK);
        adm_reset_log();
        wheel_capture_line_n = 0;
        wheel_capture_ring_n = 0;
        wheel_outline_capture = 1;
        draw_wheel();
        wheel_outline_capture = 0;
        assert(adm_has_text("Fortune wheel") == 1 && adm_has_text("Wins:") == 1);
        assert(adm_has_text("Cups") == 1);   /* sector units; no hidden "???" slice anymore */
        int hub_fill = 0, black_outline = 0, old_outline_color = 0;
        for (int ci = 0; ci < adm_color_n; ci++) {
            uint32_t rgb = adm_colors[ci] & 0xFFFFFFu;
            if (rgb == 0x101010u) hub_fill = 1;
            if (rgb == 0x000000u) black_outline = 1;
            if (rgb == 0x6A6A6Au || rgb == 0xB0B0B0u || rgb == 0x3C3C3Cu ||
                rgb == 0x828282u) old_outline_color = 1;
        }
        assert(hub_fill == 1 && black_outline == 1 && old_outline_color == 0);
        assert(wheel_capture_line_n == 6 && wheel_capture_ring_n == 1); /* 5 dividers + pointer */
        for (int oi = 0; oi < wheel_capture_line_n; oi++) {
            assert((wheel_capture_line_color[oi] & 0xFFFFFFu) == 0x000000u);
            assert(fabs(wheel_capture_line_width[oi] - 4.0) < 1e-6);
        }
        assert((wheel_capture_ring_color[0] & 0xFFFFFFu) == 0x000000u);
        assert(fabs(wheel_capture_ring_width[0] - 4.0) < 1e-6);
        assert(adm_has_text("Chances:") == 0 && adm_has_text("Шансы:") == 0);
        assert(adm_has_text("CUPS") == 0 && adm_has_text("GOLD") == 0);
        assert(btn_w >= 156 && btn_h >= 42);
        assert(hit_button(lobby_wheel_button_x() + btn_w / 2, lobby_wheel_button_y() + btn_h / 2,
                          lobby_wheel_button_x(), lobby_wheel_button_y(), btn_w, btn_h) == 1);
        assert(adm_has_text("Back") == 1);
        language = saved_language;
        stub_wheel_spins = 999;
        stub_wheel_wins = 5;
        wheel_award_win();
        assert(stub_wheel_spins == 999 && stub_wheel_wins == 0);

        /* The wheel eases through several turns and stops on its selected slice. */
        stub_wheel_spins = 1;
        stub_wheel_wins = 0;
        wheel_anim_t = 0;
        wheel_rotation = 0;
        wheel_result_kind = 0;
        double saved_dt_wheel = dt;
        dt = 0.05;
        wheel_spin_do();
        assert(fabs(wheel_anim_t - WHEEL_SPIN_DURATION) < 1e-9);
        update_wheel();
        double first_rotation = wheel_rotation;
        dt = 0.20;
        update_wheel();
        assert(wheel_rotation > first_rotation && wheel_rotation < wheel_spin_start + wheel_spin_delta);
        while (wheel_anim_t > 0) update_wheel();
        double alignment = fmod(wheel_result_slice * 60 + wheel_rotation, 360.0);
        assert(alignment < 1e-6 || 360.0 - alignment < 1e-6);
        dt = saved_dt_wheel;
        stub_login_status = saved_login;
        snprintf(stub_login_nick, sizeof(stub_login_nick), "%s", saved_nick);
    }
    puts("wheel quest: public button, one spin per six wins, capped at 999");
    state_destroy();
    state_ready = 0;
    puts("Состояние полной C-версии: норма");
    return 0;
}
