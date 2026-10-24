#ifndef CB_GAME_H
#define CB_GAME_H

struct AAssetManager;
struct Buffer;

void game_init(struct AAssetManager *assets);
void game_update(void);
void game_draw(struct Buffer *buffer);
void game_touch(float x, float y, int action, int pointer_id);
int game_back(void);
void game_reset(void);
void game_bye(void);

#ifdef CB_PC
/* Read-only window into the game state for the PC input bridge
 * (src/game/pc_pads.inc): joystick and combat pad geometry, plus a few
 * values worth asserting in the headless harness. */
double game_pc_state(void);
int game_pc_in_battle(void);
void game_pc_goto(double target);
void game_pc_joy(float *x, float *y, float *r);
void game_pc_pads(float *ax, float *ay, float *a2x, float *a2y, float *bx, float *by, float *radius);
/* Where in an active fight a mouse click must stay a click instead of
 * becoming the punch: the open chat panel covers the whole fight, and the
 * closed chat's small button opens it. The in-battle back button is not on
 * this list - Esc leaves the fight, so a swing over that corner never quits. */
int game_pc_battle_mouse_ui(float x, float y);
/* Whether the current class draws the super pad at all. The ordinary cube has
 * no super, so a PC "super" press would land on empty screen - where the game
 * reads it as the joystick and the player suddenly walks. */
int game_pc_has_super(void);
double game_pc_player_hp(void);
void game_pc_player_xy(double *x, double *y);
void game_pc_login_fields(float *nick_x, float *nick_y, float *pass_x, float *pass_y, float *btn_x, float *btn_y,
                          float *btn_w, float *btn_h);
void game_pc_login_consent(float *x, float *y, float *size);
double game_pc_login_status(void);
const char *game_pc_login_nick(void);
double game_pc_enemy_hp(void);
double game_pc_finished(void);
double game_pc_chaos(void);
double game_pc_ult_mode(void);
double game_pc_ball(void);
double game_pc_bounces(void);
double game_pc_remote_bricks(void);
double game_pc_class(void);
double game_pc_block(void);
void game_pc_pick_class(double cls);
/* Harness only: join or create a named online room, so two headless clients
 * can meet in a real fight. game_pc_room_tick() finishes the handshake. */
void game_pc_join_room(const char *name, double create);
void game_pc_room_tick(void);
double game_pc_charge(void);
void game_pc_wheel(float steps);
#endif

#endif
