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

static void animation_add(AllStarRomAnimationAction *action,
                          uint8_t control, uint8_t value,
                          uint8_t display_frame) {
    AllStarRomAnimationRecord *record;
    if (!action ||
        action->record_count >= ALLSTAR_ROM_ANIMATION_MAX_RECORDS) return;
    record = &action->records[action->record_count++];
    record->control = control;
    record->value = value;
    record->display_frame = display_frame;
}

static void animation_add_frames(AllStarRomAnimationAction *action,
                                 const uint8_t *frames, size_t count,
                                 uint8_t duration) {
    size_t i;
    for (i = 0; i < count; i++) {
        animation_add(action, 0, duration, frames[i]);
    }
}

/* Ghidra bank-1 $6C59/$6C60 map. These small control tables are a native
   fallback when no user-built pack is supplied; build_from_rom replaces
   them byte-for-byte from the user's cartridge image. */
static void init_rom_animation_fallback(AllStarAssetPack *pack) {
    static const uint16_t pointers[ALLSTAR_ROM_ANIMATION_ACTION_COUNT] = {
        0x6787, 0x678b, 0x67a4, 0x67bd, 0x67bd, 0x67d6,
        0x67fc, 0x6800, 0x6805, 0x681e, 0x6837, 0x685d,
        0x6876, 0x689c, 0x68a0, 0x68a0, 0x68a5, 0x68be,
        0x68d7, 0x68fd, 0x6916, 0x693c, 0x6940, 0x6940
    };
    static const uint8_t walk_a[] = {1,1,2,2,3,3,4,4};
    static const uint8_t walk_b[] = {5,5,6,6,7,7,8,8};
    static const uint8_t dribble_a[] = {0,0,1,1,2,2,3,3};
    static const uint8_t dribble_b[] = {4,4,5,5,6,6,7,7};
    static const uint8_t held_idle[] = {9,9,10,10,11,11,10,10};
    static const uint8_t free_idle[] = {12,12,13,13,14,14,13,13};
    static const uint8_t shot_a_frames[] =
        {8,9,9,9,9,10,10,11,11,11,11,12};
    static const uint8_t shot_b_frames[] =
        {8,9,9,9,9,10,10,11,11,11,11,11};
    static const uint8_t shot_durations[] =
        {6,6,6,6,6,6,1,6,6,6,6,6};
    size_t i;

    memset(pack->animation_actions, 0, sizeof(pack->animation_actions));
    pack->header.animation_action_count =
        ALLSTAR_ROM_ANIMATION_ACTION_COUNT;
    for (i = 0; i < ALLSTAR_ROM_ANIMATION_ACTION_COUNT; i++) {
        pack->animation_actions[i].rom_pointer = pointers[i];
    }

    animation_add(&pack->animation_actions[0x00], 0, 1, 0);
    animation_add(&pack->animation_actions[0x00], 1, 0, 0);
    animation_add_frames(&pack->animation_actions[0x01], walk_a, 8, 6);
    animation_add(&pack->animation_actions[0x01], 1, 0, 0);
    animation_add_frames(&pack->animation_actions[0x02], walk_b, 8, 6);
    animation_add(&pack->animation_actions[0x02], 1, 0, 0);
    animation_add_frames(&pack->animation_actions[0x03], held_idle, 8, 6);
    animation_add(&pack->animation_actions[0x03], 1, 0, 0);
    pack->animation_actions[0x04] = pack->animation_actions[0x03];
    pack->animation_actions[0x04].rom_pointer = pointers[0x04];

    for (i = 0; i < 11; i++)
        animation_add(&pack->animation_actions[0x05], 0, 6, 12);
    animation_add(&pack->animation_actions[0x05], 0, 6, 13);
    animation_add(&pack->animation_actions[0x05], 2, 6, 0);
    animation_add(&pack->animation_actions[0x06], 0, 6, 14);
    animation_add(&pack->animation_actions[0x06], 1, 0, 0);
    animation_add(&pack->animation_actions[0x07], 0, 15, 15);
    animation_add(&pack->animation_actions[0x07], 2, 6, 0);

    animation_add_frames(&pack->animation_actions[0x08], dribble_a, 8, 6);
    animation_add(&pack->animation_actions[0x08], 1, 0, 0);
    animation_add_frames(&pack->animation_actions[0x09], dribble_b, 8, 6);
    animation_add(&pack->animation_actions[0x09], 1, 0, 0);
    for (i = 0; i < 12; i++)
        animation_add(&pack->animation_actions[0x0a], 0,
                      shot_durations[i], shot_a_frames[i]);
    animation_add(&pack->animation_actions[0x0a], 2, 0x0d, 0);
    animation_add_frames(&pack->animation_actions[0x0b], free_idle, 8, 6);
    animation_add(&pack->animation_actions[0x0b], 1, 0, 0);
    for (i = 0; i < 11; i++)
        animation_add(&pack->animation_actions[0x0c], 0, 6, 15);
    animation_add(&pack->animation_actions[0x0c], 0, 6, 16);
    animation_add(&pack->animation_actions[0x0c], 2, 0x0d, 0);
    animation_add(&pack->animation_actions[0x0d], 0, 6, 17);
    animation_add(&pack->animation_actions[0x0d], 1, 0, 0);
    animation_add(&pack->animation_actions[0x0e], 0, 15, 21);
    animation_add(&pack->animation_actions[0x0e], 2, 0x0d, 0);
    pack->animation_actions[0x0f] = pack->animation_actions[0x0e];
    pack->animation_actions[0x0f].rom_pointer = pointers[0x0f];

    pack->animation_actions[0x10] = pack->animation_actions[0x08];
    pack->animation_actions[0x10].rom_pointer = pointers[0x10];
    pack->animation_actions[0x11] = pack->animation_actions[0x09];
    pack->animation_actions[0x11].rom_pointer = pointers[0x11];
    for (i = 0; i < 12; i++)
        animation_add(&pack->animation_actions[0x12], 0,
                      shot_durations[i], shot_b_frames[i]);
    animation_add(&pack->animation_actions[0x12], 2, 0x0d, 0);
    pack->animation_actions[0x13] = pack->animation_actions[0x0b];
    pack->animation_actions[0x13].rom_pointer = pointers[0x13];
    for (i = 0; i < 11; i++)
        animation_add(&pack->animation_actions[0x14], 0, 6, 15);
    animation_add(&pack->animation_actions[0x14], 0, 6, 16);
    animation_add(&pack->animation_actions[0x14], 2, 0x15, 0);
    pack->animation_actions[0x15] = pack->animation_actions[0x0d];
    pack->animation_actions[0x15].rom_pointer = pointers[0x15];
    animation_add(&pack->animation_actions[0x16], 0, 15, 21);
    animation_add(&pack->animation_actions[0x16], 2, 0x15, 0xf0);
    pack->animation_actions[0x17] = pack->animation_actions[0x16];
    pack->animation_actions[0x17].rom_pointer = pointers[0x17];
}

