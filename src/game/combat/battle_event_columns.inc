/* Event 5 (Columns): like Ded Moroz, one finite event round; the figure never turns. */

static void columns_event_reset(void) {
    columns_phase = 1;
    columns_t = 0;
    columns_shot_active = 0;
    columns_shot_x = 0;
    columns_shot_y = screen_h / 2;
    columns_shot_cd = 0.65;
    columns_thrown = 0;
    columns_dy = 0;
}

static void update_columns_event(void) {
    if (event_mode != 5) {
        columns_phase = 0;
        columns_shot_active = 0;
        columns_shot_cd = 0;
        columns_thrown = 0;
        columns_dy = 0;
        return;
    }
    if (columns_phase == 0) {
        columns_event_reset();
    }
    columns_t = columns_t + dt;

    if (columns_phase == 1) {
        columns_shot_cd = columns_shot_cd - dt;
        if (columns_shot_active == 0 && columns_thrown < columns_shot_total && columns_shot_cd <= 0) {
            /* Fixed facing: launch every cane along +X from the same horizontal lane. */
            columns_shot_x = screen_w / 2 + columns_size * 0.55;
            columns_shot_y = screen_h / 2;
            columns_shot_active = 1;
            columns_shot_cd = columns_shot_interval;
            columns_thrown = columns_thrown + 1;
        }
        if (columns_thrown >= columns_shot_total) {
            columns_phase = 2;
        }
    } else if (columns_phase == 2) {
        /* Match Santa's one-shot round: after the last throw, the character leaves. */
        columns_dy = columns_dy - columns_exit_speed * dt;
        if (columns_dy < 0 - screen_h / 2 - columns_size) {
            columns_phase = 3;
        }
    }

    if (columns_shot_active == 1) {
        columns_shot_x = columns_shot_x + columns_shot_speed * dt;
        if (game_state == ST_ONLINE && player->hp > 0 &&
            dist(columns_shot_x, columns_shot_y, player->x, player->y) < player->size * 0.55 + 15) {
            take_damage_at(columns_shot_damage, columns_shot_x - 20, columns_shot_y);
            columns_shot_active = 0;
        } else if (columns_shot_x > screen_w + 40) {
            columns_shot_active = 0;
        }
    }
}

static void draw_columns_cane(double x, double y) {
    /* Sideways cane with a curved handle trailing as the shaft flies forward.
     * Every offset scales with the event figure, so a bigger Columns stays
     * readable instead of throwing tiny sticks. */
    double s = columns_size / 150;
    double back = x - 19 * s;
    double tip = x + 16 * s;
    double up = y - 12 * s;
    double hook = y - 16 * s;
    double hx = x - 8 * s;
    double rim = 8 * s;
    double core = 4 * s;
    line(back, y, tip, y, rim, RGB(68,38,17));
    line(back, y, tip, y, core, RGB(255,213,106));
    line(back, y, back, up, rim, RGB(68,38,17));
    line(back, up, hx, hook, rim, RGB(68,38,17));
    line(back, y, back, up, core, RGB(255,213,106));
    line(back, up, hx, hook, core, RGB(255,213,106));
    circle(tip, y, core, RGB(255,240,176));
}

static void draw_columns_event(void) {
    if (event_mode != 5 || columns_phase == 0 || columns_phase == 3) {
        return;
    }
    double cx = screen_w / 2;
    double cy = screen_h / 2 + columns_dy;
    double pop = clamp(columns_t / 0.4, 0, 1);
    pop = 1 - (1 - pop) * (1 - pop);
    double scale = columns_size / 500 * pop;
    double half = columns_size / 2 * pop;
    double wob = 4 * pop * sin(columns_t * 3) * (columns_phase == 1);
    if (tex_ready(EVENT_COLUMNS_TEX) == 1) {
        tex(cx - half, cy - half + wob, EVENT_COLUMNS_TEX, 0, scale);
    } else {
        roundrect(cx - half, cy - half, columns_size * pop, columns_size * pop, 18, RGB(23,74,130));
        text_scaled(language == 1 ? "Колонки" : "Columns", cx - 30, cy - 8, RGB(255,255,255), 0.45);
    }
    draw_overhead_nick(language == 1 ? "Колонки" : "Columns", cx, cy - half - 8 + wob, RGB(255,213,74));
    if (columns_shot_active == 1) {
        draw_columns_cane(columns_shot_x, columns_shot_y);
    }
}

static void draw_columns_hud(void) {
    if (event_mode != 5 || columns_phase == 0 || columns_phase == 3) {
        return;
    }
    double remaining = columns_shot_total - columns_thrown;
    const char *label = language == 1 ? "Тростей осталось: " : "Canes left: ";
    ctext_scaled(ds_concat(label, ds_num_to_string(remaining)), back_y + btn_h + 6, RGB(255,255,255), 0.5);
}
