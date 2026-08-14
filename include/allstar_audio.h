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
    ALLSTAR_SFX_MENU_SELECT,
    ALLSTAR_SFX_COUNT
} AllStarSfxId;

typedef enum {
    ALLSTAR_BGM_NONE = 0,
    ALLSTAR_BGM_TITLE,
    ALLSTAR_BGM_MENU,
    ALLSTAR_BGM_GAMEPLAY,
    ALLSTAR_BGM_ONE_ON_ONE,
    ALLSTAR_BGM_THREE_POINT,
    ALLSTAR_BGM_VICTORY,
    ALLSTAR_BGM_COUNT
} AllStarBgmId;

typedef struct {
    bool enabled;
    float volume;
    AllStarBgmId current_bgm;
} AllStarAudioEngine;

/* Ghidra: Call_000_0002 - Audio Driver Tick & Hardware Updates */
void allstar_audio_init(AllStarAudioEngine *audio);
void allstar_audio_update(AllStarAudioEngine *audio, float dt);

/* Ghidra: Call_000_000c - Sound Channel Tone Generator */
void allstar_audio_generate_tone(int channel, float frequency_hz, float duration_sec);

/* Ghidra: Call_000_0078 / Call_000_007b - BGM Start & Stop Dispatcher */
void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm);
void allstar_audio_stop_bgm(AllStarAudioEngine *audio);

/* Ghidra: Call_000_0aa3 / Call_000_07b4 - SFX Voice Player */
void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx);

#endif /* ALLSTAR_AUDIO_H */
