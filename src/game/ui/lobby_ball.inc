/* A small physics toy in the lobby, independent of the seasonal NPC cube. */
static double lobby_ball_x;
static double lobby_ball_y;
static double lobby_ball_vx;
static double lobby_ball_vy;
static double lobby_ball_r;
static double lobby_ball_pointer = -1;
static double lobby_ball_drag_dx;
static double lobby_ball_drag_dy;
static double lobby_ball_down_x;
static double lobby_ball_down_y;
static double lobby_ball_last_x;
static double lobby_ball_last_y;
static double lobby_ball_move;
static double lobby_ball_emotion;
static double lobby_ball_speech_t;
static double lobby_ball_phrase;
static double lobby_ball_initialized;

static double lobby_ball_floor(void) { return screen_h - 14; }
static double lobby_ball_radius(void) { return clamp(fmin(screen_w, screen_h) * 0.065, 23, 38); }
static double lobby_ball_default_x(void) { return clamp(screen_w - lobby_ball_radius() - 62, lobby_ball_radius() + 12, screen_w - lobby_ball_radius() - 12); }

static const char *lobby_ball_phrase_text(void) {
    if (language == 1) {
        if (lobby_ball_phrase == 0) return "Привет!";
        if (lobby_ball_phrase == 1) return "Лови!";
        if (lobby_ball_phrase == 2) return "Ух ты!";
        return "Поиграем?";
    }
    if (lobby_ball_phrase == 0) return "Hello!";
    if (lobby_ball_phrase == 1) return "Catch me!";
    if (lobby_ball_phrase == 2) return "Whee!";
    return "Play with me?";
}

static void lobby_ball_say(double phrase) {
    lobby_ball_phrase = phrase;
    lobby_ball_speech_t = 2.2;
}

static void lobby_ball_init(void) {
    if (lobby_ball_initialized == 1) return;
    lobby_ball_r = lobby_ball_radius();
    lobby_ball_x = lobby_ball_default_x();
    lobby_ball_y = lobby_ball_floor() - lobby_ball_r;
    lobby_ball_emotion = 0;
    lobby_ball_phrase = 0;
    lobby_ball_initialized = 1;
}

static double lobby_ball_emotion_x(void) { return clamp(lobby_ball_default_x() + lobby_ball_r + 21, 24, screen_w - 20); }
static double lobby_ball_emotion_y(double index) { return lobby_ball_floor() - lobby_ball_r - 12 - index * 32; }

static void lobby_ball_tick(void) {
    if (game_state != ST_LOBBY) return;
    lobby_ball_init();
    lobby_ball_r = lobby_ball_radius();
    double dt_safe = clamp(dt, 0, 0.05);
    double floor_y = lobby_ball_floor() - lobby_ball_r;
    if (lobby_ball_pointer < 0) {
        lobby_ball_vy = lobby_ball_vy + 1100 * dt_safe;
        lobby_ball_x = lobby_ball_x + lobby_ball_vx * dt_safe;
        lobby_ball_y = lobby_ball_y + lobby_ball_vy * dt_safe;
        if (lobby_ball_x < lobby_ball_r + 3) {
            lobby_ball_x = lobby_ball_r + 3;
            if (lobby_ball_vx < 0) lobby_ball_vx = -lobby_ball_vx * 0.68;
        } else if (lobby_ball_x > screen_w - lobby_ball_r - 3) {
            lobby_ball_x = screen_w - lobby_ball_r - 3;
            if (lobby_ball_vx > 0) lobby_ball_vx = -lobby_ball_vx * 0.68;
        }
        if (lobby_ball_y > floor_y) {
            lobby_ball_y = floor_y;
            if (fabs(lobby_ball_vy) > 75) {
                lobby_ball_vy = -lobby_ball_vy * 0.58;
                lobby_ball_vx = lobby_ball_vx * 0.82;
            } else {
                lobby_ball_vy = 0;
                lobby_ball_vx = lobby_ball_vx * 0.94;
                if (fabs(lobby_ball_vx) < 6) lobby_ball_vx = 0;
            }
        }
    }
    if (lobby_ball_speech_t > 0) {
        lobby_ball_speech_t = fmax(0, lobby_ball_speech_t - dt_safe);
    }
}

static void lobby_ball_draw_expression(double x, double y, double scale, double emotion, uint32_t ink) {
    double eye_dx = 9 * scale;
    double eye_y = y - 4 * scale;
    if (emotion == 1) {
        circle(x - eye_dx, eye_y, 3.2 * scale, ink);
        circle(x + eye_dx, eye_y, 3.2 * scale, ink);
        circle(x, y + 8 * scale, 3.2 * scale, ink);
    } else if (emotion == 2) {
        line(x - 15 * scale, eye_y - 7 * scale, x - 4 * scale, eye_y - 4 * scale, 2 * scale, ink);
        line(x + 4 * scale, eye_y - 4 * scale, x + 15 * scale, eye_y - 7 * scale, 2 * scale, ink);
        circle(x - eye_dx, eye_y + 1 * scale, 2.3 * scale, ink);
        circle(x + eye_dx, eye_y + 1 * scale, 2.3 * scale, ink);
        line(x - 7 * scale, y + 11 * scale, x, y + 8 * scale, 2 * scale, ink);
        line(x, y + 8 * scale, x + 7 * scale, y + 11 * scale, 2 * scale, ink);
    } else if (emotion == 3) {
        line(x - 14 * scale, eye_y, x - 5 * scale, eye_y, 2.5 * scale, ink);
        line(x + 5 * scale, eye_y, x + 14 * scale, eye_y, 2.5 * scale, ink);
        line(x - 6 * scale, y + 9 * scale, x + 6 * scale, y + 9 * scale, 2 * scale, ink);
    } else {
        circle(x - eye_dx, eye_y, 2.5 * scale, ink);
        circle(x + eye_dx, eye_y, 2.5 * scale, ink);
        line(x - 8 * scale, y + 5 * scale, x - 4 * scale, y + 10 * scale, 2 * scale, ink);
        line(x - 4 * scale, y + 10 * scale, x + 4 * scale, y + 10 * scale, 2 * scale, ink);
        line(x + 4 * scale, y + 10 * scale, x + 8 * scale, y + 5 * scale, 2 * scale, ink);
    }
}

