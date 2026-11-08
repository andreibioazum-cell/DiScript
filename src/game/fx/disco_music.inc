/* Music of the disco event (event 1): our own first disco loop,
 * disco_01_roller_rink_sunset.wav. It is written by tools/generate_disco_loops.py
 * from the maths in this repository, so the game carries no third-party
 * recording and no borrowed melody - it replaced the "Disco Medusae" track
 * that used to be bundled on main. The loop is 30 s of stereo WAV and wraps
 * seamlessly, so it can run for the whole fight without a click. */

#define DISCO_MUSIC "disco_01_roller_rink_sunset.wav"
#define DISCO_FADE_IN 0.35
#define DISCO_FADE_OUT 0.50

/* The loop owns the sound only while a disco fight is running. */
static int disco_should_play(void) {
    return (game_state == ST_SOLO || game_state == ST_ONLINE) && event_mode == 1 && finished == 0;
}

/* Highest-priority music: the lobby, winter and duel themes must stay quiet
 * while the loop plays, and until its fade-out is over. */
static int disco_music_blocks(void) {
    return (disco_ok == 1 && disco_should_play() == 1) || disco_level > 0;
}

static void disco_silence_other_music(void) {
    if (lobby_music_current != NULL && snd_playing(lobby_music_current) == 1) {
        snd_stop(lobby_music_current);
        music_level = 0;
    }
    if (snd_playing(WINTER_JINGLE) == 1) {
        snd_stop(WINTER_JINGLE);
    }
    if (snd_playing(SERAPHIM_MUSIC) == 1) {
        snd_stop(SERAPHIM_MUSIC);
        seraphim_level = 0;
    }
}

static void update_disco_music(void) {
    double target = clamp(music_volume / 100.0, 0, 1);
    if (disco_should_play() == 1) {
        if (disco_ok == 0) {
            disco_ok = snd_load(DISCO_MUSIC);
            if (disco_ok == 1) {
                ds_log("disco: own loop ready (%s)", DISCO_MUSIC);
            } else {
                ds_log("disco: no loop (%s)", DISCO_MUSIC);
            }
        }
        if (disco_ok != 1) {
            return;
        }
        disco_silence_other_music();
        if (snd_playing(DISCO_MUSIC) == 0) {
            disco_level = 0;
            snd_volume(DISCO_MUSIC, 0);
            /* The mixer owns the loop: never restart it on every game tick. */
            if (snd_loop(DISCO_MUSIC) == 0) {
                disco_ok = 0;
                return;
            }
        }
        disco_level = clamp(disco_level + dt / DISCO_FADE_IN, 0, 1);
        snd_volume(DISCO_MUSIC, target * disco_level);
        return;
    }
    if (disco_ok != 1 || disco_level <= 0) {
        return;
    }
    disco_level = clamp(disco_level - dt / DISCO_FADE_OUT, 0, 1);
    snd_volume(DISCO_MUSIC, target * disco_level);
    if (disco_level <= 0 && snd_playing(DISCO_MUSIC) == 1) {
        snd_stop(DISCO_MUSIC);
    }
}
