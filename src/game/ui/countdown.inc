/* Minute-accurate countdown to New Year 2027 in Votkinsk (UTC+4).
 *
 * ds_wall_clock() is UTC epoch time.  New Year in Votkinsk is therefore
 * 2026-12-31 20:00 UTC, one hour before Moscow.  The display deliberately
 * has no badge or panel: it is a small, plain line at the top of the lobby. */

#define COUNTDOWN_YEAR 2027
#define COUNTDOWN_UTC_OFFSET (4L * 60L * 60L)
#define COUNTDOWN_MIN_CLOCK 1767225600L /* 2026-01-01T00:00:00Z; earlier means an unset clock */
#define COUNTDOWN_TEXT_MAX 72

static char countdown_text[COUNTDOWN_TEXT_MAX];
static double countdown_ok;
static long countdown_last_minute = -1;
static double countdown_last_language = -1;

/* Days from 1970-01-01 to a civil date (Howard Hinnant's days_from_civil). */
static double countdown_days_from_civil(long y, long m, long d) {
    long era, yoe, doy, doe;
    y = m <= 2 ? y - 1 : y;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (double)(era * 146097L + doe - 719468L);
}

/* 1 January 2027 at 00:00 in Votkinsk, expressed as a UTC epoch second. */
static long countdown_target(void) {
    return (long)(countdown_days_from_civil(COUNTDOWN_YEAR, 1, 1) * 86400.0) - COUNTDOWN_UTC_OFFSET;
}

/* Remaining minutes, rounded up so the countdown never reaches zero before
 * midnight.  It still changes only once a minute. */
static long countdown_minutes_left(long now) {
    long target;
    if (now < COUNTDOWN_MIN_CLOCK) {
        return -1;
    }
    target = countdown_target();
    if (now >= target) {
        return 0;
    }
    return (target - now + 59L) / 60L;
}

/* A compact timer only - it deliberately does not say what it counts down to. */
static void countdown_build(void) {
    long now = ds_wall_clock();
    long left = countdown_minutes_left(now);
    long minute = now / 60L;
    long days, hours, minutes;
    if (left < 0) {
        countdown_ok = 0;
        countdown_text[0] = 0;
        return;
    }
    countdown_ok = 1;
    if (minute == countdown_last_minute && language == countdown_last_language && countdown_text[0]) {
        return;
    }
    countdown_last_minute = minute;
    countdown_last_language = language;
    days = left / (24L * 60L);
    hours = (left / 60L) % 24L;
    minutes = left % 60L;
    if (language == 1) {
        snprintf(countdown_text, sizeof(countdown_text), "%ld д. %ld ч. %ld мин.", days, hours, minutes);
    } else {
        snprintf(countdown_text, sizeof(countdown_text), "%ld d. %ld h. %ld min.", days, hours, minutes);
    }
}

static double countdown_font(void) {
    return clamp(screen_w / 1500.0, 0.30, 0.42);
}

static double countdown_x(void) {
    return (screen_w - text_ink_width(countdown_text) * countdown_font()) / 2;
}

static double countdown_y(void) {
    /* Below the Packs, parent badge, and greeting, yet still in the top area. */
    return 60;
}

/* The countdown belongs to the lobby only.  It stays clear of gameplay and
 * other menus, and it has no background behind it. */
static void draw_countdown(void) {
    if (game_state != ST_LOBBY) {
        return;
    }
    countdown_build();
    if (countdown_ok == 0 || countdown_text[0] == 0) {
        return;
    }
    text_scaled(countdown_text, countdown_x(), countdown_y(), 0xFFFFFFFF, countdown_font());
}