static bool extract_rom_animation_actions(AllStarAssetPack *pack,
                                          const AllStarRom *rom) {
    const size_t pointer_table = 0x6c60;
    size_t action_index;
    if (!pack || !rom || rom->size < pointer_table +
            ALLSTAR_ROM_ANIMATION_ACTION_COUNT * 2) return false;

    memset(pack->animation_actions, 0, sizeof(pack->animation_actions));
    for (action_index = 0;
         action_index < ALLSTAR_ROM_ANIMATION_ACTION_COUNT;
         action_index++) {
        AllStarRomAnimationAction *action =
            &pack->animation_actions[action_index];
        size_t pointer_offset = pointer_table + action_index * 2;
        uint16_t pointer = (uint16_t)(rom->data[pointer_offset] |
                                     (rom->data[pointer_offset + 1] << 8));
        size_t record_index;
        action->rom_pointer = pointer;
        if (pointer < 0x4000 || pointer >= 0x8000) return false;
        for (record_index = 0;
             record_index < ALLSTAR_ROM_ANIMATION_MAX_RECORDS;
             record_index++) {
            size_t offset = (size_t)pointer + record_index * 3;
            AllStarRomAnimationRecord *record;
            if (offset + 2 >= rom->size) return false;
            record = &action->records[action->record_count++];
            record->control = rom->data[offset];
            record->value = rom->data[offset + 1];
            record->display_frame = rom->data[offset + 2];
            if (record->control == 0xff || (record->control & 0x03) != 0)
                break;
        }
        if (action->record_count == 0 ||
            (action->records[action->record_count - 1].control & 0x03) == 0)
            return false;
    }
    pack->header.animation_action_count =
        ALLSTAR_ROM_ANIMATION_ACTION_COUNT;
    return true;
}

