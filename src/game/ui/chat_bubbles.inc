/* Overhead chat bubble buffers filtered at receive time. */
static char bubble_buf[4][CF_OUT_MAX];

static void bubble_set(double s, const char *raw) {
    int slot = (int)s;
    const char *m;
    if (slot < 0 || slot > 3) {
        return;
    }
    chat_clean_into(bubble_buf[slot], sizeof(bubble_buf[slot]), raw);
    m = bubble_buf[slot];
    if (s == 0) {
        bubble_msg0 = m;
        bubble_t0 = chat_bubble_time;
    } else if (s == 1) {
        bubble_msg1 = m;
        bubble_t1 = chat_bubble_time;
    } else if (s == 2) {
        bubble_msg2 = m;
        bubble_t2 = chat_bubble_time;
    } else if (s == 3) {
        bubble_msg3 = m;
        bubble_t3 = chat_bubble_time;
    }
}

static const char *bubble_msg(double s) {
    if (s == 0) {
        return bubble_msg0;
    } else if (s == 1) {
        return bubble_msg1;
    } else if (s == 2) {
        return bubble_msg2;
    }
    return bubble_msg3;
}

static double bubble_left(double s) {
    if (s == 0) {
        return bubble_t0;
    } else if (s == 1) {
        return bubble_t1;
    } else if (s == 2) {
        return bubble_t2;
    }
    return bubble_t3;
}

static void chat_bubble_show(const char *uid, const char *msg) {
    if (str_len(msg) == 0) {
        return;
    }
    double s = net_slot();
    if (str_eq(uid, net_login_nick()) == 1) {
        bubble_set(s, msg);
        return;
    }
    double i = 0;
    while (i < max_players) {
        if (i != s && str_eq(uid, net_player_nick(i)) == 1) {
            bubble_set(i, msg);
        }
        i = i + 1;
    }
}

static void chat_bubbles_watch(void) {
    if (bubble_t0 > 0) {
        bubble_t0 = bubble_t0 - dt;
        if (bubble_t0 < 0) {
            bubble_t0 = 0;
        }
    }
    if (bubble_t1 > 0) {
        bubble_t1 = bubble_t1 - dt;
        if (bubble_t1 < 0) {
            bubble_t1 = 0;
        }
    }
    if (bubble_t2 > 0) {
        bubble_t2 = bubble_t2 - dt;
        if (bubble_t2 < 0) {
            bubble_t2 = 0;
        }
    }
    if (bubble_t3 > 0) {
        bubble_t3 = bubble_t3 - dt;
        if (bubble_t3 < 0) {
            bubble_t3 = 0;
        }
    }
    if (net_slot() < 0) {
        return;
    }
    double cnt = net_chat_count();
    if (cnt <= 0) {
        return;
    }
    double last = cnt - 1;
    const char *lastkey = net_chat_key(last);
    if (str_eq(lastkey, chat_seen_key) == 1) {
        return;
    }
    double starti = last;
    double k = last - 1;
    while (k >= 0) {
        if (str_eq(net_chat_key(k), chat_seen_key) == 1) {
            starti = k + 1;
            k = -1;
        } else {
            k = k - 1;
        }
    }
    double m = starti;
    while (m <= last) {
        chat_bubble_show(net_chat_uid(m), net_chat_text(m));
        m = m + 1;
    }
    /* Copy key characters instead of aliasing net_chat_key()'s shared static buffer. */
    double ki = 0;
    while (lastkey[(int)ki] != 0 && ki < (double)(sizeof(chat_seen_key_buf) - 1)) {
        chat_seen_key_buf[(int)ki] = lastkey[(int)ki];
        ki = ki + 1;
    }
    chat_seen_key_buf[(int)ki] = 0;
    chat_seen_key = chat_seen_key_buf;
}

static void draw_chat_bubble_at(double x, double y, const char *msg, double t) {
    if (t <= 0 || str_len(msg) == 0 || chat_is_sticker_msg(msg) == 1) {
        return;
    }
    /* Ease bubble rise and opacity over lifetime. */
    double age = chat_bubble_time - t;
    double kin = clamp(age / 0.18, 0, 1);
    kin = kin * kin * (3 - 2 * kin);
    double kout = clamp(t / chat_bubble_fade, 0, 1);
    kout = kout * kout * (3 - 2 * kout);
    double a = floor(kin * kout * chat_bubble_alpha);
    if (a <= 0) {
        return;
    }
    uint32_t box = (((unsigned)(int)a) << 24) | 0x00FFFFFF;
    double hw = 25 * cube_scale * player_scale;
    double bw = 0;
    double bh = 0;
    double bx = 0;
    double by = 0;
    double sc = 0.5;
    {
        double tw = text_ink_width(msg) * sc;
        double maxw = screen_w - 24 - 2 * chat_bubble_pad;
        if (tw > maxw) {
            sc = sc * maxw / tw;
            tw = maxw;
        }
        bw = tw + 2 * chat_bubble_pad;
        bh = chat_bubble_h;
    }
    bx = clamp(x - bw / 2, 4, screen_w - bw - 4);
    by = y - hw - bh - 30;
    if (by < 4) {
        by = 4;
    }
    by = by + (1 - kin) * 10;
    roundrect(bx, by, bw, bh, 10, box);
    uint32_t ink = (((unsigned)(int)a) << 24) | ((unsigned)(int)chat_bubble_ink);
    text_in_box(msg, bx, by, bw, bh, ink, sc);
}

static void chat_bubbles_reset(void) {
    chat_seen_key_buf[0] = 0;
    bubble_msg0 = "";
    bubble_msg1 = "";
    bubble_msg2 = "";
    bubble_msg3 = "";
    bubble_t0 = 0;
    bubble_t1 = 0;
    bubble_t2 = 0;
    bubble_t3 = 0;
    chat_seen_key = "";
}

static void draw_chat_bubbles(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = net_slot();
    double i = 0;
    while (i < max_players) {
        if (i == s) {
            draw_chat_bubble_at(player->x, player->y, bubble_msg(i), bubble_left(i));
        } else {
            double b = i * remote_fields;
            if (arr_get(remotes, b + 5) == 1) {
                draw_chat_bubble_at(arr_get(remotes, b + 1), arr_get(remotes, b + 2), bubble_msg(i), bubble_left(i));
            }
        }
        i = i + 1;
    }
}
