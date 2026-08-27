#ifndef ALLSTAR_ASSET_PACK_H
#define ALLSTAR_ASSET_PACK_H

#include "allstar_types.h"
#include "allstar_rom.h"

#define ALLSTAR_ASSET_MAGIC 0x41535452 /* 'ASTR' */
#define ALLSTAR_ASSET_VERSION 18

#define ALLSTAR_MAX_TILES 512
#define ALLSTAR_MAX_ROSTER 30
#define ALLSTAR_ROM_ANIMATION_ACTION_COUNT 24
#define ALLSTAR_ROM_ANIMATION_MAX_RECORDS 13
#define ALLSTAR_PLAYER_SOURCE_TILE_COUNT 563
#define ALLSTAR_PLAYER_FRAME_COUNT 60
#define ALLSTAR_PLAYER_FRAME_TILE_COUNT 18
#define ALLSTAR_BALL_SOURCE_TILE_COUNT 42
#define ALLSTAR_BALL_OAM_PAIR_COUNT 32
#define ALLSTAR_COURT_TILE_COUNT 86
#define ALLSTAR_NET_TILE_COUNT 17
#define ALLSTAR_FREE_THROW_BG_TILE_COUNT 256
#define ALLSTAR_FREE_THROW_OBJ_TILE_COUNT 30
#define ALLSTAR_FREE_THROW_POSE_COUNT 3
#define ALLSTAR_FREE_THROW_POSE_MAP_SIZE 72
#define ALLSTAR_FREE_THROW_NET_MAP_COUNT 4
#define ALLSTAR_FREE_THROW_NET_MAP_SIZE 15
#define ALLSTAR_FREE_THROW_BALL_MAP_COUNT 3
#define ALLSTAR_FREE_THROW_BALL_MAP_SIZE 16
#define ALLSTAR_ROM_SFX_PROGRAM_COUNT 11
#define ALLSTAR_ROM_SFX_MAX_FRAMES 144
#define ALLSTAR_ROM_MUSIC_PROGRAM_COUNT 1
#define ALLSTAR_ROM_MUSIC_MAX_FRAMES 4096
#define ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART (1u << 0)
#define ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO (1u << 1)
#define ALLSTAR_ASSET_FEATURE_FREE_THROW_ART (1u << 2)
#define ALLSTAR_ASSET_FEATURE_ROM_MUSIC (1u << 3)

#define ALLSTAR_ROM_SFX_CHANNEL_1 (1u << 0)
#define ALLSTAR_ROM_SFX_CHANNEL_2 (1u << 1)
#define ALLSTAR_ROM_SFX_TRIGGER_1 (1u << 2)
#define ALLSTAR_ROM_SFX_TRIGGER_2 (1u << 3)
#define ALLSTAR_ROM_SFX_CHANNEL_4 (1u << 4)
#define ALLSTAR_ROM_SFX_TRIGGER_4 (1u << 5)

#define ALLSTAR_ROM_MUSIC_SQUARE1 (1u << 0)
#define ALLSTAR_ROM_MUSIC_TRIGGER1 (1u << 1)
#define ALLSTAR_ROM_MUSIC_SQUARE2 (1u << 2)
#define ALLSTAR_ROM_MUSIC_TRIGGER2 (1u << 3)
#define ALLSTAR_ROM_MUSIC_WAVE (1u << 4)
#define ALLSTAR_ROM_MUSIC_TRIGGER_WAVE (1u << 5)
#define ALLSTAR_ROM_MUSIC_NOISE (1u << 6)
#define ALLSTAR_ROM_MUSIC_TRIGGER_NOISE (1u << 7)

/* Bank 1 $6A8C consumes the lists selected by the pointer table at $6C60.
   Normal records use all three bytes. Loop records use only control; action
   transitions use control and value. */
typedef struct {
    uint8_t control;
    uint8_t value;
    uint8_t display_frame;
} AllStarRomAnimationRecord;

