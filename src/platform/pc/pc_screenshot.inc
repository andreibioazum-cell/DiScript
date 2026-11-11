/* ---- BMP screenshot (headless checks read them) ------------------------- */
static int pc_write_bmp(const char *path, const uint32_t *px, int w, int h, int stride) {
    FILE *f;
    int row, pad;
    unsigned char hdr[54];
    uint32_t file_size;
    if (!path || !px || w < 1 || h < 1)
        return 0;
    pad = (4 - (w * 3) % 4) % 4;
    file_size = 54u + (uint32_t)((w * 3 + pad) * h);
    memset(hdr, 0, sizeof(hdr));
    hdr[0] = 'B';
    hdr[1] = 'M';
    hdr[2] = (unsigned char)(file_size);
    hdr[3] = (unsigned char)(file_size >> 8);
    hdr[4] = (unsigned char)(file_size >> 16);
    hdr[5] = (unsigned char)(file_size >> 24);
    hdr[10] = 54;
    hdr[14] = 40;
    hdr[18] = (unsigned char)(w);
    hdr[19] = (unsigned char)(w >> 8);
    hdr[20] = (unsigned char)(w >> 16);
    hdr[21] = (unsigned char)(w >> 24);
    hdr[22] = (unsigned char)(h);
    hdr[23] = (unsigned char)(h >> 8);
    hdr[24] = (unsigned char)(h >> 16);
    hdr[25] = (unsigned char)(h >> 24);
    hdr[26] = 1;
    hdr[28] = 24;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    fwrite(hdr, 1, sizeof(hdr), f);
    {
        unsigned char *line = (unsigned char *)calloc((size_t)(w * 3 + pad), 1);
        if (!line) {
            fclose(f);
            return 0;
        }
        for (row = h - 1; row >= 0; row--) {
            const uint32_t *src = px + (size_t)row * (size_t)stride;
            int x;
            for (x = 0; x < w; x++) {
                /* BMP stores blue first; the engine's pixels are r | g<<8 | b<<16. */
                line[x * 3 + 0] = (unsigned char)((src[x] >> 16) & 0xff);
                line[x * 3 + 1] = (unsigned char)((src[x] >> 8) & 0xff);
                line[x * 3 + 2] = (unsigned char)(src[x] & 0xff);
            }
            fwrite(line, 1, (size_t)(w * 3 + pad), f);
        }
        free(line);
    }
    fclose(f);
    return 1;
}
