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

/* ---- procedural audio --------------------------------------------------- *
 * The language exposes tone/SFX synthesis and a global stop. Noise and music
 * generation live here rather than in the application so scripts can use them
 * on every platform. */

void snd_stop_all(void) {
    snd_lock_take();
    for (int i = 0; i < SND_VOICES; i++) {
        snd_voices[i].active = 0;
        snd_voices[i].snd = NULL;
        snd_voices[i].pos = 0;
    }
    snd_lock_drop();
}

/* Render `frames` of a waveform into a fresh sample buffer and register it
 * under `name`, replacing any previous sound with that name. */
static int snd_install(const char *name, const double *wave, size_t frames) {
    int16_t *samples = (int16_t *)malloc(frames * sizeof(int16_t));
    if (!samples)
        return 0;
    for (size_t i = 0; i < frames; i++) {
        double v = wave[i];
        if (v > 1.0)
            v = 1.0;
        if (v < -1.0)
            v = -1.0;
        samples[i] = (int16_t)(v * 32000.0);
    }
    snd_lock_take();
    SndSound *s = snd_find(name);
    if (!s) {
        for (int i = 0; i < SND_MAX_SOUNDS; i++)
            if (!snd_sounds[i].name) {
                s = &snd_sounds[i];
                break;
            }
    }
    if (!s) {
        snd_lock_drop();
        free(samples);
        return 0;
    }
    free(s->samples);
    free(s->name);
    s->name = strdup_safe(name);
    s->samples = samples;
    s->frames = frames;
    s->src_rate = SND_RATE;
    s->volume = 1.0;
    snd_lock_drop();
    return s->name != NULL;
}

static double snd_wave_at(const char *wave, double phase) {
    if (!wave)
        wave = "sine";
    if (strcmp(wave, "square") == 0)
        return (phase - floor(phase)) < 0.5 ? 0.5 : -0.5;
    if (strcmp(wave, "saw") == 0 || strcmp(wave, "sawtooth") == 0) {
        double t = phase - floor(phase);
        return t * 2.0 - 1.0;
    }
    if (strcmp(wave, "triangle") == 0) {
        double t = phase - floor(phase);
        return fabs(t * 4.0 - 2.0) - 1.0;
    }
    return sin(phase * 6.283185307179586);
}

int snd_synth_tone(double freq_hz, double duration_sec, const char *wave, double volume) {
    char name[64];
    if (freq_hz <= 0.0 || duration_sec <= 0.0 || duration_sec > 10.0)
        return 0;
    if (volume <= 0.0)
        volume = 0.25;
    size_t frames = (size_t)(duration_sec * (double)SND_RATE);
    if (frames == 0 || frames > (size_t)SND_RATE * 10)
        return 0;
    double *buf = (double *)malloc(frames * sizeof(double));
    if (!buf)
        return 0;
    for (size_t i = 0; i < frames; i++) {
        double phase = freq_hz * (double)i / (double)SND_RATE;
        /* Short attack/release so the tone does not click. */
        double t = (double)i / (double)frames;
        double env = t < 0.02 ? t / 0.02 : (t > 0.9 ? (1.0 - t) / 0.1 : 1.0);
        buf[i] = snd_wave_at(wave, phase) * volume * env;
    }
    snprintf(name, sizeof(name), "__tone_%.0f_%.0f", freq_hz, duration_sec * 1000.0);
    int ok = snd_install(name, buf, frames);
    free(buf);
    if (ok)
        snd_play(name);
    return ok;
}

int snd_synth_sfx(const char *preset, double volume) {
    /* A handful of named presets built from the same generator. */
    double freq = 440.0, dur = 0.15;
    const char *wave = "sine";
    if (preset) {
        if (strcmp(preset, "click") == 0) {
            freq = 1200.0;
            dur = 0.05;
            wave = "square";
        } else if (strcmp(preset, "blip") == 0) {
            freq = 880.0;
            dur = 0.08;
        } else if (strcmp(preset, "hit") == 0) {
            freq = 140.0;
            dur = 0.12;
            wave = "saw";
        } else if (strcmp(preset, "coin") == 0) {
            freq = 1568.0;
            dur = 0.1;
            wave = "triangle";
        } else if (strcmp(preset, "explode") == 0) {
            freq = 70.0;
            dur = 0.35;
            wave = "saw";
        }
    }
    return snd_synth_tone(freq, dur, wave, volume > 0.0 ? volume : 0.3);
}
