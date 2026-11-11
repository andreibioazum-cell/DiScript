/* PC audio output thread: Windows waveOut and headless POSIX mixer sink. */
#include <pthread.h>

#define AT_FRAMES 256
static pthread_t snd_thread;
static volatile int snd_run = 0;
static volatile int snd_paused = 0;
static int snd_thread_ok = 0;

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#define SND_WIN_FRAMES 1024
#define SND_HDRS 4
static int16_t snd_pc_mixbuf[SND_WIN_FRAMES * 2];
static HWAVEOUT snd_wo = NULL;
static HANDLE snd_ev = NULL;
static WAVEHDR snd_hdr[SND_HDRS];
static char snd_hdrbuf[SND_HDRS][SND_WIN_FRAMES * 2 * sizeof(int16_t)];
static int snd_hdr_prepared[SND_HDRS];

static int snd_wave_open(void) {
    WAVEFORMATEX fmt;
    int i;
    if (snd_wo)
        return 1;
    if (!snd_ev) {
        snd_ev = CreateEventW(NULL, FALSE, TRUE, NULL);
        if (!snd_ev) {
            ds_log_err("audio: CreateEventW failed");
            return 0;
        }
    }
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 2;
    fmt.nSamplesPerSec = SND_RATE;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = (fmt.nChannels * fmt.wBitsPerSample) / 8;
    fmt.nAvgBytesPerSec = fmt.nSamplesPerSec * fmt.nBlockAlign;
    if (waveOutOpen(&snd_wo, WAVE_MAPPER, &fmt, (DWORD_PTR)snd_ev, 0, CALLBACK_EVENT) != MMSYSERR_NOERROR) {
        snd_wo = NULL;
        CloseHandle(snd_ev);
        snd_ev = NULL;
        ds_log_err("audio: waveOutOpen failed");
        return 0;
    }
    for (i = 0; i < SND_HDRS; i++) {
        memset(&snd_hdr[i], 0, sizeof(snd_hdr[i]));
        snd_hdr[i].lpData = snd_hdrbuf[i];
        snd_hdr[i].dwBufferLength = sizeof(snd_hdrbuf[i]);
        if (waveOutPrepareHeader(snd_wo, &snd_hdr[i], sizeof(WAVEHDR)) == MMSYSERR_NOERROR) {
            snd_hdr[i].dwFlags |= WHDR_DONE;
            snd_hdr_prepared[i] = 1;
        }
    }
    return 1;
}

static void snd_wave_close(void) {
    int i;
    if (snd_wo) {
        waveOutReset(snd_wo);
        for (i = 0; i < SND_HDRS; i++)
            if (snd_hdr_prepared[i])
                waveOutUnprepareHeader(snd_wo, &snd_hdr[i], sizeof(WAVEHDR));
        waveOutClose(snd_wo);
        snd_wo = NULL;
    }
    if (snd_ev) {
        CloseHandle(snd_ev);
        snd_ev = NULL;
    }
    memset(snd_hdr_prepared, 0, sizeof(snd_hdr_prepared));
}

static void *snd_out_thread(void *arg) {
    int next = 0;
    (void)arg;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    if (!snd_wave_open())
        return NULL;
    while (snd_run) {
        WAVEHDR *h;
        if (snd_paused) {
            Sleep(20);
            continue;
        }
        h = &snd_hdr[next];
        if (!(h->dwFlags & WHDR_PREPARED)) {
            next = (next + 1) % SND_HDRS;
            continue;
        }
        while (snd_run && !snd_paused && (h->dwFlags & WHDR_INQUEUE) && !(h->dwFlags & WHDR_DONE))
            WaitForSingleObject(snd_ev, 20);
        if (!snd_run)
            break;
        if (snd_paused || ((h->dwFlags & WHDR_INQUEUE) && !(h->dwFlags & WHDR_DONE)))
            continue;
        snd_frame(snd_pc_mixbuf, SND_WIN_FRAMES);
        memcpy(h->lpData, snd_pc_mixbuf, sizeof(snd_hdrbuf[0]));
        h->dwBufferLength = sizeof(snd_hdrbuf[0]);
        h->dwFlags &= ~(DWORD)WHDR_DONE;
        if (waveOutWrite(snd_wo, h, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
            ds_log_err("audio: waveOutWrite failed");
        next = (next + 1) % SND_HDRS;
    }
    snd_wave_close();
    return NULL;
}
#else
#include <time.h>
static int16_t snd_pc_mixbuf[AT_FRAMES * 2];
static void *snd_out_thread(void *arg) {
    struct timespec ts;
    (void)arg;
    ts.tv_sec = 0;
    ts.tv_nsec = (long)((double)AT_FRAMES * 1e9 / (double)SND_RATE);
    while (snd_run) {
        if (snd_paused) {
            nanosleep(&ts, NULL);
            continue;
        }
        snd_frame(snd_pc_mixbuf, AT_FRAMES);
        nanosleep(&ts, NULL);
    }
    return NULL;
}
#endif

static int snd_backend_start(void) {
    if (snd_thread_ok)
        return 1;
    snd_paused = 0;
    snd_run = 1;
    if (pthread_create(&snd_thread, NULL, snd_out_thread, NULL) != 0) {
        snd_run = 0;
        ds_log_err("audio: cannot start output thread");
        return 0;
    }
    snd_thread_ok = 1;
    return 1;
}

static void snd_backend_stop(void) {
    if (!snd_thread_ok)
        return;
    snd_run = 0;
    snd_paused = 0;
#ifdef _WIN32
    if (snd_ev)
        SetEvent(snd_ev);
#endif
    pthread_join(snd_thread, NULL);
    snd_thread_ok = 0;
}

static void snd_backend_pause(void) { snd_paused = 1; }

static void snd_backend_resume(void) { snd_paused = 0; }

void ds_sound_set_java_vm(void *vm) { (void)vm; }

int ds_sound_init(AAssetManager *assets) {
    snd_amgr = assets;
    return snd_backend_start();
}

void ds_sound_shutdown(void) {
    snd_backend_stop();
    snd_release_sounds();
}

void ds_sound_suspend(void) { snd_backend_stop(); }

void ds_sound_pause(void) { snd_backend_pause(); }

void ds_sound_resume(void) {
    if (!snd_thread_ok) {
        snd_backend_start();
        return;
    }
    snd_backend_resume();
}
