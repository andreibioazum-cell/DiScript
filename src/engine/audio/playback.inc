static int snd_start(const char *name, int loop) {
    const char *base = snd_base_name(name);
    if (!*base)
        return 0;
    snd_lock_take();
    SndSound *s = snd_find(base);
    if (!s) {
        snd_lock_drop();
        return 0;
    }
    SndVoice *v = snd_voice_for(s);
    v->snd = s;
    v->pos = 0;
    v->loop = loop;
    v->active = 1;
    snd_lock_drop();
    return 1;
}

int snd_play(const char *name) { return snd_start(name, 0); }
int snd_loop(const char *name) { return snd_start(name, 1); }

void snd_stop(const char *name) {
    const char *base = snd_base_name(name);
    if (!*base)
        return;
    snd_lock_take();
    SndSound *s = snd_find(base);
    if (s)
        for (int i = 0; i < SND_VOICES; i++)
            if (snd_voices[i].active && snd_voices[i].snd == s)
                snd_voices[i].active = 0;
    snd_lock_drop();
}

void snd_stop_all(void) {
    snd_lock_take();
    for (int i = 0; i < SND_VOICES; i++)
        snd_voices[i].active = 0;
    snd_lock_drop();
}

int snd_playing(const char *name) {
    const char *base = snd_base_name(name);
    if (!*base)
        return 0;
    snd_lock_take();
    SndSound *s = snd_find(base);
    int playing = 0;
    if (s)
        for (int i = 0; i < SND_VOICES; i++)
            if (snd_voices[i].active && snd_voices[i].snd == s) {
                playing = 1;
                break;
            }
    snd_lock_drop();
    return playing;
}

void snd_volume(const char *name, double volume) {
    const char *base = snd_base_name(name);
    if (!*base)
        return;
    if (volume < 0)
        volume = 0;
    if (volume > 1)
        volume = 1;
    snd_lock_take();
    SndSound *s = snd_find(base);
    if (s)
        s->volume = volume;
    snd_lock_drop();
}

int snd_register_pcm(const char *name, const int16_t *stereo_samples, size_t frames, int src_rate) {
    const char *base = snd_base_name(name);
    if (!*base || !stereo_samples || frames == 0 || frames > 44100 * 15)
        return 0;
    int16_t *copy = (int16_t *)malloc(frames * 2 * sizeof(int16_t));
    if (!copy)
        return 0;
    memcpy(copy, stereo_samples, frames * 2 * sizeof(int16_t));

    snd_lock_take();
    SndSound *s = snd_find(base);
    if (!s) {
        int slot = -1;
        for (int i = 0; i < SND_MAX_SOUNDS; i++) {
            if (!snd_sounds[i].name) {
                slot = i;
                break;
            }
        }
        if (slot < 0) {
            /* Evict oldest dynamic synth slot */
            for (int i = SND_MAX_SOUNDS - 1; i >= 0; i--) {
                if (snd_sounds[i].name && strncmp(snd_sounds[i].name, "_syn", 4) == 0) {
                    slot = i;
                    break;
                }
            }
            if (slot < 0) {
                snd_lock_drop();
                free(copy);
                return 0;
            }
            for (int vi = 0; vi < SND_VOICES; vi++)
                if (snd_voices[vi].snd == &snd_sounds[slot])
                    snd_voices[vi].active = 0;
            free(snd_sounds[slot].name);
            free(snd_sounds[slot].samples);
            memset(&snd_sounds[slot], 0, sizeof(SndSound));
        }
        s = &snd_sounds[slot];
        s->name = snd_strdup(base);
        if (!s->name) {
            snd_lock_drop();
            free(copy);
            return 0;
        }
    } else {
        for (int vi = 0; vi < SND_VOICES; vi++)
            if (snd_voices[vi].snd == s)
                snd_voices[vi].active = 0;
        free(s->samples);
    }
    s->samples = copy;
    s->frames = frames;
    s->src_rate = src_rate > 0 ? src_rate : SND_RATE;
    s->volume = 1.0;
    snd_lock_drop();
    return 1;
}

