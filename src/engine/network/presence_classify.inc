/* Pure classifier for rooms/main/players presence snapshots. */

static int presence_same_nick(const char *slot, size_t len, const char *own) {
    size_t i;
    if (strlen(own) != len)
        return 0;
    for (i = 0; i < len; i++) {
        char a = slot[i];
        char b = own[i];
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
    }
    return 1;
}

/* Classify snapshot: 2 own account present, 1 other player present, 0 empty, -1 invalid JSON. */
static int presence_classify(const char *json, const char *own_nick) {
    static const char key[] = "\"nick\":\"";
    const char *p = json;
    int other = 0;
    if (!json || !own_nick || !own_nick[0])
        return -1;
    while ((p = strstr(p, key)) != NULL) {
        const char *value = p + sizeof(key) - 1;
        const char *end = strchr(value, '"');
        size_t len;
        if (!end)
            break;
        len = (size_t)(end - value);
        if (len > 0) {
            if (presence_same_nick(value, len, own_nick))
                return 2;
            other = 1;
        }
        p = end + 1;
    }
    return other ? 1 : 0;
}
