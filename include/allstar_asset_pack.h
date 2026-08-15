#ifndef ALLSTAR_ASSET_PACK_H
#define ALLSTAR_ASSET_PACK_H

#include "allstar_types.h"
#include "allstar_rom.h"

#define ALLSTAR_ASSET_MAGIC 0x41535452 /* 'ASTR' */
#define ALLSTAR_ASSET_VERSION 3

#define ALLSTAR_MAX_TILES 512
#define ALLSTAR_MAX_ROSTER 30
#define ALLSTAR_ROM_ANIMATION_ACTION_COUNT 24
#define ALLSTAR_ROM_ANIMATION_MAX_RECORDS 13

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
    bool is_loaded;
} AllStarAssetPack;

bool allstar_asset_pack_build_from_rom(AllStarAssetPack *pack, const AllStarRom *rom);
bool allstar_asset_pack_save_file(const AllStarAssetPack *pack, const char *filepath);
bool allstar_asset_pack_load_file(AllStarAssetPack *pack, const char *filepath);
void allstar_asset_pack_init_default(AllStarAssetPack *pack);

#endif /* ALLSTAR_ASSET_PACK_H */
