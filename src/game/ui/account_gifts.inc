#include "gift_icons.inc"

/* Two-column sticker shop and owned collection UI. */

#define ACCT_GIFT_GAP 10
#define ACCT_GIFT_COL_GAP 28
#define ACCT_GIFT_PANEL_PAD 8

static double acct_gift_cols_top(void) { return acct_top() + 58; }
static double acct_gift_head_h(void) { return 30; }
static double acct_gift_view_top(void) { return acct_gift_cols_top() + acct_gift_head_h(); }
static double acct_gift_shop_rows(void) {
    /* Keep the admin-only Pumpkin Cube visible as a read-only catalogue card. */
    double n = net_gift_types();
    return n > 1 ? n : 1;
}
static double acct_gift_type_count(void) {
    double n = net_gift_types();
    return n > 1 ? n : 1;
}
/* Keep cards readable and let both columns scroll in a bounded viewport. */
static double acct_gift_card_h(void) { return clamp(screen_h * 0.085, 58, 108); }
static double acct_gift_step(void) { return acct_gift_card_h() + ACCT_GIFT_GAP; }
static double acct_gift_view_h(void) {
    double avail = screen_h - screen_margin - 250 - acct_gift_view_top();
    double four_cards = acct_gift_step() * 4 - ACCT_GIFT_GAP;
    if (avail > four_cards) avail = four_cards;
    return avail > 40 ? avail : 40;
}
static double acct_gift_view_bottom(void) { return acct_gift_view_top() + acct_gift_view_h(); }
static double acct_gift_footer_top(void) { return acct_gift_view_bottom(); }
static double acct_gift_col_w(void) { return (acct_w() - ACCT_GIFT_COL_GAP) / 2; }
/* owned == 0 is the shop column on the left, 1 the collection on the right. */
static double acct_gift_x(double owned) { return owned == 1 ? acct_x() + acct_gift_col_w() + ACCT_GIFT_COL_GAP : acct_x(); }
static double acct_gift_panel_x(double owned) { return acct_gift_x(owned) - ACCT_GIFT_PANEL_PAD; }
static double acct_gift_panel_y(void) { return acct_gift_cols_top() - 6; }
static double acct_gift_panel_w(void) { return acct_gift_col_w() + 2 * ACCT_GIFT_PANEL_PAD; }
static double acct_gift_panel_h(void) { return acct_gift_head_h() + acct_gift_view_h() + 12; }
static double acct_gift_price(double i) { return net_gift_price(i); }
static double acct_gift_refund(double i) { return net_gift_refund(i); }
static const char *acct_gift_name(double i) {
    if (i == 0) return language == 1 ? "Какашка" : "Poop";
    if (i == 1) return language == 1 ? "Торт" : "Cake";
    if (i == 2) return language == 1 ? "Мишка" : "Teddy Bear";
    if (i == 3) return language == 1 ? "Ракета" : "Rocket";
    if (i == 4) return language == 1 ? "Изумруд" : "Emerald";
    return language == 1 ? "Тыква-куб" : "Pumpkin Cube";
}
static const char *acct_gift_cups_line(double n) {
    return ds_concat(ds_concat(ds_num_to_string(n), " "), tr_cups_unit());
}

/* Owned sticker list helpers. */

