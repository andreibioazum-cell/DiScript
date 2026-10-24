#ifndef NET_H
#define NET_H
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

/* Firebase endpoint, web key and room. config.inc and the background presence
 * check (presence.inc) both read these, so the job works in a cold process. */
#ifndef NET_FIREBASE_URL
/* The PC test harness overrides this at build time to point at
 * test/fake_firebase.py (plain http on localhost). */
#define NET_FIREBASE_URL "https://cubicbattleserver-19ae2-default-rtdb.firebaseio.com"
#endif
#ifndef NET_FIREBASE_KEY
#define NET_FIREBASE_KEY "AIzaSyDJEqopxP5EqEoxP_ehO3jFXbNztu3DgVs"
#endif
#define NET_DEFAULT_ROOM "main"

/* Two secrets. Both are 32 random [A-Za-z0-9] chars and never appear in the
 * sources in the clear: the pieces are encoded (every byte xored with 0x5a)
 * and rebuilt at runtime by net_write_secret() and net_pass_suffix().
 *
  * NET_WRITE_SECRET is the "write password". firebase.rules.json accepts a
  * write only when the body carries a "w" field with this value, so a script
  * that has just the API key and an account changes nothing. The value sits in
  * the database in the open, so it must NOT equal the second secret.
 *
  * NET_PASS_SECRET is the account password suffix (player password + this
  * suffix). It used to be "|cb4v1", easy to guess: with it anyone could brute
  * force another nick's password. It never reaches the database.
 *
  * Change both at once with tools/rotate_db_secret.py: it generates new values,
  * rewrites these macros and firebase.rules.json. Do not edit by hand: the
  * database and the client must stay in sync. */
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
/* Phone language (ISO 639-1, e.g. "ru"): it picks the language of a fresh install. */
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
/* The buK brick: origin, direction and the shot number of the throw. */
void net_publish_brick(double x, double y, double dx, double dy, double shot);
void net_publish_codecracker_super(double on);
void net_set_class(double cls);
double net_status(void);
double net_slot(void);
/* Live root flags (AzumBuff, FlashSale); each defaults to off without a
 * fresh successful read of its own Firebase node. */
double net_azum_buff(void);
double net_flash_sale(void);
double net_hitbox_chaos(void);
/* Force an immediate re-read of both live flags (entering the shop). */
void net_live_flags_refresh(void);
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

double net_load_astra(void);
double net_load_astra_level(void);
double net_load_astra_levels_unlocked(void);
void net_save_astra(double owned, double level, double levels_unlocked);

/* CodeCracker ownership and level branch: its own, not shared with the ordinary cube. */
double net_load_codecracker(void);
double net_load_codecracker_level(void);
double net_load_codecracker_levels_unlocked(void);
void net_save_codecracker(double owned, double level, double levels_unlocked);
/* Forget a pending cloud save: the device holds nothing worth pushing. */
void net_mark_progress_synced(void);
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


/* Room directory: many rooms instead of one shared "main", with optional
 * password protection (hashed in rooms/<name>/meta). */
void net_roomlist_fetch(const char *url);
double net_roomlist_status(void);
double net_roomlist_count(void);
const char *net_roomlist_name(double idx);
/* create=1 creates the room (fails if it exists), create=0 checks joining.
 * Status: 0 idle, 1 busy, 2 ok, 3 wrong/needed password, 4 exists, 5 error, 6 bad name. */
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
double net_load_hitboxes(void);
void net_save_settings(double language, double hitboxes);
double net_load_music_volume(void);
void net_save_music_volume(double volume);
double net_load_winter_theme(void);
void net_save_winter_theme(double on);
double net_load_new_theme(void);
void net_save_new_theme(double on);

void settings_mark_legal(void);
double settings_legal_ts(void);
/* Personal data consent (152-FZ): version 0 means withdrawn. */
void settings_set_pd_consent(double version);
double settings_pd_consent_version(void);
void net_pd_consent_upload(void);
/* Delete the account, the cloud profile and the data on the device; the account
 * password is required. Status: 0 - nothing, 1 - running, 2 - deleted, 3 -
 * network error, 4 - wrong password (the result is returned once). */
void net_delete_account(const char *password);
double net_delete_account_status(void);
/* Change the account password. Status: 0 - nothing, 1 - running, 2 - changed,
 * 3 - network error, 4 - wrong current password, 5 - the new password is not
 * accepted, 6 - not signed in (the result is returned once). */
void net_change_password(const char *old_pass, const char *new_pass);
double net_change_password_status(void);
/* Admin reset of ANOTHER player's account password. A game client cannot
 * do this in Firebase Auth directly (it takes the account's own token),
 * so the client calls the adminResetPassword Cloud Function, which
 * verifies the caller's admin token and uses the Admin SDK. Deploy it
 * from functions/ (see FIREBASE.md); if it is not deployed the call
 * simply ends in the error status. Statuses mirror net_change_password:
 * 0 idle, 1 running, 2 changed, 3 error/not deployed, 4 bad new password,
 * 5 bad target nick. */
#define NET_ADMIN_RESET_URL "https://us-central1-cubicbattleserver-19ae2.cloudfunctions.net/adminResetPassword"
/* Gifts live in the Realtime Database (users/<nick>/gifts), so no Cloud
 * Function is involved: buy/send/grant are plain authenticated writes.
 * net_gift_action: grant 0 = send one owned sticker, 3 = sell one sticker
 * back for half its price, 1 = admin grant,
 * 2 = buy for yourself. Statuses: 2 ok, 3 network/server error,
 * 4 recipient not found, 5 not enough cups, 6 no sticker in inventory,
 * 7 bad recipient (or gifting yourself), 8 collection full (999). */
double net_gift_count(double gift);
/* Local count change: a sticker was bought, sold or sent, so the collection
 * changes right away. Only that one counter moves; the rest stay as they are. */
void net_gift_count_add(double gift, double delta);
void net_gift_refresh(void);
double net_gift_action(double gift, const char *recipient, double grant, double hue);
double net_gift_action_status(void);
double net_gift_hue(void);
/* Catalogue of five stickers: 0 poop, 1 cake, 2 bear, 3 rocket, 4 emerald
 * (the most expensive one). net_gift_types() is the list length; the price
 * is what the shop charges, the refund is half of it. */
double net_gift_types(void);
double net_gift_price(double gift);
double net_gift_refund(double gift);
/* Cup balance the database holds after the last successful buy/sell, or -1
 * when the finished action was not one. */
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
