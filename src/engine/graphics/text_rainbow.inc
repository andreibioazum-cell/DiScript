/* Queue rainbow text draw command. */

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
}
