/* Runtime decoding of XOR-obfuscated write secret and account password suffix. */

static const char *net_secret(char *buf, size_t cap, const char *const *chunks, size_t count) {
    size_t i, at = 0;
    for (i = 0; i < count && at + 1 < cap; i++) {
        const char *p = chunks[i];
        for (; *p && at + 1 < cap; p++) {
            buf[at++] = (char)((unsigned char)*p ^ NET_SECRET_XOR);
            buf[at] = 0;
        }
    }
    buf[at] = 0;
    return buf;
}

/* Return decoded database write secret. */
static const char *net_write_secret(void) {
    static const char *chunks[] = {NET_WRITE_SECRET_0, NET_WRITE_SECRET_1, NET_WRITE_SECRET_2};
    static char secret[NET_WRITE_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Account password suffix. It never goes to the database. */
static const char *net_pass_suffix(void) {
    static const char *chunks[] = {NET_PASS_SECRET_0, NET_PASS_SECRET_1, NET_PASS_SECRET_2};
    static char secret[NET_PASS_SECRET_LEN + 1];
    if (!secret[0])
        net_secret(secret, sizeof(secret), chunks, sizeof(chunks) / sizeof(chunks[0]));
    return secret;
}

/* Legacy password suffix accepted only for one-time account migration. */
#define FB_LEGACY_PASS_SUFFIX "|cb4v1"

/* Append write secret field to JSON object body before closing brace. */
static int net_seal_body(char *body, size_t cap) {
    size_t n;
    const char *secret = net_write_secret();
    if (!body || cap < 4)
        return 0;
    n = strlen(body);
    if (n < 2 || body[n - 1] != '}')
        return 0;
        /* Leave body unmodified if sealed payload exceeds buffer capacity. */
    if (n + strlen(secret) + 8 > cap)
        return 0;
    body[--n] = 0;
    snprintf(body + n, cap - n, ",\"w\":\"%s\"}", secret);
    return 1;
}

/* Pre-decode secrets at startup before worker threads spawn. */
static void net_secret_prepare(void) {
    (void)net_write_secret();
    (void)net_pass_suffix();
}
