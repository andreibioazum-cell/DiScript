/* Astra counter-strike tuning, developer access gate, and lunge motion. */

static const double astra_ability_hp_factor = 0.95;
static const double astra_ability_punch_factor = 0.93;
static const double astra_ability_lunge_dist = 48.0;
static const double astra_ability_lunge_time = 0.16;

#define ASTRA_STRIKE_TIME 0.2
#define ASTRA_STRIKE_STUN 0.4
#define ASTRA_STRIKE_DAMAGE 2.5

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

/* Gate developer-restricted seasonal content. */
static double dev_access(void) { return astra_ability_allowed(); }

static double candies_on(void) { return candy_enabled == 1 && dev_access() == 1; }

static double astra_ability_on(void) { return player_class == CLASS_ASTRA; }

static double astra_ability_local(void) { return astra_ability_on(); }

static double astra_strike_damage_for(double lv) {
    double d = ASTRA_STRIKE_DAMAGE;
    if (lv >= 3) {
        d = d * (1 + astra_level_throw_bonus);
    }
    return d;
}

static double astra_strike_stun_for(double lv) {
    double s = ASTRA_STRIKE_STUN;
    if (lv >= 3) {
        s = s + astra_level_stun_bonus;
    }
    return s;
}

static double astra_ability_lunge_active(void) {
    if (astra_ability_lunge_fx != astra_ability_lunge_tx || astra_ability_lunge_fy != astra_ability_lunge_ty) {
        return 1;
    }
    return 0;
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

/* Start Astra's short forward lunge for the counter-strike. */
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
    double dist = astra_ability_lunge_dist;
    if (sqrt(joy.dx * joy.dx + joy.dy * joy.dy) > 0.15) {
        dist = dist + player_walk_speed() * astra_ability_lunge_time;
    }
    astra_ability_lunge_tx = clamp(player->x + dx * dist, player->size, screen_w - player->size);
    astra_ability_lunge_ty = clamp(player->y + dy * dist, player->size, screen_h - player->size);
    dust_trail(astra_ability_lunge_tx, astra_ability_lunge_ty, dx, dy);
}

static void astra_ability_lunge_tick(void) {
    if (astra_ability_lunge_active() == 0) {
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
