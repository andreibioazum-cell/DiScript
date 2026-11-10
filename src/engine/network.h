#ifndef NET_H
#define NET_H
#include <stddef.h>
#if defined(__ANDROID__) || defined(CB_PC)
#include <jni.h>
#endif

#define NET_SLOTS 4
#define NET_OFFLINE 0
#define NET_CONNECTING 1
#define NET_PLAYING 3
#define NET_ERROR 4
#define NET_LOGIN_IDLE 0
#define NET_LOGIN_OK 2
#define NET_LOGIN_BAD_NICK 5
#define NET_LOGIN_WRONG_PASS 6
#define NET_LOGIN_BAD_PASS 7

/* Firebase endpoint, web API key, and default room. */
#ifndef NET_FIREBASE_URL
/* Overridden at build time by PC test harnesses. */
#define NET_FIREBASE_URL "https://cubicbattleserver-19ae2-default-rtdb.firebaseio.com"
#endif
#ifndef NET_FIREBASE_KEY
#define NET_FIREBASE_KEY "AIzaSyDJEqopxP5EqEoxP_ehO3jFXbNztu3DgVs"
#endif
#define NET_DEFAULT_ROOM "main"

/* XOR-encoded database write secret and password suffix.
 * Rotate both together with tools/rotate_db_secret.py. */
#define NET_SECRET_XOR 0x5a

#define NET_WRITE_SECRET_LEN 32
#define NET_WRITE_SECRET_0 "\030\027\033\062\040\061\150\154\063\055\014\020"
#define NET_WRITE_SECRET_1 "\033\021\151\021\052\012\042\065\034\031\040\061"
#define NET_WRITE_SECRET_2 "\056\152\142\034\026\152\022\071"

#define NET_PASS_SECRET_LEN 32
#define NET_PASS_SECRET_0 "\063\154\017\075\040\151\033\066\036\026\076\075"
#define NET_PASS_SECRET_1 "\022\020\154\063\017\054\061\027\025\055\024\062"
#define NET_PASS_SECRET_2 "\021\021\024\025\064\066\055\071"

#ifdef __ANDROID__
void net_set_java_vm(JavaVM *vm);
#endif

void net_connect(const char *url, const char *room);
void net_disconnect(void);
void net_set_data_path(const char *path);
/* Texture pack folder (<data>/packs) and its clamped file helpers. */
int net_packs_root(char *dst, size_t cap);
int net_packs_mkdir(const char *rel);
int net_packs_exists(const char *rel);
int net_packs_write(const char *rel, const void *data, int len);
int net_packs_read(const char *rel, void *buf, int cap);
int net_packs_size(const char *rel);
int net_packs_list(const char *rel, char *out, int cap);
int net_packs_remove(const char *rel);
/* Absolute-path helpers for the in-game file picker (import any .zip). */
int net_fs_list(const char *dir, char *out, int cap);
int net_fs_read(const char *path, void *buf, int cap);
int net_fs_write(const char *path, const void *data, int len);
int net_fs_home(char *dst, size_t cap);
int net_fs_dir_readable(const char *dir);
/* App data folders reported by the platform layer (internal, external). */
void net_fs_set_dirs(const char *internal, const char *external);
/* System document pickers (Android SAF). Other platforms return 0. */
int net_fs_pick_import(void);
int net_fs_pick_export(const char *suggested);
int net_fs_take_fd(int *kind);
int net_fs_read_fd(int fd, void *buf, int cap);
int net_fs_write_fd(int fd, const void *data, int len);
void net_fs_close_fd(int fd);
/* Set device ISO 639-1 language code for initial install defaults. */
void net_set_system_language(const char *code);

void net_set_firebase_key(const char *key);
void net_autologin(const char *url);
int net_presence_check(const char *data_dir);
double net_auth(const char *url, const char *nick, const char *pass);
void net_logout(void);
double net_login_status(void);
const char *net_login_nick(void);
const char *net_login_pass(void);

void net_publish(double x, double y, double angle, double hp, double alive);
void net_publish_punch(double x, double y, double dx, double dy, double punch);
void net_publish_snow(double x, double y, double dx, double dy, double snow);

void net_publish_turrets(double x1, double y1, double hp1, double x2, double y2, double hp2, double x3, double y3,
                         double hp3, double count);

void net_publish_dash(double x, double y, double dx, double dy, double dash);

void net_publish_grab(double x, double y, double dx, double dy, double grab);
void net_publish_universe(double x, double y, double counter);

