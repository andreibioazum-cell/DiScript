#include <string.h>

typedef struct {
    const unsigned char *in;
    int in_len;
    int pos;
    unsigned int bitbuf;
    int bitcnt;
    int err;
} ZipBits;

typedef struct {
    unsigned char *out;
    int cap;
    int len;
    int err;
} ZipOut;

static int zip_bits(ZipBits *b, int need) {
    unsigned int v = 0;
    int i;
    for (i = 0; i < need; i++) {
        if (b->bitcnt == 0) {
            if (b->pos >= b->in_len) {
                b->err = 1;
                return 0;
            }
            b->bitbuf = b->in[b->pos++];
            b->bitcnt = 8;
        }
        v |= (b->bitbuf & 1u) << i;
        b->bitbuf >>= 1;
        b->bitcnt--;
    }
    return (int)v;
}

static void zip_put(ZipOut *o, unsigned char c) {
    if (o->len >= o->cap) {
        o->err = 1;
        return;
    }
    o->out[o->len++] = c;
}

/* Canonical Huffman table: count per length + sorted symbols (puff style). */
static void zip_table(unsigned short *count, unsigned short *symbol, const unsigned char *lengths, int n) {
    int i;
    unsigned short offs[16];
    for (i = 0; i < 16; i++)
        count[i] = 0;
    for (i = 0; i < n; i++)
        count[lengths[i]]++;
    count[0] = 0;
    offs[0] = 0;
    for (i = 1; i < 16; i++)
        offs[i] = (unsigned short)(offs[i - 1] + count[i - 1]);
    for (i = 0; i < n; i++)
        if (lengths[i])
            symbol[offs[lengths[i]]++] = (unsigned short)i;
}

static int zip_decode(ZipBits *b, const unsigned short *count, const unsigned short *symbol) {
    int len, code = 0, first = 0, index = 0;
    for (len = 1; len < 16; len++) {
        code |= zip_bits(b, 1);
        if (b->err)
            return -1;
        if (code - first < count[len])
            return symbol[index + (code - first)];
        index += count[len];
        first = (first + count[len]) << 1;
        code <<= 1;
    }
    return -1;
}

static const unsigned short zip_len_base[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                                31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const unsigned char zip_len_extra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const unsigned short zip_dist_base[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                                 33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                                 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char zip_dist_extra[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static void zip_codes(ZipBits *b, ZipOut *o, const unsigned short *lcount, const unsigned short *lsym,
                      const unsigned short *dcount, const unsigned short *dsym) {
    for (;;) {
        int sym = zip_decode(b, lcount, lsym);
        if (sym < 0) {
            o->err = 1;
            return;
        }
        if (sym < 256) {
            zip_put(o, (unsigned char)sym);
        } else if (sym == 256) {
            return;
        } else {
            int len, dist, i;
            sym -= 257;
            if (sym >= 29) {
                o->err = 1;
                return;
            }
            len = zip_len_base[sym] + zip_bits(b, zip_len_extra[sym]);
            sym = zip_decode(b, dcount, dsym);
            if (sym < 0 || sym >= 30) {
                o->err = 1;
                return;
            }
            dist = zip_dist_base[sym] + zip_bits(b, zip_dist_extra[sym]);
            if (b->err || dist > o->len) {
                o->err = 1;
                return;
            }
            for (i = 0; i < len; i++)
                zip_put(o, o->out[o->len - dist]);
            if (o->err)
                return;
        }
        if (o->err || b->err)
            return;
    }
}

static int zip_inflate(const unsigned char *in, int in_len, unsigned char *out, int cap) {
    ZipBits b;
    ZipOut o;
    int last = 0;
    unsigned short lcount[16], lsym[288], dcount[16], dsym[32];
    memset(&b, 0, sizeof(b));
    memset(&o, 0, sizeof(o));
    b.in = in;
    b.in_len = in_len;
    o.out = out;
    o.cap = cap;
    while (!last && !b.err && !o.err) {
        int type;
        last = zip_bits(&b, 1);
        type = zip_bits(&b, 2);
        if (b.err)
            break;
        if (type == 0) {
            int len, nlen, i;
            b.bitbuf = 0;
            b.bitcnt = 0;
            if (b.pos + 4 > b.in_len) {
                b.err = 1;
                break;
            }
            len = in[b.pos] | (in[b.pos + 1] << 8);
            nlen = in[b.pos + 2] | (in[b.pos + 3] << 8);
            b.pos += 4;
            if ((len ^ 0xFFFF) != nlen || b.pos + len > b.in_len) {
                b.err = 1;
                break;
            }
            for (i = 0; i < len; i++)
                zip_put(&o, in[b.pos++]);
        } else if (type == 1) {
            unsigned char lens[288];
            int i;
            for (i = 0; i < 144; i++) lens[i] = 8;
            for (i = 144; i < 256; i++) lens[i] = 9;
            for (i = 256; i < 280; i++) lens[i] = 7;
            for (i = 280; i < 288; i++) lens[i] = 8;
            zip_table(lcount, lsym, lens, 288);
            for (i = 0; i < 30; i++) lens[i] = 5;
            zip_table(dcount, dsym, lens, 30);
            zip_codes(&b, &o, lcount, lsym, dcount, dsym);
        } else if (type == 2) {
            static const unsigned char order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            unsigned char lens[320];
            unsigned short ccount[16], csym[19];
            int hlit, hdist, hclen, i, n = 0;
            hlit = zip_bits(&b, 5) + 257;
            hdist = zip_bits(&b, 5) + 1;
            hclen = zip_bits(&b, 4) + 4;
            if (b.err || hlit > 286 || hdist > 30) {
                b.err = 1;
                break;
            }
            memset(lens, 0, sizeof(lens));
            for (i = 0; i < hclen; i++)
                lens[order[i]] = (unsigned char)zip_bits(&b, 3);
            zip_table(ccount, csym, lens, 19);
            n = 0;
            while (n < hlit + hdist) {
                int sym = zip_decode(&b, ccount, csym);
                int rep = 0, val = 0;
                if (sym < 0) {
                    b.err = 1;
                    break;
                }
                if (sym < 16) {
                    lens[n++] = (unsigned char)sym;
                    continue;
                }
                if (sym == 16) {
                    if (n == 0) {
                        b.err = 1;
                        break;
                    }
                    val = lens[n - 1];
                    rep = 3 + zip_bits(&b, 2);
                } else if (sym == 17) {
                    val = 0;
                    rep = 3 + zip_bits(&b, 3);
                } else {
                    val = 0;
                    rep = 11 + zip_bits(&b, 7);
                }
                if (b.err || n + rep > hlit + hdist) {
                    b.err = 1;
                    break;
                }
                while (rep-- > 0)
                    lens[n++] = (unsigned char)val;
            }
            if (b.err)
                break;
            zip_table(lcount, lsym, lens, hlit);
            zip_table(dcount, dsym, lens + hlit, hdist);
            zip_codes(&b, &o, lcount, lsym, dcount, dsym);
        } else {
            b.err = 1;
        }
    }
    if (b.err || o.err)
        return -1;
    return o.len;
}
