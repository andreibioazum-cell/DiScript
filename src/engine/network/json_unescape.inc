/* Decode JSON string escapes (\uXXXX, UTF-16 surrogate pairs, control escapes). */
static unsigned long json_hex4(const char *p) {
    unsigned long v = 0;
    int k;
    if (!p || p[0] != '\\' || p[1] != 'u')
        return RGB(255, 255, 255);
    for (k = 2; k < 6; k++) {
        char c = p[k];
        v <<= 4;
        if (c >= '0' && c <= '9')
            v |= (unsigned long)(c - '0');
        else if (c >= 'a' && c <= 'f')
            v |= (unsigned long)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            v |= (unsigned long)(c - 'A' + 10);
        else
            return RGB(255, 255, 255);
    }
    return v;
}

static void json_put_utf8(char *out, size_t cap, size_t *i, unsigned long cp) {
    char u[4];
    size_t n = 0, k;
    if (cp < 0x80) {
        u[n++] = (char)cp;
    } else if (cp < 0x800) {
        u[n++] = (char)(0xC0 | (cp >> 6));
        u[n++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        u[n++] = (char)(0xE0 | (cp >> 12));
        u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[n++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        u[n++] = (char)(0xF0 | (cp >> 18));
        u[n++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u[n++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u[n++] = (char)(0x80 | (cp & 0x3F));
    }
    for (k = 0; k < n && *i + 1 < cap; k++)
        out[(*i)++] = u[k];
}

/* Extract and unescape JSON string value into UTF-8 output buffer. */
static void strv(const char *json, const char *path, char *out, size_t cap) {
    const char *v;
    size_t i = 0;
    if (!out || !cap)
        return;
    out[0] = 0;
    v = path_val(json, path);
    if (!v || *v != '\"')
        return;
    for (v++; *v && *v != '\"' && i + 1 < cap; v++) {
        if (*v != '\\' || !v[1]) {
            out[i++] = *v;
            continue;
        }
        v++;
        switch (*v) {
        case 'n':
            out[i++] = '\n';
            break;
        case 't':
            out[i++] = '\t';
            break;
        case 'r':
            out[i++] = '\r';
            break;
        case 'b':
            out[i++] = '\b';
            break;
        case 'f':
            out[i++] = '\f';
            break;
        case 'u': {
            unsigned long cp = json_hex4(v - 1);
            if (cp == RGB(255, 255, 255)) {
                                /* Preserve literal 'u' on malformed hex escape. */
                out[i++] = 'u';
                break;
            }
            v += 4;
                /* Combine UTF-16 surrogate pair into one code point. */
                if (cp >= 0xD800 && cp <= 0xDBFF) {
                unsigned long lo = json_hex4(v + 1);
                if (lo != RGB(255, 255, 255) && lo >= 0xDC00 && lo <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    v += 6;
                } else
                    cp = 0xFFFD;
            } else if (cp >= 0xDC00 && cp <= 0xDFFF)
                cp = 0xFFFD;
            json_put_utf8(out, cap, &i, cp);
            break;
        }
        default:
            out[i++] = *v;
            break;
        }
    }
    out[i] = 0;
}