void net_publish_thud(double counter);
void net_publish_block(double on);
/* Publish ebuC's projectile state. */
void net_publish_brick(double x, double y, double dx, double dy, double shot);
void net_publish_xball(double x, double y, double dx, double dy, double shot);
void net_publish_codecracker_super(double on);
void net_set_class(double cls);
double net_status(void);
double net_slot(void);
/* Live server flags (AzumBuff, FlashSale, Christmas). */
double net_azum_buff(void);
double net_flash_sale(void);
double net_christmas(void);
/* Refresh all live server flags immediately. */
void net_live_flags_refresh(void);
/* 1 while Firebase answers the flag poller (offline => ordinary game). */
double net_flags_reachable(void);
double net_event(void);
void net_event_set(double mode);
double net_player_online(double slot);
double net_player_x(double slot);
double net_player_y(double slot);
double net_player_angle(double slot);
double net_player_hp(double slot);
double net_player_alive(double slot);
const char *net_player_nick(double slot);
double net_player_punch_x(double slot);
double net_player_punch_y(double slot);
double net_player_punch_dx(double slot);
double net_player_punch_dy(double slot);
double net_player_punch(double slot);
double net_player_snow_x(double slot);
double net_player_snow_y(double slot);
double net_player_snow_dx(double slot);
double net_player_snow_dy(double slot);
double net_player_snow(double slot);
double net_player_station_x(double slot);
double net_player_station_y(double slot);
double net_player_station_hp(double slot);
double net_player_station2_x(double slot);
double net_player_station2_y(double slot);
double net_player_station2_hp(double slot);
double net_player_station3_x(double slot);
double net_player_station3_y(double slot);
double net_player_station3_hp(double slot);

double net_player_grab_x(double slot);
double net_player_grab_y(double slot);
double net_player_grab_dx(double slot);
double net_player_grab_dy(double slot);
double net_player_grab(double slot);
double net_player_universe_x(double slot);
double net_player_universe_y(double slot);
double net_player_universe(double slot);

double net_player_thud(double slot);
double net_player_block(double slot);
double net_player_brick_x(double slot);
double net_player_brick_y(double slot);
double net_player_brick_dx(double slot);
double net_player_brick_dy(double slot);
double net_player_brick(double slot);
double net_player_xball_x(double slot);
double net_player_xball_y(double slot);
double net_player_xball_dx(double slot);
double net_player_xball_dy(double slot);
double net_player_xball(double slot);
double net_player_codecracker_super(double slot);
double net_player_dash(double slot);
double net_player_dash_x(double slot);
double net_player_dash_y(double slot);
double net_player_class(double slot);
double net_player_level(double slot);
void net_set_level(double level);

void net_set_skin(double skin);
double net_player_skin(double slot);

double net_load_cups(void);
double net_load_candies(void);
double net_load_class(void);
double net_load_azum(void);
double net_load_santa(void);
double net_load_ebuc(void);
double net_load_level(void);
double net_load_levels_unlocked(void);
double net_load_ordinary_level(void);
double net_load_ordinary_levels_unlocked(void);
double net_load_azum_level(void);
double net_load_azum_levels_unlocked(void);
double net_load_santa_level(void);
double net_load_santa_levels_unlocked(void);
double net_load_ebuc_level(void);
double net_load_ebuc_levels_unlocked(void);

double net_load_bp_level(void);
void net_set_bp_xp(double v);
double net_load_bp_xp(void);
double net_load_azum_skin(void);
int net_load_wheel_spins(void);
int net_load_wheel_wins(void);
void net_save_wheel_progress(int spins, int wins);

double net_load_astra(void);
double net_load_astra_level(void);
double net_load_astra_levels_unlocked(void);
void net_save_astra(double owned, double level, double levels_unlocked);

/* CodeCracker ownership and level state. */
double net_load_codecracker(void);
double net_load_codecracker_level(void);
double net_load_codecracker_levels_unlocked(void);
void net_save_codecracker(double owned, double level, double levels_unlocked);
/* Clear local cloud-dirty flag. */
void net_mark_progress_synced(void);
void net_progress_flush(void);
void net_save_progress_all(double cups, double candies, double cls, double azum, double santa, double ebuc,
                           double level, double levels_unlocked, double ordinary_level, double ordinary_levels_unlocked,
                           double azum_level, double azum_levels_unlocked, double santa_level,
                           double santa_levels_unlocked, double ebuc_level, double ebuc_levels_unlocked,
                           double bp_level, double azum_skin);

#define ACH_FLAG_ALL_CHARACTERS (1u << 3)
double net_load_achievement_flags(void);
void net_mark_achievement_flag(double flag);

double net_load_azum_revives(void);
void net_save_azum_revives(double revives);

double net_promo_2027_used(void);
void net_promo_2027_mark_used(void);


/* Room directory and password-protected room creation/joining. */
void net_roomlist_fetch(const char *url);
double net_roomlist_status(void);
double net_roomlist_count(void);
const char *net_roomlist_name(double idx);
/* Prepare room create (create=1) or join (create=0).
 * Status: 0 idle, 1 busy, 2 ok, 3 password needed/wrong, 4 exists, 5 error, 6 invalid name. */
