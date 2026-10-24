/* Profanity, transliteration, lookalike, and URL filter for chat and nicknames. */

#define CF_WORD_MAX 64
#define CF_OUT_MAX 512

/* Fold ASCII and Cyrillic codepoints to lowercase (mapping yo to ye). */
static unsigned cf_lower(unsigned c) {
    if (c >= 'A' && c <= 'Z')
        return c + 32;
    if (c >= 0x410 && c <= 0x42F)
        return c + 0x20;
    if (c == 0x401 || c == 0x451)
        return 0x435;
    return c;
}

static unsigned cf_utf8_next(const unsigned char **p, const unsigned char *end) {
    const unsigned char *s = *p;
    unsigned c = s[0];
    if (c < 0x80 || s + 1 >= end) {
        *p = s + 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        *p = s + 2;
        return ((c & 0x1F) << 6) | (s[1] & 0x3F);
    }
    if ((c & 0xF0) == 0xE0 && s + 2 < end) {
        *p = s + 3;
        return ((c & 0x0F) << 12) | ((unsigned)(s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    }
    *p = s + 1;
    return 0xFFFD;
}

/* Latin letters and digits read by look as Cyrillic; 0 means drop the sign. */
static unsigned cf_visual(unsigned c) {
    static const char from[] = "acekmoptxyhunrb30694@";
    static const unsigned to[] = {0x430, 0x441, 0x435, 0x43A, 0x43C, 0x43E, 0x440, 0x442, 0x445, 0x443, 0x43D,
                                  0x438, 0x43F, 0x433, 0x431, 0x437, 0x43E, 0x431, 0x434, 0x447, 0x430};
    const char *f;
    if (c >= 0x430 && c <= 0x44F)
        return c;
    if (c < 0x80 && c != 0) {
        f = strchr(from, (int)c);
        if (f)
            return to[f - from];
    }
    return 0;
}

/* Latin letters read by sound (translit). cf_norm handles double pairs. */
static unsigned cf_phonetic(unsigned c) {
    static const char from[] = "abvgdezijklmnoprstufhxcywq30469";
    static const unsigned to[] = {0x430, 0x431, 0x432, 0x433, 0x434, 0x435, 0x437, 0x438, 0x439, 0x43A, 0x43B,
                                  0x43C, 0x43D, 0x43E, 0x43F, 0x440, 0x441, 0x442, 0x443, 0x444, 0x445, 0x445,
                                  0x446, 0x439, 0x432, 0x43A, 0x437, 0x43E, 0x447, 0x431, 0x434};
    const char *f;
    if (c >= 0x430 && c <= 0x44F)
        return c;
    if (c < 0x80 && c != 0) {
        f = strchr(from, (int)c);
        if (f)
            return to[f - from];
    }
    return 0;
}

static unsigned cf_digraph(unsigned a, unsigned b) {
    if (b == 'h') {
        if (a == 's') return 0x448; /* sha */
        if (a == 'c') return 0x447; /* che */
        if (a == 'z') return 0x436; /* zhe */
        if (a == 'k') return 0x445; /* kha */
    }
    if (a == 'y' || a == 'j') {
        if (b == 'a') return 0x44F; /* ya */
        if (b == 'u') return 0x44E; /* yu */
        if (b == 'o' || b == 'e') return 0x435;
    }
    if (a == 't' && b == 's')
        return 0x446; /* tse */
    return 0;
}

/* Word -> lowercase Cyrillic in UTF-8. phonetic: read latin as translit. */
static void cf_norm(const unsigned *w, int n, int phonetic, char *out, size_t cap) {
    size_t o = 0;
    unsigned last = 0;
    int i;
    for (i = 0; i < n; i++) {
        unsigned c = 0;
        if (phonetic && i + 1 < n)
            c = cf_digraph(w[i], w[i + 1]);
        if (c)
            i++;
        else
            c = phonetic ? cf_phonetic(w[i]) : cf_visual(w[i]);
        if (!c && ((w[i] >= 'a' && w[i] <= 'z'))) {
            /* Insert separator for unmapped Latin characters to prevent false root matches. */
            if (o + 2 >= cap)
                break;
            out[o++] = '#';
            last = 0;
            continue;
        }
        if (!c || c == last)
            continue;
        last = c;
        if (o + 3 >= cap)
            break;
        out[o++] = (char)(0xC0 | (c >> 6));
        out[o++] = (char)(0x80 | (c & 0x3F));
    }
    out[o] = 0;
}

static int cf_starts(const char *s, const char *p) {
    return strncmp(s, p, strlen(p)) == 0;
}

static int cf_in(const char *w, const char *const *list) {
    for (; *list; list++)
        if (strcmp(w, *list) == 0)
            return 1;
    return 0;
}

/* The root plus a tail letter, but not inside "psihuy" or "zastrahuy". */
static int cf_has_hu(const char *w) {
    static const char *const tails[] = {"хуй", "хуе", "хуя", "хуи", "хую", 0};
    const char *const *t;
    for (t = tails; *t; t++) {
        const char *p = w;
        while ((p = strstr(p, *t)) != NULL) {
            size_t before = (size_t)(p - w);
            int ok = !(before >= 6 && strncmp(p - 6, "пси", 6) == 0) && !(before >= 8 && strncmp(p - 8, "стра", 8) == 0);
            if (ok)
                return 1;
            p += strlen(*t);
        }
    }
    return 0;
}

/* Match "eb" root at word start or after known Russian prefixes. */
static int cf_has_eb(const char *w) {
    static const char *const pre[] = {"", "за", "на", "по", "вы", "съ", "у", "от", "ото", "отъ", "до", "раз", "разъ",
                                      "рас", "про", "из", "изъ", "при", "пере", "недо", "въ", "вз", "взъ", "долбо",
                                      "долба", 0};
    const char *const *p;
    for (p = pre; *p; p++) {
        size_t n = strlen(*p);
        if (strncmp(w, *p, n) == 0 && cf_starts(w + n, "еб"))
            return 1;
    }
    return 0;
}

static int cf_bad_cyr(const char *w, int skip_eb) {
    static const char *const roots[] = {"пизд", "пезд", "залуп", "мудак", "мудил", "пидор", "пидар", "пидрил",
                                        "пидрас", "гандон", "гондон", "шлюх", "дроч", "бляд", "долбоеб",
                                        "долбаеб", "уебок", "уебан", "уебищ", "мандав", "сукабл", 0};
    /* Prefix-only roots to avoid false positives inside longer words. */
    static const char *const heads[] = {"блят", "порн", 0};
    static const char *const words[] = {"бля", "сука", "суки", "суке", "суку", "сукой", "сучка", "сучку", "сучки",
                                        "сучара", "сукин", "манда", "манды", 0};
    const char *const *r;
    if (!w[0])
        return 0;
    for (r = roots; *r; r++)
        if (strstr(w, *r))
            return 1;
    for (r = heads; *r; r++)
        if (cf_starts(w, *r))
            return 1;
    return cf_in(w, words) || cf_has_hu(w) || (!skip_eb && cf_has_eb(w));
}

static int cf_bad_latin(const char *raw) {
    static const char *const roots[] = {"fuck", "shit", "bitch", "cunt", "nigg", "fagg", "whore", "porn", 0};
    char low[CF_WORD_MAX + 1];
    size_t i;
    const char *const *r;
    for (i = 0; raw[i] && i < CF_WORD_MAX; i++)
        low[i] = (char)cf_lower((unsigned char)raw[i]);
    low[i] = 0;
    for (r = roots; *r; r++)
        if (strstr(low, *r))
            return 1;
    return 0;
}

/* A link or a site address: http, www, ://, t.me, "something.ru" and so on. */
static int cf_is_link(const char *raw) {
    static const char *const tlds[] = {"ru", "su", "com", "net", "org", "io", "me", "gg", "xyz", "ua", "by", "kz",
                                       "info", "biz", "site", "online", "top", "link", "cc", "tv", "app", "pro",
                                       "\xd1\x80\xd1\x84", 0};
    char low[CF_WORD_MAX + 1];
    size_t i, n;
    const char *dot;
    for (i = 0; raw[i] && i < CF_WORD_MAX; i++)
        low[i] = (char)cf_lower((unsigned char)raw[i]);
    low[i] = 0;
    if (strstr(low, "http") || strstr(low, "www") || strstr(low, "://") || strstr(low, "t.me"))
        return 1;
    for (dot = strchr(low, '.'); dot; dot = strchr(dot + 1, '.')) {
        const char *const *t;
        if (dot == low)
            continue;
        for (t = tlds; *t; t++) {
            n = strlen(*t);
            if (strncmp(dot + 1, *t, n) == 0 && (dot[1 + n] == 0 || dot[1 + n] == '/'))
                return 1;
        }
    }
    return 0;
}

/* Allowed Latin words starting with "eb" (eBay, ebook, ebuC). */
static int cf_latin_eb_ok(const char *raw) {
    static const char *const ok[] = {"ebuc", "ebay", "ebook", 0};
    const char *const *k;
    for (k = ok; *k; k++) {
        size_t i = 0;
        while ((*k)[i] && raw[i] && (char)cf_lower((unsigned char)raw[i]) == (*k)[i])
            i++;
        if (!(*k)[i])
            return 1;
    }
    return 0;
}

/* 1 - the word (len bytes at raw) must be hidden. */
static int chat_word_blocked(const char *raw, size_t len) {
    unsigned w[CF_WORD_MAX];
    char buf[CF_WORD_MAX + 1], norm[CF_WORD_MAX * 2 + 1];
    const unsigned char *p = (const unsigned char *)raw, *end = p + len;
    int n = 0, cyr = 0, skip_eb;
    if (len > CF_WORD_MAX)
        len = CF_WORD_MAX;
    memcpy(buf, raw, len);
    buf[len] = 0;
    end = p + len;
    while (p < end && n < CF_WORD_MAX) {
        unsigned c = cf_lower(cf_utf8_next(&p, end));
        if (c >= 0x430 && c <= 0x44F)
            cyr = 1;
        w[n++] = c;
    }
    if (cf_is_link(buf) || cf_bad_latin(buf))
        return 1;
    skip_eb = !cyr && cf_latin_eb_ok(buf);
    cf_norm(w, n, 0, norm, sizeof(norm));
    if (cf_bad_cyr(norm, skip_eb))
        return 1;
    if (!cyr) {
        cf_norm(w, n, 1, norm, sizeof(norm));
        if (cf_bad_cyr(norm, skip_eb))
            return 1;
    }
    return 0;
}

/* Cleans text into dst: banned words become "***". */
static void chat_clean_into(char *dst, size_t cap, const char *src) {
    size_t o = 0;
    if (!cap)
        return;
    if (!src)
        src = "";
    while (*src && o + 1 < cap) {
        size_t len = 0;
        if (*src == ' ' || *src == '\t') {
            dst[o++] = *src++;
            continue;
        }
        while (src[len] && src[len] != ' ' && src[len] != '\t')
            len++;
        if (chat_word_blocked(src, len)) {
            const char *mask = "***";
            while (*mask && o + 1 < cap)
                dst[o++] = *mask++;
        } else {
            size_t k;
            for (k = 0; k < len && o + 1 < cap; k++)
                dst[o++] = src[k];
        }
        src += len;
    }
    dst[o] = 0;
}

/* The same for drawing: the string lives in the frame pool like other labels. */
static const char *chat_clean(const char *src) {
    char out[CF_OUT_MAX];
    chat_clean_into(out, sizeof(out), src);
    return ds_concat(out, "");
}

/* 1 - the nick contains swearing: such a nick is not let into the game. */
static double nick_blocked(const char *nick) {
    return nick && chat_word_blocked(nick, strlen(nick)) ? 1 : 0;
}
