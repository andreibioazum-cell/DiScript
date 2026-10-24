/* Astra's current ability tuning. These values describe its grab ultimate
 * in solo and online battles. */

static const double astra_ability_hp_factor = 0.95;
static const double astra_ability_punch_factor = 0.93;
static const double astra_ability_interval_factor = 0.8;
static const double astra_ability_final_delay = 0.4;
static const double astra_ability_throw_factor = 1.10;
static const double astra_ability_lunge_dist = 37.4;
static const double astra_ability_lunge_time = 0.16;
static const double astra_ability_catch_buffer = 0.06;
/* The normal Astra class always uses this kit, including remote players. */
static double astra_ability_of(double cls, double skin) {
    (void)skin;
    return cls == CLASS_ASTRA;
}

static double astra_ability_of_slot(double slot) {
    double b = slot * remote_fields;
    return astra_ability_of(arr_get(remotes, b + 10), arr_get(remotes, b + 11));
}

/* Dev rights belong to the profile, not the password (see is_admin in
 * ui/chat.inc): the nick of the signed-in account decides, a password change
 * changes nothing. */
static double astra_ability_is_dev(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}

/* Cached permission bit. Refreshed when the account changes (login, profile
 * load, battle start), never while drawing or ticking, so the dev check does
 * not run a single time per frame. */
static void astra_ability_refresh(void) { astra_ability_ok = astra_ability_is_dev(); }

static double astra_ability_allowed(void) { return astra_ability_ok; }

/* Developer-only features: for now the winter theme, Santa and candies are
 * available only to the administrator account (Dimasi4ek229).
 * Everyone else sees the winter button greyed out, no Santa in the shop and
 * no candies anywhere; when the features open for all, this one function is
 * the only switch to flip. */
static double dev_access(void) { return astra_ability_allowed(); }

static double candies_on(void) { return candy_enabled == 1 && dev_access() == 1; }

/* Astra is an ordinary purchasable class now, so its kit is not developer-gated. */
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

/* Keep a short catch window after the body completes its lunge. This stops the
 * initial zone from grabbing someone during the visual dash while still giving
 * online clients 0.10 s to register the catch. */
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

/* Punch damage of a fighter as the receiving client must apply it: cls + skin
 * come from the same synced snapshot the rest of the fight is built on. */
static double astra_ability_punch_damage_net(double cls, double lv, double skin) {
    return astra_ability_punch_damage_for(cls, lv, astra_ability_of(cls, skin));
}

static double astra_ability_self_damage(void) {
    return astra_ability_punch_damage_for(player_class, player_level, astra_ability_local());
}

/* The forward step Astra takes when the ultimate is pressed.
 *
 * It is short and smooth: the body bursts off and glides to the target over
 * astra_ability_lunge_time with an ease-out instead of jumping there in one
 * frame. On the move the step is lengthened by the ground the walk would
 * have covered, so it always reads as a dash. The grab origin - the hitbox
 * and the zone published to the other clients - is the end of the step,
 * captured at cast time, so the grab stays identical for everyone and no
 * extra network write is needed. The glide itself is ordinary local movement
 * (a few pixels per frame, movement is locked during the grab anyway). */
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
    /* On the move the step has to beat the walk, or it is no dash at all:
     * 37 px in 0.16 s is just about walking pace, so a walking Astra used to
     * simply stop and ease forward. The ground the walk would have covered
     * in that time is added on top, so the lunge always carries her
     * astra_ability_lunge_dist further than walking would. */
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
    /* Ease-out: the lunge bursts off at three times its average speed and
     * settles onto the grab origin. The old smoothstep started from a dead
     * stop, which on the move read as braking rather than a dash. */
    double p = 1 - astra_ability_lunge_t / astra_ability_lunge_time;
    p = 1 - p * p * p;
    player->x = astra_ability_lunge_fx + (astra_ability_lunge_tx - astra_ability_lunge_fx) * p;
    player->y = astra_ability_lunge_fy + (astra_ability_lunge_ty - astra_ability_lunge_fy) * p;
}
