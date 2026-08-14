#include "allstar_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

typedef struct {
    uint8_t *data;
    uint32_t size;
    WAVEFORMATEX wfx;
    bool loaded;
} CachedWav;

static CachedWav g_bgm_cache[ALLSTAR_BGM_COUNT];
static CachedWav g_sfx_cache[ALLSTAR_SFX_COUNT];

static HANDLE g_bgm_thread = NULL;
static volatile bool g_bgm_running = false;
static volatile AllStarBgmId g_bgm_requested = ALLSTAR_BGM_NONE;
static volatile AllStarBgmId g_bgm_active = ALLSTAR_BGM_NONE;
static CRITICAL_SECTION g_audio_cs;
static bool g_audio_cs_init = false;

static bool load_wav_file(const char *filename, CachedWav *out) {
    if (!out) return false;
    memset(out, 0, sizeof(CachedWav));

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "assets\\audio\\%s", filename);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "build\\assets\\audio\\%s", filename);
        f = fopen(path, "rb");
    }
    if (!f) return false;

    uint8_t header[44];
    if (fread(header, 1, 44, f) != 44) {
        fclose(f);
        return false;
    }

    uint16_t channels = *(uint16_t*)(header + 22);
    uint32_t sample_rate = *(uint32_t*)(header + 24);
    uint16_t bits_per_sample = *(uint16_t*)(header + 34);
    uint32_t data_size = *(uint32_t*)(header + 40);

    if (data_size == 0 || data_size > 20000000) {
        fclose(f);
        return false;
    }

    out->data = (uint8_t*)malloc(data_size);
    if (!out->data) {
        fclose(f);
        return false;
    }
    fread(out->data, 1, data_size, f);
    fclose(f);

    out->size = data_size;
    out->wfx.wFormatTag = WAVE_FORMAT_PCM;
    out->wfx.nChannels = channels;
    out->wfx.nSamplesPerSec = sample_rate;
    out->wfx.wBitsPerSample = bits_per_sample;
    out->wfx.nBlockAlign = (channels * bits_per_sample) / 8;
    out->wfx.nAvgBytesPerSec = sample_rate * out->wfx.nBlockAlign;
    out->loaded = true;
    return true;
}

static DWORD WINAPI bgm_worker_thread(LPVOID param) {
    (void)param;
    HWAVEOUT hWave = NULL;
    AllStarBgmId current_playing = ALLSTAR_BGM_NONE;

    while (g_bgm_running) {
        AllStarBgmId req = g_bgm_requested;
        if (req != current_playing) {
            if (hWave) {
                waveOutReset(hWave);
                waveOutClose(hWave);
                hWave = NULL;
            }
            current_playing = req;
            g_bgm_active = req;

            if (req != ALLSTAR_BGM_NONE && g_bgm_cache[req].loaded) {
                waveOutOpen(&hWave, WAVE_MAPPER, &g_bgm_cache[req].wfx, 0, 0, CALLBACK_NULL);
            }
        }

        if (current_playing != ALLSTAR_BGM_NONE && hWave && g_bgm_cache[current_playing].loaded) {
            CachedWav *w = &g_bgm_cache[current_playing];
            WAVEHDR hdr;
            memset(&hdr, 0, sizeof(WAVEHDR));
            hdr.lpData = (LPSTR)w->data;
            hdr.dwBufferLength = w->size;
            waveOutPrepareHeader(hWave, &hdr, sizeof(WAVEHDR));
            waveOutWrite(hWave, &hdr, sizeof(WAVEHDR));

            while (g_bgm_running && g_bgm_requested == current_playing && !(hdr.dwFlags & WHDR_DONE)) {
                Sleep(10);
            }
            waveOutUnprepareHeader(hWave, &hdr, sizeof(WAVEHDR));
        } else {
            Sleep(20);
        }
    }

    if (hWave) {
        waveOutReset(hWave);
        waveOutClose(hWave);
    }
    return 0;
}

static DWORD WINAPI play_sfx_thread(LPVOID param) {
    uintptr_t sfx_id = (uintptr_t)param;
    if (sfx_id >= ALLSTAR_SFX_COUNT) return 0;
    CachedWav *w = &g_sfx_cache[sfx_id];
    if (!w->loaded) return 0;

    HWAVEOUT hWave = NULL;
    if (waveOutOpen(&hWave, WAVE_MAPPER, &w->wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        WAVEHDR hdr;
        memset(&hdr, 0, sizeof(WAVEHDR));
        hdr.lpData = (LPSTR)w->data;
        hdr.dwBufferLength = w->size;
        waveOutPrepareHeader(hWave, &hdr, sizeof(WAVEHDR));
        waveOutWrite(hWave, &hdr, sizeof(WAVEHDR));

        while (!(hdr.dwFlags & WHDR_DONE)) {
            Sleep(5);
        }
        waveOutUnprepareHeader(hWave, &hdr, sizeof(WAVEHDR));
        waveOutClose(hWave);
    }
    return 0;
}
#endif

void allstar_audio_init(AllStarAudioEngine *audio) {
    if (!audio) return;
    memset(audio, 0, sizeof(AllStarAudioEngine));
    audio->enabled = true;
    audio->volume = 1.0f;
    audio->current_bgm = ALLSTAR_BGM_NONE;

#ifdef _WIN32
    if (!g_audio_cs_init) {
        InitializeCriticalSection(&g_audio_cs);
        g_audio_cs_init = true;
    }

    /* Pre-cache audio tracks */
    load_wav_file("bgm_title.wav", &g_bgm_cache[ALLSTAR_BGM_TITLE]);
    load_wav_file("bgm_menu.wav", &g_bgm_cache[ALLSTAR_BGM_MENU]);
    load_wav_file("bgm_gameplay.wav", &g_bgm_cache[ALLSTAR_BGM_GAMEPLAY]);
    load_wav_file("sfx_menu_move.wav", &g_sfx_cache[ALLSTAR_SFX_MENU_MOVE]);

    g_bgm_running = true;
    g_bgm_requested = ALLSTAR_BGM_NONE;
    g_bgm_active = ALLSTAR_BGM_NONE;
    if (!g_bgm_thread) {
        g_bgm_thread = CreateThread(NULL, 0, bgm_worker_thread, NULL, 0, NULL);
    }
#endif
}

void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx) {
    if (!audio || !audio->enabled) return;
#ifdef _WIN32
    HANDLE hThread = CreateThread(NULL, 0, play_sfx_thread, (LPVOID)(uintptr_t)sfx, 0, NULL);
    if (hThread) CloseHandle(hThread);
#endif
}

void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm) {
    if (!audio || !audio->enabled) return;
    if (audio->current_bgm == bgm) return;
    audio->current_bgm = bgm;

#ifdef _WIN32
    g_bgm_requested = bgm;
#endif
}

void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
#ifdef _WIN32
    g_bgm_requested = ALLSTAR_BGM_NONE;
#endif
}

void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
