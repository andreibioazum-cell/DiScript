/* Gift sticker sprite rendering and rarity colors. */

#define GIFT_TEX_PX 192.0
#define GIFT_EMERALD_TINT 0xFF00A85Au

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

/* Keep the emerald fixed green; the shared clock still animates nicknames. */
static double gift_shimmer_time(void) { return gift_shimmer_t; }

/* The gem has one stable emerald-green tint, with no account hue setting. */
static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    double sc = s / GIFT_TEX_PX;
    tex(x, y, gift_tex(id), 0, sc);
    if (id == 4) {
        tex_tint(x, y, gift_tex(id), 0, sc, GIFT_EMERALD_TINT);
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
