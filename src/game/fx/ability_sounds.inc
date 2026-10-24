/* Local player ability sound effects. */

/* Play preloaded ability sound if available. */
static void ability_snd(const char *name) {
    if (snd_load(name) == 1) {
        snd_play(name);
    }
}

/* Preload ability sound samples at startup. */
static void ability_sounds_init(void) {
    const char *names[] = {PUNCH_SOUND, DASH_SOUND, GRAB_SOUND, GIFT_SOUND,
                           STATION_SOUND, OVERCLOCK_SOUND, ZOMBIE_PUNCH_SOUND, ZOMBIE_DASH_SOUND};
    int i = 0;
    while (i < 8) {
        if (snd_load(names[i]) == 1) {
            snd_volume(names[i], 0.8);
        }
        i = i + 1;
    }
}

static double player_is_zombie_azum(void) {
    return player_class == CLASS_AZUM && current_skin() == SKIN_ZOMBIE ? 1 : 0;
}

static void punch_snd(void) {
    ability_snd(player_is_zombie_azum() == 1 ? ZOMBIE_PUNCH_SOUND : PUNCH_SOUND);
}

static void dash_snd(void) {
    ability_snd(player_is_zombie_azum() == 1 ? ZOMBIE_DASH_SOUND : DASH_SOUND);
}
