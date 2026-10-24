/* Every class has its own level branch, CodeCracker too: it is a separate class,
 * not the ordinary cube under another name. */
static double class_level_of(double cls) { return arr_get(class_level_tbl, cls); }

static double class_unlocked_of(double cls) { return arr_get(class_unlocked_tbl, cls); }

static double class_id_valid(double cls) {
    return cls == CLASS_ORDINARY || cls == CLASS_AZUM || cls == CLASS_SANTA || cls == CLASS_EBUC ||
           cls == CLASS_ASTRA || cls == CLASS_CODECRACKER;
}

static double class_owned_of(double cls) {
    if (cls == CLASS_ORDINARY) {
        return 1;
    }
    return arr_get(class_owned_tbl, cls);
}

static void set_class_owned(double cls, double owned) {
    arr_set(class_owned_tbl, cls, owned);
}

static void set_class_progress(double cls, double lv, double un) {
    arr_set(class_level_tbl, cls, lv);
    arr_set(class_unlocked_tbl, cls, un);
}

/* Flash sale: while the live Firebase FlashSale flag is 1, EVERY price
 * (classes and level unlocks) drops by 10%. The discounted price is
 * rounded to the nearest whole unit: 65 -> 59, 100 -> 90, 30 -> 27.
 * The flag is only ever read (GET) by the client: it is never created in
 * Firebase by the game, and without a fresh successful read (offline,
 * node missing, value not 1) prices stay at their normal values. */
#define FLASH_SALE_FRACTION 0.10

static void update_flash_sale(void) {
    double active = net_flash_sale();
    if (active != flash_sale_active) {
        ds_log("flash sale %s (Firebase FlashSale read)", active == 1 ? "ON: all prices -10%" : "off");
    }
    flash_sale_active = active;
}

static double flash_sale_price(double cost) {
    if (flash_sale_active == 1 && cost > 0) {
        return floor(cost * (1 - FLASH_SALE_FRACTION) + 0.5);
    }
    return cost;
}

static double class_base_cost_of(double cls) {
    if (cls == CLASS_AZUM) {
        return azum_cost;
    }
    if (cls == CLASS_ASTRA) {
        return astra_cost;
    }
    if (cls == CLASS_SANTA) {
        return santa_candy_cost;
    }
    if (cls == CLASS_EBUC) {
        return ebuc_cost;
    }
    if (cls == CLASS_CODECRACKER) {
        return codecracker_cost;
    }
    return 0;
}

static double class_cost_of(double cls) { return flash_sale_price(class_base_cost_of(cls)); }

static double class_pays_candies(double cls) {
    if (cls == CLASS_SANTA) {
        return 1;
    }
    return 0;
}

static const char *class_currency_of(double cls) {
    if (class_pays_candies(cls) == 1) {
        return tr_candies_unit();
    }
    return tr_cups_unit();
}

static const char *class_name_of(double cls) {
    if (cls == CLASS_AZUM) {
        return tr_class_azum();
    }
    if (cls == CLASS_ASTRA) {
        return tr_class_astra();
    }
    if (cls == CLASS_SANTA) {
        return tr_class_santa();
    }
    if (cls == CLASS_EBUC) {
        return tr_class_ebuc();
    }
    if (cls == CLASS_CODECRACKER) {
        return tr_class_codecracker();
    }
    return tr_class_ordinary();
}

static const char *class_desc_of(double cls) {
    if (cls == CLASS_AZUM) {
        return tr_class_azum_desc();
    }
    if (cls == CLASS_ASTRA) {
        return tr_class_astra_desc();
    }
    if (cls == CLASS_SANTA) {
        return tr_class_santa_desc();
    }
    if (cls == CLASS_EBUC) {
        return tr_class_ebuc_desc();
    }
    if (cls == CLASS_CODECRACKER) {
        return tr_class_codecracker_desc();
    }
    return tr_class_ordinary_desc();
}

static const char *class_pick_label(double cls) {
    if (player_class == cls) {
        return tr_class_selected();
    }
    if (class_owned_of(cls) == 0) {
        return tr_buy_for(class_cost_of(cls), class_currency_of(cls));
    }
    return tr_class_pick();
}

