/* Parental control: a device-only PIN plus an age mode that limits chat and
 * hides nicks with swearing. Nothing about it is sent to the server. */

#define PARENT_ADULT 0 /* 14+ years: everything is allowed, the infinity badge shows */
#define PARENT_TEEN 1  /* 11-13 years: chat stays, chat and nicks are filtered */
#define PARENT_KID 2  /* 1-10 years: no chat, nicks with swearing are hidden */

static double parent_open;
static double parent_stage; /* 0 ask pin, 1 create pin, 2 choose age */
static double parent_age;   /* 0 = 14+, 1 = 11-13, 2 = 1-10 */
static double parent_pin;   /* stored hash, 0 = no pin yet */
static double parent_loaded;
static char parent_buf[24];
static char parent_msg[64];

static const char *tr_parent(void) { return packs_tr("Родительский контроль", "Parental control"); }
static const char *tr_parent_title(void) { return packs_tr("Родительский контроль", "Parental control"); }
static const char *tr_parent_ask(void) { return packs_tr("Введите пароль родителя", "Enter the parent password"); }
static const char *tr_parent_new(void) { return packs_tr("Придумайте пароль", "Choose a password"); }
static const char *tr_parent_open(void) { return packs_tr("Открыть", "Open"); }
static const char *tr_parent_save(void) { return packs_tr("Сохранить", "Save"); }
static const char *tr_parent_done(void) { return packs_tr("Готово", "Done"); }
static const char *tr_parent_wrong(void) { return packs_tr("Неверный пароль", "Wrong password"); }
static const char *tr_parent_short(void) {
    return packs_tr("Пароль от 4 символов", "At least 4 characters");
}
static const char *tr_parent_hint(void) {
    return packs_tr("Пароль хранится только на этом устройстве", "The password stays on this device only");
}
static const char *tr_parent_kid(void) { return packs_tr("1-10 лет", "1-10 years"); }
static const char *tr_parent_teen(void) { return packs_tr("11-13 лет", "11-13 years"); }
static const char *tr_parent_adult(void) { return packs_tr("14 и старше", "14 and older"); }
static const char *tr_parent_kid_note(void) {
    return packs_tr("Чат закрыт, ники с матом скрыты", "No chat, nicks with swearing are hidden");
}
static const char *tr_parent_teen_note(void) {
    return packs_tr("Чат с фильтром, ники с матом закрыты звёздочками",
                    "Chat stays, swearing in chat and nicks is filtered");
}
static const char *tr_parent_adult_note(void) { return packs_tr("Всё доступно", "Everything is available"); }
static const char *tr_parent_state(void) {
    if (parent_age >= PARENT_KID)
        return tr_parent_kid();
    if (parent_age >= PARENT_TEEN)
        return tr_parent_teen();
    return tr_parent_adult();
}

/* FNV-1a: the device keeps a hash, never the typed password. */
static double parent_hash(const char *s) {
    unsigned h = 2166136261u;
    while (s && *s) {
        h ^= (unsigned char)*s++;
        h *= 16777619u;
    }
    return (double)(h & 0x7FFFFFFFu);
}

static void parent_load(void) {
    if (parent_loaded == 1)
        return;
    parent_loaded = 1;
    parent_pin = net_load_parent_pin();
    parent_age = net_load_parent_age();
    if (parent_age < 0 || parent_age > PARENT_KID)
        parent_age = 0;
}

static void parent_note(void) { parent_msg[0] = 0; }

static void parental_open(void) {
    parent_load();
    parent_open = 1;
    parent_stage = parent_pin > 0 ? 0 : 1;
    parent_buf[0] = 0;
    parent_note();
    keyboard_clear();
    keyboard_show();
    ui_nav_gen = ui_nav_gen + 1;
}

static void parental_close(void) {
    parent_open = 0;
    keyboard_hide();
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}

