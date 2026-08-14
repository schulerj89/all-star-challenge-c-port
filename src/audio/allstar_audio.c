#include "allstar_audio.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>

static DWORD WINAPI play_sfx_thread(LPVOID param) {
    uintptr_t sfx = (uintptr_t)param;
    switch ((AllStarSfxId)sfx) {
        case ALLSTAR_SFX_MENU_MOVE:
            Beep(880, 20);
            break;
        case ALLSTAR_SFX_MENU_SELECT:
            Beep(659, 25);
            Beep(987, 45);
            break;
        case ALLSTAR_SFX_DRIBBLE:
            Beep(130, 20);
            break;
        case ALLSTAR_SFX_SHOOT:
            Beep(523, 30);
            break;
        case ALLSTAR_SFX_SWISH:
            Beep(1046, 40);
            break;
        case ALLSTAR_SFX_RIM_CLANK:
            Beep(220, 50);
            break;
        case ALLSTAR_SFX_BUZZER:
            Beep(180, 300);
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