typedef struct {
    uint16_t rom_pointer;
    uint8_t record_count;
    AllStarRomAnimationRecord records[ALLSTAR_ROM_ANIMATION_MAX_RECORDS];
} AllStarRomAnimationAction;

typedef struct {
    uint8_t tile_indices[ALLSTAR_PLAYER_FRAME_TILE_COUNT];
} AllStarRomPlayerFrame;

typedef struct {
    uint8_t left_tile;
    uint8_t right_tile;
    uint8_t extra_left_tile;
    uint8_t extra_right_tile;
} AllStarRomOamPair;

/* Focused $3014 APU-program decode for gameplay commands
   $02/$04/$05/$07/$08/$09/$0A/$0C/$0D/$0E/$0F.
   Frequencies are the DMG 11-bit NR13/NR14 and NR23/NR24 values after the
   ROM's $3244 pitch modulation has been applied for that 59.7 Hz frame. */
typedef struct {
    uint16_t square1_frequency;
    uint16_t square2_frequency;
    uint8_t noise_polynomial;
    uint8_t flags;
} AllStarRomSfxFrame;

typedef struct {
    uint8_t command;
    uint8_t program_id;
    uint8_t priority_frames;
    uint8_t frame_count;
    uint8_t square1_sweep;
    uint8_t square1_duty_length;
    uint8_t square1_envelope;
    uint8_t square2_duty_length;
    uint8_t square2_envelope;
    uint8_t noise_length;
    uint8_t noise_envelope;
    uint8_t noise_control;
    uint16_t stream_pointer_1;
    uint16_t stream_pointer_2;
    uint32_t source_checksum;
    AllStarRomSfxFrame frames[ALLSTAR_ROM_SFX_MAX_FRAMES];
} AllStarRomSfxProgram;

/* Per-59.7 Hz APU state decoded from the title song's native control and
   note streams. Instrument parameters are retained per frame because songs
   switch descriptors while running. */
typedef struct {
    uint16_t square1_frequency;
    uint16_t square2_frequency;
    uint16_t wave_frequency;
    uint8_t flags;
    uint8_t square1_sweep;
    uint8_t square1_duty_length;
    uint8_t square1_envelope;
    uint8_t square2_duty_length;
    uint8_t square2_envelope;
    uint8_t wave_output_level;
    uint8_t wave_table;
    uint8_t noise_length;
    uint8_t noise_envelope;
    uint8_t noise_polynomial;
    uint8_t noise_control;
    /*
     * The NR51 ($FF25) routing the cartridge would be holding on this frame.
     * $35B6 takes each voice's pan code from bits 2-3 of its instrument
     * descriptor and ORs in $3777/$377B/$377F/$3783 -- right, left, or both --
     * while $3587 ANDs a resting voice's bits back off.  The title theme puts
     * square 1 hard right and square 2 hard left, so ignoring this collapses
     * the arrangement into the middle.
     */
    uint8_t panning;
} AllStarRomMusicFrame;

typedef struct {
    uint8_t song_id;
    uint8_t update_skip;
    uint16_t frame_count;
    uint16_t loop_frame;
    uint16_t program_pointer;
    uint16_t offset_pointer;
    uint32_t source_checksum;
    uint8_t wave_tables[16][16];
    AllStarRomMusicFrame frames[ALLSTAR_ROM_MUSIC_MAX_FRAMES];
} AllStarRomMusicProgram;

typedef struct {
    char name[24];
    char first_name[16];
    char last_name[16];
    char team[20];
    char height_str[8];
    char weight_str[8];
    char ppg_str[8];
    uint8_t number;
    uint8_t speed;
    uint8_t shooting_3pt;
    uint8_t shooting_2pt;
    uint8_t free_throw;
    uint8_t defense;
    uint8_t skin_tone; /* 0x90: Dark, 0x91: Light */
    uint8_t portrait_tile_offset;
} AllStarPlayerStats;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t tile_count;
    uint32_t player_count;
    uint32_t audio_sequence_count;
    uint32_t animation_action_count;
    uint32_t player_source_tile_count;
    uint32_t player_frame_count;
    uint32_t ball_source_tile_count;
    uint32_t ball_oam_pair_count;
    uint32_t court_tile_count;
    uint32_t net_tile_count;
    uint32_t rom_sfx_program_count;
    uint32_t rom_music_program_count;
    uint32_t feature_flags;
    uint32_t checksum;
} AllStarAssetHeader;