static void parent_submit(void) {
    if (keyboard_visible() == 1)
        keyboard_get_text_to(parent_buf, (int)sizeof(parent_buf));
    if (str_len(parent_buf) < 4) {
        snprintf(parent_msg, sizeof(parent_msg), "%s", tr_parent_short());
        return;
    }
    if (parent_stage == 1) {
        parent_pin = parent_hash(parent_buf);
        net_save_parent_pin((int)parent_pin);
        parent_stage = 2;
        parent_note();
        keyboard_hide();
        return;
    }
    if (parent_hash(parent_buf) != parent_pin) {
        snprintf(parent_msg, sizeof(parent_msg), "%s", tr_parent_wrong());
        keyboard_clear();
        parent_buf[0] = 0;
        return;
    }
    parent_stage = 2;
    parent_note();
    keyboard_hide();
}

static void parent_set_age(double age) {
    parent_load();
    parent_age = age;
    net_save_parent_age((int)age);
    parent_note();
    ds_log("parental control: age mode %d", (int)age);
}

/* 1 when chat is not allowed at all (1-10 years). */
static double parent_chat_off(void) {
    parent_load();
    return parent_age >= PARENT_KID ? 1 : 0;
}

/* Nicks with swearing: hidden for 1-10, masked for 11-13, untouched for 14+. */
static const char *parent_nick(const char *nick) {
    static char out[24];
    parent_load();
    if (!nick || !*nick || parent_age <= 0 || nick_blocked(nick) == 0)
        return nick;
    if (parent_age >= PARENT_KID) {
        snprintf(out, sizeof(out), "%s", "***");
        return out;
    }
    snprintf(out, sizeof(out), "%.1s***", nick);
    return out;
}

/* Chat text goes through the same filter for every age, the joke is that the
 * 11-13 mode keeps the profanity visible as asterisks. */

/* Infinity badge for the 14+ mode, drawn with renderer primitives only. */
static void draw_infinity(double cx, double cy, double r, uint32_t color, double thickness) {
    const double pi2 = 6.283185307179586;
    const int steps = 40;
    double t = 0, px = cx, py = cy;
    int i;
    for (i = 0; i <= steps; i++) {
        double s = sin(t), c = cos(t);
        double denom = 1 + s * s;
        double x = cx + r * 1.15 * c / denom;
        double y = cy + r * 1.15 * s * c / denom;
        if (i > 0)
            line(px, py, x, y, thickness > 0 ? thickness : 2.5, color);
        px = x;
        py = y;
        t = t + pi2 / steps;
    }
}

static double parent_card_w(void) { return fit_w(560); }
static double parent_card_x(void) { return center_x(parent_card_w()); }
static double parent_card_y(void) { return 24; }
static double parent_card_h(void) { return screen_h - 48; }
static double parent_field_y(void) { return parent_card_y() + 126; }
static double parent_btn_w(void) { return (parent_card_w() - 40 - 10) / 2; }
static double parent_btn_x(double col) { return parent_card_x() + 20 + col * (parent_btn_w() + 10); }
static double parent_age_y(double row) { return parent_card_y() + 150 + row * (btn_h + 14); }
static double parent_done_y(void) { return parent_card_y() + parent_card_h() - 24 - btn_h; }

static void draw_parent_field(void) {
    const char *label = parent_stage == 1 ? tr_parent_new() : tr_parent_ask();
    double x = parent_card_x() + 20, y = parent_field_y();
    char shown[24];
    int i = 0, n = 0;
    while (parent_buf[i] && n < (int)sizeof(shown) - 1) {
        if ((parent_buf[i] & 0xC0) != 0x80)
            shown[n++] = '*';
        i = i + 1;
    }
    shown[n] = 0;
    text_scaled(label, x, y - 26, text_color, 0.5);
    roundrect(x, y, parent_card_w() - 40, login_fh, field_radius, RGB(53,53,74));
    text_in_field(shown, x + field_text_pad, y, login_fh, text_color, 0.6);
}

static void draw_parent_age(double row, double age, const char *label) {
    double w = parent_card_w() - 40 - 66;
    double y = parent_age_y(row);
    if (parent_age == age) {
        roundrect(parent_card_x() + 14, y - 4, parent_card_w() - 28, btn_h + 8, 16, RGB(74,58,120));
    }
    button_scaled(label, parent_card_x() + 20, y, w, btn_h, 0.52);
    if (age < PARENT_KID)
        draw_infinity(parent_card_x() + 20 + w + 33, y + btn_h / 2, 13, parent_age == age ? text_color : RGB(0,0,0),
                      2.5);
}