static const char *class_msg_text(void) {
    if (class_msg_kind == 1) {
        return tr_class_need_candies();
    }
    return tr_class_need_cups();
}

static double skin_is_active(double cls, double skin) {
    if (skin != SKIN_SPECIAL) {
        return 0;
    }
    if (cls == CLASS_AZUM || cls == CLASS_EBUC) {
        return 1;
    }
    return 0;
}

static double class_skin_of(double cls) {
    if (cls == CLASS_EBUC) {
        return ebuc_skin;
    }
    if (cls == CLASS_AZUM) {
        return azum_skin;
    }
    return SKIN_NORMAL;
}

static double current_skin(void) { return class_skin_of(player_class); }

static double skin_unlocked_for(double cls, double skin) {
    if (skin == SKIN_NORMAL) {
        return 1;
    }
    if (cls == CLASS_EBUC) {
        /* Elf ebuc (буК, genitive буКа): achievement "Подарок буКа" – be at the Ded Moroz event online as Santa, or admin battle-pass tier 4. */
        if ((is_admin() == 1 && bp_claimed >= 4) || has_achievement(ACH_BUK_GIFT) == 1) {
            return 1;
        }
        return 0;
    }
    if (cls == CLASS_AZUM && has_achievement(ACH_LEGENDS) == 1) {
        return 1;
    }
    return 0;
}

static double skin_unlocked(double skin) { return skin_unlocked_for(player_class, skin); }

static const char *skin_name_for(double cls, double skin) {
    if (skin != SKIN_SPECIAL || (cls != CLASS_EBUC && cls != CLASS_AZUM)) {
        return tr_skin_normal();
    }
    if (cls == CLASS_EBUC) {
        return tr_skin_elf();
    }
    return tr_skin_zombie();
}

static const char *skin_name_of(double skin) { return skin_name_for(player_class, skin); }

static const char *skin_hint_for(double cls) {
    if (cls == CLASS_EBUC) {
        return tr_skin_elf_hint();
    }
    return tr_skin_legends_hint();
}

static void pick_skin(double skin) {
    if (skin < 0 || skin >= SKIN_COUNT) {
        return;
    }
    if (class_has_skins(player_class) == 0) {
        return;
    }
    if (skin_unlocked(skin) == 0) {
        skin_msg_t = msg_time;
        return;
    }
    if (player_class == CLASS_EBUC) {
        ebuc_skin = skin;
    } else {
        azum_skin = skin;
    }
    save_progress();
}

static double class_visible(double cls) {
    if (cls == CLASS_SANTA && dev_access() == 0 && (is_admin() == 0 || bp_claimed < 7)) {
        return 0;
    }
    return 1;
}

static double class_at_order(double i) {
    if (i == 2) {
        return CLASS_EBUC;
    }
    if (i == 3) {
        return CLASS_SANTA;
    }
    if (i == 4) {
        return CLASS_ASTRA;
    }
    if (i == 5) {
        return CLASS_CODECRACKER;
    }
    return i;
}

