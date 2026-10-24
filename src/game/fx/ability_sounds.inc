/* Ability sounds. Every ability has its own short effect, the files live in
 * assets/audio and are synthesised by tools/gen_ability_sounds.py:
 *   punch            punch.wav
 *   Azum dash        dash.wav
 *   Astra grab       grab.wav
 *   Santa gift       gift.wav
 *   ebuC station     station.wav
 *   overclock        overclock.wav
 * The Azum zombie skin has its own: the punch growls, the dash moans
 * (zombie_punch.wav / zombie_dash.wav). Called from the ability start points
 * (start_punch_now, start_dash_now, start_grab_now, fire_super), so an online
 * opponent does not voice their abilities - only the local player is heard. */

/* Play an ability effect. Loading is lazy and cached by snd_load itself: a
 * missing file simply stays silent (snd_play is not called). */
static void ability_snd(const char *name) {
    if (snd_load(name) == 1) {
        snd_play(name);
    }
}

/* Preload at start, a bit quieter than the interface music, so the first punch
 * of a fight does not hit the disk and the ears. */
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