/* The on-screen keyboard keeps its own buffer: pull it in every frame so the
 * password field actually shows what the parent types. */
static void parent_sync_input(void) {
    keyboard_get_text_to(parent_buf, sizeof(parent_buf));
}

static void draw_parental(void) {
    double cx = parent_card_x(), cy = parent_card_y(), cw = parent_card_w(), ch = parent_card_h();
    if (parent_stage != 2) {
        parent_sync_input();
        if (keyboard_enter_pressed() == 1)
            parent_submit();
    }
    rect(0, 0, screen_w, screen_h, 0xCC000000);
    roundrect(cx, cy, cw, ch, 18, card_bg);
    ctext_scaled(tr_parent_title(), cy + 22, text_color, 0.7);
    ctext_scaled(tr_parent_hint(), cy + 56, RGB(0,0,0), 0.4);
    if (parent_stage != 2) {
        draw_parent_field();
        button_scaled(parent_stage == 1 ? tr_parent_save() : tr_parent_open(), parent_btn_x(0), cy + 216,
                      parent_card_w() - 40, btn_h, 0.5);
        ctext_scaled(parent_msg, cy + 296, RGB(255,154,154), 0.45);
    } else {
        ctext_scaled(tr_parent_state(), cy + 96, text_color, 0.6);
        draw_parent_age(0, PARENT_KID, tr_parent_kid());
        draw_parent_age(1, PARENT_TEEN, tr_parent_teen());
        draw_parent_age(2, 0, tr_parent_adult());
        ctext_scaled(parent_age >= PARENT_KID ? tr_parent_kid_note()
                                              : (parent_age >= PARENT_TEEN ? tr_parent_teen_note()
                                                                           : tr_parent_adult_note()),
                     parent_age_y(2) + btn_h + 34, RGB(184,184,200), 0.42);
    }
    button_scaled(tr_parent_done(), parent_btn_x(0), parent_done_y(), parent_card_w() - 40, btn_h, 0.5);
}

/* Always-visible proof of the adult mode: the rendered infinity badge. */
static void draw_parent_badge(void) {
    double w = 62, h = 34, x = screen_w - w - 16, y = 16;
    if (parent_age > PARENT_ADULT || parent_pin <= 0)
        return;
    roundrect(x, y, w, h, 12, RGB(46,42,70));
    draw_infinity(x + w / 2, y + h / 2, 12, text_color, 2.6);
}

/* Settings row: current mode, plus the infinity badge in adult mode. */
static void draw_parent_settings(void) {
    const char *label = ds_concat(tr_parent(), ds_concat(": ", tr_parent_state()));
    button_scaled(label, menu_x(), settings_row_y(5), menu_btn_w(), settings_btn_h(),
                  fit_text_scale(label, menu_btn_w() - 24, 0.7));
    draw_parent_badge();
}

static void touch_parental(double x, double y, double action, double id) {
    double i;
    if (action == 3) {
        press_points_drop(id);
        return;
    }
    if (action != 0)
        return;
    if (parent_stage != 2) {
        if (hit_rect(x, y, parent_card_x() + 20, parent_field_y(), parent_card_w() - 40, login_fh) == 1) {
            keyboard_show();
            return;
        }
        if (hit_button(x, y, parent_btn_x(0), parent_card_y() + 216, parent_card_w() - 40, btn_h) == 1) {
            play_button_click();
            parent_submit();
            return;
        }
    } else {
        i = 0;
        while (i < 3) {
            double age = (double)(2 - i);
            if (hit_button(x, y, parent_card_x() + 20, parent_age_y(i), parent_card_w() - 40 - 66, btn_h) == 1) {
                play_button_click();
                parent_set_age(age);
                return;
            }
            i = i + 1;
        }
    }
    if (hit_button(x, y, parent_btn_x(0), parent_done_y(), parent_card_w() - 40, btn_h) == 1) {
        play_button_click();
        parental_close();
    }
}
