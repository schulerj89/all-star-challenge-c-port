#ifndef ALLSTAR_AUDIO_H
#define ALLSTAR_AUDIO_H

#include "allstar_types.h"
#include "allstar_asset_pack.h"

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
    ALLSTAR_SFX_SHOE_SQUEAK,
    ALLSTAR_SFX_SCORE_CHIME,
    ALLSTAR_SFX_FREE_THROW_NET,
    ALLSTAR_SFX_FREE_THROW_CONTACT,
    ALLSTAR_SFX_HORSE_LETTER,
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
    AllStarSfxId last_sfx;
    uint32_t sfx_play_count;
    bool rom_sfx_bound;
    uint32_t rom_sfx_source_checksum;
} AllStarAudioEngine;

/* Native PCM platform layer. The cartridge command/APU sequencer begins in
   the reviewed fixed-bank $3014 region and is not yet ported whole. */
void allstar_audio_init(AllStarAudioEngine *audio);
/* Synthesize focused $04/$05/$07/$08/$09/$0A/$0C/$0D/$0E/$0F commands from decoded ROM programs
   stored in a user-built asset pack. These replace native fallbacks. */
bool allstar_audio_bind_rom_sfx(AllStarAudioEngine *audio,
                                const AllStarAssetPack *pack);
bool allstar_audio_export_rom_sfx_wav(const AllStarAssetPack *pack,
                                      uint8_t command,
                                      const char *filepath);
void allstar_audio_update(AllStarAudioEngine *audio, float dt);

/* Native fallback-tone utility; not a ROM-routine mapping. */
void allstar_audio_generate_tone(int channel, float frequency_hz, float duration_sec);

/* Native PCM stream controls; not mappings for reset/vector addresses. */
void allstar_audio_play_bgm(AllStarAudioEngine *audio, AllStarBgmId bgm);
void allstar_audio_stop_bgm(AllStarAudioEngine *audio);

/* Event-level SFX dispatch. One-on-One command timing is mapped separately. */
void allstar_audio_play_sfx(AllStarAudioEngine *audio, AllStarSfxId sfx);

#endif /* ALLSTAR_AUDIO_H */
