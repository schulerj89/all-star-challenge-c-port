#include "allstar_audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

#define MIX_BUFFER_SAMPLES 2048
#define MIX_CHANNELS 2
#define MIX_SAMPLE_RATE 48000

typedef struct {
    int16_t *samples;
    uint32_t sample_count; /* in 16-bit stereo frame units */
    bool loaded;
} PcmSound;

static PcmSound g_bgm[ALLSTAR_BGM_COUNT];
static PcmSound g_sfx[ALLSTAR_SFX_COUNT];

static HANDLE g_mixer_thread = NULL;
static volatile bool g_mixer_running = false;
static volatile AllStarBgmId g_requested_bgm = ALLSTAR_BGM_NONE;
static volatile AllStarBgmId g_current_bgm = ALLSTAR_BGM_NONE;
static uint32_t g_bgm_playhead = 0;

/* Active SFX voice slots */
#define MAX_SFX_VOICES 4
typedef struct {
    AllStarSfxId sfx_id;
    uint32_t playhead;
    bool active;
} SfxVoice;

static SfxVoice g_sfx_voices[MAX_SFX_VOICES];
static CRITICAL_SECTION g_audio_lock;
static bool g_lock_initialized = false;

static bool load_pcm_wav(const char *filename, PcmSound *out) {
    if (!out) return false;
    memset(out, 0, sizeof(PcmSound));

    char path[MAX_PATH];
    snprintf(path, sizeof(path), "assets\\audio\\%s", filename);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "build\\assets\\audio\\%s", filename);
        f = fopen(path, "rb");
    }
    if (!f) {
        char exe_dir[MAX_PATH];
        GetModuleFileNameA(NULL, exe_dir, MAX_PATH);
        char *last_slash = strrchr(exe_dir, '\\');
        if (last_slash) *last_slash = '\0';
        snprintf(path, sizeof(path), "%s\\assets\\audio\\%s", exe_dir, filename);
        f = fopen(path, "rb");
    }
    if (!f) return false;

    /* Read RIFF header */
    uint8_t riff[12];
    if (fread(riff, 1, 12, f) != 12 || memcmp(riff, "RIFF", 4) != 0 || memcmp(riff + 8, "WAVE", 4) != 0) {
        fclose(f);
        return false;
    }

    uint16_t channels = 2;
    uint32_t sample_rate = 48000;
    uint16_t bits_per_sample = 16;
    uint8_t *raw_buf = NULL;
    uint32_t data_size = 0;

    /* Parse RIFF chunks */
    uint8_t chunk_hdr[8];
    while (fread(chunk_hdr, 1, 8, f) == 8) {
        uint32_t chunk_size = *(uint32_t*)(chunk_hdr + 4);
        if (memcmp(chunk_hdr, "fmt ", 4) == 0) {
            uint8_t fmt_data[16];
            if (fread(fmt_data, 1, 16, f) == 16) {
                channels = *(uint16_t*)(fmt_data + 2);
                sample_rate = *(uint32_t*)(fmt_data + 4);
                bits_per_sample = *(uint16_t*)(fmt_data + 14);
                if (chunk_size > 16) fseek(f, (long)(chunk_size - 16), SEEK_CUR);
            }
        } else if (memcmp(chunk_hdr, "data", 4) == 0) {
            data_size = chunk_size;
            raw_buf = (uint8_t*)malloc(data_size);
            if (raw_buf) {
                fread(raw_buf, 1, data_size, f);
            }
            break;
        } else {
            fseek(f, (long)chunk_size, SEEK_CUR);
        }
    }
    fclose(f);

    if (!raw_buf || data_size == 0) {
        if (raw_buf) free(raw_buf);
        return false;
    }

    uint32_t bytes_per_frame = channels * (bits_per_sample / 8);
    uint32_t total_frames = data_size / bytes_per_frame;
    out->samples = (int16_t*)malloc(total_frames * MIX_CHANNELS * sizeof(int16_t));
    if (!out->samples) {
        free(raw_buf);
        return false;
    }

    int16_t *src16 = (int16_t*)raw_buf;
    for (uint32_t i = 0; i < total_frames; i++) {
        if (channels == 1) {
            out->samples[i * 2 + 0] = src16[i];
            out->samples[i * 2 + 1] = src16[i];
        } else {
            out->samples[i * 2 + 0] = src16[i * 2 + 0];
            out->samples[i * 2 + 1] = src16[i * 2 + 1];
        }
    }
    free(raw_buf);

    out->sample_count = total_frames;
    out->loaded = true;
    return true;
}

