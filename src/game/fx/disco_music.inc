/* Event 1 music: a real published recording, bundled for offline playback.
 * "Disco Medusae" by Kevin MacLeod (incompetech.com), CC BY 4.0.
 * https://creativecommons.org/licenses/by/4.0/
 * Source ISRC USUAN1500041. Format conversion only; see docs/music.md. */

#define DISCO_MUSIC "disco_medusae_kevin_macleod.mp3"
#define DISCO_CREDIT "Disco Medusae - Kevin MacLeod"
#define DISCO_LICENSE_LINE "CC BY 4.0 | incompetech.com"
#define DISCO_FADE_IN 0.35
#define DISCO_FADE_OUT 0.50

static int disco_should_play(void) {
    return (game_state == ST_SOLO || game_state == ST_ONLINE) && event_mode == 1 && finished == 0;
}

/* Highest-priority music. Keep ownership through the fade-out so the lobby,
 * winter and duel loops cannot overlap the last half-second of the song. */
static int disco_music_blocks(void) {
    return (disco_ok == 1 && disco_should_play() == 1) || disco_level > 0;
}

static void disco_silence_other_music(void) {
    if (lobby_music_current != NULL && snd_playing(lobby_music_current) == 1)
        snd_stop(lobby_music_current);
    if (snd_playing(WINTER_JINGLE) == 1) snd_stop(WINTER_JINGLE);
    if (snd_playing(SERAPHIM_MUSIC) == 1) snd_stop(SERAPHIM_MUSIC);
    music_level = 0;
    seraphim_level = 0;
}

static void update_disco_music(void) {
    double target = clamp(music_volume / 100.0, 0, 1);
    if (disco_should_play() == 1) {
        if (disco_ok < 0) {
            disco_ok = snd_load(DISCO_MUSIC);
            if (disco_ok == 1) ds_log("disco: Kevin MacLeod track ready (CC BY 4.0)");
            else ds_log("disco: recording unavailable");
        }
        if (disco_ok != 1) return;
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
    if (disco_ok != 1 || disco_level <= 0) return;
    disco_level = clamp(disco_level - dt / DISCO_FADE_OUT, 0, 1);
    snd_volume(DISCO_MUSIC, target * disco_level);
    if (disco_level <= 0 && snd_playing(DISCO_MUSIC) == 1) snd_stop(DISCO_MUSIC);
}

static double disco_credit_w(void) { return fmin(560, screen_w - 24); }
static double disco_credit_y(void) { return screen_h - 80; }
static int disco_credit_visible(void) {
    return disco_should_play() == 1 && chat_open == 0;
}
static int disco_credit_hit(double x, double y) {
    return disco_credit_visible() == 1 &&
           hit_rect(x, y, (screen_w - disco_credit_w()) / 2, disco_credit_y(), disco_credit_w(), 48) == 1;
}

/* Persistent, readable credit while the recording plays. Tapping it opens
 * the full source/licence/modification notice in the in-game legal viewer. */
static void draw_disco_credit(void) {
    if (disco_credit_visible() == 0) return;
    double w = disco_credit_w(), x = (screen_w - w) / 2, y = disco_credit_y();
    roundrect(x, y, w, 48, 12, 0xC0101010);
    ctext_scaled(DISCO_CREDIT, y + 6, 0xFFFFFF, fit_text_scale(DISCO_CREDIT, w - 20, 0.43));
    ctext_scaled(DISCO_LICENSE_LINE, y + 27, 0xCCCCCC, fit_text_scale(DISCO_LICENSE_LINE, w - 20, 0.32));
}
