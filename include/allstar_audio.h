#ifndef ALLSTAR_AUDIO_H
#define ALLSTAR_AUDIO_H

#include "allstar_types.h"

typedef enum {
    ALLSTAR_SFX_NONE = 0,
    ALLSTAR_SFX_DRIBBLE,
    ALLSTAR_SFX_SHOOT,
    ALLSTAR_SFX_SWISH,
    ALLSTAR_SFX_RIM_CLANK,
    ALLSTAR_SFX_BUZZER,
    ALLSTAR_SFX_WHISTLE,
    ALLSTAR_SFX_CHEER,
    ALLSTAR_SFX_MENU_MOVE,
    ALLSTAR_SFX_MENU_SELECT
} AllStarSfxId;

typedef enum {
    ALLSTAR_BGM_NONE = 0,
    ALLSTAR_BGM_TITLE,
    ALLSTAR_BGM_MENU,
    ALLSTAR_BGM_ONE_ON_ONE,
    ALLSTAR_BGM_THREE_POINT,
    ALLSTAR_BGM_VICTORY
} AllStarBgmId;

typedef struct {
    bool enabled;
    float volume;
    AllStarBgmId current_bgm;
} AllStarAudioEngine;

void allstar_audio_init(AllStarAudioEngine *audio);
void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx);
void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm);
void allstar_audio_stop_bgm(AllStarAudioEngine *audio);
void allstar_audio_update(AllStarAudioEngine *audio, float dt);

#endif /* ALLSTAR_AUDIO_H */
