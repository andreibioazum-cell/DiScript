/* Sticker pictures ship as PNG sprites (assets/textures/gift_*.png, drawn by
 * tools/gen_gift_textures.py), so the shop shows real artwork instead of
 * shapes assembled from rects and circles. Every sprite is 192x192 with a
 * transparent background and is drawn at the size of the card slot.
 *
 * Rarity gives each sticker a colour for its lettering:
 *   0 common    white
 *   1 rare      light blue
 *   2 epic      violet
 *   3 exclusive rainbow (every glyph takes the next hue step) */

#define GIFT_TEX_PX 192.0

static const char *gift_tex(double id) {
    if (id == 1) return "gift_cake.png";
    if (id == 2) return "gift_bear.png";
    if (id == 3) return "gift_rocket.png";
    if (id >= 4) return "gift_emerald.png";
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

/* Emerald hue: 0 is the plain rainbow, 1..7 a starting offset on the wheel. */
static double gift_emerald_base_hue(void) {
    double h = net_gift_hue();
    if (h < 0) h = 0;
    if (h > 7) h = 7;
    return h / 8.0;
}

/* Continuous rainbow shimmer across the full hue wheel over elapsed time. */
static double gift_emerald_phase(void) {
    return gift_emerald_base_hue() + gift_shimmer_t * 0.42;
}

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

/* The emerald: the white stone plus a shimmering layer of the hue, so the
 * colour flows over it and drifts with time. The tint reads only on a white
 * base - on a coloured sprite the hue would just turn to mud. */
static void gift_icon(double id, double x, double y, double s) {
    if (s <= 0) return;
    double sc = s / GIFT_TEX_PX;
    tex(x, y, gift_tex(id), 0, sc);
    if (id >= 4) {
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

/* White, light blue and violet; the exclusive emerald is signed white as
 * well - its colour lives on the stone itself, which drifts through the hue
 * wheel, not in the lettering. */
static uint32_t gift_rarity_color(double id) {
    double r = gift_rarity(id);
    if (r == 1) return 0xFF5CC8FF;
    if (r == 2) return 0xFFC08CFF;
    return 0xFFFFFFFF;
}

/* Centred rarity-coloured lettering inside a box, with the same baseline the
 * other card labels use (text_in_box). The exclusive emerald is signed in
 * plain white: the colour belongs to the stone itself, which drifts through
 * the hue wheel on its own. */
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