static DWORD WINAPI audio_mixer_thread(LPVOID param) {
    (void)param;
    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = MIX_CHANNELS;
    wfx.nSamplesPerSec = MIX_SAMPLE_RATE;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (MIX_CHANNELS * 16) / 8;
    wfx.nAvgBytesPerSec = MIX_SAMPLE_RATE * wfx.nBlockAlign;

    HWAVEOUT hWave = NULL;
    if (waveOutOpen(&hWave, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) {
        return 0;
    }

    #define NUM_BUFFERS 3
    WAVEHDR headers[NUM_BUFFERS];
    int16_t mix_buffers[NUM_BUFFERS][MIX_BUFFER_SAMPLES * MIX_CHANNELS];

    for (int i = 0; i < NUM_BUFFERS; i++) {
        memset(&headers[i], 0, sizeof(WAVEHDR));
        memset(mix_buffers[i], 0, sizeof(mix_buffers[i]));
        headers[i].lpData = (LPSTR)mix_buffers[i];
        headers[i].dwBufferLength = sizeof(mix_buffers[i]);
        waveOutPrepareHeader(hWave, &headers[i], sizeof(WAVEHDR));
        headers[i].dwFlags |= WHDR_DONE; /* Mark as ready for immediate refill */
    }

    int buf_idx = 0;
    while (g_mixer_running) {
        WAVEHDR *hdr = &headers[buf_idx];
        if (!(hdr->dwFlags & WHDR_DONE)) {
            Sleep(2);
            continue;
        }

        EnterCriticalSection(&g_audio_lock);

        /* Handle BGM track switch */
        AllStarBgmId req = g_requested_bgm;
        if (req != g_current_bgm) {
            g_current_bgm = req;
            g_bgm_playhead = 0;
        }

        int16_t *buf = mix_buffers[buf_idx];
        memset(buf, 0, sizeof(mix_buffers[buf_idx]));

        /* Mix BGM stream */
        if (g_current_bgm != ALLSTAR_BGM_NONE && g_bgm[g_current_bgm].loaded) {
            PcmSound *track = &g_bgm[g_current_bgm];
            for (uint32_t s = 0; s < MIX_BUFFER_SAMPLES; s++) {
                if (g_bgm_playhead >= track->sample_count) {
                    g_bgm_playhead = 0; /* Seamless infinite loop */
                }
                buf[s * 2 + 0] = track->samples[g_bgm_playhead * 2 + 0];
                buf[s * 2 + 1] = track->samples[g_bgm_playhead * 2 + 1];
                g_bgm_playhead++;
            }
        }

        /* Mix active SFX voices */
        for (int v = 0; v < MAX_SFX_VOICES; v++) {
            if (g_sfx_voices[v].active) {
                AllStarSfxId sfx_id = g_sfx_voices[v].sfx_id;
                if (sfx_id < ALLSTAR_SFX_COUNT && g_sfx[sfx_id].loaded) {
                    PcmSound *sfx = &g_sfx[sfx_id];
                    for (uint32_t s = 0; s < MIX_BUFFER_SAMPLES; s++) {
                        if (g_sfx_voices[v].playhead < sfx->sample_count) {
                            int32_t left = (int32_t)buf[s * 2 + 0] + (int32_t)sfx->samples[g_sfx_voices[v].playhead * 2 + 0];
                            int32_t right = (int32_t)buf[s * 2 + 1] + (int32_t)sfx->samples[g_sfx_voices[v].playhead * 2 + 1];

                            /* Saturation clip */
                            if (left > 32767) left = 32767;
                            else if (left < -32768) left = -32768;
                            if (right > 32767) right = 32767;
                            else if (right < -32768) right = -32768;

                            buf[s * 2 + 0] = (int16_t)left;
                            buf[s * 2 + 1] = (int16_t)right;
                            g_sfx_voices[v].playhead++;
                        } else {
                            g_sfx_voices[v].active = false;
                            break;
                        }
                    }
                } else {
                    g_sfx_voices[v].active = false;
                }
            }
        }

        LeaveCriticalSection(&g_audio_lock);

        waveOutWrite(hWave, hdr, sizeof(WAVEHDR));
        buf_idx = (buf_idx + 1) % NUM_BUFFERS;
    }

    waveOutReset(hWave);
    for (int i = 0; i < NUM_BUFFERS; i++) {
        waveOutUnprepareHeader(hWave, &headers[i], sizeof(WAVEHDR));
    }
    waveOutClose(hWave);
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
    if (!g_lock_initialized) {
        InitializeCriticalSection(&g_audio_lock);
        g_lock_initialized = true;
    }

    /* Pre-cache audio tracks into RAM */
    load_pcm_wav("bgm_title.wav", &g_bgm[ALLSTAR_BGM_TITLE]);
    load_pcm_wav("bgm_menu.wav", &g_bgm[ALLSTAR_BGM_MENU]);
    load_pcm_wav("bgm_gameplay.wav", &g_bgm[ALLSTAR_BGM_GAMEPLAY]);
    load_pcm_wav("sfx_menu_move.wav", &g_sfx[ALLSTAR_SFX_MENU_MOVE]);
    load_pcm_wav("sfx_menu_select.wav", &g_sfx[ALLSTAR_SFX_MENU_SELECT]);

    memset(g_sfx_voices, 0, sizeof(g_sfx_voices));
    g_mixer_running = true;
    g_requested_bgm = ALLSTAR_BGM_NONE;
    g_current_bgm = ALLSTAR_BGM_NONE;

    if (!g_mixer_thread) {
        g_mixer_thread = CreateThread(NULL, 0, audio_mixer_thread, NULL, 0, NULL);
    }
#endif
}

void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx) {
    if (!audio || !audio->enabled) return;
#ifdef _WIN32
    if (!g_lock_initialized) return;
    EnterCriticalSection(&g_audio_lock);
    /* Find free or oldest voice */
    int slot = -1;
    for (int i = 0; i < MAX_SFX_VOICES; i++) {
        if (!g_sfx_voices[i].active) {
            slot = i;
            break;
        }
    }
    if (slot == -1) slot = 0; /* Steal voice 0 */

    g_sfx_voices[slot].sfx_id = sfx;
    g_sfx_voices[slot].playhead = 0;
    g_sfx_voices[slot].active = true;
    LeaveCriticalSection(&g_audio_lock);
#endif
}

/* Ghidra: Call_000_000c - Sound channel hardware tone generator */
void allstar_audio_generate_tone(int channel, float frequency_hz, float duration_sec) {
    (void)channel;
    (void)frequency_hz;
    (void)duration_sec;
}

/* Ghidra: Call_000_0078 - BGM start dispatcher */
void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm) {
    if (!audio || !audio->enabled) return;
    if (audio->current_bgm == bgm) return;
    audio->current_bgm = bgm;

#ifdef _WIN32
    g_requested_bgm = bgm;
#endif
}

/* Ghidra: Call_000_007b - BGM stop routine */
void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
#ifdef _WIN32
    g_requested_bgm = ALLSTAR_BGM_NONE;
#endif
}

/* Ghidra: Call_000_0002 - Sound driver interrupt tick */
void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
