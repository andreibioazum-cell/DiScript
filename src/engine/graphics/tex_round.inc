/* Queue unrotated texture clipped to a rounded rectangle. */
void tex_round(float x, float y, const char *name, float s, float r) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    DSCmd *p = push(DS_CMD_TEX_ROUND);
    if (!p)
        return;
    p->v.txr.x = x;
    p->v.txr.y = y;
    p->v.txr.sc = s;
    p->v.txr.r = r;
    p->v.txr.tx = t;
}

/* Queue texture with alpha modulation. */
void tex_alpha(float x, float y, const char *name, float a, float s, double alpha) {
    if (!frame_open)
        return;
    Texture *t = load_png(name);
    if (!t)
        return;
    double k = alpha;
    if (k < 0) {
        k = 0;
    }
    if (k > 1) {
        k = 1;
    }
    unsigned int byte = (unsigned int)floor(k * 254) + 1;
    if (byte > 255) {
        byte = 255;
    }
    DSCmd *p = push(DS_CMD_TEX_FADE);
    if (!p)
        return;
    p->v.tx2.x = x;
    p->v.tx2.y = y;
    p->v.tx2.a = a;
    p->v.tx2.sc = s;
    p->v.tx2.tx = t;
    p->v.tx2.c = pack_c((byte << 24) | 0x00FFFFFF);
}
