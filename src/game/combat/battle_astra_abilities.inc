/* Astra counter-strike tuning, developer access gate, and lunge motion. */

static const double astra_ability_hp_factor = 0.95;
static const double astra_ability_punch_factor = 0.93;
#define ASTRA_PULLBACK_DIST 14.0
#define ASTRA_PULLBACK_TIME 0.09
#define ASTRA_DASH_TIME 0.44
static const double astra_ability_lunge_dist = 245.0;
static const double astra_ability_lunge_time = ASTRA_PULLBACK_TIME + ASTRA_DASH_TIME;

#define ASTRA_STRIKE_DELAY ASTRA_PULLBACK_TIME
#define ASTRA_STRIKE_ACTIVE (ASTRA_DASH_TIME + 0.06)
#define ASTRA_STRIKE_TIME (ASTRA_STRIKE_DELAY + ASTRA_STRIKE_ACTIVE)
#define ASTRA_STRIKE_STUN 1.0
#define ASTRA_STRIKE_DAMAGE 4.5

static double astra_dash_progress(double u) {
    double q = 1 - u;
    return 0.35 * u + 0.65 * (1 - q * q);
}

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

/* Physical pullback and forward dash handle sprite motion directly. */
static double astra_strike_shift(double active, double t) {
    (void)active;
    (void)t;
    return 0;
}

static double astra_ability_lunge_active(void) {
    if (astra_ability_lunge_fx != astra_ability_lunge_tx || astra_ability_lunge_fy != astra_ability_lunge_ty) {
        return 1;
    }
    if (astra_state == 1 && astra_ability_local() == 1 && astra_ability_lunge_t < astra_ability_lunge_time) {
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

/* Start Astra's 0.09s backward windup followed by fixed-length forward dash. */
static void astra_ability_lunge_start(void) {
    astra_ability_lunge_t = 0;
    astra_ability_lunge_fx = player->x;
    astra_ability_lunge_fy = player->y;
    astra_ability_lunge_tx = player->x;
    astra_ability_lunge_ty = player->y;
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    if (astra_ability_local() == 0) {
        return;
    }
    double jm = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
    if (jm > 0.15) {
        player_dir_x = joy.dx / jm;
        player_dir_y = joy.dy / jm;
        player->angle = atan2(joy.dy, joy.dx);
    }
    double dx = cos(player->angle);
    double dy = sin(player->angle);
    astra_dx = dx;
    astra_dy = dy;
    astra_ability_lunge_dx = dx;
    astra_ability_lunge_dy = dy;
    astra_ability_lunge_tx = clamp(player->x + dx * astra_ability_lunge_dist, player->size, screen_w - player->size);
    astra_ability_lunge_ty = clamp(player->y + dy * astra_ability_lunge_dist, player->size, screen_h - player->size);
}

static void astra_ability_lunge_tick(void) {
    if (astra_ability_lunge_active() == 0) {
        return;
    }
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    double jm = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
    if (jm > 0.15) {
        player_dir_x = joy.dx / jm;
        player_dir_y = joy.dy / jm;
        player->angle = atan2(joy.dy, joy.dx);
    }
    double prev_t = astra_ability_lunge_t;
    astra_ability_lunge_t = astra_ability_lunge_t + dt;
    /* Allow aiming direction to follow rotation during the 0.09s windup. */
    if (prev_t < ASTRA_PULLBACK_TIME) {
        double dx = cos(player->angle);
        double dy = sin(player->angle);
        astra_dx = dx;
        astra_dy = dy;
        astra_ability_lunge_dx = dx;
        astra_ability_lunge_dy = dy;
        astra_ability_lunge_tx =
            clamp(astra_ability_lunge_fx + dx * astra_ability_lunge_dist, player->size, screen_w - player->size);
        astra_ability_lunge_ty =
            clamp(astra_ability_lunge_fy + dy * astra_ability_lunge_dist, player->size, screen_h - player->size);
        if (astra_ability_lunge_t >= ASTRA_PULLBACK_TIME) {
            dust_trail(astra_ability_lunge_tx, astra_ability_lunge_ty, dx, dy);
        }
    }
    double bx = clamp(astra_ability_lunge_fx - astra_ability_lunge_dx * ASTRA_PULLBACK_DIST, player->size,
                      screen_w - player->size);
    double by = clamp(astra_ability_lunge_fy - astra_ability_lunge_dy * ASTRA_PULLBACK_DIST, player->size,
                      screen_h - player->size);
    if (astra_ability_lunge_t < ASTRA_PULLBACK_TIME) {
        double u = clamp(astra_ability_lunge_t / ASTRA_PULLBACK_TIME, 0, 1);
        double p = sin(pi * 0.5 * u);
        player->x = astra_ability_lunge_fx + (bx - astra_ability_lunge_fx) * p;
        player->y = astra_ability_lunge_fy + (by - astra_ability_lunge_fy) * p;
        return;
    }
    if (astra_ability_lunge_t >= astra_ability_lunge_time) {
        astra_ability_lunge_t = astra_ability_lunge_time;
        astra_ability_lunge_fx = astra_ability_lunge_tx;
        astra_ability_lunge_fy = astra_ability_lunge_ty;
        player->x = astra_ability_lunge_tx;
        player->y = astra_ability_lunge_ty;
        return;
    }
    /* Smooth forward dash from pullback point to aimed target. */
    double u = clamp((astra_ability_lunge_t - ASTRA_PULLBACK_TIME) / ASTRA_DASH_TIME, 0, 1);
    double p = astra_dash_progress(u);
    player->x = bx + (astra_ability_lunge_tx - bx) * p;
    player->y = by + (astra_ability_lunge_ty - by) * p;
    tick_dust(astra_ability_lunge_dx, astra_ability_lunge_dy, 1);
}