static double acct_gift_own_total(void) {
    double n = 0, i = 0;
    while (i < acct_gift_type_count()) {
        n = n + net_gift_count(i);
        i = i + 1;
    }
    return n;
}
/* Which sticker sits in a row; -1 when the row is past the end. */
static double acct_gift_own_id(double row) {
    double base = 0, i = 0;
    if (row < 0) return -1;
    while (i < acct_gift_type_count()) {
        double c = net_gift_count(i);
        if (row < base + c) return i;
        base = base + c;
        i = i + 1;
    }
    return -1;
}
static double acct_gift_content_h(void) {
    double total = acct_gift_own_total();
    return total > 0 ? total * acct_gift_step() - ACCT_GIFT_GAP : 0;
}
static double acct_gift_shop_content_h(void) {
    return acct_gift_shop_rows() * acct_gift_step() - ACCT_GIFT_GAP;
}
static double acct_gift_shop_scroll_max(void) {
    double extra = acct_gift_shop_content_h() - acct_gift_view_h();
    return extra > 0 ? extra : 0;
}
static double acct_gift_shop_scroll_needed(void) {
    return acct_gift_shop_scroll_max() > 0 ? 1 : 0;
}
static double acct_gift_scroll_max(void) {
    double extra = acct_gift_content_h() - acct_gift_view_h();
    return extra > 0 ? extra : 0;
}
static double acct_gift_scroll_needed(void) {
    return acct_gift_scroll_max() > 0 ? 1 : 0;
}
static void acct_gift_scroll_reset(void) {
    acct_gift_drag = -1;
    acct_gift_drag_row = -1;
    acct_gift_vel = 0;
    acct_gift_shop_vel = 0;
    acct_gift_scroll = 0;
    acct_gift_shop_scroll = 0;
    acct_gift_drag_owned = 1;
    scroll_track_reset(&acct_gift_track);
}
static void acct_gift_shop_scroll_by(double dy) {
    if (acct_gift_shop_scroll_needed() == 0) {
        acct_gift_shop_scroll = 0;
        acct_gift_shop_vel = 0;
        return;
    }
    acct_gift_shop_scroll = clamp(acct_gift_shop_scroll + dy, 0, acct_gift_shop_scroll_max());
}
static void acct_gift_scroll_by(double dy) {
    if (acct_gift_scroll_needed() == 0) {
        acct_gift_scroll = 0;
        acct_gift_vel = 0;
        return;
    }
    acct_gift_scroll = clamp(acct_gift_scroll + dy, 0, acct_gift_scroll_max());
}
static double acct_gift_own_sel(void) {
    if (acct_gift_is_owned != 1) return -1;
    if (acct_gift_own_row < 0 || acct_gift_own_row >= acct_gift_own_total()) return -1;
    return acct_gift_own_row;
}
static double acct_gift_sel_id(void) {
    if (acct_gift_is_owned == 1) return acct_gift_own_id(acct_gift_own_sel());
    return acct_gift_choice;
}
static double acct_gift_picked(void) {
    if (acct_gift_is_owned == 1) return acct_gift_own_sel() >= 0 ? 1 : 0;
    return acct_gift_choice >= 0 && acct_gift_choice < acct_gift_shop_rows() ? 1 : 0;
}
/* Advance both gift lists with the same soft inertial scrolling. */
static void acct_gift_scroll_tick_one(double owned) {
    double *scroll = owned == 1 ? &acct_gift_scroll : &acct_gift_shop_scroll;
    double *vel = owned == 1 ? &acct_gift_vel : &acct_gift_shop_vel;
    double mx = owned == 1 ? acct_gift_scroll_max() : acct_gift_shop_scroll_max();
    if (mx <= 0) {
        *scroll = 0;
        *vel = 0;
        return;
    }
    *scroll = clamp(*scroll, 0, mx);
    if (acct_gift_drag >= 0) {
        if (acct_gift_drag_owned == owned) {
            *vel = 0;
            scroll_track_tick(&acct_gift_track);
        }
        return;
    }
    if (fabs(*vel) < 0.6) {
        *vel = 0;
        return;
    }
    *scroll = *scroll + *vel * dt;
    *vel = *vel * exp(-4.8 * dt);
    if (*scroll <= 0) {
        *scroll = 0;
        *vel = 0;
    } else if (*scroll >= mx) {
        *scroll = mx;
        *vel = 0;
    }
}
static void acct_gift_scroll_tick(void) {
    if (acct_open != ACCT_GIFTS) {
        acct_gift_scroll_reset();
        return;
    }
    acct_gift_scroll_tick_one(0);
    acct_gift_scroll_tick_one(1);
}

/* Action row layout and sticker viewer. */