void net_room_prepare(const char *url, const char *name, const char *pass, double create);
const char *net_current_room(void);
double net_room_prep_status(void);
void net_room_prep_reset(void);

void net_leaderboard_fetch(const char *url);
double net_leaderboard_status(void);
double net_leaderboard_count(void);
const char *net_leaderboard_nick(double idx);
double net_leaderboard_cups(double idx);

double net_load_language(void);
/* Player reports: anyone files, only the owner reads them. */
double net_report_send(const char *target, const char *text, const char *room);
void net_reports_fetch(void);
double net_reports_status(void);
double net_reports_count(void);
const char *net_report_from(double idx);
const char *net_report_target(double idx);
const char *net_report_text(double idx);
const char *net_report_room(double idx);
const char *net_report_key(double idx);
void net_report_delete(double idx);

/* Parental control, device-only storage. */
double net_load_parent_pin(void);
double net_load_pin2(void);
void net_save_pin2(int hash);
void net_save_parent_pin(int hash);
double net_load_parent_age(void);
void net_save_parent_age(int age);
double net_load_hitboxes(void);
void net_save_settings(double language, double hitboxes);
double net_load_music_volume(void);
void net_save_music_volume(double volume);
double net_load_winter_theme(void);
void net_save_winter_theme(double on);
double net_load_new_theme(void);
void net_save_new_theme(double on);
double net_load_theme_mode(void);
void net_save_theme_mode(double mode);
double net_load_theme_chosen(void);
void net_save_theme_chosen(double on);
double net_load_graphics_mode(void);
void net_save_graphics_mode(double mode);

void settings_mark_legal(void);
double settings_legal_ts(void);
/* Personal data consent version (0 = withdrawn). */
void settings_set_pd_consent(double version);
double settings_pd_consent_version(void);
void net_pd_consent_upload(void);
/* Delete account, cloud profile, and local data.
 * Status: 0 idle, 1 running, 2 deleted, 3 error, 4 wrong password. */
void net_delete_account(const char *password);
double net_delete_account_status(void);
/* Change account password.
 * Status: 0 idle, 1 running, 2 changed, 3 error, 4 wrong password, 5 invalid new password, 6 signed out. */
void net_change_password(const char *old_pass, const char *new_pass);
double net_change_password_status(void);
/* Reset another player's password via the adminResetPassword Cloud Function. */
#define NET_ADMIN_RESET_URL "https://us-central1-cubicbattleserver-19ae2.cloudfunctions.net/adminResetPassword"
/* Gift sticker inventory and actions (users/<nick>/gifts).
 * grant: 0 send owned, 1 admin grant, 2 buy, 3 sell for half price.
 * Status: 0 idle, 1 running, 2 ok, 3 error, 4 missing user, 5 low cups, 6 none owned, 7 invalid target, 8 full. */
double net_gift_count(double gift);
/* Adjust cached local count for one sticker. */
void net_gift_count_add(double gift, double delta);
void net_gift_refresh(void);
double net_gift_action(double gift, const char *recipient, double grant);
double net_gift_action_status(void);
#define NET_GIFT_TYPE_COUNT 5
#define NET_GIFT_SHOP_TYPE_COUNT 5

/* Sticker catalogue metadata (0 poop, 1 cake, 2 bear, 3 rocket, 4 emerald). */
double net_gift_types(void);
double net_gift_price(double gift);
double net_gift_refund(double gift);
/* Updated server cup balance after buy/sell, or -1 otherwise. */
double net_gift_cups(void);
void net_admin_reset_password(const char *nick, const char *new_pass);
double net_admin_reset_status(void);
double net_chat_is_passwd(const char *msg);
const char *net_chat_passwd_target(const char *msg);
const char *net_chat_passwd_new(const char *msg);

double net_banned(void);
double net_is_banned(const char *nick);
void net_ban_set(const char *nick, double banned);
double net_chat_is_ban(const char *msg);
double net_chat_is_unban(const char *msg);
const char *net_chat_ban_target(const char *msg);
const char *net_chat_unban_target(const char *msg);

double net_chat_is_text_cmd(const char *msg);
const char *net_chat_text_cmd_text(const char *msg);
const char *net_chat_text_cmd_color(const char *msg);
void net_banner_send(const char *text, const char *color);
void net_banner_clear(void);
double net_banner_ts(void);
const char *net_banner_text(void);
const char *net_banner_color(void);

void net_chat_send(const char *text);
void net_chat_trim(double keep);
double net_chat_count(void);
const char *net_chat_text(double idx);
const char *net_chat_uid(double idx);

const char *net_chat_key(double idx);

#endif
