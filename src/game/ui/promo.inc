static double promo_field_y(void) { return content_top() + 40; }

static double promo_btn_y(void) {
    return promo_field_y() + 56 + 20;
}

static double promo_msg_y(void) {
    return promo_btn_y() + btn_h + 16;
}

/* Static copy of promo status text so pooled strings can expire safely. */
static char promo_msg_buf[128];

static void promo_set_msg(const char *text) {
    /* Manual bounded copy: the game translation unit has no stdio. */
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(promo_msg_buf) - 1)) {
        promo_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    promo_msg_buf[(int)i] = 0;
    promo_msg_text = promo_msg_buf;
    promo_msg_t = 4.0;
}

static void promo_leave(void) {
    promo_focus = 0;
    keyboard_hide();
}

/* Return cup reward for fixed promo code `code`. */
static double promo_reward_of(const char *code) {
    if (str_eq(code, "2027") == 1) {
        return promo_2027_cups;
    }
    return 0;
}

static void promo_submit(void) {
    if (promo_focus == 1) {
        keyboard_get_text_to(promo_input_buf, sizeof(promo_input_buf));
        promo_input = promo_input_buf;
    }
    const char *code = str_upper(str_trim(promo_input));
    if (str_len(code) == 0) {
        promo_set_msg(tr_promo_need_code());
        return;
    }
    if (net_login_status() != 2) {
        promo_set_msg(tr_promo_need_login());
        promo_leave();
        return;
    }
    double reward = promo_reward_of(code);
    if (reward == 0) {
        promo_set_msg(tr_promo_bad());
        return;
    }
    if (net_promo_2027_used() == 1) {
        promo_set_msg(tr_promo_used_msg());
        return;
    }
    cups = cups + reward;
    net_promo_2027_mark_used();
    save_progress();
    ds_log("promo 2027 redeemed: +%g cups", reward);
    promo_leave();
    promo_set_msg(tr_promo_success_2027());
}

static void update_promo(void) {
    if (promo_focus == 0 || t_dir != 0) {
        return;
    }
    if (keyboard_visible() == 1) {
        keyboard_get_text_to(promo_input_buf, sizeof(promo_input_buf));
        promo_input = promo_input_buf;
    }
    if (keyboard_enter_pressed() == 1) {
        promo_submit();
    }
}

static void draw_promo(void) {
    screen_fill();
    draw_back();
    ctext(tr_promo_title(), title_y(), text_color);
    double w = fit_w(560);
    double x = center_x(w);
    double fy = promo_field_y();
    roundrect(x, fy, w, 56, field_radius, card_bg);
    double caret = 0;
    if (promo_focus == 1 && keyboard_visible() == 1) {
        caret = 1;
    }
    const char *shown = promo_input;
    if (caret == 1) {
        shown = ds_concat(shown, "|");
    }
    if (str_len(shown) == 0) {
        shown = tr_promo_enter_ph();
    }
    text_in_field(shown, x + field_text_pad, fy, 56, text_color, 0.6);
    button(tr_promo_submit(), x, promo_btn_y(), w, btn_h);
    promo_msg_t = tick_down(promo_msg_t);
    draw_msg(promo_msg_text, promo_msg_y(), promo_msg_t);
}

static void touch_promo(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        promo_leave();
        start_transition(ST_MODES);
        return;
    }
    double w = fit_w(560);
    double fx = center_x(w);
    if (hit_rect(x, y, fx, promo_field_y(), w, 56) == 1) {
        promo_focus = 1;
        keyboard_show();
        return;
    }
    if (hit_button(x, y, fx, promo_btn_y(), w, btn_h) == 1) {
        promo_submit();
        return;
    }
    promo_leave();
}