static double acct_gift_hit_row(double y, double owned) {
    double scroll = owned == 1 ? acct_gift_scroll : acct_gift_shop_scroll;
    return floor((y - acct_gift_view_top() + scroll) / acct_gift_step());
}
static double hit_acct_gift(double x, double y, double i) {
    double cy;
    if (acct_open != ACCT_GIFTS || i < 0 || i >= acct_gift_shop_rows() ||
        y < acct_gift_view_top() || y > acct_gift_view_bottom()) return 0;
    cy = acct_gift_view_top() + i * acct_gift_step() - acct_gift_shop_scroll;
    return hit_rect(x, y, acct_gift_x(0), cy, acct_gift_col_w(), acct_gift_card_h()) == 1;
}
static double hit_acct_gift_any(double x, double y) {
    double i = 0;
    while (i < acct_gift_shop_rows()) {
        if (hit_acct_gift(x, y, i) == 1) return 1;
        i = i + 1;
    }
    return 0;
}
/* Return width of action-row buttons (Look, Buy/Gift, Sell). */
static double acct_gift_inspect_w(void) {
    if (acct_gift_is_owned == 1 && acct_gift_sel_id() == NET_GIFT_PUMPKIN_CUBE_ID) {
        return acct_w();
    }
    return (acct_w() - 2 * ACCT_GIFT_COL_GAP) / 3;
}
static double acct_gift_action_x(double col) {
    return acct_x() + (acct_gift_inspect_w() + ACCT_GIFT_COL_GAP) * col;
}
static double hit_acct_gift_buy(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 0 &&
           acct_gift_sel_id() != NET_GIFT_PUMPKIN_CUBE_ID &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_w(), btn_h) == 1;
}
static double hit_acct_gift_send(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 1 &&
           acct_gift_sel_id() != NET_GIFT_PUMPKIN_CUBE_ID &&
           hit_button(x, y, acct_gift_action_x(1), acct_below_fields(), acct_gift_inspect_w(), btn_h) == 1;
}
static double hit_acct_gift_sell(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 1 &&
           acct_gift_sel_id() != NET_GIFT_PUMPKIN_CUBE_ID &&
           hit_button(x, y, acct_gift_action_x(2), acct_below_fields(), acct_gift_inspect_w(), btn_h) == 1;
}
static double hit_acct_gift_inspect(double x, double y) {
    return acct_open == ACCT_GIFTS && acct_gift_picked() == 1 && acct_gift_is_owned == 1 &&
           acct_gift_busy != 1 && acct_gift_view_open == 0 &&
           hit_button(x, y, acct_x(), acct_below_fields(), acct_gift_inspect_w(), btn_h) == 1;
}

/* Draw full-resolution 192x192 sticker preview modal. */
static void draw_acct_gift_viewer(void) {
    double id = acct_gift_sel_id();
    double isz = GIFT_TEX_PX;
    double ix = (screen_w - isz) / 2;
    double iy = (screen_h - isz) / 2 - 34;
    const char *name;
    const char *hint;
    rect(0, 0, screen_w, screen_h, 0xF2121212);
    if (acct_gift_picked() == 0) {
        acct_gift_view_open = 0;
        return;
    }
    gift_icon(id, ix, iy, isz);
    name = acct_gift_name(id);
    gift_text_rarity(name, 0, iy + isz + 20, screen_w, 30, id, fit_text_scale(name, screen_w - 24, 0.64));
    hint = language == 1 ? "Полное разрешение · нажмите, чтобы закрыть" : "Full resolution · tap to close";
    ctext_scaled(hint, iy + isz + 64, text_color, fit_text_scale(hint, fit_w(screen_w), 0.5));
}

