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

/* Root admins: Dimasi4ek229 does everything, qwertyuiopaj1234 everything
 * except handing out gifts (see admin_gifts_allowed). */
static double admin_slot(const char *nick) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1) {
        return 1;
    }
    if (admin_nick_equal(nick, QWERTY_NICK) == 1) {
        return 2;
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
        admin_nick_equal(net_login_nick(), QWERTY_NICK) == 1 ||
        admin_nick_equal(net_login_nick(), "Efy") == 1) {
        return 1;
    }
    return 0;
}

/* Holiday admins retain their New Year preview when the global flag is off.
 * The live global Christmas event is handled below for every online player;
 * combat modifiers in CCP stay available to all three moderators. */
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

/* The global holiday flag gives every player a New Year default until they
 * explicitly choose a theme. A saved ordinary/Halloween choice remains respected. */
static double seasonal_content_allowed(void) {
    return net_christmas() == 1 || christmas_admin_allowed() == 1 ? 1 : 0;
}

static double active_theme_mode(void) {
    if (theme_chosen == 0 && seasonal_content_allowed() == 1) {
        return 1;
    }
    return theme_mode;
}

/* Identify Sanch so his earned rewards remain available when the global
 * event is off; the active global Christmas flag still covers all players. */
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


/* Return 1 when signed-in player can manage gifts in CCP. Handing out gifts
 * stays exclusive to the root admin: qwertyuiopaj1234 gets every other admin
 * power, but never the gift tab and never free stickers. */
static double admin_gifts_allowed(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    if (admin_nick_equal(net_login_nick(), ADMIN_NICK) == 1) {
        return 1;
    }
    return 0;
}

/* Return 1 when the online player has seasonal Battle Pass access. */
static double bpass_allowed(void) {
    return seasonal_content_allowed();
}

/* Keep already-earned Battle Pass rewards visible when seasonal access is
 * off; Sanch's claimed Santa and candies persist even between events. */
static double bpass_rewards_allowed(void) {
    if (seasonal_content_allowed() == 1 || sanch_rewards_kept() == 1) {
        return 1;
    }
    return 0;
}

/* Chat actions sign with the acting admin's own nick. */
static const char *admin_nick_of(double slot) {
    (void)slot;
    if (net_login_status() == 2 && admin_slot(net_login_nick()) != 0) {
        return net_login_nick();
    }
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
    if (admin_nick_equal(nick, "Efy") == 1) {
        return RGB(255,213,74);
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
    if (admin_nick_equal(nick, "Efy") == 1) {
        return RGB(179,136,255);
    }
    return base;
}

static double nick_shimmers(const char *nick) {
    if (admin_nick_equal(nick, ADMIN_NICK) == 1 ||
        admin_nick_equal(nick, SANCH_NICK) == 1 ||
        admin_nick_equal(nick, QWERTY_NICK) == 1 ||
        admin_nick_equal(nick, "Efy") == 1) {
        return 1;
    }
    return 0;
}
