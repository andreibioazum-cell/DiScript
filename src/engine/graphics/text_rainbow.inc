/* Queue rainbow and subtle shimmer text draw commands. */

void text_rainbow(const char *s, float x, float y, float sc, float hue) {
    if (!frame_open || !s || !ensure_font())
        return;
    DSCmd *p = push(DS_CMD_TEXT_RAINBOW);
    if (!p)
        return;
    p->v.tr.s = strdup_safe(s);
    if (!p->v.tr.s) {
        ds_log_err("renderer: no memory to copy text");
        cmd_n = cmd_n - 1;
        return;
    }
    p->v.tr.x = x;
    p->v.tr.y = y;
    p->v.tr.sc = sc;
    p->v.tr.hue = hue;
    p->v.tr.c = 0;
}

void text_shimmer(const char *s, float x, float y, uint32_t c, float sc, float phase) {
    if (!frame_open || !s || !ensure_font())
        return;
    DSCmd *p = push(DS_CMD_TEXT_RAINBOW);
    if (!p)
        return;
    p->v.tr.s = strdup_safe(s);
    if (!p->v.tr.s) {
        ds_log_err("renderer: no memory to copy text");
        cmd_n = cmd_n - 1;
        return;
    }
    p->v.tr.x = x;
    p->v.tr.y = y;
    p->v.tr.sc = sc;
    p->v.tr.hue = phase;
    p->v.tr.c = (c & RGB(0,0,0)) ? c : (RGB(0,0,0) | c);
}
