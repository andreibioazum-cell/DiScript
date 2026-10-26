/* Level prices, level shop actions and the old-rules price guards. */

/* Four levels. Levels 1-3 cost 100 cups, the fourth jackpot level costs 600.
 * The table lives in the game code itself, so a save, a cloud record or an old
 * rule set can never make level 1 cost 900 cups or the fourth one cost 1000. */
#define LEVEL_PRICE_REGULAR 100
#define LEVEL_PRICE_LAST 600
#define LEVEL_NEXT_AFTER_THREE 4

static double level_price_of(double n) {
    if (n >= LEVEL_NEXT_AFTER_THREE) {
        return LEVEL_PRICE_LAST;
    }
    return LEVEL_PRICE_REGULAR;
}

static double level_cost(double n) { return flash_sale_price(level_price_of(n)); }

/* Old rules guard: a purchase only happens when the charged price is exactly
 * the price of the current table. An old rule set that would sell level 1 for
 * 900 cups can never take a single cup. */
static double level_price_ok(double n, double cost) {
    double want = level_cost(n);
    if (cost != cost || want != want || cost <= 0 || want <= 0) {
        return 0;
    }
    return cost == want ? 1 : 0;
}

/* Prices of the characters are fixed in the code as well. An old rule set that
 * hands out a character for free (a zero or broken price) is refused. */
#define CLASS_PRICE_AZUM 85
#define CLASS_PRICE_SANTA 125
#define CLASS_PRICE_EBUC 155
#define CLASS_PRICE_ASTRA 175
#define CLASS_PRICE_CODECRACKER 550

static double class_price_of(double cls) {
    if (cls == CLASS_AZUM) {
        return CLASS_PRICE_AZUM;
    }
    if (cls == CLASS_SANTA) {
        return CLASS_PRICE_SANTA;
    }
    if (cls == CLASS_EBUC) {
        return CLASS_PRICE_EBUC;
    }
    if (cls == CLASS_ASTRA) {
        return CLASS_PRICE_ASTRA;
    }
    if (cls == CLASS_CODECRACKER) {
        return CLASS_PRICE_CODECRACKER;
    }
    return 0;
}

/* Same guard for characters: a paid class must have a real price of its own. */
static double class_price_ok(double cls) {
    double cost = class_cost_of(cls);
    double base = class_price_of(cls);
    if (cls == CLASS_ORDINARY) {
        return 1;
    }
    if (cost != cost || base <= 0 || cost <= 0) {
        return 0;
    }
    return cost == flash_sale_price(base) ? 1 : 0;
}

static void sync_selected_class(void) {
    player_level = class_level_of(player_class);
    levels_unlocked = class_unlocked_of(player_class);
}

static void select_level(double cls, double n) {
    if (class_owned_of(cls) == 0 || n > class_unlocked_of(cls)) {
        return;
    }
    set_class_progress(cls, n, class_unlocked_of(cls));
    if (cls == player_class) {
        sync_selected_class();
    }
    save_progress();
}

static double balance_can_pay(double bal, double cost) {
    if (cost != cost || cost <= 0 || bal != bal || bal < 0) {
        return 0;
    }
    return floor(bal + 1e-9) >= cost ? 1 : 0;
}

static double buy_level(double cls, double n) {
    /* You can never buy a level for a character you do not own. */
    if (class_id_valid(cls) == 0 || class_owned_of(cls) == 0) {
        level_msg_kind = 3;
        level_msg_t = msg_time;
        return 0;
    }
    /* Only the next level in order, exactly as the shop shows it. */
    if (n < 1 || n > max_level || n != class_unlocked_of(cls) + 1) {
        level_msg_kind = 1;
        level_msg_t = msg_time;
        return 0;
    }
    double cost = level_cost(n);
    /* Even if an old rule set brings its own price, it is ignored. */
    if (level_price_ok(n, cost) == 0) {
        level_msg_kind = 2;
        level_msg_t = msg_time;
        return 0;
    }
    if (cups != cups || cups < 0) {
        cups = 0;
    }
        /* Admin cups are never taken: the level opens for free. */
    double free_buy = admin_playing() == 1;
    if (free_buy == 0 && balance_can_pay(cups, cost) == 0) {
        level_msg_kind = 0;
        level_msg_t = msg_time;
        return 0;
    }
    if (free_buy == 0) {
        cups = cups - cost;
        if (cups < 0) cups = 0;
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
        level_msg_kind = 3;
        level_msg_t = msg_time;
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
    if (level_msg_kind == 2) {
        return tr_level_price_guard();
    }
    if (level_msg_kind == 3) {
        return tr_level_need_class();
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
    /* Old rules never hand out a character: the price must be the real one. */
    if (class_price_ok(cls) == 0) {
        class_msg_kind = 2;
        class_msg_t = msg_time;
        return;
    }
    double cost = class_cost_of(cls);
    if (cups != cups || cups < 0) cups = 0;
    if (candies != candies || candies < 0) candies = 0;
    class_msg_kind = class_pays_candies(cls);
    if (class_pays_candies(cls) == 1) {
        if (balance_can_pay(candies, cost) == 0) {
            class_msg_t = msg_time;
            return;
        }
        candies = candies - cost;
        if (candies < 0) candies = 0;
    } else if (admin_playing() == 1) {
                /* Admin cups are never taken: the class is free. */
    } else {
        if (balance_can_pay(cups, cost) == 0) {
            class_msg_t = msg_time;
            return;
        }
        cups = cups - cost;
        if (cups < 0) cups = 0;
    }
    set_class_owned(cls, 1);
    select_class(cls);
    ds_log("bought class %g", cls);
}
