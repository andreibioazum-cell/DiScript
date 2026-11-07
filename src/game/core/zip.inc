#include <string.h>

#define ZIP_MAX_NAME 96

typedef struct {
    const char *name;
    const unsigned char *data;
    unsigned int len;
} ZipEntry;

#include "zip_inflate.inc"

static unsigned int zip_u16(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int zip_u32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/* Locate an entry in the central directory. Returns 1 when found. */
static int zip_find(const unsigned char *zip, int zip_len, const char *name, int *method, int *csize, int *usize,
                    int *out_lho) {
    const unsigned char *cd = 0;
    int cd_len = 0, i, name_len, p;
    if (!zip || zip_len < 22)
        return 0;
    for (i = zip_len - 22; i >= 0; i--) {
        if (zip_u32(zip + i) == 0x06054b50u) {
            cd_len = (int)zip_u32(zip + i + 12);
            p = (int)zip_u32(zip + i + 16);
            if (p >= 0 && cd_len >= 0 && p + cd_len <= zip_len) {
                cd = zip + p;
                break;
            }
        }
    }
    if (!cd)
        return 0;
    name_len = (int)strlen(name);
    p = 0;
    while (p + 46 <= cd_len) {
        int nl, el, cl, lho_off, m, cs, us;
        if (zip_u32(cd + p) != 0x02014b50u)
            return 0;
        nl = zip_u16(cd + p + 28);
        el = zip_u16(cd + p + 30);
        cl = zip_u16(cd + p + 32);
        m = zip_u16(cd + p + 10);
        cs = (int)zip_u32(cd + p + 20);
        us = (int)zip_u32(cd + p + 24);
        lho_off = (int)zip_u32(cd + p + 42);
        if (nl == name_len && p + 46 + nl <= cd_len && memcmp(cd + p + 46, name, (size_t)nl) == 0) {
            if (m != 0 && m != 8)
                return 0;
            if (cs < 0 || us < 0 || lho_off < 0 || lho_off + 30 > zip_len)
                return 0;
            if (method)
                *method = m;
            if (csize)
                *csize = cs;
            if (usize)
                *usize = us;
            if (out_lho)
                *out_lho = lho_off;
            return 1;
        }
        p += 46 + nl + el + cl;
    }
    return 0;
}

/* Extract one entry into `out`. Returns the byte count, or -1. */
static int zip_extract(const unsigned char *zip, int zip_len, const char *name, unsigned char *out, int cap) {
    int method = 0, csize = 0, usize = 0, lho = 0, nl, el, start;
    if (!zip_find(zip, zip_len, name, &method, &csize, &usize, &lho))
        return -1;
    if (usize > cap)
        return -1;
    nl = zip_u16(zip + lho + 26);
    el = zip_u16(zip + lho + 28);
    start = lho + 30 + nl + el;
    if (start < 0 || start + csize > zip_len)
        return -1;
    if (method == 0) {
        if (csize != usize)
            return -1;
        memcpy(out, zip + start, (size_t)usize);
        return usize;
    }
    return zip_inflate(zip + start, csize, out, cap);
}

/* Copy entry names (newline separated) into `dst`. Returns the number of names. */
static int zip_list(const unsigned char *zip, int zip_len, char *dst, int cap) {
    const unsigned char *cd = 0;
    int cd_len = 0, i, p, n = 0, used = 0;
    if (!zip || zip_len < 22)
        return 0;
    for (i = zip_len - 22; i >= 0; i--) {
        if (zip_u32(zip + i) == 0x06054b50u) {
            cd_len = (int)zip_u32(zip + i + 12);
            p = (int)zip_u32(zip + i + 16);
            if (p >= 0 && cd_len >= 0 && p + cd_len <= zip_len) {
                cd = zip + p;
                break;
            }
        }
    }
    if (!cd)
        return 0;
    p = 0;
    while (p + 46 <= cd_len) {
        int nl, el, cl, j;
        if (zip_u32(cd + p) != 0x02014b50u)
            break;
        nl = zip_u16(cd + p + 28);
        el = zip_u16(cd + p + 30);
        cl = zip_u16(cd + p + 32);
        if (nl <= 0 || nl >= ZIP_MAX_NAME || p + 46 + nl > cd_len)
            break;
        if (used + nl + 2 >= cap)
            break;
        for (j = 0; j < nl; j++)
            dst[used++] = (char)cd[p + 46 + j];
        dst[used++] = '\n';
        n++;
        p += 46 + nl + el + cl;
    }
    if (used < cap)
        dst[used] = 0;
    return n;
}

static unsigned int zip_crc32(const unsigned char *data, int len) {
    unsigned int crc = RGB(255,255,255);
    int i, k;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (k = 0; k < 8; k++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc ^ RGB(255,255,255);
}

/* Build a stored (uncompressed) archive. Returns the byte count or -1. */
static int zip_build(const ZipEntry *entries, int count, unsigned char *out, int cap) {
    int i, p = 0, cd_start, cd_len;
    for (i = 0; i < count; i++) {
        int nl = (int)strlen(entries[i].name);
        int len = (int)entries[i].len;
        if (nl <= 0 || nl >= ZIP_MAX_NAME || len < 0)
            return -1;
        if (p + 30 + nl + len > cap)
            return -1;
        memset(out + p, 0, 30);
        out[p + 0] = 0x50;
        out[p + 1] = 0x4b;
        out[p + 2] = 0x03;
        out[p + 3] = 0x04;
        out[p + 4] = 20;
        {
            unsigned int crc = zip_crc32(entries[i].data, len);
            out[p + 14] = (unsigned char)(crc & 0xFF);
            out[p + 15] = (unsigned char)((crc >> 8) & 0xFF);
            out[p + 16] = (unsigned char)((crc >> 16) & 0xFF);
            out[p + 17] = (unsigned char)((crc >> 24) & 0xFF);
            out[p + 18] = (unsigned char)(len & 0xFF);
            out[p + 19] = (unsigned char)((len >> 8) & 0xFF);
            out[p + 20] = (unsigned char)((len >> 16) & 0xFF);
            out[p + 21] = (unsigned char)((len >> 24) & 0xFF);
            out[p + 22] = out[p + 18];
            out[p + 23] = out[p + 19];
            out[p + 24] = out[p + 20];
            out[p + 25] = out[p + 21];
        }
        out[p + 26] = (unsigned char)(nl & 0xFF);
        out[p + 27] = (unsigned char)((nl >> 8) & 0xFF);
        out[p + 28] = 0;
        out[p + 29] = 0;
        memcpy(out + p + 30, entries[i].name, (size_t)nl);
        if (len > 0)
            memcpy(out + p + 30 + nl, entries[i].data, (size_t)len);
        p += 30 + nl + len;
    }
    cd_start = p;
    for (i = 0; i < count; i++) {
        int nl = (int)strlen(entries[i].name);
        int len = (int)entries[i].len;
        int lho = 0, j;
        if (p + 46 + nl > cap)
            return -1;
        for (j = 0; j < i; j++)
            lho += 30 + (int)strlen(entries[j].name) + (int)entries[j].len;
        memset(out + p, 0, 46);
        out[p + 0] = 0x50;
        out[p + 1] = 0x4b;
        out[p + 2] = 0x01;
        out[p + 3] = 0x02;
        out[p + 4] = 20;
        out[p + 5] = 20;
        {
            unsigned int crc = zip_crc32(entries[i].data, len);
            out[p + 16] = (unsigned char)(crc & 0xFF);
            out[p + 17] = (unsigned char)((crc >> 8) & 0xFF);
            out[p + 18] = (unsigned char)((crc >> 16) & 0xFF);
            out[p + 19] = (unsigned char)((crc >> 24) & 0xFF);
        }
        out[p + 10] = 0;
        out[p + 12] = 0;
        out[p + 14] = 0;
        out[p + 28] = (unsigned char)(nl & 0xFF);
        out[p + 29] = (unsigned char)((nl >> 8) & 0xFF);
        out[p + 20] = (unsigned char)(len & 0xFF);
        out[p + 21] = (unsigned char)((len >> 8) & 0xFF);
        out[p + 22] = (unsigned char)((len >> 16) & 0xFF);
        out[p + 23] = (unsigned char)((len >> 24) & 0xFF);
        out[p + 24] = out[p + 20];
        out[p + 25] = out[p + 21];
        out[p + 26] = out[p + 22];
        out[p + 27] = out[p + 23];
        out[p + 42] = (unsigned char)(lho & 0xFF);
        out[p + 43] = (unsigned char)((lho >> 8) & 0xFF);
        out[p + 44] = (unsigned char)((lho >> 16) & 0xFF);
        out[p + 45] = (unsigned char)((lho >> 24) & 0xFF);
        memcpy(out + p + 46, entries[i].name, (size_t)nl);
        p += 46 + nl;
    }
    cd_len = p - cd_start;
    if (p + 22 > cap)
        return -1;
    memset(out + p, 0, 22);
    out[p + 0] = 0x50;
    out[p + 1] = 0x4b;
    out[p + 2] = 0x05;
    out[p + 3] = 0x06;
    out[p + 8] = (unsigned char)(count & 0xFF);
    out[p + 9] = (unsigned char)((count >> 8) & 0xFF);
    out[p + 10] = out[p + 8];
    out[p + 11] = out[p + 9];
    out[p + 12] = (unsigned char)(cd_len & 0xFF);
    out[p + 13] = (unsigned char)((cd_len >> 8) & 0xFF);
    out[p + 14] = (unsigned char)((cd_len >> 16) & 0xFF);
    out[p + 15] = (unsigned char)((cd_len >> 24) & 0xFF);
    out[p + 16] = (unsigned char)(cd_start & 0xFF);
    out[p + 17] = (unsigned char)((cd_start >> 8) & 0xFF);
    out[p + 18] = (unsigned char)((cd_start >> 16) & 0xFF);
    out[p + 19] = (unsigned char)((cd_start >> 24) & 0xFF);
    p += 22;
    return p;
}
