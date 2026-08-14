#ifndef ALLSTAR_ASSET_PACK_H
#define ALLSTAR_ASSET_PACK_H

#include "allstar_types.h"
#include "allstar_rom.h"

#define ALLSTAR_ASSET_MAGIC 0x41535452 /* 'ASTR' */
#define ALLSTAR_ASSET_VERSION 1

#define ALLSTAR_MAX_TILES 512
#define ALLSTAR_MAX_ROSTER 30

typedef struct {
    char name[20];
    char team[20];
    uint8_t number;
    uint8_t speed;
    uint8_t shooting_3pt;
    uint8_t shooting_2pt;
    uint8_t free_throw;
    uint8_t defense;
    uint8_t portrait_tile_offset;
} AllStarPlayerStats;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t tile_count;
    uint32_t player_count;
    uint32_t audio_sequence_count;
    uint32_t checksum;
} AllStarAssetHeader;

typedef struct {
    AllStarAssetHeader header;
    AllStarTile tiles[ALLSTAR_MAX_TILES];
    AllStarPlayerStats players[ALLSTAR_MAX_ROSTER];
    uint8_t court_tilemap[32 * 32];
    uint8_t menu_tilemap[32 * 32];
    bool is_loaded;
} AllStarAssetPack;

bool allstar_asset_pack_build_from_rom(AllStarAssetPack *pack, const AllStarRom *rom);
bool allstar_asset_pack_save_file(const AllStarAssetPack *pack, const char *filepath);
bool allstar_asset_pack_load_file(AllStarAssetPack *pack, const char *filepath);
void allstar_asset_pack_init_default(AllStarAssetPack *pack);

#endif /* ALLSTAR_ASSET_PACK_H */
