/* Corner countdown to the next New Year (1 January 2027).
 *
 * The badge sits in the bottom-right corner of the menu screens and reads the
 * device clock. ds_wall_clock() is the platform hook; when it cannot read a
 * clock (or the clock is set before 2026, which means it is simply wrong) the
 * badge stays hidden instead of counting from a bogus date. */

#define COUNTDOWN_YEAR 2027
#define COUNTDOWN_MIN_CLOCK 1767225600L /* 2026-01-01T00:00:00Z; earlier means an unset clock */
#define COUNTDOWN_TEXT_MAX 56

static char countdown_text[COUNTDOWN_TEXT_MAX];
static double countdown_ok;
static long countdown_last_day = -1;

static double countdown_leap(long year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0 ? 1 : 0;
}

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

/* Whole local days from `now` to 1 January COUNTDOWN_YEAR, or -1 before the
 * clock is trustworthy. */
static long countdown_days_left(long now) {
    double days;
    if (now < COUNTDOWN_MIN_CLOCK) {
        return -1;
    }
    days = countdown_days_from_civil(COUNTDOWN_YEAR, 1, 1) - (double)(now / 86400L);
    if (days < 0) {
        days = 0;
    }
    return (long)days;
}

/* "До Нового года 2027: 53 дня" / "New Year 2027 in 53 days". */
static void countdown_build(void) {
    long now = ds_wall_clock();
    long left = countdown_days_left(now);
    long day = now / 86400L;
    long rest;
    if (left < 0) {
        countdown_ok = 0;
        countdown_text[0] = 0;
        return;
    }
    countdown_ok = 1;
    if (day == countdown_last_day && countdown_text[0]) {
        return;
    }
    countdown_last_day = day;
    rest = left % 100;
    if (left == 0) {
        snprintf(countdown_text, sizeof(countdown_text), "%s",
                 language == 1 ? "С Новым годом!" : "Happy New Year!");
    } else if (language == 1) {
        const char *word = "дней";
        if (rest < 11 || rest > 14) {
            long tail = rest % 10;
            if (tail == 1) {
                word = "день";
            } else if (tail >= 2 && tail <= 4) {
                word = "дня";
            }
        }
        snprintf(countdown_text, sizeof(countdown_text), "До Нового года %d: %ld %s",
                 (int)COUNTDOWN_YEAR, left, word);
    } else if (left == 1) {
        snprintf(countdown_text, sizeof(countdown_text), "New Year %d in 1 day", (int)COUNTDOWN_YEAR);
    } else {
        snprintf(countdown_text, sizeof(countdown_text), "New Year %d in %ld days", (int)COUNTDOWN_YEAR, left);
    }
}

static double countdown_font(void) {
    return clamp(screen_w / 1500.0, 0.30, 0.42);
}

static double countdown_w(void) {
    return text_ink_width(countdown_text) * countdown_font() + 20;
}

static double countdown_h(void) {
    return 28;
}

static double countdown_x(void) {
    return screen_w - countdown_w() - 12;
}

static double countdown_y(void) {
    return screen_h - countdown_h() - 10;
}

/* Draw the badge in the bottom-right corner; it belongs to the menu screens,
 * never to a fight. */
static void draw_countdown(void) {
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return;
    }
    countdown_build();
    if (countdown_ok == 0 || countdown_text[0] == 0) {
        return;
    }
    roundrect(countdown_x(), countdown_y(), countdown_w(), countdown_h(), 9, 0xCC000000);
    roundrect_outline(countdown_x(), countdown_y(), countdown_w(), countdown_h(), 9, 1.5, 0x66FFFFFF);
    text_in_box(countdown_text, countdown_x() + 10, countdown_y(), countdown_w() - 20, countdown_h(),
                0xFFFFFFFF, countdown_font());
}