static unsigned int g_synth_ring = 0;
static uint32_t g_synth_rng = 0x12345678u;

static double synth_osc(double phase, int wave_kind) {
    double p = phase - floor(phase);
    switch (wave_kind) {
    case 1: /* square */
        return p < 0.5 ? 0.85 : -0.85;
    case 2: /* saw */
        return 2.0 * p - 1.0;
    case 3: /* triangle */
        return p < 0.5 ? (4.0 * p - 1.0) : (3.0 - 4.0 * p);
    case 4: { /* white noise */
        g_synth_rng = g_synth_rng * 1664525u + 1013904223u;
        return ((double)(g_synth_rng & 0xFFFFu) / 32767.5) - 1.0;
    }
    default: /* sine */
        return sin(p * 6.283185307179586);
    }
}

int snd_synth_tone(double freq_hz, double duration_sec, const char *wave, double volume) {
    if (freq_hz < 20.0)
        freq_hz = 20.0;
    if (freq_hz > 12000.0)
        freq_hz = 12000.0;
    if (duration_sec < 0.01)
        duration_sec = 0.01;
    if (duration_sec > 3.0)
        duration_sec = 3.0;
    if (volume < 0.0)
        volume = 0.0;
    if (volume > 1.0)
        volume = 1.0;
    int wk = 0;
    if (wave) {
        if (strcmp(wave, "square") == 0) wk = 1;
        else if (strcmp(wave, "saw") == 0) wk = 2;
        else if (strcmp(wave, "triangle") == 0 || strcmp(wave, "tri") == 0) wk = 3;
        else if (strcmp(wave, "noise") == 0) wk = 4;
    }
    size_t frames = (size_t)(duration_sec * SND_RATE);
    if (frames < 64)
        frames = 64;
    int16_t *pcm = (int16_t *)malloc(frames * 2 * sizeof(int16_t));
    if (!pcm)
        return 0;
    double phase = 0.0;
    double step = freq_hz / (double)SND_RATE;
    size_t atk = frames / 16 < 220 ? frames / 16 : 220;
    size_t rel = frames / 4 < 2200 ? frames / 4 : 2200;
    for (size_t i = 0; i < frames; i++) {
        double env = 1.0;
        if (atk > 0 && i < atk)
            env = (double)i / (double)atk;
        else if (rel > 0 && i + rel > frames)
            env = (double)(frames - i) / (double)rel;
        double s = synth_osc(phase, wk) * env * volume * 18000.0;
        int16_t iv = (int16_t)(s < -32767.0 ? -32767 : (s > 32767.0 ? 32767 : s));
        pcm[i * 2] = iv;
        pcm[i * 2 + 1] = iv;
        phase += step;
    }
    char slot_name[32];
    snprintf(slot_name, sizeof(slot_name), "_syn_%u", (g_synth_ring++) & 7u);
    int ok = snd_register_pcm(slot_name, pcm, frames, SND_RATE);
    free(pcm);
    return ok ? snd_play(slot_name) : 0;
}

