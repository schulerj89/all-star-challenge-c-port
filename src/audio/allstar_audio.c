#include "allstar_audio.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
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
    /* Trigger authentic Game Boy tone frequencies */
    switch (sfx) {
        case ALLSTAR_SFX_MENU_MOVE:
            Beep(880, 20);
            break;
        case ALLSTAR_SFX_MENU_SELECT:
            Beep(659, 30);
            Beep(880, 50);
            break;
        case ALLSTAR_SFX_DRIBBLE:
            Beep(130, 25);
            break;
        case ALLSTAR_SFX_SHOOT:
            Beep(523, 40);
            break;
        case ALLSTAR_SFX_SWISH:
            Beep(1046, 50);
            break;
        case ALLSTAR_SFX_RIM_CLANK:
            Beep(220, 60);
            break;
        case ALLSTAR_SFX_BUZZER:
            Beep(180, 350);
            break;
        default:
            break;
    }
#else
    (void)sfx;
#endif
}

void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm) {
    if (!audio || !audio->enabled) return;
    audio->current_bgm = bgm;
}

void allstar_audio_stop_bgm(AllStarAudioEngine *audio) {
    if (!audio) return;
    audio->current_bgm = ALLSTAR_BGM_NONE;
}

void allstar_audio_update(AllStarAudioEngine *audio, float dt) {
    if (!audio || !audio->enabled) return;
    (void)dt;
}