static double classes_visible_count(void) {
    double n = 0;
    double i = 0;
    while (i < class_count) {
        if (class_visible(class_at_order(i)) == 1) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static double classes_page_size(void) {
    double available = screen_w - 2 * screen_margin;
    return clamp(floor((available + class_gap) / (class_card_min_w + class_gap)), 1, class_count);
}

/* The shop is one horizontal strip, not pages. It scrolls only when the
 * cards genuinely do not fit the viewport: classes_scroll_max() is 0 then,
 * the strip stays centred and no drag or scrollbar can move it. */
static double classes_viewport_w(void) { return screen_w - 2 * screen_margin; }

static double classes_strip_w(void) {
    double n = classes_visible_count();
    if (n <= 0) {
        return 0;
    }
    return classes_card_w() * n + class_gap * (n - 1);
}

static double classes_scroll_max(void) {
    double extra = classes_strip_w() - classes_viewport_w();
    return extra > 0 ? extra : 0;
}

static double classes_scroll_needed(void) { return classes_scroll_max() > 0 ? 1 : 0; }

static void classes_scroll_clamp(void) {
    if (classes_scroll < 0) {
        classes_scroll = 0;
    } else if (classes_scroll > classes_scroll_max()) {
        classes_scroll = classes_scroll_max();
    }
}

static double classes_row_x(void) {
    classes_scroll_clamp();
    if (classes_scroll_needed() == 0) {
        return center_x(classes_strip_w());
    }
    return screen_margin - classes_scroll;
}

static void classes_scroll_by(double dx) {
    classes_scroll = classes_scroll + dx;
    classes_scroll_clamp();
}

static double visible_class_at(double vi) {
    double n = 0;
    double i = 0;
    while (i < class_count) {
        double cls = class_at_order(i);
        if (class_visible(cls) == 1) {
            if (n == vi) {
                return cls;
            }
            n = n + 1;
        }
        i = i + 1;
    }
    return CLASS_ORDINARY;
}

static double level_cost(double n) { return flash_sale_price(level_base_cost + (n - 1) * level_cost_step); }

static void sync_selected_class(void) {
    player_level = class_level_of(player_class);
    levels_unlocked = class_unlocked_of(player_class);
}

static void select_level(double cls, double n) {
    set_class_progress(cls, n, class_unlocked_of(cls));
    if (cls == player_class) {
        sync_selected_class();
    }
    save_progress();
}

static double buy_level(double cls, double n) {
    double cost = level_cost(n);
        /* Admin cups are never taken: the level opens for free. */
    double free_buy = admin_playing() == 1;
    if (free_buy == 0 && cups < cost) {
        level_msg_kind = 0;
        level_msg_t = msg_time;
        return 0;
    }
    if (free_buy == 0) {
        cups = cups - cost;
    }
    set_class_progress(cls, n, n);
    if (cls == player_class) {
        sync_selected_class();
    }
    level_msg_t = 0;
    save_progress();
    return 1;
}

static void level_action(double cls, double n) {
    if (n < 1 || n > max_level) {
        return;
    }
    if (class_owned_of(cls) == 0) {
        class_msg_kind = 0;
        class_msg_t = msg_time;
        return;
    }
    double un = class_unlocked_of(cls);
    if (n <= un) {
        select_level(cls, n);
        return;
    }
    if (n == un + 1) {
        buy_level(cls, n);
        return;
    }
    level_msg_kind = 1;
    level_msg_t = msg_time;
}

static const char *level_status_of(double cls, double n) {
    if (n <= class_unlocked_of(cls)) {
        return tr_level_status_open();
    }
    if (n == class_unlocked_of(cls) + 1) {
        return tr_level_unlock_cost(level_cost(n));
    }
    return tr_level_status_locked();
}

static const char *level_msg_text(void) {
    if (level_msg_kind == 1) {
        return tr_level_locked();
    }
    return tr_level_need_cups();
}

static void select_class(double cls) {
    if (class_id_valid(cls) == 0 || class_visible(cls) == 0 || class_owned_of(cls) == 0) {
        return;
    }
    player_class = cls;
    sync_selected_class();
    save_progress();
}

static void pick_class(double cls) {
    if (class_id_valid(cls) == 0 || class_visible(cls) == 0) {
        return;
    }
    if (class_owned_of(cls) == 1) {
        select_class(cls);
        return;
    }
    double cost = class_cost_of(cls);
    class_msg_kind = class_pays_candies(cls);
    if (class_pays_candies(cls) == 1) {
        if (candies < cost) {
            class_msg_t = msg_time;
            return;
        }
        candies = candies - cost;
    } else if (admin_playing() == 1) {
                /* Admin cups are never taken: the class is free. */
    } else {
        if (cups < cost) {
            class_msg_t = msg_time;
            return;
        }
        cups = cups - cost;
    }
    set_class_owned(cls, 1);
    select_class(cls);
    ds_log("bought class %g", cls);
}