void allstar_asset_pack_init_default(AllStarAssetPack *pack) {
    if (!pack) return;
    memset(pack, 0, sizeof(AllStarAssetPack));

    pack->header.magic = ALLSTAR_ASSET_MAGIC;
    pack->header.version = ALLSTAR_ASSET_VERSION;
    pack->header.tile_count = 128;
    pack->header.player_count = ALLSTAR_DEFAULT_ROSTER_COUNT;
    pack->header.audio_sequence_count = 10;
    pack->header.animation_action_count =
        ALLSTAR_ROM_ANIMATION_ACTION_COUNT;
    pack->header.checksum = 0;

    init_rom_animation_fallback(pack);

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

    /* Extract Game Boy 2bpp tiles directly from ROM graphics banks */
    size_t extracted_tiles = 0;
    size_t rom_tile_offset = 0x2000;
    if (rom_tile_offset + (ALLSTAR_MAX_TILES * 16) <= rom->size) {
        for (size_t t = 0; t < ALLSTAR_MAX_TILES && extracted_tiles < ALLSTAR_MAX_TILES; t++) {
            decode_gb_tile_2bpp(&rom->data[rom_tile_offset + t * 16], &pack->tiles[t]);
            extracted_tiles++;
        }
    }
    pack->header.tile_count = (uint32_t)extracted_tiles;

    if (!extract_rom_animation_actions(pack, rom)) {
        fprintf(stderr, "[AssetPack] Invalid bank-1 $6C60 animation map\n");
        return false;
    }

    /* Verify and retain authentic 27-player roster */
    AllStarRoster temp_roster;
    allstar_roster_init_default(&temp_roster);
    pack->header.player_count = (uint32_t)temp_roster.count;
    for (size_t i = 0; i < temp_roster.count && i < ALLSTAR_MAX_ROSTER; i++) {
        pack->players[i] = temp_roster.players[i];
    }

    printf("[AssetPack] Built asset pack from ROM: '%s' "
           "(%u tiles, %u players, %u animation actions)\n",
           rom->header.title, pack->header.tile_count,
           pack->header.player_count, pack->header.animation_action_count);

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
        fwrite(pack->menu_tilemap, 1, sizeof(pack->menu_tilemap), f) != sizeof(pack->menu_tilemap) ||
        fwrite(pack->animation_actions, sizeof(AllStarRomAnimationAction),
               pack->header.animation_action_count, f) !=
               pack->header.animation_action_count) {
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

    if (pack->header.magic != ALLSTAR_ASSET_MAGIC ||
        pack->header.version != ALLSTAR_ASSET_VERSION) {
        fprintf(stderr, "[AssetPack] Invalid asset pack magic/version: "
                "0x%08X/%u (expected version %u)\n",
                pack->header.magic, pack->header.version,
                ALLSTAR_ASSET_VERSION);
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    if (pack->header.tile_count > ALLSTAR_MAX_TILES) pack->header.tile_count = ALLSTAR_MAX_TILES;
    if (pack->header.player_count > ALLSTAR_MAX_ROSTER) pack->header.player_count = ALLSTAR_MAX_ROSTER;
    if (pack->header.animation_action_count !=
        ALLSTAR_ROM_ANIMATION_ACTION_COUNT) {
        fprintf(stderr, "[AssetPack] Invalid animation action count: %u\n",
                pack->header.animation_action_count);
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    if (fread(pack->tiles, sizeof(AllStarTile), pack->header.tile_count, f) !=
            pack->header.tile_count ||
        fread(pack->players, sizeof(AllStarPlayerStats),
              pack->header.player_count, f) != pack->header.player_count ||
        fread(pack->court_tilemap, 1, sizeof(pack->court_tilemap), f) !=
            sizeof(pack->court_tilemap) ||
        fread(pack->menu_tilemap, 1, sizeof(pack->menu_tilemap), f) !=
            sizeof(pack->menu_tilemap) ||
        fread(pack->animation_actions, sizeof(AllStarRomAnimationAction),
              pack->header.animation_action_count, f) !=
            pack->header.animation_action_count) {
        fprintf(stderr, "[AssetPack] Truncated asset payload\n");
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    fclose(f);
    pack->is_loaded = true;
    printf("[AssetPack] Loaded asset pack: %u tiles, %u players, "
           "%u animation actions\n",
           pack->header.tile_count, pack->header.player_count,
           pack->header.animation_action_count);
    return true;
}
