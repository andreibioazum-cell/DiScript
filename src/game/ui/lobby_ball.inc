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
static double lobby_ball_drag_vx;
static double lobby_ball_drag_vy;
static double lobby_ball_drag_time;
static double lobby_ball_drag_quiet;
static double lobby_ball_speech_t;
static double lobby_ball_phrase;
static double lobby_ball_initialized;

static double lobby_ball_floor(void) { return screen_h - 14; }
/* Let the ball reach the actual top edge; its radius keeps the sprite on-screen. */
static double lobby_ball_ceiling(void) { return 0; }
static double lobby_ball_radius(void) { return clamp(fmin(screen_w, screen_h) * 0.065, 23, 38); }
static double lobby_ball_left(void) { return lobby_ball_radius() + 3; }
static double lobby_ball_right(void) { return fmax(lobby_ball_left() + 2 * lobby_ball_radius(), screen_w - lobby_ball_radius() - 3); }
static double lobby_ball_default_x(void) { return clamp(screen_w - lobby_ball_radius() - 62, lobby_ball_left(), lobby_ball_right()); }
static double lobby_ball_min_y(void) { return lobby_ball_ceiling() + lobby_ball_radius(); }
static double lobby_ball_max_y(void) { return lobby_ball_floor() - lobby_ball_radius(); }
static double lobby_ball_bubble_y(void) {
    double y = lobby_ball_y - lobby_ball_r - 43;
    /* Hide the bubble rather than pinning it over the ball at the top edge. */
    return y < 8 ? -1 : y;
}

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
    lobby_ball_y = lobby_ball_max_y();
    lobby_ball_phrase = 0;
    lobby_ball_initialized = 1;
}

static void lobby_ball_tick(void) {
    if (game_state != ST_LOBBY) return;
    lobby_ball_init();
    lobby_ball_r = lobby_ball_radius();
    double dt_safe = clamp(dt, 0, 0.05);
    if (lobby_ball_pointer >= 0) {
        lobby_ball_drag_time = lobby_ball_drag_time + dt_safe;
        lobby_ball_drag_quiet = lobby_ball_drag_quiet + dt_safe;
        double decay = exp(-1.2 * dt_safe);
        lobby_ball_drag_vx = lobby_ball_drag_vx * decay;
        lobby_ball_drag_vy = lobby_ball_drag_vy * decay;
    } else {
        /* Substeps keep a fast throw from tunnelling through a wall or floor. */
        int steps = (int)ceil(dt_safe / (1.0 / 120.0));
        if (steps < 1) steps = 1;
        double step_dt = dt_safe / steps;
        for (int i = 0; i < steps; i++) {
            lobby_ball_vy = lobby_ball_vy + 1750 * step_dt;
            lobby_ball_vx = lobby_ball_vx * exp(-0.16 * step_dt);
            lobby_ball_x = lobby_ball_x + lobby_ball_vx * step_dt;
            lobby_ball_y = lobby_ball_y + lobby_ball_vy * step_dt;

            if (lobby_ball_x < lobby_ball_left()) {
                lobby_ball_x = lobby_ball_left();
                if (lobby_ball_vx < 0) lobby_ball_vx = -lobby_ball_vx * 0.78;
            } else if (lobby_ball_x > lobby_ball_right()) {
                lobby_ball_x = lobby_ball_right();
                if (lobby_ball_vx > 0) lobby_ball_vx = -lobby_ball_vx * 0.78;
            }
            if (lobby_ball_y < lobby_ball_min_y()) {
                lobby_ball_y = lobby_ball_min_y();
                if (lobby_ball_vy < 0) lobby_ball_vy = -lobby_ball_vy * 0.76;
            } else if (lobby_ball_y > lobby_ball_max_y()) {
                lobby_ball_y = lobby_ball_max_y();
                if (lobby_ball_vy > 95) {
                    lobby_ball_vy = -lobby_ball_vy * 0.68;
                    lobby_ball_vx = lobby_ball_vx * 0.90;
                } else {
                    lobby_ball_vy = 0;
                    lobby_ball_vx = lobby_ball_vx * 0.92;
                    if (fabs(lobby_ball_vx) < 7) lobby_ball_vx = 0;
                }
            }
        }
    }
    if (lobby_ball_speech_t > 0) {
        lobby_ball_speech_t = fmax(0, lobby_ball_speech_t - dt_safe);
    }
}

static void lobby_ball_draw_face(double x, double y, double scale, uint32_t ink) {
    double eye_dx = 9 * scale;
    double eye_y = y - 4 * scale;
    circle(x - eye_dx, eye_y, 2.5 * scale, ink);
    circle(x + eye_dx, eye_y, 2.5 * scale, ink);
    line(x - 8 * scale, y + 5 * scale, x - 4 * scale, y + 10 * scale, 2 * scale, ink);
    line(x - 4 * scale, y + 10 * scale, x + 4 * scale, y + 10 * scale, 2 * scale, ink);
    line(x + 4 * scale, y + 10 * scale, x + 8 * scale, y + 5 * scale, 2 * scale, ink);
}

