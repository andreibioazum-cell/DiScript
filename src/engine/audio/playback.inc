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

int snd_play(const char *name) {
    return snd_start(name, 0);
}
int snd_loop(const char *name) {
    return snd_start(name, 1);
}

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
