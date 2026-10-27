static double admin_nick_equal(const char *a, const char *b) {
    if (!a || !b) {
        return 0;
    }
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = ca + ('a' - 'A');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = cb + ('a' - 'A');
        }
        if (ca != cb) {
            return 0;
        }
        i = i + 1;
    }
    return a[i] == b[i];
}

static double admin_slot(const char *nick) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return 1;
    }
    return 0;
}

/* Return 1 when signed-in player has admin privileges. */
static double admin_playing(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}

/* Return 1 when signed-in player has access to combat modifiers in CCP. */
static double admin_modifiers_allowed(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    if (admin_nick_equal(net_login_nick(), ADMIN_NICK) == 1 ||
        admin_nick_equal(net_login_nick(), SANCH_NICK) == 1 ||
        admin_nick_equal(net_login_nick(), QWERTY_NICK) == 1) {
        return 1;
    }
    return 0;
}

/* Christmas is always on for the two holiday admins and never
 * on for Sanch. Combat modifiers in CCP stay available to all three. */
static double christmas_admin_allowed(void) {
    /* With the Internet off the game is completely ordinary: even the two
     * holiday admins lose the Christmas visuals, snowballs and theme. */
    if (net_flags_reachable() != 1 || net_login_status() != 2) {
        return 0;
    }
    if (admin_nick_equal(net_login_nick(), ADMIN_NICK) == 1 ||
        admin_nick_equal(net_login_nick(), QWERTY_NICK) == 1) {
        return 1;
    }
    return 0;
}

/* Sanch gets no Christmas at all: no theme, no events, no snowballs. */
static double sanch_no_christmas(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_nick_equal(net_login_nick(), SANCH_NICK) == 1 ? 1 : 0;
}

/* Rewards already earned by Sanch (Santa and candies) stay with him forever,
 * including in the next Christmas season. */
static double sanch_rewards_kept(void) {
    return sanch_no_christmas();
}

/* Seasonal theme and Battle Pass belong to the two holiday admins only. */
static double seasonal_admin_allowed(void) {
    return christmas_admin_allowed();
}

/* Return 1 when signed-in player can manage gifts in CCP. */
static double admin_gifts_allowed(void) {
    return admin_playing();
}

/* Return 1 when a signed-in CCP moderator has access to the Battle Pass. */
static double bpass_allowed(void) {
    return seasonal_admin_allowed();
}

/* Keep earned Battle Pass rewards (Santa from tier 7) for the holiday admins
 * and for Sanch, who keeps his rewards even without the holiday itself. */
static double bpass_rewards_allowed(void) {
    if (seasonal_admin_allowed() == 1 || sanch_rewards_kept() == 1) {
        return 1;
    }
    return 0;
}

static const char *admin_nick_of(double slot) {
    (void)slot;
    return ADMIN_NICK;
}

static double admin_color_of(const char *nick, double base) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return admin_color;
    }
    if (admin_nick_equal(nick, SANCH_NICK) == 1) {
        return sanch_color;
    }
    if (admin_nick_equal(nick, QWERTY_NICK) == 1) {
        return qwerty_color;
    }
    return base;
}

static double admin_head_color_of(const char *nick, double base) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return admin_head_color;
    }
    if (admin_nick_equal(nick, SANCH_NICK) == 1) {
        return sanch_head_color;
    }
    if (admin_nick_equal(nick, QWERTY_NICK) == 1) {
        return qwerty_head_color;
    }
    return base;
}

static double nick_shimmers(const char *nick) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1 ||
        admin_nick_equal(nick, SANCH_NICK) == 1 ||
        admin_nick_equal(nick, QWERTY_NICK) == 1) {
        return 1;
    }
    return 0;
}
