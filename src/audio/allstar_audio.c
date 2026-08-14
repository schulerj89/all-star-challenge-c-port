#include "allstar_audio.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

static void play_wav_file(const char *filename) {
    char path[MAX_PATH];
    snprintf(path, sizeof(path), "assets\\audio\\%s", filename);
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(path, sizeof(path), "build\\assets\\audio\\%s", filename);
        f = fopen(path, "rb");
    }
    if (!f) return;

    /* Read WAV header */
    uint8_t header[44];
    if (fread(header, 1, 44, f) != 44) {
        fclose(f);
        return;
    }

    uint16_t channels = *(uint16_t*)(header + 22);
    uint32_t sample_rate = *(uint32_t*)(header + 24);
    uint16_t bits_per_sample = *(uint16_t*)(header + 34);
    uint32_t data_size = *(uint32_t*)(header + 40);

    if (data_size == 0 || data_size > 5000000) {
        fclose(f);
        return;
    }

    uint8_t *buffer = (uint8_t*)malloc(data_size);
    if (!buffer) {
        fclose(f);
        return;
    }
    fread(buffer, 1, data_size, f);
    fclose(f);

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = channels;
    wfx.nSamplesPerSec = sample_rate;
    wfx.wBitsPerSample = bits_per_sample;
    wfx.nBlockAlign = (channels * bits_per_sample) / 8;
    wfx.nAvgBytesPerSec = sample_rate * wfx.nBlockAlign;

    HWAVEOUT hWaveOut = NULL;
    if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) == MMSYSERR_NOERROR) {
        WAVEHDR header_hdr;
        memset(&header_hdr, 0, sizeof(header_hdr));
        header_hdr.lpData = (LPSTR)buffer;
        header_hdr.dwBufferLength = data_size;
        waveOutPrepareHeader(hWaveOut, &header_hdr, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header_hdr, sizeof(WAVEHDR));

        while (!(header_hdr.dwFlags & WHDR_DONE)) {
            Sleep(5);
        }
        waveOutUnprepareHeader(hWaveOut, &header_hdr, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);
    }
    free(buffer);
}

static DWORD WINAPI play_sfx_thread(LPVOID param) {
    uintptr_t sfx = (uintptr_t)param;
    switch ((AllStarSfxId)sfx) {
        case ALLSTAR_SFX_MENU_MOVE:
            play_wav_file("sfx_menu_move.wav");
            break;
        default:
            break;
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
    const char *wav_name = NULL;
    switch (bgm) {
        case ALLSTAR_BGM_TITLE:
            wav_name = "bgm_title.wav";
            break;
        case ALLSTAR_BGM_MENU:
            wav_name = "bgm_menu.wav";
            break;
        case ALLSTAR_BGM_GAMEPLAY:
            wav_name = "bgm_gameplay.wav";
            break;
        default:
            break;
    }

    if (wav_name) {
        char path1[MAX_PATH];
        char path2[MAX_PATH];
        snprintf(path1, sizeof(path1), "assets\\audio\\%s", wav_name);
        snprintf(path2, sizeof(path2), "build\\assets\\audio\\%s", wav_name);

        FILE *f = fopen(path1, "rb");
        if (f) {
            fclose(f);
            PlaySoundA(path1, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            return;
        }
        f = fopen(path2, "rb");
        if (f) {
            fclose(f);
            PlaySoundA(path2, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
            return;
        }
    } else {
        PlaySoundA(NULL, NULL, 0);
    }
#endif
}

void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
#ifdef _WIN32
    PlaySoundA(NULL, NULL, 0);
#endif
}

void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