/* Handle independent shop/collection scrolls and card selection. */
static double touch_acct_gift_list(double x, double y, double action, double id) {
    double owned, px, ox, oy, ow, oh, inside_scroll, inside_cards, scroll_needed;
    if (acct_open != ACCT_GIFTS) return 0;
    if (x >= acct_gift_panel_x(0) && x <= acct_gift_panel_x(0) + acct_gift_panel_w()) {
        owned = 0;
    } else if (x >= acct_gift_panel_x(1) && x <= acct_gift_panel_x(1) + acct_gift_panel_w()) {
        owned = 1;
    } else {
        owned = -1;
    }
    if (acct_gift_drag >= 0 && action == 0 && id != acct_gift_drag) {
        press_points_drop(id);
        return 1;
    }
    if (acct_gift_drag >= 0 && (action == 2 || action == 1 || action == 3)) {
        if (id != acct_gift_drag) return 0;
        owned = acct_gift_drag_owned;
    }
    if (owned < 0) return 0;
    px = acct_gift_panel_x(owned);
    ox = acct_gift_x(owned);
    oy = acct_gift_view_top();
    ow = acct_gift_col_w();
    oh = acct_gift_view_h();
    inside_scroll = x >= px && x <= px + acct_gift_panel_w() &&
                    y >= acct_gift_panel_y() && y <= oy + oh + 12;
    inside_cards = x >= ox && x <= ox + ow && y >= oy && y <= oy + oh;
    scroll_needed = owned == 1 ? acct_gift_scroll_needed() : acct_gift_shop_scroll_needed();
    if (action == 2) {
        if (acct_gift_drag < 0 || id != acct_gift_drag) return 0;
        double d = acct_gift_drag_y - y;
        acct_gift_drag_y = y;
        if (fabs(d) < 1e-6) return 1;
        acct_gift_drag_move = acct_gift_drag_move + fabs(d);
        if (acct_gift_drag_move >= 10) press_points_drop(id);
        if (scroll_needed == 1) {
            if (owned == 1) acct_gift_scroll_by(d); else acct_gift_shop_scroll_by(d);
            scroll_track_move(&acct_gift_track, d);
        }
        return 1;
    }
    if (action == 1 || action == 3) {
        if (acct_gift_drag < 0 || id != acct_gift_drag) return 0;
        if (action == 1 && acct_gift_drag_move < 12 && acct_gift_drag_row >= 0 && acct_gift_busy == 0) {
            if (owned == 1) {
                double picked = acct_gift_own_id(acct_gift_drag_row);
                if (picked >= 0) {
                    acct_gift_choice = picked;
                    acct_gift_is_owned = 1;
                    acct_gift_own_row = acct_gift_drag_row;
                    play_button_click();
                }
            } else if (acct_gift_drag_row < acct_gift_shop_rows()) {
                acct_gift_choice = acct_gift_drag_row;
                acct_gift_is_owned = 0;
                acct_gift_own_row = -1;
                play_button_click();
            }
        }
        acct_gift_drag = -1;
        acct_gift_drag_row = -1;
        if (owned == 1) {
            acct_gift_vel = action == 1 && scroll_needed == 1 ? scroll_track_release(&acct_gift_track, 2200.0) : 0;
        } else {
            acct_gift_shop_vel = action == 1 && scroll_needed == 1 ? scroll_track_release(&acct_gift_track, 2200.0) : 0;
        }
        if (action == 3) scroll_track_reset(&acct_gift_track);
        return 1;
    }
    if (action != 0) return 0;
    if (acct_gift_busy != 0 && inside_scroll == 0) return 0;
    if (inside_scroll) {
        if (!inside_cards || acct_gift_busy != 0) press_points_drop(id);
        acct_gift_drag = id;
        acct_gift_drag_owned = owned;
        acct_gift_drag_y = y;
        acct_gift_drag_move = 0;
        acct_gift_drag_row = inside_cards && acct_gift_busy == 0 ? acct_gift_hit_row(y, owned) : -1;
        if (owned == 1) acct_gift_vel = 0; else acct_gift_shop_vel = 0;
        scroll_track_reset(&acct_gift_track);
        return 1;
    }
    return 0;
}

