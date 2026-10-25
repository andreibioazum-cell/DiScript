static FighterSpritePair look_sprite_pair(double look) {
    FighterSpritePair pair = {"", ""};
    if (look == LOOK_AZUM) {
        pair.idle = AZUM_TEX;
        pair.punch = AZUM_PUNCH_TEX;
    } else if (look == LOOK_ZOMBIE) {
        pair.idle = AZUM_ZOMBIE_TEX;
        pair.punch = AZUM_ZOMBIE_PUNCH_TEX;
    } else if (look == LOOK_SANTA) {
        pair.idle = SANTA_TEX;
        pair.punch = SANTA_PUNCH_TEX;
    } else if (look == LOOK_EBUC) {
        pair.idle = EBUC_TEX;
        pair.punch = EBUC_PUNCH_TEX;
    } else if (look == LOOK_ELF) {
        pair.idle = EBUC_ELF_TEX;
        pair.punch = EBUC_ELF_PUNCH_TEX;
    } else if (look == LOOK_ORDINARY) {
        pair.idle = ORDINARY_TEX;
        pair.punch = PUNCH_TEX;
    }
    return pair;
}

static const char *look_idle_tex(double look) { return look_sprite_pair(look).idle; }

static const char *look_punch_tex(double look) { return look_sprite_pair(look).punch; }

static const char *look_tex(double look, double punching) {
    if (punching == 1) {
        return look_punch_tex(look);
    }
    return look_idle_tex(look);
}

static double look_ready(double look) {
    if (look == LOOK_NONE) {
        return 0;
    }
    FighterSpritePair pair = look_sprite_pair(look);
    /* Require both idle and punch textures before switching fighter appearance. */
    return tex_ready(pair.idle) == 1 && tex_ready(pair.punch) == 1;
}

static double class_is_astra(double cls) { return cls == CLASS_ASTRA; }

static double class_look(double cls, double skin) {
    if (skin_is_active(cls, skin) == 1) {
        if (cls == CLASS_EBUC) {
            return LOOK_ELF;
        }
        return LOOK_ZOMBIE;
    }
    if (cls == CLASS_AZUM) {
        return LOOK_AZUM;
    }
    if (cls == CLASS_SANTA) {
        return LOOK_SANTA;
    }
    if (cls == CLASS_EBUC) {
        return LOOK_EBUC;
    }
    return LOOK_ORDINARY;
}

static double card_look(double cls, double skin) {
    double look = class_look(cls, skin);
    if (look_ready(look) == 1) {
        return look;
    }
    return LOOK_NONE;
}

/* High-resolution shop/lobby render. Astra uses its ordinary Astra card art. */
static const char *class_card_tex(double cls, double skin) {
    if (cls == CLASS_AZUM) {
        if (skin_is_active(cls, skin) == 1) {
            return AZUM_ZOMBIE_CARD_TEX;
        }
        return AZUM_CARD_TEX;
    }
    if (cls == CLASS_SANTA) {
        return SANTA_CARD_TEX;
    }
    if (cls == CLASS_EBUC) {
        if (skin_is_active(cls, skin) == 1) {
            return EBUC_ELF_CARD_TEX;
        }
        return EBUC_CARD_TEX;
    }
    if (cls == CLASS_ASTRA) {
        return ASTRA_CARD_TEX;
    }
    return ORDINARY_CARD_TEX;
}

static double class_card_render_ready(double cls, double skin) { return tex_ready(class_card_tex(cls, skin)); }

static double class_has_super(double cls) {
    if (cls == CLASS_AZUM || cls == CLASS_SANTA || cls == CLASS_EBUC || cls == CLASS_CODECRACKER ||
        class_is_astra(cls)) {
        return 1;
    }
    return 0;
}

static double class_punch_damage_for(double cls, double lv) {
    if (cls == CLASS_AZUM && azum_buff_active == 1) {
        return punch_damage * 1.10;
    }
    if (cls == CLASS_SANTA) {
        return santa_damage;
    }
    if (cls == CLASS_EBUC) {
        return ebuc_damage;
    }
    if (cls == CLASS_ORDINARY && lv >= 1) {
        return punch_damage * (1 + ordinary_damage_bonus);
    }
    return punch_damage;
}

