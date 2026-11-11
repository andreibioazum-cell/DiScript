/* Multi-touch rounded-rectangle hit testing and button squeeze animations. */

/* Test whether point lies inside a rounded rectangle. */
static double hit_round(double px, double py, double x, double y, double w, double h, double rad) {
    if (hit_rect(px, py, x, y, w, h) == 0) {
        return 0;
    }
    if (rad > w / 2) {
        rad = w / 2;
    }
    if (rad > h / 2) {
        rad = h / 2;
    }
    if (rad <= 0) {
        return 1;
    }
    double cx = clamp(px, x + rad, x + w - rad);
    double cy = clamp(py, y + rad, y + h - rad);
    double dx = px - cx;
    double dy = py - cy;
    return dx * dx + dy * dy <= rad * rad ? 1 : 0;
}

/* Test whether point hits a standard rounded button. */
static double hit_button(double px, double py, double x, double y, double w, double h) {
    return hit_round(px, py, x, y, w, h, btn_radius);
}

/* Button press squeeze and shade constants. */
#define PRESS_IN_TIME 0.06
#define PRESS_OUT_TIME 0.13
#define PRESS_SQUEEZE 0.05
#define PRESS_SHADE 0.25

/* Touches that went down and have not found their button yet, one per finger. */
#define PRESS_POINTS 4
/* Squeezes running right now, one per pressed button. */
#define PRESS_PULSES 4

static double press_point_x[PRESS_POINTS];
static double press_point_y[PRESS_POINTS];
static double press_point_id[PRESS_POINTS];
static double press_point_n;

static double press_k[PRESS_PULSES];
static double press_phase[PRESS_PULSES];
static double press_btn_x[PRESS_PULSES];
static double press_btn_y[PRESS_PULSES];
static double press_btn_w[PRESS_PULSES];
static double press_btn_h[PRESS_PULSES];
static double press_btn_gen[PRESS_PULSES];

/* Active overlay layer (0 screen, 1 warning/chat, 2 docs, 3 account). */
static double press_overlay;

/* Return 1 when current draw layer is occluded by an active modal or transition. */
static double press_covered(void) {
    if (acct_open != 0) {
        return press_overlay != 3;
    }
    if (docs_open == 1) {
        return press_overlay != 2;
    }
    if (warn_open == 1) {
        return press_overlay != 1;
    }
    if (press_overlay == 1) {
        return 0;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return finished != 0 || (game_state == ST_ONLINE && chat_open == 1);
    }
    return 0;
}

/* Record or update active touch point for pointer `id`. */
static void press_points_add(double x, double y, double id) {
    double i = 0;
    while (i < press_point_n) {
        if (press_point_id[(int)i] == id) {
            press_point_x[(int)i] = x;
            press_point_y[(int)i] = y;
            return;
        }
        i = i + 1;
    }
    if (press_point_n >= PRESS_POINTS) {
        /* Older fingers keep their buttons: the newest touch is the one to go. */
        press_point_n = PRESS_POINTS - 1;
    }
    press_point_x[(int)press_point_n] = x;
    press_point_y[(int)press_point_n] = y;
    press_point_id[(int)press_point_n] = id;
    press_point_n = press_point_n + 1;
}

/* One finger up: only its own point leaves the queue. */
static void press_points_drop(double id) {
    double i = 0;
    while (i < press_point_n) {
        if (press_point_id[(int)i] == id) {
            double j = i;
            while (j + 1 < press_point_n) {
                press_point_x[(int)j] = press_point_x[(int)(j + 1)];
                press_point_y[(int)j] = press_point_y[(int)(j + 1)];
                press_point_id[(int)j] = press_point_id[(int)(j + 1)];
                j = j + 1;
            }
            press_point_n = press_point_n - 1;
            return;
        }
        i = i + 1;
    }
}

static void press_points_clear(void) { press_point_n = 0; }

/* Clear all active squeeze pulses on screen change. */
static void press_pulses_clear(void) {
    double i = 0;
    while (i < PRESS_PULSES) {
        press_k[(int)i] = 0;
        press_phase[(int)i] = 0;
        i = i + 1;
    }
}

/* Claim or restart a squeeze pulse for the given control bounds. */
static void press_pulse_claim(double x, double y, double w, double h) {
    double i = 0;
    double slot = -1;
    while (i < PRESS_PULSES) {
        double same = press_phase[(int)i] != 0 && press_btn_gen[(int)i] == ui_nav_gen && x == press_btn_x[(int)i] &&
                      y == press_btn_y[(int)i] && w == press_btn_w[(int)i] && h == press_btn_h[(int)i];
        if (same == 1) {
            press_k[(int)i] = 0;
            press_phase[(int)i] = 1;
            return;
        }
        if (slot < 0 && press_phase[(int)i] == 0) {
            slot = i;
        }
        i = i + 1;
    }
    if (slot < 0) {
        slot = 0;
    }
    press_btn_x[(int)slot] = x;
    press_btn_y[(int)slot] = y;
    press_btn_w[(int)slot] = w;
    press_btn_h[(int)slot] = h;
    press_btn_gen[(int)slot] = ui_nav_gen;
    press_k[(int)slot] = 0;
    press_phase[(int)slot] = 1;
}

/* Claim touch points inside rounded control bounds and return smoothstepped squeeze factor. */
static double button_press_k_round(double x, double y, double w, double h, double rad) {
    double i = 0;
    double best = 0;
    if (press_covered() == 0) {
        while (i < press_point_n) {
            if (hit_round(press_point_x[(int)i], press_point_y[(int)i], x, y, w, h, rad) == 1) {
                press_points_drop(press_point_id[(int)i]);
                press_pulse_claim(x, y, w, h);
                continue;
            }
            i = i + 1;
        }
    }
    i = 0;
    while (i < PRESS_PULSES) {
        double same = press_phase[(int)i] != 0 && press_btn_gen[(int)i] == ui_nav_gen && x == press_btn_x[(int)i] &&
                      y == press_btn_y[(int)i] && w == press_btn_w[(int)i] && h == press_btn_h[(int)i];
        if (same == 1) {
            /* Smoothstep: the squeeze eases in and the spring back eases out. */
            double p = press_k[(int)i];
            double eased = p * p * (3 - 2 * p);
            if (eased > best) {
                best = eased;
            }
        }
        i = i + 1;
    }
    return best;
}

static double button_press_k(double x, double y, double w, double h) {
    return button_press_k_round(x, y, w, h, btn_radius);
}

static void update_button_press(void) {
    double i = 0;
    while (i < PRESS_PULSES) {
        if (press_phase[(int)i] == 1) {
            press_k[(int)i] = press_k[(int)i] + dt / PRESS_IN_TIME;
            if (press_k[(int)i] >= 1) {
                press_k[(int)i] = 1;
                press_phase[(int)i] = 2;
            }
        } else if (press_phase[(int)i] == 2) {
            press_k[(int)i] = press_k[(int)i] - dt / PRESS_OUT_TIME;
            if (press_k[(int)i] <= 0) {
                /* Back to rest. */
                press_k[(int)i] = 0;
                press_phase[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}