/* Draw unmasked recipient nickname input field. */
static void acct_gift_draw_field(void) {
    double y = acct_field_y(0);
    double caret = acct_field == 0 && keyboard_visible() == 1;
    const char *shown = caret == 1 ? ds_concat(acct_buf[0], "|") : acct_buf[0];
    text(language == 1 ? "Ник получателя" : "Recipient nickname", acct_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(acct_x(), y, acct_w(), login_fh, field_radius, card_bg);
    text_in_field(shown, acct_x() + field_text_pad, y, login_fh, text_color, 0.6);
}

/* Right-aligned text, vertically centred in a row of height h. */
static void acct_gift_text_right(const char *s, double right, double y, double h, uint32_t c, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    text_scaled(s, right - tw, y + (h - th) / 2 - tt + 1.5, c, sc);
}

/* Draw sticker card (clipped by scissor when in scrollable collection). */
static void acct_gift_card(double id, double owned, double card_y, double clip_top, double clip_bottom, double selected) {
    double full = acct_gift_card_h();
    if (card_y + full <= clip_top || card_y >= clip_bottom) return;
    double x = acct_gift_x(owned), w = acct_gift_col_w();
    double dragging = owned == 1 && acct_gift_drag >= 0 && acct_gift_drag_move >= 10 ? 1 : 0;
    double p = dragging == 0 ? button_press_k(x, card_y, w, full) : 0;
    uint32_t ink = text_color;
    uint32_t face = (uint32_t)shade(ui_purple, 1 - PRESS_SHADE * p);
    const char *name = acct_gift_name(id);
    const char *rarity = gift_rarity_name(id);
    const char *value = owned == 1 ? "" : ds_concat(ds_num_to_string(acct_gift_price(id)), language == 1 ? " куб." : " cups");
    roundrect(x, card_y, w, full, btn_radius, face);
    if (full < 96) {
        double isz = clamp(full * 0.62, 18, 44);
        double nx = x + 8 + isz + 8;
        double vw = owned == 1 ? 0 : text_ink_width(value) * 0.44;
        double iy = card_y + (full - isz) / 2;
        gift_icon(id, x + 8, iy, isz);
        gift_text_rarity_left(name, nx, card_y, full, id, fit_text_scale(name, w - (nx - x) - vw - 14, 0.58));
        if (owned == 0) acct_gift_text_right(value, x + w - 8, card_y, full, ink, 0.44);
    } else {
        double isz = clamp(full * 0.36, 20, 56);
        double iy = card_y + full * 0.10;
        double ny = iy + isz + full * 0.02;
        double ry = ny + 21;
        double py = card_y + full * 0.80;
        gift_icon(id, x + (w - isz) / 2, iy, isz);
        gift_text_rarity(name, x, ny, w, 19, id, fit_text_scale(name, w - 12, 0.6));
        gift_text_rarity(rarity, x, ry, w, 14, id, fit_text_scale(rarity, w - 16, 0.34));
        if (owned == 0) text_in_box(value, x, py, w, 18, ink, fit_text_scale(value, w - 12, 0.48));
    }
    if (selected == 1) {
        roundrect_outline(x, card_y, w, full, btn_radius, 3, 0xFFFFD34D);
    }
}

static void acct_gift_panel(double owned) {
    roundrect(acct_gift_panel_x(owned), acct_gift_panel_y(), acct_gift_panel_w(), acct_gift_panel_h(), 14, card_bg);
}

static void acct_gift_draw_scrollbar(double owned) {
    double needed = owned == 1 ? acct_gift_scroll_needed() : acct_gift_shop_scroll_needed();
    double content = owned == 1 ? acct_gift_content_h() : acct_gift_shop_content_h();
    double max_scroll = owned == 1 ? acct_gift_scroll_max() : acct_gift_shop_scroll_max();
    double scroll = owned == 1 ? acct_gift_scroll : acct_gift_shop_scroll;
    double vt = acct_gift_view_top(), vh = acct_gift_view_h();
    if (needed == 0 || max_scroll <= 0) return;
    double track_w = 5;
    double track_x = acct_gift_panel_x(owned) + acct_gift_panel_w() - 7;
    double track_col = card_bg;
    double thumb_h = fmax(28, vh * vh / content);
    if (thumb_h > vh) thumb_h = vh;
    double thumb_y = vt + (vh - thumb_h) * scroll / max_scroll;
    roundrect(track_x, vt, track_w, vh, 2, track_col);
    roundrect(track_x, thumb_y, track_w, thumb_h, 2, text_color);
}

static void acct_gift_draw_list(double owned) {
    double vt = acct_gift_view_top(), vb = acct_gift_view_bottom();
    double total = owned == 1 ? acct_gift_own_total() : acct_gift_shop_rows();
    double scroll = owned == 1 ? acct_gift_scroll : acct_gift_shop_scroll;
    double row = floor(scroll / acct_gift_step());
    double cy = vt + row * acct_gift_step() - scroll;
    if (row < 0) row = 0;
    ds_gfx_scissor(acct_gift_x(owned) - 2, vt, acct_gift_col_w() + 4, vb - vt);
    while (row < total && cy < vb) {
        double id = owned == 1 ? acct_gift_own_id(row) : row;
        double picked = owned == 1 ? (acct_gift_own_sel() == row ? 1 : 0)
                                   : (acct_gift_picked() == 1 && acct_gift_is_owned == 0 && acct_gift_choice == row);
        acct_gift_card(id, owned, cy, vt, vb, picked);
        row = row + 1;
        cy = cy + acct_gift_step();
    }
    ds_gfx_scissor(0, 0, 0, 0);
    acct_gift_draw_scrollbar(owned);
}

static void draw_acct_gifts(void) {
    double vt = acct_gift_view_top();
    acct_gift_scroll = clamp(acct_gift_scroll, 0, acct_gift_scroll_max());
    acct_gift_shop_scroll = clamp(acct_gift_shop_scroll, 0, acct_gift_shop_scroll_max());
    acct_gift_panel(0);
    acct_gift_panel(1);
    acct_gift_draw_list(0);
    if (acct_gift_own_total() < 1) {
        const char *empty = language == 1 ? "Пока нет подарков" : "No stickers yet";
        text_in_box(empty, acct_gift_x(1), vt, acct_gift_col_w(), 22, text_color,
                    fit_text_scale(empty, acct_gift_col_w() - 12, 0.5));
    } else {
        acct_gift_draw_list(1);
    }
    const char *left = language == 1 ? "МАГАЗИН" : "SHOP";
    const char *right = language == 1 ? "МОИ ПОДАРКИ" : "MY GIFTS";
    text_in_box(left, acct_gift_x(0), acct_gift_cols_top(), acct_gift_col_w(), 22, text_color,
                fit_text_scale(left, acct_gift_col_w() - 8, 0.58));
    text_in_box(right, acct_gift_x(1), acct_gift_cols_top(), acct_gift_col_w(), 22, text_color,
                fit_text_scale(right, acct_gift_col_w() - 8, 0.58));
    /* The title itself is drawn by draw_account(); only the hint is ours. */
    const char *hint_line = language == 1 ? "Слева - магазин, справа - ваши стикеры"
                                          : "Shop on the left, your stickers on the right";
    ctext_scaled(hint_line, acct_top() + 27, text_color, fit_text_scale(hint_line, fit_w(screen_w), 0.5));
    acct_gift_draw_field();
    if (acct_gift_view_open == 1) {
        /* Hide underlying action row while sticker preview modal is open. */
        return;
    }
    if (acct_gift_busy == 1) {
        const char *wait = acct_gift_busy_mode == 2 ? (language == 1 ? "Покупаем..." : "Buying...")
                           : acct_gift_busy_mode == 3 ? (language == 1 ? "Продаём..." : "Selling...")
                                                      : (language == 1 ? "Отправляем..." : "Sending...");
        button_scaled(wait, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(wait, acct_w() - 24, 0.6));
    } else if (acct_gift_picked() == 0) {
        const char *hint = language == 1 ? "Нажмите на стикер" : "Tap a sticker";
        ctext_scaled(hint, acct_below_fields() + (btn_h - 16) / 2, text_color, 0.55);
    } else if (acct_gift_is_owned == 0 && acct_gift_sel_id() == NET_GIFT_PUMPKIN_CUBE_ID) {
        const char *exclusive = language == 1 ? "Эксклюзив · выдаёт администратор · 555 кубков"
                                              : "Exclusive · granted by admin · 555 cups";
        ctext_scaled(exclusive, acct_below_fields() + (btn_h - 16) / 2, text_color,
                     fit_text_scale(exclusive, acct_w() - 12, 0.5));
    } else if (acct_gift_is_owned == 0) {
        /* The shop has only a purchase action; full-resolution inspection is
         * reserved for stickers already in My Gifts. */
        const char *buy = ds_concat(language == 1 ? "Купить · " : "Buy · ",
                                    acct_gift_cups_line(acct_gift_price(acct_gift_sel_id())));
        if (admin_playing() == 0 && cups < acct_gift_price(acct_gift_sel_id())) {
            button_disabled_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h,
                                   fit_text_scale(buy, acct_w() - 24, 0.6));
        } else {
            button_scaled(buy, acct_x(), acct_below_fields(), acct_w(), btn_h,
                          fit_text_scale(buy, acct_w() - 24, 0.6));
        }
    } else {
        double bw = acct_gift_inspect_w();
        const char *look = language == 1 ? "Осмотреть" : "Inspect";
        const char *send = language == 1 ? "Подарить" : "Send";
        const char *sell = ds_concat(language == 1 ? "Продать · +" : "Sell · +",
                                     acct_gift_cups_line(acct_gift_refund(acct_gift_sel_id())));
        button_scaled(look, acct_x(), acct_below_fields(), bw, btn_h, fit_text_scale(look, bw - 12, 0.56));
        if (acct_gift_sel_id() == NET_GIFT_PUMPKIN_CUBE_ID) {
            return;
        }
        button_scaled(send, acct_gift_action_x(1), acct_below_fields(), bw, btn_h, fit_text_scale(send, bw - 12, 0.56));
        button_scaled(sell, acct_gift_action_x(2), acct_below_fields(), bw, btn_h, fit_text_scale(sell, bw - 12, 0.52));
    }
}