/* Global health pool multiplier across all fighters. */
static double round_hp_mult(void) { return 1.0; }

static double class_base_hp_for(double cls) {
    if (cls == CLASS_CODECRACKER) {
        return codecracker_hp;
    }
    if (cls == CLASS_EBUC) {
        return ebuc_hp;
    }
    if (cls == CLASS_AZUM) {
        return azum_hp * (azum_buff_active == 1 ? 1.10 : 1);
    }
    if (class_is_astra(cls)) {
        return astra_hp;
    }
    /* Ordinary and Santa share the common pool. */
    return 20;
}

static double player_max_hp_for(double cls) { return class_base_hp_for(cls) * round_hp_mult(); }

/* CodeCracker per-level bonuses: +dmg (lv1), shorter cd (lv2), higher absorb (lv3). */
static const double codecracker_lv1_damage_bonus = 0.10;
static const double codecracker_lv2_cd_bonus = 0.25;
static const double codecracker_lv3_absorb_bonus = 0.10;

/* Damage of one bite at the given CodeCracker level. */
static double codecracker_bite_damage(double lv) {
    double d = codecracker_touch_damage;
    if (lv >= 1) {
        d = d * (1 + codecracker_lv1_damage_bonus);
    }
    return d;
}

/* Overclock cooldown: 4 seconds, 3 seconds from the second level. */
static double codecracker_super_cd_for(double lv) {
    double cd = codecracker_super_cd;
    if (lv >= 2) {
        cd = cd * (1 - codecracker_lv2_cd_bonus);
    }
    return cd;
}

/* Damage absorbed while the overclock runs: half, 60% from the third level. */
static double codecracker_absorb_for(double lv) {
    double a = codecracker_super_absorb;
    if (lv >= 3) {
        a = a + codecracker_lv3_absorb_bonus;
    }
    return a;
}

static double ebuc_station_hp_for(double lv) {
    double h = ebuc_station_hp;
    if (lv >= 1) {
        h = h + ebuc_station_hp_bonus * 2;
    }
    return h;
}

static double ebuc_universe_damage_for(double lv) {
    double d = ebuc_universe_damage;
    if (lv >= 3) {
        d = d + ebuc_universe_damage_bonus;
    }
    return d;
}

static double santa_poison_time_for(double lv) {
    (void)lv;
    return santa_poison_time;
}

static double santa_poison_damage_for(double lv) {
    double d = santa_poison_dps;
    if (lv >= 1) {
        d = d + santa_poison_damage_bonus;
    }
    if (lv >= 2) {
        d = d + santa_poison_damage_bonus;
    }
    return d;
}

static double santa_freeze_time_for(double lv) {
    double t = super_freeze;
    if (lv >= 3) {
        t = t + santa_freeze_time_bonus;
    }
    return t * santa_stun_scale;
}

static double azum_dash_damage_for(void) {
    return azum_dash_damage * (azum_buff_active == 1 ? 1.10 : 1);
}

#define ENEMY_OTHER_DAMAGE_MULT 0.80

static double enemy_damage_mult_for(double cls) {
    (void)cls;
    return ENEMY_OTHER_DAMAGE_MULT;
}

static double enemy_azum_dash_damage_for(void) {
    return enemy_dash_damage * (azum_buff_active == 1 ? 1.10 : 1);
}

static void update_azum_buff(void) {
    double active = net_azum_buff();
    if (active == azum_buff_active) {
        return;
    }
    double factor = active == 1 ? 1.10 : 1 / 1.10;
    azum_buff_active = active;
    if (player_class == CLASS_AZUM) {
        player->max_hp = player->max_hp * factor;
        player->hp = player->hp * factor;
    }
    if (enemy_class == CLASS_AZUM) {
        enemy->max_hp = enemy->max_hp * factor;
        enemy->hp = enemy->hp * factor;
    }
}
