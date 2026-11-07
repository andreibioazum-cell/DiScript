#include "engine/runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define SND_RATE 44100
#define SND_MAX_SOUNDS 16
#define SND_VOICES 8
#define SND_MAX_BYTES (64u << 20)
#include "engine/audio/decode_mp3.inc"

typedef struct {
    char *name;
    int16_t *samples;
    size_t frames;
    int src_rate;
    double volume;
} SndSound;

typedef struct {
    SndSound *snd;
    double pos;
    int loop;
    int active;
} SndVoice;

static SndSound snd_sounds[SND_MAX_SOUNDS];
static SndVoice snd_voices[SND_VOICES];

#include <pthread.h>
static pthread_mutex_t snd_lock = PTHREAD_MUTEX_INITIALIZER;
static void snd_lock_take(void) {
    pthread_mutex_lock(&snd_lock);
}
static void snd_lock_drop(void) {
    pthread_mutex_unlock(&snd_lock);
}

#if defined(__ANDROID__) || defined(CB_PC)
static AAssetManager *snd_amgr = NULL;
#endif

static char *snd_strdup(const char *s) {
    if (!s)
        s = "";
    size_t n = strlen(s) + 1;
    char *c = (char *)malloc(n);
    if (c)
        memcpy(c, s, n);
    return c;
}

static int snd_open_candidate(const char *path, uint8_t **out, size_t *sz) {
    if (!snd_amgr || !path || !out || !sz)
        return 0;
    AAsset *a = AAssetManager_open(snd_amgr, path, AASSET_MODE_BUFFER);
    if (!a)
        return 0;
    off_t len = AAsset_getLength(a);
    if (len <= 0 || (unsigned long)len > SND_MAX_BYTES) {
        AAsset_close(a);
        return 0;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        AAsset_close(a);
        return 0;
    }
    size_t off = 0;
    while (off < (size_t)len) {
        int nr = AAsset_read(a, buf + off, (size_t)len - off);
        if (nr <= 0)
            break;
        off += (size_t)nr;
    }
    AAsset_close(a);
    if (off != (size_t)len) {
        free(buf);
        return 0;
    }
    *out = buf;
    *sz = off;
    return 1;
}

static const char *snd_base_name(const char *name) {
    if (!name)
        return "";
    const char *n = name;
    while (strncmp(n, "./", 2) == 0) n += 2;
    if (strncmp(n, "assets/audio/", 13) == 0)
        n += 13;
    else if (strncmp(n, "audio/", 6) == 0)
        n += 6;
    else if (strncmp(n, "sounds/", 7) == 0)
        n += 7;
    const char *slash = strrchr(n, '/');
    if (slash)
        n = slash + 1;
    if (!*n || strchr(n, '\\'))
        return "";
    return n;
}

static int snd_open_by_name(const char *base, uint8_t **out, size_t *sz) {
    char path[1200];

    size_t n = strlen(base);
    int has_waw = n > 4 && strcmp(base + n - 4, ".waw") == 0;
    snprintf(path, sizeof(path), "sounds/%s", base);
    if (snd_open_candidate(path, out, sz))
        return 1;
    if (has_waw) {
        snprintf(path, sizeof(path), "sounds/%.*s.wav", (int)(n - 4), base);
        if (snd_open_candidate(path, out, sz))
            return 1;
    }
    snprintf(path, sizeof(path), "%s", base);
    if (snd_open_candidate(path, out, sz))
        return 1;
    return 0;
}

