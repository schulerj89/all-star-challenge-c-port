#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Decode Game Boy 2bpp planar tile (16 bytes) into an 8x8 indexed tile (64 bytes, values 0..3) */
static void decode_gb_tile_2bpp(const uint8_t *src, AllStarTile *dst) {
    for (int y = 0; y < 8; y++) {
        uint8_t byte1 = src[y * 2];
        uint8_t byte2 = src[y * 2 + 1];
        for (int x = 0; x < 8; x++) {
            int bit = 7 - x;
            uint8_t bit1 = (byte1 >> bit) & 1;
            uint8_t bit2 = (byte2 >> bit) & 1;
            uint8_t color_idx = (uint8_t)((bit2 << 1) | bit1);
            dst->pixels[y * 8 + x] = color_idx;
        }
    }
}

void allstar_asset_pack_init_default(AllStarAssetPack *pack) {
    if (!pack) return;
    memset(pack, 0, sizeof(AllStarAssetPack));

    pack->header.magic = ALLSTAR_ASSET_MAGIC;
    pack->header.version = ALLSTAR_ASSET_VERSION;
    pack->header.tile_count = 128;
    pack->header.player_count = ALLSTAR_DEFAULT_ROSTER_COUNT;
    pack->header.audio_sequence_count = 10;
    pack->header.checksum = 0;

    /* Initialize default roster */
    AllStarRoster temp_roster;
    allstar_roster_init_default(&temp_roster);
    for (size_t i = 0; i < temp_roster.count && i < ALLSTAR_MAX_ROSTER; i++) {
        pack->players[i] = temp_roster.players[i];
    }

    /* Generate default 8x8 font tile patterns (ASCII 32..127) */
    for (int i = 0; i < 128; i++) {
        AllStarTile *t = &pack->tiles[i];
        memset(t->pixels, 0, sizeof(t->pixels));
        /* Basic block glyph generation for fallback font rendering */
        if (i >= 65 && i <= 90) { /* A-Z */
            for (int y = 1; y < 7; y++) {
                t->pixels[y * 8 + 1] = 3;
                t->pixels[y * 8 + 6] = 3;
            }
            for (int x = 2; x < 6; x++) {
                t->pixels[1 * 8 + x] = 3;
                t->pixels[4 * 8 + x] = 3;
            }
        }
    }

    pack->is_loaded = true;
}

bool allstar_asset_pack_build_from_rom(AllStarAssetPack *pack, const AllStarRom *rom) {
    if (!pack || !rom || !rom->is_loaded) return false;

    allstar_asset_pack_init_default(pack);

    /* Extract Game Boy 2bpp tiles directly from ROM tile banks */
    /* Typical Beam Software GB layout stores graphics in ROM offset ranges */
    size_t extracted_tiles = 0;
    size_t rom_tile_offset = 0x2000; /* Standard graphics segment */
    if (rom_tile_offset + (ALLSTAR_MAX_TILES * 16) <= rom->size) {
        for (size_t t = 0; t < ALLSTAR_MAX_TILES && extracted_tiles < ALLSTAR_MAX_TILES; t++) {
            decode_gb_tile_2bpp(&rom->data[rom_tile_offset + t * 16], &pack->tiles[t]);
            extracted_tiles++;
        }
    }
    pack->header.tile_count = (uint32_t)extracted_tiles;

    printf("[AssetPack] Built asset pack from ROM: '%s' (%u tiles, %u players)\n",
           rom->header.title, pack->header.tile_count, pack->header.player_count);

    return true;
}

bool allstar_asset_pack_save_file(const AllStarAssetPack *pack, const char *filepath) {
    if (!pack || !filepath) return false;

    FILE *f = fopen(filepath, "wb");
    if (!f) {
        fprintf(stderr, "[AssetPack] Failed to write file: %s\n", filepath);
        return false;
    }

    if (fwrite(&pack->header, sizeof(AllStarAssetHeader), 1, f) != 1 ||
        fwrite(pack->tiles, sizeof(AllStarTile), pack->header.tile_count, f) != pack->header.tile_count ||
        fwrite(pack->players, sizeof(AllStarPlayerStats), pack->header.player_count, f) != pack->header.player_count ||
        fwrite(pack->court_tilemap, 1, sizeof(pack->court_tilemap), f) != sizeof(pack->court_tilemap) ||
        fwrite(pack->menu_tilemap, 1, sizeof(pack->menu_tilemap), f) != sizeof(pack->menu_tilemap)) {
        fprintf(stderr, "[AssetPack] Failed writing asset payload\n");
        fclose(f);
        return false;
    }

    fclose(f);
    printf("[AssetPack] Saved asset pack to: %s\n", filepath);
    return true;
}

bool allstar_asset_pack_load_file(AllStarAssetPack *pack, const char *filepath) {
    if (!pack || !filepath) return false;
    memset(pack, 0, sizeof(AllStarAssetPack));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[AssetPack] Could not open asset pack: %s (using defaults)\n", filepath);
        allstar_asset_pack_init_default(pack);
        return true;
    }

    if (fread(&pack->header, sizeof(AllStarAssetHeader), 1, f) != 1) {
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    if (pack->header.magic != ALLSTAR_ASSET_MAGIC) {
        fprintf(stderr, "[AssetPack] Invalid asset pack magic: 0x%08X\n", pack->header.magic);
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    if (pack->header.tile_count > ALLSTAR_MAX_TILES) pack->header.tile_count = ALLSTAR_MAX_TILES;
    if (pack->header.player_count > ALLSTAR_MAX_ROSTER) pack->header.player_count = ALLSTAR_MAX_ROSTER;

    fread(pack->tiles, sizeof(AllStarTile), pack->header.tile_count, f);
    fread(pack->players, sizeof(AllStarPlayerStats), pack->header.player_count, f);
    fread(pack->court_tilemap, 1, sizeof(pack->court_tilemap), f);
    fread(pack->menu_tilemap, 1, sizeof(pack->menu_tilemap), f);

    fclose(f);
    pack->is_loaded = true;
    printf("[AssetPack] Loaded asset pack: %u tiles, %u players\n",
           pack->header.tile_count, pack->header.player_count);
    return true;
}