static void lobby_ball_draw_selector(double index) {
    double x = lobby_ball_emotion_x();
    double y = lobby_ball_emotion_y(index);
    double selected = lobby_ball_emotion == index ? 1 : 0;
    circle(x, y, selected == 1 ? 14 : 12, selected == 1 ? 0xFFF8D36A : 0xDDF2F4FF);
    circle(x, y, 10, 0xFF8E80EE);
    lobby_ball_draw_expression(x, y, 0.47, index, 0xFF211E3C);
}

static void lobby_ball_draw(void) {
    if (game_state != ST_LOBBY) return;
    lobby_ball_init();
    double x = lobby_ball_x;
    double y = lobby_ball_y;
    double r = lobby_ball_r;
    circle(x + 2, lobby_ball_floor() - 3, r * 0.88, 0x55302650);
    circle(x, y, r, 0xFF4A3EAA);
    circle(x, y - 2, r - 4, 0xFF9788F4);
    circle(x - r * 0.30, y - r * 0.38, r * 0.20, 0x77FFFFFF);
    lobby_ball_draw_expression(x, y, r / 32.0, lobby_ball_emotion, 0xFF252044);
    lobby_ball_draw_selector(0);
    lobby_ball_draw_selector(1);
    lobby_ball_draw_selector(2);
    if (lobby_ball_speech_t > 0) {
        const char *phrase = lobby_ball_phrase_text();
        double w = fmax(76, text_ink_width(phrase) * 0.82 + 24);
        w = fmin(w, fmax(80, screen_w - 16));
        double bx = clamp(x - w * 0.5, 8, screen_w - w - 8);
        double by = fmax(12, y - r - 47);
        roundrect(bx, by, w, 31, 10, 0xF8FFFFFF);
        roundrect_outline(bx, by, w, 31, 10, 1.5, 0xFF38334F);
        line(x - 5, by + 29, x, by + 38, 1.8, 0xFF38334F);
        line(x, by + 38, x + 5, by + 29, 1.8, 0xFF38334F);
        text_scaled(phrase, bx + (w - text_ink_width(phrase) * 0.78) * 0.5,
                    by + 8, 0xFF252044, 0.78);
    }
}

static double lobby_ball_touch(double x, double y, double action, double id) {
    if (game_state != ST_LOBBY || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) return 0;
    lobby_ball_init();
    if (lobby_ball_pointer >= 0) {
        if (id != lobby_ball_pointer) return 0;
        if (action == 2) {
            lobby_ball_x = x + lobby_ball_drag_dx;
            lobby_ball_y = y + lobby_ball_drag_dy;
            lobby_ball_move = lobby_ball_move + fabs(x - lobby_ball_last_x) + fabs(y - lobby_ball_last_y);
            lobby_ball_last_x = x;
            lobby_ball_last_y = y;
            return 1;
        }
        if (action == 1 || action == 3) {
            double dx = x - lobby_ball_down_x;
            double dy = y - lobby_ball_down_y;
            if (action == 1 && lobby_ball_move > 12) {
                lobby_ball_vx = clamp(dx * 4.2, -1050, 1050);
                lobby_ball_vy = clamp(dy * 4.2, -1250, 1250);
                lobby_ball_emotion = 1;
                lobby_ball_say(1);
            } else if (action == 1) {
                lobby_ball_emotion = fmod(lobby_ball_emotion + 1, 4);
                lobby_ball_say(2);
            }
            lobby_ball_pointer = -1;
            return 1;
        }
        return 1;
    }
    if (action != 0) return 0;
    double ex = lobby_ball_emotion_x();
    for (double i = 0; i < 3; i = i + 1) {
        double ey = lobby_ball_emotion_y(i);
        if (hypot(x - ex, y - ey) <= 18) {
            lobby_ball_emotion = i;
            lobby_ball_say(i == 0 ? 0 : i == 1 ? 2 : 3);
            play_button_click();
            return 1;
        }
    }
    if (hypot(x - lobby_ball_x, y - lobby_ball_y) <= lobby_ball_r + 9) {
        lobby_ball_pointer = id;
        lobby_ball_drag_dx = lobby_ball_x - x;
        lobby_ball_drag_dy = lobby_ball_y - y;
        lobby_ball_down_x = x;
        lobby_ball_down_y = y;
        lobby_ball_last_x = x;
        lobby_ball_last_y = y;
        lobby_ball_move = 0;
        lobby_ball_vx = 0;
        lobby_ball_vy = 0;
        return 1;
    }
    return 0;
}