static uint32_t rd_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t rd_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int wav_decode(const uint8_t *data, size_t size, int16_t **out, size_t *frames, int *rate) {
    if (!data || size < 12)
        return 0;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
        return 0;
    int fmt_ok = 0, channels = 0, bits = 0, hz = 0;
    const uint8_t *audio = NULL;
    size_t audio_len = 0;
    size_t pos = 12;
    while (pos + 8 <= size) {
        const uint8_t *id = data + pos;
        uint32_t csz = rd_u32(data + pos + 4);
        size_t body = pos + 8;
        size_t avail = size - body;
        size_t take = (size_t)csz < avail ? (size_t)csz : avail;
        if (memcmp(id, "fmt ", 4) == 0 && take >= 16) {
            uint16_t format = rd_u16(data + body);
            channels = rd_u16(data + body + 2);
            hz = (int)rd_u32(data + body + 4);
            bits = rd_u16(data + body + 14);

            if (format == 0xFFFE && take >= 40) {
                uint16_t sub = rd_u16(data + body + 24);
                if (sub == 1)
                    format = 1;
            }
            if (format != 1) {
                ds_log_err("sound not loaded: only uncompressed PCM WAV is supported");
                return 0;
            }
            if (channels < 1 || channels > 2) {
                ds_log_err("sound not loaded: only mono or stereo WAV is supported");
                return 0;
            }
            if (bits != 8 && bits != 16 && bits != 24 && bits != 32) {
                ds_log_err("sound not loaded: WAV must be 8/16/24/32-bit PCM");
                return 0;
            }
            if (hz < 4000 || hz > 192000) {
                ds_log_err("sound not loaded: strange sample rate %d", hz);
                return 0;
            }
            fmt_ok = 1;
        } else if (memcmp(id, "data", 4) == 0) {
            audio = data + body;
            audio_len = take;
        }
        size_t next = body + ((csz + 1) & ~(uint32_t)1);
        if (next <= pos)
            break;
        pos = next;
    }
    if (!fmt_ok || !audio || audio_len == 0) {
        ds_log_err("sound not loaded: broken WAV file (no fmt/data chunk)");
        return 0;
    }
    int bps = bits / 8;
    if (audio_len / (size_t)(bps * channels) == 0) {
        ds_log_err("sound not loaded: WAV has no samples");
        return 0;
    }
    size_t n = audio_len / (size_t)(bps * channels);
    if (n > SND_MAX_BYTES / 4) {
        ds_log_err("sound not loaded: WAV too long");
        return 0;
    }
    int16_t *samples = (int16_t *)malloc(n * 2 * sizeof(int16_t));
    if (!samples) {
        ds_log_err("sound not loaded: out of memory");
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        const uint8_t *p = audio + i * (size_t)(bps * channels);
        int l = 0, r = 0;
        if (bits == 8) {
            l = ((int)p[0] - 128) << 8;
            if (channels == 2)
                r = ((int)p[1] - 128) << 8;
            else
                r = l;
        } else if (bits == 16) {
            l = (int16_t)rd_u16(p);
            if (channels == 2)
                r = (int16_t)rd_u16(p + 2);
            else
                r = l;
        } else if (bits == 24) {
            l = ((int)p[0] << 8 | (int)p[1] << 16 | (int)p[2] << 24) >> 16;
            if (channels == 2) {
                const uint8_t *q = p + 3;
                r = ((int)q[0] << 8 | (int)q[1] << 16 | (int)q[2] << 24) >> 16;
            } else
                r = l;
        } else {
            l = (int)((int32_t)rd_u32(p) >> 16);
            if (channels == 2)
                r = (int)((int32_t)rd_u32(p + 4) >> 16);
            else
                r = l;
        }
        samples[i * 2] = (int16_t)l;
        samples[i * 2 + 1] = (int16_t)r;
    }
    *out = samples;
    *frames = n;
    *rate = hz;
    return 1;
}

static SndSound *snd_find(const char *base) {
    for (int i = 0; i < SND_MAX_SOUNDS; i++)
        if (snd_sounds[i].name && strcmp(snd_sounds[i].name, base) == 0)
            return &snd_sounds[i];
    return NULL;
}

int snd_load(const char *name) {
    const char *base = snd_base_name(name);
    if (!*base) {
        ds_log_err("sound not loaded: bad name '%s'", name ? name : "(null)");
        return 0;
    }
    snd_lock_take();
    if (snd_find(base)) {
        snd_lock_drop();
        return 1;
    }
    int slot = -1;
    for (int i = 0; i < SND_MAX_SOUNDS; i++)
        if (!snd_sounds[i].name) {
            slot = i;
            break;
        }
    if (slot < 0) {
        snd_lock_drop();
        ds_log_err("sound not loaded: too many sounds (max %d)", SND_MAX_SOUNDS);
        return 0;
    }

    char *key = snd_strdup(base);
    if (!key) {
        snd_lock_drop();
        ds_log_err("sound not loaded: out of memory");
        return 0;
    }
    snd_sounds[slot].name = key;
    snd_lock_drop();
    uint8_t *data = NULL;
    size_t size = 0;
    if (!snd_open_by_name(base, &data, &size)) {
        snd_lock_take();
        free(snd_sounds[slot].name);
        snd_sounds[slot].name = NULL;
        snd_lock_drop();
        free(data);
        ds_log_err("sound not loaded: '%s' not found in assets/audio", base);
        return 0;
    }
    int16_t *samples = NULL;
    size_t frames = 0;
    int rate = 0;
    size_t base_len = strlen(base);
    int is_mp3 = base_len > 4 && strcmp(base + base_len - 4, ".mp3") == 0;
    int ok = is_mp3 ? mp3_decode(data, size, &samples, &frames, &rate)
                    : wav_decode(data, size, &samples, &frames, &rate);
    free(data);
    snd_lock_take();
    if (!ok) {
        free(snd_sounds[slot].name);
        snd_sounds[slot].name = NULL;
        snd_lock_drop();
        return 0;
    }
    snd_sounds[slot].samples = samples;
    snd_sounds[slot].frames = frames;
    snd_sounds[slot].src_rate = rate > 0 ? rate : SND_RATE;
    snd_sounds[slot].volume = 1.0;
    snd_lock_drop();
    return 1;
}

static SndVoice *snd_voice_for(SndSound *s) {
    /* Prefer an idle voice so overlapping UI clicks do not cut each other off. */
    for (int i = 0; i < SND_VOICES; i++)
        if (!snd_voices[i].active)
            return &snd_voices[i];
    /* Reuse a one-shot voice before stealing a looping track. */
    for (int i = 0; i < SND_VOICES; i++)
        if (!snd_voices[i].loop)
            return &snd_voices[i];
    (void)s;
    return &snd_voices[0];
}
