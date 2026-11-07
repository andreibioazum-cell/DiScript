/* Read-only UI geometry and test hooks for PC controls and headless runs. */

double game_pc_state(void) { return game_state; }

int game_pc_in_battle(void) { return game_state == ST_SOLO || game_state == ST_ONLINE; }

int game_pc_battle_mouse_ui(float x, float y) {
    if (game_state != ST_ONLINE)
        return 0;
    if (chat_open == 1)
        return 1; /* the open chat covers the fight: every click is chat's */
    if (parent_chat_off() == 1)
        return 0; /* kid mode: no chat button at all */
    return hit_button((double)x, (double)y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h()) == 1;
}

void game_pc_goto(double target) { start_transition(target); }

void game_pc_joy(float *x, float *y, float *r) {
    if (x)
        *x = joy.x;
    if (y)
        *y = joy.y;
    if (r)
        *r = joy.r;
}

void game_pc_pads(float *ax, float *ay, float *a2x, float *a2y, float *bx, float *by, float *radius) {
    if (ax)
        *ax = (float)atk_x;
    if (ay)
        *ay = (float)atk_y;
    if (a2x)
        *a2x = (float)atk2_x;
    if (a2y)
        *a2y = (float)atk2_y;
    if (bx)
        *bx = (float)blk_x;
    if (by)
        *by = (float)blk_y;
    if (radius)
        *radius = (float)battle_pad_r();
}

/* Christmas snowball pad position for the PC keyboard bridge. */
void game_pc_xmas_pad(float *x, float *y, float *r) {
    if (x) *x = (float)christmas_pad_x();
    if (y) *y = (float)christmas_pad_y();
    if (r) *r = (float)christmas_pad_r();
}

/* The super pad is drawn (and armed) only for classes that have a super. */
int game_pc_has_super(void) { return class_has_super(player_class) == 1 ? 1 : 0; }

double game_pc_player_hp(void) { return player ? player->hp : 0.0; }

void game_pc_login_fields(float *nick_x, float *nick_y, float *pass_x, float *pass_y, float *btn_x, float *btn_y,
                          float *btn_w, float *button_h) {
    if (nick_x)
        *nick_x = (float)login_x();
    if (nick_y)
        *nick_y = (float)login_nick_y();
    if (pass_x)
        *pass_x = (float)login_x();
    if (pass_y)
        *pass_y = (float)login_pass_y();
    if (btn_x)
        *btn_x = (float)login_btn_x();
    if (btn_y)
        *btn_y = (float)login_btn_y();
    if (btn_w)
        *btn_w = (float)login_btn_w;
    if (button_h)
        *button_h = (float)btn_h;
}

void game_pc_player_xy(double *x, double *y) {
    if (x)
        *x = player ? player->x : 0.0;
    if (y)
        *y = player ? player->y : 0.0;
}

double game_pc_enemy_hp(void) { return enemy ? enemy->hp : 0.0; }

double game_pc_finished(void) { return finished; }
double game_pc_event(void) { return event_mode; }
int game_pc_disco_music(void) { return disco_ok == 1 && snd_playing(DISCO_MUSIC) == 1; }
double game_pc_ult_mode(void) { return ball_mode; }

double game_pc_chaos(void) { return 0; }
double game_pc_ball(void) { return ball.active; }
double game_pc_bounces(void) { return ball.bounces; }
double game_pc_remote_bricks(void) { return remote_bricks_live(); }
double game_pc_class(void) { return player_class; }
double game_pc_block(void) { return player_block; }

    /* Select owned class for headless tests. */
void game_pc_pick_class(double cls) { pick_class(cls); }
double game_pc_charge(void) { return 0; }
void game_pc_wheel(float steps) {
    if (docs_open == 1 || warn_open == 1 || studio_open == 1) return;
    if (admin_cmd_wheel((double)steps) == 1) return;
    if (acct_open == ACCT_GIFTS && acct_gift_view_open == 0) {
        double delta = -(double)steps * 64.0;
        acct_gift_shop_scroll_by(delta);
        acct_gift_scroll_by(delta);
    } else if (game_state == ST_ACHIEVEMENTS && acct_open == 0) {
        ach_scroll_by(-(double)steps * 64.0);
    } else if (game_state == ST_ROOMS && rooms_mode == ROOMS_MODE_LIST && acct_open == 0) {
        rooms_scroll_by(-(double)steps * 64.0);
    } else if (game_state == ST_CLASSES && acct_open == 0) {
        classes_scroll_by(-(double)steps * 80.0);
    }
}

/* Return login consent checkbox bounds for PC mouse input. */
void game_pc_login_consent(float *x, float *y, float *size) {
    if (x)
        *x = (float)(login_consent_x() + login_box_sz() * 0.5);
    if (y)
        *y = (float)(login_consent_y() + login_box_sz() * 0.5);
    if (size)
        *size = (float)login_box_sz();
}

double game_pc_login_status(void) { return login_status; }

const char *game_pc_login_nick(void) { return login_nick; }

/* Join or create a named online room from the headless test harness. */
static char pc_room_name[25];
static double pc_room_waiting;

void game_pc_join_room(const char *name, double create) {
    double i = 0;
    while (name[(int)i] != 0 && i < (double)(sizeof(pc_room_name) - 1)) {
        pc_room_name[(int)i] = name[(int)i];
        i = i + 1;
    }
    pc_room_name[(int)i] = 0;
    net_room_prepare(FIREBASE, pc_room_name, "", create);
    pc_room_waiting = 1;
}

void game_pc_room_tick(void) {
    if (pc_room_waiting == 0) {
        return;
    }
    if (net_room_prep_status() != 2) {
        return;
    }
    pc_room_waiting = 0;
    ROOM = pc_room_name;
    start_transition(ST_ONLINE);
}