typedef struct {
    AllStarAssetHeader header;
    AllStarTile tiles[ALLSTAR_MAX_TILES];
    AllStarPlayerStats players[ALLSTAR_MAX_ROSTER];
    uint8_t court_tilemap[32 * 32];
    uint8_t menu_tilemap[32 * 32];
    AllStarRomAnimationAction
        animation_actions[ALLSTAR_ROM_ANIMATION_ACTION_COUNT];
    AllStarTile player_source_tiles[ALLSTAR_PLAYER_SOURCE_TILE_COUNT];
    AllStarRomPlayerFrame player_frames[ALLSTAR_PLAYER_FRAME_COUNT];
    AllStarTile ball_source_tiles[ALLSTAR_BALL_SOURCE_TILE_COUNT];
    AllStarRomOamPair ball_oam_pairs[ALLSTAR_BALL_OAM_PAIR_COUNT];
    AllStarTile court_tiles[ALLSTAR_COURT_TILE_COUNT];
    AllStarTile net_tiles[ALLSTAR_NET_TILE_COUNT];
    /* Fixed $2243/$1CBD Free Throw VRAM image. The bank-1 $640F BG range
       ($C0..$FA) is also the shared gameplay font consumed by Horse
       $0749/$7BA8/$06C0. OBJ tiles are the $1884 4x4 ball source. */
    AllStarTile free_throw_bg_tiles[ALLSTAR_FREE_THROW_BG_TILE_COUNT];
    AllStarTile free_throw_obj_tiles[ALLSTAR_FREE_THROW_OBJ_TILE_COUNT];
    /* Fixed-bank $22A9 tile copied by $2243 to OBJ tile $7F. */
    AllStarTile free_throw_reticle_tile;
    uint8_t free_throw_tilemap[32 * 32];
    uint8_t free_throw_pose_maps[ALLSTAR_FREE_THROW_POSE_COUNT]
                                [ALLSTAR_FREE_THROW_POSE_MAP_SIZE];
    uint8_t free_throw_net_maps[ALLSTAR_FREE_THROW_NET_MAP_COUNT]
                               [ALLSTAR_FREE_THROW_NET_MAP_SIZE];
    uint8_t free_throw_ball_maps[ALLSTAR_FREE_THROW_BALL_MAP_COUNT]
                                [ALLSTAR_FREE_THROW_BALL_MAP_SIZE];
    AllStarRomSfxProgram rom_sfx_programs[ALLSTAR_ROM_SFX_PROGRAM_COUNT];
    AllStarRomMusicProgram
        rom_music_programs[ALLSTAR_ROM_MUSIC_PROGRAM_COUNT];
    bool is_loaded;
} AllStarAssetPack;

bool allstar_asset_pack_build_from_rom(AllStarAssetPack *pack, const AllStarRom *rom);
bool allstar_asset_pack_save_file(const AllStarAssetPack *pack, const char *filepath);
/*
 * $35B6 routes a starting voice through NR51: bits 2-3 of its instrument
 * descriptor's first byte give a pan code, and the voice's four-entry table
 * ($3777, $377B, $377F, $3783) turns that into NR51 bits -- 0 neither side,
 * 1 right, 2 left, 3 both.  All four tables are the same pair of bits shifted
 * by the voice index, which is what this returns.
 */
uint8_t allstar_asset_pack_rom_music_voice_panning(uint8_t descriptor_flags,
                                                   int voice);

bool allstar_asset_pack_load_file(AllStarAssetPack *pack, const char *filepath);
void allstar_asset_pack_init_default(AllStarAssetPack *pack);

#endif /* ALLSTAR_ASSET_PACK_H */