static void lobby_ball_draw(void) {
    if (game_state != ST_LOBBY) return;
    lobby_ball_init();
    double x = lobby_ball_x;
    double y = lobby_ball_y;
    double r = lobby_ball_r;
    /* Boundaries are physical but invisible: keep the lobby free of rails. */
    circle(x, y, r, RGB(74,62,170));
    circle(x, y - 2, r - 4, RGB(137,123,223));
    lobby_ball_draw_face(x, y, r / 32.0, RGB(37,32,68));
    if (lobby_ball_speech_t > 0) {
        const char *phrase = lobby_ball_phrase_text();
        double w = fmax(76, text_ink_width(phrase) * 0.82 + 24);
        w = fmin(w, fmax(80, screen_w - 16));
        double bx = clamp(x - w * 0.5, 8, screen_w - w - 8);
        double by = lobby_ball_bubble_y();
        if (by >= 0) {
            roundrect(bx, by, w, 31, 10, RGB(0,0,0));
            roundrect_outline(bx, by, w, 31, 10, 1.5, RGB(0,0,0));
            line(x - 5, by + 29, x, by + 38, 1.8, RGB(0,0,0));
            line(x, by + 38, x + 5, by + 29, 1.8, RGB(0,0,0));
            text_scaled(phrase, bx + (w - text_ink_width(phrase) * 0.78) * 0.5,
                        by + 8, RGB(255,255,255), 0.78);
        }
    }
}

static void lobby_ball_record_drag_velocity(double x, double y) {
    double dx = x - lobby_ball_last_x;
    double dy = y - lobby_ball_last_y;
    double distance = hypot(dx, dy);
    if (distance > 0.4) {
        double sample_dt = clamp(dt, 1.0 / 120.0, 0.05);
        double sample_vx = clamp(dx / sample_dt, -2400, 2400);
        double sample_vy = clamp(dy / sample_dt, -2400, 2400);
        if (lobby_ball_drag_quiet > 0.08 || lobby_ball_move < 1) {
            lobby_ball_drag_vx = sample_vx;
            lobby_ball_drag_vy = sample_vy;
        } else {
            lobby_ball_drag_vx = lobby_ball_drag_vx * 0.30 + sample_vx * 0.70;
            lobby_ball_drag_vy = lobby_ball_drag_vy * 0.30 + sample_vy * 0.70;
        }
        lobby_ball_drag_vx = clamp(lobby_ball_drag_vx, -2400, 2400);
        lobby_ball_drag_vy = clamp(lobby_ball_drag_vy, -2400, 2400);
        lobby_ball_drag_quiet = 0;
        lobby_ball_move = lobby_ball_move + distance;
    }
    lobby_ball_last_x = x;
    lobby_ball_last_y = y;
}

static double lobby_ball_touch(double x, double y, double action, double id) {
    if (game_state != ST_LOBBY || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) return 0;
    lobby_ball_init();
    if (lobby_ball_pointer >= 0) {
        if (id != lobby_ball_pointer) return 0;
        if (action == 2) {
            lobby_ball_record_drag_velocity(x, y);
            lobby_ball_x = clamp(x + lobby_ball_drag_dx, lobby_ball_left(), lobby_ball_right());
            lobby_ball_y = clamp(y + lobby_ball_drag_dy, lobby_ball_min_y(), lobby_ball_max_y());
            return 1;
        }
        if (action == 1 || action == 3) {
            lobby_ball_record_drag_velocity(x, y);
            if (action == 1 && lobby_ball_move > 12) {
                double vx = lobby_ball_drag_vx;
                double vy = lobby_ball_drag_vy;
                double speed = hypot(vx, vy);
                if (speed < 260 && lobby_ball_drag_quiet < 0.18) {
                    double dx = x - lobby_ball_down_x;
                    double dy = y - lobby_ball_down_y;
                    double length = hypot(dx, dy);
                    if (length > 4) {
                        double fallback_speed = clamp(lobby_ball_move / fmax(lobby_ball_drag_time, 0.08) * 0.72, 420, 1700);
                        vx = dx / length * fallback_speed;
                        vy = dy / length * fallback_speed;
                    }
                }
                lobby_ball_vx = clamp(vx * 1.10, -2200, 2200);
                lobby_ball_vy = clamp(vy * 1.10, -2200, 2200);
                /* Reflect an upward throw immediately if the ball is already at its ceiling. */
                if (lobby_ball_y <= lobby_ball_min_y() && lobby_ball_vy < 0) lobby_ball_vy = -lobby_ball_vy * 0.76;
                lobby_ball_say(1);
            } else if (action == 1) {
                lobby_ball_say(0);
            }
            lobby_ball_pointer = -1;
            return 1;
        }
        return 1;
    }
    if (action != 0) return 0;
    if (hypot(x - lobby_ball_x, y - lobby_ball_y) <= lobby_ball_r + 9) {
        lobby_ball_pointer = id;
        lobby_ball_drag_dx = lobby_ball_x - x;
        lobby_ball_drag_dy = lobby_ball_y - y;
        lobby_ball_down_x = x;
        lobby_ball_down_y = y;
        lobby_ball_last_x = x;
        lobby_ball_last_y = y;
        lobby_ball_move = 0;
        lobby_ball_drag_vx = 0;
        lobby_ball_drag_vy = 0;
        lobby_ball_drag_time = 0;
        lobby_ball_drag_quiet = 0;
        lobby_ball_vx = 0;
        lobby_ball_vy = 0;
        return 1;
    }
    return 0;
}
