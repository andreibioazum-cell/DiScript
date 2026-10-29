/* Gift sticker sprite rendering, rarity colors, and emerald hue shimmer. */

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

/* Emerald base hue is fixed to 0 (always rainbow). */
static double gift_emerald_base_hue(void) { return 0.0; }

/* Continuous rainbow shimmer across the full hue wheel over elapsed time. */
static double gift_emerald_phase(void) { return gift_shimmer_t * 0.42; }

static double gift_shimmer_time(void) { return gift_shimmer_t; }

/* The hue colour comes from the same wheel as the rainbow letters. */
static uint32_t gift_hue_color(double hue) {
    double h = hue - floor(hue);
    double r = fabs(h * 6 - 3) - 1, g = 2 - fabs(h * 6 - 2), b = 2 - fabs(h * 6 - 4);
    if (r < 0) r = 0;
    if (r > 1) r = 1;
    if (g < 0) g = 0;
    if (g > 1) g = 1;
    if (b < 0) b = 0;
    if (b > 1) b = 1;
    return 0xFF000000u | ((uint32_t)(r * 255) << 16) | ((uint32_t)(g * 255) << 8) | (uint32_t)(b * 255);
}

/* Draw sticker sprite; overlay animated hue tint for emerald. */
static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    double sc = s / GIFT_TEX_PX;
    tex(x, y, gift_tex(id), 0, sc);
    if (id == 4) {
        double ph = gift_emerald_phase();
        double a1 = clamp(145 + 35 * sin(gift_shimmer_t * 3.4), 95, 190);
        double a2 = clamp(85 + 35 * cos(gift_shimmer_t * 4.7), 45, 130);
        uint32_t c1 = gift_hue_color(ph);
        uint32_t c2 = gift_hue_color(ph + 0.18);
        tex_tint(x, y, gift_tex(id), 0, sc, (uint32_t)(((unsigned)(int)a1) << 24) | (c1 & 0x00FFFFFFu));
        tex_tint(x, y, gift_tex(id), 0, sc, (uint32_t)(((unsigned)(int)a2) << 24) | (c2 & 0x00FFFFFFu));
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
