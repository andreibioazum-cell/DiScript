/* Astra grab ultimate tuning and lunge motion. */

static const double astra_ability_hp_factor = 0.95;
static const double astra_ability_punch_factor = 0.93;
static const double astra_ability_interval_factor = 0.8;
static const double astra_ability_final_delay = 0.4;
static const double astra_ability_throw_factor = 1.10;
static const double astra_ability_lunge_dist = 37.4;
static const double astra_ability_lunge_time = 0.16;
static const double astra_ability_catch_buffer = 0.06;
/* Return 1 when class uses Astra's grab kit. */
static double astra_ability_of(double cls, double skin) {
    (void)skin;
    return cls == CLASS_ASTRA;
}

static double astra_ability_of_slot(double slot) {
    double b = slot * remote_fields;
    return astra_ability_of(arr_get(remotes, b + 10), arr_get(remotes, b + 11));
}

/* Check whether signed-in account has developer access. */
static double astra_ability_is_dev(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}

/* Refresh cached developer-access flag on account or battle changes. */
static void astra_ability_refresh(void) { astra_ability_ok = astra_ability_is_dev(); }

static double astra_ability_allowed(void) { return astra_ability_ok; }

/* Gate developer-restricted seasonal content (winter theme, Santa). */
static double dev_access(void) { return astra_ability_allowed(); }

static double candies_on(void) { return candy_enabled == 1 && dev_access() == 1; }

/* Return 1 when local player is Astra. */
static double astra_ability_on(void) { return player_class == CLASS_ASTRA; }

static double astra_ability_local(void) { return astra_ability_on(); }

static double astra_ability_hit_interval_for(double kit) {
    if (kit == 1) {
        return astra_hit_interval * astra_ability_interval_factor;
    }
    return astra_hit_interval;
}

static double astra_ability_lunge_time_for(double kit) {
    if (kit == 1) {
        return astra_ability_lunge_time;
    }
    return 0;
}

/* Return catch window duration after lunge completion. */
static double astra_ability_catch_time_for(double kit) {
    if (kit == 1) {
        return astra_ability_lunge_time_for(kit) + astra_ability_catch_buffer;
    }
    return astra_catch_time;
}

static double astra_ability_beat_time_for(double i, double kit) {
    return astra_ability_catch_time_for(kit) + astra_ability_hit_interval_for(kit) * i;
}

static double astra_ability_final_delay_for(double kit) {
    if (kit == 1) {
        return astra_ability_final_delay;
    }
    return astra_final_delay;
}

static double astra_ability_final_time_for(double kit) {
    return astra_ability_catch_time_for(kit) + astra_ability_hit_interval_for(kit) * astra_hit_count + astra_ability_final_delay_for(kit);
}

static double astra_ability_grab_time_for(double kit) { return astra_ability_final_time_for(kit) + astra_throw_time; }

static double astra_ability_beats_done_for(double t, double kit) {
    double n = 0;
    double i = 1;
    while (i <= astra_hit_count) {
        if (t >= astra_ability_beat_time_for(i, kit)) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static double astra_ability_zone_live_for(double t, double caught, double kit) {
    if (t >= astra_ability_lunge_time_for(kit) && t <= astra_ability_catch_time_for(kit)) {
        return 1;
    }
    if (caught != 1) {
        return 0;
    }
    double i = 1;
    while (i <= astra_hit_count) {
        double bt = astra_ability_beat_time_for(i, kit);
        if (t >= bt && t <= bt + astra_hit_show) {
            return 1;
        }
        i = i + 1;
    }
    double ft = astra_ability_final_time_for(kit);
    if (t >= ft && t <= ft + astra_hit_show) {
        return 1;
    }
    return 0;
}

static double astra_ability_throw_dist_for(double lv, double kit) {
    double d = astra_throw_dist_for(lv);
    if (kit == 1) {
        d = d * astra_ability_throw_factor;
    }
    return d;
}

static double astra_ability_throw_stun_for(double lv, double kit) {
    (void)kit;
    return astra_throw_stun_for(lv);
}

static double astra_ability_max_hp_for(double cls, double kit) {
    double hp = player_max_hp_for(cls);
    if (kit == 1 && class_is_astra(cls)) {
        hp = hp * astra_ability_hp_factor;
    }
    return hp;
}

static double astra_ability_punch_damage_for(double cls, double lv, double kit) {
    double d = class_punch_damage_for(cls, lv);
    if (kit == 1 && class_is_astra(cls)) {
        d = d * astra_ability_punch_factor;
    }
    return d;
}

/* Compute punch damage from synced class, level, and skin snapshot. */
static double astra_ability_punch_damage_net(double cls, double lv, double skin) {
    return astra_ability_punch_damage_for(cls, lv, astra_ability_of(cls, skin));
}

static double astra_ability_self_damage(void) {
    return astra_ability_punch_damage_for(player_class, player_level, astra_ability_local());
}

/* Start Astra's forward lunge toward the grab origin. */
static void astra_ability_lunge_start(void) {
    astra_ability_lunge_t = 0;
    astra_ability_lunge_fx = player->x;
    astra_ability_lunge_fy = player->y;
    astra_ability_lunge_tx = player->x;
    astra_ability_lunge_ty = player->y;
    if (astra_ability_local() == 0) {
        return;
    }
    double dx = cos(player->angle);
    double dy = sin(player->angle);
    /* Add walking distance over lunge duration so moving casts still dash ahead. */
    double dist = astra_ability_lunge_dist;
    if (sqrt(joy.dx * joy.dx + joy.dy * joy.dy) > 0.15) {
        dist = dist + player_walk_speed() * astra_ability_lunge_time;
    }
    astra_ability_lunge_tx = clamp(player->x + dx * dist, player->size, screen_w - player->size);
    astra_ability_lunge_ty = clamp(player->y + dy * dist, player->size, screen_h - player->size);
    dust_trail(astra_ability_lunge_tx, astra_ability_lunge_ty, dx, dy);
}

static void astra_ability_lunge_tick(void) {
    if (astra_ability_lunge_fx == astra_ability_lunge_tx && astra_ability_lunge_fy == astra_ability_lunge_ty) {
        return;
    }
    astra_ability_lunge_t = astra_ability_lunge_t + dt;
    if (astra_ability_lunge_t >= astra_ability_lunge_time) {
        astra_ability_lunge_t = astra_ability_lunge_time;
        astra_ability_lunge_fx = astra_ability_lunge_tx;
        astra_ability_lunge_fy = astra_ability_lunge_ty;
        player->x = astra_ability_lunge_tx;
        player->y = astra_ability_lunge_ty;
        return;
    }
    /* Cubic ease-out interpolation to lunge target. */
    double p = 1 - astra_ability_lunge_t / astra_ability_lunge_time;
    p = 1 - p * p * p;
    player->x = astra_ability_lunge_fx + (astra_ability_lunge_tx - astra_ability_lunge_fx) * p;
    player->y = astra_ability_lunge_fy + (astra_ability_lunge_ty - astra_ability_lunge_fy) * p;
}