int snd_synth_sfx(const char *preset, double volume) {
    if (!preset)
        preset = "click";
    if (volume < 0.0) volume = 0.0;
    if (volume > 1.0) volume = 1.0;
    double dur = 0.16, f0 = 440.0, f1 = 880.0;
    int wk = 0;
    if (strcmp(preset, "jump") == 0) { dur = 0.18; f0 = 240.0; f1 = 680.0; wk = 1; }
    else if (strcmp(preset, "laser") == 0) { dur = 0.14; f0 = 960.0; f1 = 180.0; wk = 2; }
    else if (strcmp(preset, "coin") == 0) { dur = 0.22; f0 = 987.77; f1 = 1318.5; wk = 0; }
    else if (strcmp(preset, "hit") == 0) { dur = 0.12; f0 = 220.0; f1 = 65.0; wk = 4; }
    else if (strcmp(preset, "explosion") == 0) { dur = 0.36; f0 = 140.0; f1 = 35.0; wk = 4; }
    else if (strcmp(preset, "powerup") == 0) { dur = 0.30; f0 = 330.0; f1 = 990.0; wk = 3; }
    else { dur = 0.05; f0 = 720.0; f1 = 520.0; wk = 3; }

    size_t frames = (size_t)(dur * SND_RATE);
    int16_t *pcm = (int16_t *)malloc(frames * 2 * sizeof(int16_t));
    if (!pcm)
        return 0;
    double phase = 0.0;
    int is_coin = strcmp(preset, "coin") == 0;
    for (size_t i = 0; i < frames; i++) {
        double t = (double)i / (double)frames;
        double freq = is_coin ? (t < 0.35 ? f0 : f1) : (f0 + (f1 - f0) * t);
        double env = (1.0 - t) * (t < 0.04 ? t / 0.04 : 1.0);
        double s = synth_osc(phase, wk) * env * volume * 17000.0;
        int16_t iv = (int16_t)(s < -32767.0 ? -32767 : (s > 32767.0 ? 32767 : s));
        pcm[i * 2] = iv;
        pcm[i * 2 + 1] = iv;
        phase += freq / (double)SND_RATE;
    }
    char slot_name[32];
    snprintf(slot_name, sizeof(slot_name), "_syn_%u", (g_synth_ring++) & 7u);
    int ok = snd_register_pcm(slot_name, pcm, frames, SND_RATE);
    free(pcm);
    return ok ? snd_play(slot_name) : 0;
}

static int snd_clamp16(int v) {
    if (v < -32768)
        return -32768;
    if (v > 32767)
        return 32767;
    return v;
}

static void snd_frame(int16_t *out, int frames) {
    if (!out || frames <= 0)
        return;
    memset(out, 0, (size_t)frames * 2 * sizeof(int16_t));
    snd_lock_take();
    for (int vi = 0; vi < SND_VOICES; vi++) {
        SndVoice *v = &snd_voices[vi];
        if (!v->active || !v->snd || !v->snd->samples || v->snd->frames == 0)
            continue;
        SndSound *s = v->snd;
        double step = (double)s->src_rate / (double)SND_RATE;
        if (step <= 0)
            step = 1;
        double vol = s->volume;
        for (int i = 0; i < frames; i++) {
            if (v->pos >= (double)s->frames) {
                if (v->loop && s->frames > 1)
                    v->pos -= (double)s->frames;
                else {
                    v->active = 0;
                    break;
                }
            }
            size_t i0 = (size_t)v->pos;
            double frac = v->pos - (double)i0;
            size_t i1 = i0 + 1;
            if (i1 >= s->frames)
                i1 = i0;
            int l = (int)((1.0 - frac) * s->samples[i0 * 2] + frac * s->samples[i1 * 2]);
            int r = (int)((1.0 - frac) * s->samples[i0 * 2 + 1] + frac * s->samples[i1 * 2 + 1]);
            out[i * 2] = (int16_t)snd_clamp16(out[i * 2] + (int)(l * vol));
            out[i * 2 + 1] = (int16_t)snd_clamp16(out[i * 2 + 1] + (int)(r * vol));
            v->pos += step;
        }
    }
    snd_lock_drop();
}

static void snd_release_sounds(void) {
    snd_lock_take();
    for (int i = 0; i < SND_MAX_SOUNDS; i++) {
        free(snd_sounds[i].name);
        snd_sounds[i].name = NULL;
        free(snd_sounds[i].samples);
        snd_sounds[i].samples = NULL;
        snd_sounds[i].frames = 0;
        snd_sounds[i].src_rate = SND_RATE;
        snd_sounds[i].volume = 1;
    }
    for (int i = 0; i < SND_VOICES; i++) {
        snd_voices[i].active = 0;
        snd_voices[i].snd = NULL;
        snd_voices[i].pos = 0;
    }
    snd_lock_drop();
}
