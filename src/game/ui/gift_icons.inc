/* Gift sticker sprite rendering and rarity colors. */

#define GIFT_TEX_PX 192.0

static const char *gift_tex(double id) {
    if (id == 1) return "gift_cake.png";
    if (id == 2) return "gift_bear.png";
    if (id == 3) return "gift_rocket.png";
    if (id == 4) return "gift_emerald.png";
    if (id == NET_GIFT_PUMPKIN_CUBE_ID) return ORDINARY_PUMPKIN_CARD_TEX;
    return "gift_poop.png";
}

static double gift_shimmer_t;

static void gift_shimmer_tick(void) {
    double step = ds_frame_seconds();
    if (!(step > 0) || step > 0.25) {
        step = dt;
    }
    if (step > 0) {
        gift_shimmer_t = gift_shimmer_t + step;
        if (gift_shimmer_t >= 10000.0) {
            gift_shimmer_t = gift_shimmer_t - 10000.0;
        }
    }
}

/* The same clock smoothly moves the gem through the full HSV color wheel. */
static double gift_shimmer_time(void) { return gift_shimmer_t; }

static uint32_t gift_emerald_tint(void) {
    double phase = fmod(gift_shimmer_t * 0.14, 1.0);
    if (phase < 0) phase = phase + 1;
    double sector = phase * 6;
    int i = (int)floor(sector);
    double f = sector - i;
    double r = 0, g = 0, b = 0;
    if (i == 0) { r = 1; g = f; }
    else if (i == 1) { r = 1 - f; g = 1; }
    else if (i == 2) { g = 1; b = f; }
    else if (i == 3) { g = 1 - f; b = 1; }
    else if (i == 4) { r = f; b = 1; }
    else { r = 1; b = 1 - f; }
    uint32_t ri = (uint32_t)floor(r * 255 + 0.5);
    uint32_t gi = (uint32_t)floor(g * 255 + 0.5);
    uint32_t bi = (uint32_t)floor(b * 255 + 0.5);
    return 0xFF000000u | (ri << 16) | (gi << 8) | bi;
}

/* Draw a continuously cycling rainbow tint; hue is not stored in profiles. */
static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    double sc = s / GIFT_TEX_PX;
    tex(x, y, gift_tex(id), 0, sc);
    if (id == 4) {
        tex_tint(x, y, gift_tex(id), 0, sc, gift_emerald_tint());
    }
}

static double gift_rarity(double id) {
    if (id >= 4) return 3;
    if (id >= 2) return 2;
    if (id == 1) return 1;
    return 0;
}

static const char *gift_rarity_name(double id) {
    double r = gift_rarity(id);
    if (language == 1) {
        if (r == 1) return "РЕДКИЙ";
        if (r == 2) return "ЭПИЧЕСКИЙ";
        if (r == 3) return "ЭКСКЛЮЗИВНЫЙ";
        return "ОБЫЧНЫЙ";
    }
    if (r == 1) return "RARE";
    if (r == 2) return "EPIC";
    if (r == 3) return "EXCLUSIVE";
    return "COMMON";
}

/* Return rarity label color for sticker `id`. */
static uint32_t gift_rarity_color(double id) {
    double r = gift_rarity(id);
    if (r == 1) return 0xFF5CC8FF;
    if (r == 2) return 0xFFC08CFF;
    return 0xFFFFFFFF;
}

/* Draw centered rarity-colored sticker text inside box. */
static void gift_text_rarity(const char *s, double x, double y, double w, double h, double id, double sc) {
    double tw = text_ink_width(s) * sc;
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double tx = x + (w - tw) / 2, ty = y + (h - th) / 2 - tt + 1.5;
    text_scaled(s, tx, ty, gift_rarity_color(id), sc);
}

/* Left-aligned variant for the one-line cards. */
static void gift_text_rarity_left(const char *s, double x, double y, double h, double id, double sc) {
    double th = text_ink_height(s) * sc;
    double tt = text_ink_top(s) * sc;
    double ty = y + (h - th) / 2 - tt + 1.5;
    text_scaled(s, x, ty, gift_rarity_color(id), sc);
}
