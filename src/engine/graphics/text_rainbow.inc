/* Queue rainbow and subtle shimmer text draw commands using per-frame arena. */

void text_rainbow(const char *s, float x, float y, float sc, float hue) {
    if (!frame_open || !s || !*s || !ensure_font())
        return;
    char *copy = cmd_arena_str_n(s, strlen(s));
    if (!copy)
        return;
    DSCmd *p = push(DS_CMD_TEXT_RAINBOW);
    if (!p)
        return;
    p->v.tr.s = copy;
    p->v.tr.x = x;
    p->v.tr.y = y;
    p->v.tr.sc = sc;
    p->v.tr.hue = hue;
    p->v.tr.c = 0;
}

void text_shimmer(const char *s, float x, float y, uint32_t c, float sc, float phase) {
    if (!frame_open || !s || !*s || !ensure_font())
        return;
    char *copy = cmd_arena_str_n(s, strlen(s));
    if (!copy)
        return;
    DSCmd *p = push(DS_CMD_TEXT_RAINBOW);
    if (!p)
        return;
    p->v.tr.s = copy;
    p->v.tr.x = x;
    p->v.tr.y = y;
    p->v.tr.sc = sc;
    p->v.tr.hue = phase;
    p->v.tr.c = (c & 0xFF000000u) ? c : (0xFF000000u | c);
}
