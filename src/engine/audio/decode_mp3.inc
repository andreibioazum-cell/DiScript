/* MP3 decoding for bundled music via minimp3 (CC0, third_party/minimp3.h).
 * The game ships a small MP3 and decodes it to PCM once at load time. */
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#endif
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3
#include "third_party/minimp3.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

static int mp3_decode(const uint8_t *data, size_t size, int16_t **out, size_t *frames, int *rate) {
    if (!data || size < 4 || !out || !frames || !rate) return 0;
    mp3dec_t dec;
    mp3dec_init(&dec);
    mp3d_sample_t pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
    int16_t *buf = NULL;
    size_t cap = 0, n = 0;
    int hz = 0, channels = 0;
    const uint8_t *p = data;
    size_t left = size;
    while (left > 0) {
        mp3dec_frame_info_t info;
        memset(&info, 0, sizeof(info));
        int samples = mp3dec_decode_frame(&dec, p, (int)left, pcm, &info);
        if (info.frame_bytes <= 0 || (size_t)info.frame_bytes > left) break;
        p += info.frame_bytes;
        left -= (size_t)info.frame_bytes;
        if (samples <= 0) continue;
        if (hz == 0) {
            hz = info.hz;
            channels = info.channels;
        }
        if (info.hz != hz || info.channels != channels || channels < 1 || channels > 2) continue;
        size_t need = n + (size_t)samples;
        if (need > SND_MAX_BYTES / 4) {
            ds_log_err("sound not loaded: MP3 too long");
            free(buf);
            return 0;
        }
        if (need > cap) {
            size_t next = cap ? cap * 2 : 4096;
            if (next < need) next = need;
            if (next > SND_MAX_BYTES / 4) next = need;
            int16_t *grown = (int16_t *)realloc(buf, next * 2 * sizeof(int16_t));
            if (!grown) {
                ds_log_err("sound not loaded: out of memory");
                free(buf);
                return 0;
            }
            buf = grown;
            cap = next;
        }
        for (int i = 0; i < samples; i++) {
            int16_t l = pcm[i * channels];
            int16_t r = channels == 2 ? pcm[i * channels + 1] : l;
            buf[(n + (size_t)i) * 2] = l;
            buf[(n + (size_t)i) * 2 + 1] = r;
        }
        n = need;
    }
    if (!buf || n == 0 || hz < 4000 || hz > 192000) {
        ds_log_err("sound not loaded: broken MP3 file");
        free(buf);
        return 0;
    }
    int16_t *exact = (int16_t *)realloc(buf, n * 2 * sizeof(int16_t));
    if (exact) buf = exact;
    *out = buf;
    *frames = n;
    *rate = hz;
    return 1;
}
