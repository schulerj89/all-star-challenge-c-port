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

/* Fixed bank $050F: the first byte is the escape marker.  Literals copy
   directly; marker,count,value emits value count times; marker,0 ends. */
static bool decode_rom_rle_050f(const AllStarRom *rom,
                                size_t start, size_t end,
                                uint8_t *output, size_t output_size) {
    uint8_t marker;
    size_t source;
    size_t written = 0;
    if (!rom || !output || start >= rom->size || end >= rom->size ||
        start >= end) return false;
    marker = rom->data[start];
    source = start + 1;
    while (source <= end) {
        uint8_t value = rom->data[source++];
        if (value != marker) {
            if (written >= output_size) return false;
            output[written++] = value;
            continue;
        }
        if (source > end) return false;
        {
            uint8_t count = rom->data[source++];
            size_t i;
            if (count == 0) {
                return written == output_size && source == end + 1;
            }
            if (source > end || written + count > output_size) return false;
            value = rom->data[source++];
            for (i = 0; i < count; i++) output[written++] = value;
        }
    }
    return false;
}

static void decode_tile_bytes(const uint8_t *bytes, size_t tile_count,
                              AllStarTile *tiles) {
    size_t tile;
    for (tile = 0; tile < tile_count; tile++) {
        decode_gb_tile_2bpp(bytes + tile * 16, &tiles[tile]);
    }
}

/* Ghidra path $0B9A->$1FFA/$2219 and $04B1/$050F plus per-frame
   $100F->$2933/$293D->$2945/$2A2B and bank-1 $6945/$69F5.  Bank-3 CPU
   addresses are translated to file offsets by adding $8000. */
static bool extract_one_on_one_art(AllStarAssetPack *pack,
                                   const AllStarRom *rom) {
    uint8_t player_bytes[ALLSTAR_PLAYER_SOURCE_TILE_COUNT * 16];
    uint8_t ball_bytes[ALLSTAR_BALL_SOURCE_TILE_COUNT * 16];
    uint8_t court_bytes[ALLSTAR_COURT_TILE_COUNT * 16];
    uint8_t net_bytes[ALLSTAR_NET_TILE_COUNT * 16];
    uint8_t court_map[640];
    static const size_t frame_starts[3] = {0x4000, 0x4120, 0x42ac};
    static const size_t frame_counts[3] = {16, 22, 22};
    static const size_t frame_offsets[3] = {0, 16, 38};
    size_t family;
    size_t frame;
    size_t pair;

    if (!pack || !rom || rom->size < 0x10000) return false;
    memcpy(player_bytes, rom->data + 0x44b8, 147 * 16);
    if (!decode_rom_rle_050f(
            rom, 0x4de8, 0x582a, player_bytes + 147 * 16, 211 * 16) ||
        !decode_rom_rle_050f(
            rom, 0x582b, 0x62a5, player_bytes + 358 * 16, 205 * 16) ||
        !decode_rom_rle_050f(
            rom, 0x62a6, 0x640e, ball_bytes, sizeof(ball_bytes)) ||
        /* One-on-One $1FFA->$2021 and $2219 select bank 3 $793F->$9600
           for the 17 net/HUD tiles whose signed BG IDs are $60..$70.
           $0B9A then calls $04B1 with A=1, selecting $7A23->$9000 and
           $7E48->$9800 for the 86-tile court and 640-byte map. */
        !decode_rom_rle_050f(
            rom, 0xf93f, 0xfa22, net_bytes, sizeof(net_bytes)) ||
        !decode_rom_rle_050f(
            rom, 0xfa23, 0xfe47, court_bytes, sizeof(court_bytes)) ||
        !decode_rom_rle_050f(
            rom, 0xfe48, 0xff68, court_map, sizeof(court_map))) {
        return false;
    }

    decode_tile_bytes(player_bytes, ALLSTAR_PLAYER_SOURCE_TILE_COUNT,
                      pack->player_source_tiles);
    decode_tile_bytes(ball_bytes, ALLSTAR_BALL_SOURCE_TILE_COUNT,
                      pack->ball_source_tiles);
    decode_tile_bytes(court_bytes, ALLSTAR_COURT_TILE_COUNT,
                      pack->court_tiles);
    decode_tile_bytes(net_bytes, ALLSTAR_NET_TILE_COUNT,
                      pack->net_tiles);

    for (family = 0; family < 3; family++) {
        for (frame = 0; frame < frame_counts[family]; frame++) {
            size_t source = frame_starts[family] +
                frame * ALLSTAR_PLAYER_FRAME_TILE_COUNT;
            memcpy(pack->player_frames[frame_offsets[family] + frame].tile_indices,
                   rom->data + source, ALLSTAR_PLAYER_FRAME_TILE_COUNT);
        }
    }
    for (frame = 0; frame < ALLSTAR_PLAYER_FRAME_COUNT; frame++) {
        static const uint16_t family_tile_count[3] = {147, 211, 205};
        size_t group = frame < 16 ? 0 : (frame < 38 ? 1 : 2);
        size_t i;
        for (i = 0; i < ALLSTAR_PLAYER_FRAME_TILE_COUNT; i++) {
            uint8_t index = pack->player_frames[frame].tile_indices[i];
            if (index >= family_tile_count[group]) return false;
        }
    }

    for (pair = 0; pair < ALLSTAR_BALL_OAM_PAIR_COUNT; pair++) {
        const uint8_t *source = rom->data + 0x4438 + pair * 4;
        AllStarRomOamPair *target = &pack->ball_oam_pairs[pair];
        target->left_tile = source[0];
        target->right_tile = source[1];
        target->extra_left_tile = source[2];
        target->extra_right_tile = source[3];
        if (target->left_tile < 0x24 || target->right_tile < 0x24 ||
            ((target->left_tile - 0x24) >> 1) >=
                ALLSTAR_BALL_SOURCE_TILE_COUNT ||
            ((target->right_tile - 0x24) >> 1) >=
                ALLSTAR_BALL_SOURCE_TILE_COUNT) {
            return false;
        }
    }
    memset(pack->court_tilemap, 0, sizeof(pack->court_tilemap));
    memcpy(pack->court_tilemap, court_map, sizeof(court_map));

    pack->header.player_source_tile_count =
        ALLSTAR_PLAYER_SOURCE_TILE_COUNT;
    pack->header.player_frame_count = ALLSTAR_PLAYER_FRAME_COUNT;
    pack->header.ball_source_tile_count = ALLSTAR_BALL_SOURCE_TILE_COUNT;
    pack->header.ball_oam_pair_count = ALLSTAR_BALL_OAM_PAIR_COUNT;
    pack->header.court_tile_count = ALLSTAR_COURT_TILE_COUNT;
    pack->header.net_tile_count = ALLSTAR_NET_TILE_COUNT;
    pack->header.feature_flags |= ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART;
    return true;
}

/* Free Throw fixed-bank $0C8E->$2243/$1CBD.  $2243 assembles two separate
   VRAM banks: bank 1 $640F supplies signed BG IDs $C0..$FA, bank 3 $708E
   supplies BG IDs $00..$A2, and bank 3 $6EF1 supplies OBJ IDs $00..$1D.
   It also copies fixed-bank $22A9 directly to OBJ tile $7F for the aiming
   reticle. $1828/$1858 and $1884 use the raw map tables copied below. */
static bool extract_free_throw_art(AllStarAssetPack *pack,
                                   const AllStarRom *rom) {
    uint8_t bg_bytes[ALLSTAR_FREE_THROW_BG_TILE_COUNT * 16];
    uint8_t obj_bytes[ALLSTAR_FREE_THROW_OBJ_TILE_COUNT * 16];
    size_t i;
    static const size_t pose_offsets[ALLSTAR_FREE_THROW_POSE_COUNT] = {
        0x6661, 0x66a9, 0x66f1
    };
    static const size_t net_offsets[ALLSTAR_FREE_THROW_NET_MAP_COUNT] = {
        0x6739, 0x6748, 0x6757, 0x6766
    };
    static const size_t ball_offsets[ALLSTAR_FREE_THROW_BALL_MAP_COUNT] = {
        0xffcd, 0xffdd, 0xffed
    };

    if (!pack || !rom || rom->size < 0x10000) return false;
    memset(bg_bytes, 0, sizeof(bg_bytes));
    memset(obj_bytes, 0, sizeof(obj_bytes));
    if (!decode_rom_rle_050f(rom, 0x640f, 0x6660,
            bg_bytes + 0xc0 * 16, 59 * 16) ||
        !decode_rom_rle_050f(rom, 0xf08e, 0xf93e,
            bg_bytes, 163 * 16) ||
        !decode_rom_rle_050f(rom, 0xeef1, 0xf08d,
            obj_bytes, sizeof(obj_bytes)) ||
        !decode_rom_rle_050f(rom, 0xff69, 0xffcc,
            pack->free_throw_tilemap,
            sizeof(pack->free_throw_tilemap))) {
        return false;
    }
    decode_tile_bytes(bg_bytes, ALLSTAR_FREE_THROW_BG_TILE_COUNT,
                      pack->free_throw_bg_tiles);
    decode_tile_bytes(obj_bytes, ALLSTAR_FREE_THROW_OBJ_TILE_COUNT,
                      pack->free_throw_obj_tiles);
    decode_gb_tile_2bpp(rom->data + 0x22a9,
                        &pack->free_throw_reticle_tile);
    for (i = 0; i < ALLSTAR_FREE_THROW_POSE_COUNT; i++)
        memcpy(pack->free_throw_pose_maps[i], rom->data + pose_offsets[i],
               ALLSTAR_FREE_THROW_POSE_MAP_SIZE);
    for (i = 0; i < ALLSTAR_FREE_THROW_NET_MAP_COUNT; i++)
        memcpy(pack->free_throw_net_maps[i], rom->data + net_offsets[i],
               ALLSTAR_FREE_THROW_NET_MAP_SIZE);
    for (i = 0; i < ALLSTAR_FREE_THROW_BALL_MAP_COUNT; i++)
        memcpy(pack->free_throw_ball_maps[i], rom->data + ball_offsets[i],
               ALLSTAR_FREE_THROW_BALL_MAP_SIZE);
    pack->header.feature_flags |= ALLSTAR_ASSET_FEATURE_FREE_THROW_ART;
    return true;
}

static uint16_t rom_word(const AllStarRom *rom, size_t offset) {
    return (uint16_t)(rom->data[offset] | (rom->data[offset + 1] << 8));
}

static uint32_t fnv1a_bytes(const uint8_t *bytes, size_t count) {
    uint32_t hash = 2166136261u;
    size_t i;
    for (i = 0; i < count; i++) {
        hash ^= bytes[i];
        hash *= 16777619u;
    }
    return hash;
}

/* Fixed $3014->$32A9->$3327/$3334->$347B consumes the command table at
   $2FB0, program pointers at $3849, instrument records at $3888, duration
   table $312B, frequency tables $3159/$31C6, and pitch cycle $3244.
   This intentionally decodes the focused square/noise programs used by
   fouls, score, rim contact, dribble-ground contact, movement, roster
   navigation, and roster accept. */
static bool decode_rom_square_stream(const AllStarRom *rom,
                                     size_t stream,
                                     int channel,
                                     AllStarRomSfxProgram *program,
                                     size_t *next_stream) {
    size_t cursor = stream;
    size_t output_frame = 0;
    uint8_t instrument;
    size_t descriptor;
    if (!rom || !program || !next_stream || stream >= rom->size) return false;
    instrument = rom->data[cursor++];
    if ((instrument & 0x80) == 0) return false;
    descriptor = 0x3888u + (size_t)(instrument & 0x7f) * 8u;
    if (descriptor + 7 >= rom->size) return false;

    if (channel == 1) {
        program->square1_sweep = rom->data[descriptor + 2];
        program->square1_duty_length = rom->data[descriptor + 3];
        program->square1_envelope = rom->data[descriptor + 4];
    } else {
        program->square2_duty_length = rom->data[descriptor + 3];
        program->square2_envelope = rom->data[descriptor + 4];
    }

    while (cursor < rom->size && rom->data[cursor] != 0xff) {
        uint8_t note;
        uint8_t effective_note;
        uint8_t duration_code;
        uint8_t duration;
        uint16_t base_frequency;
        size_t local_frame;
        if (cursor + 1 >= rom->size) return false;
        note = rom->data[cursor++];
        duration_code = rom->data[cursor++];
        /* $347B adds the instrument descriptor's signed/byte pitch base
           from +$01 before indexing the two frequency-table halves.  The
           command-$0F roster-cycle descriptor $8F contributes $0A, so its
           stream note $47 resolves as table entry $51 (frequency $07B1). */
        effective_note = (uint8_t)(note + rom->data[descriptor + 1]);
        if ((size_t)effective_note + 0x31c6u >= rom->size ||
            (size_t)effective_note + 0x3159u >= rom->size ||
            (size_t)duration_code + 0x312bu >= rom->size) return false;
        duration = rom->data[0x312b + duration_code];
        base_frequency = (uint16_t)(rom->data[0x31c6 + effective_note] |
            ((uint16_t)rom->data[0x3159 + effective_note] << 8));
        if (duration == 0 || output_frame + duration >
                ALLSTAR_ROM_SFX_MAX_FRAMES) return false;
        for (local_frame = 0; local_frame < duration; local_frame++) {
            AllStarRomSfxFrame *frame =
                &program->frames[output_frame + local_frame];
            int modulation = 0;
            uint16_t frequency;
            if ((rom->data[descriptor + 7] & 0x80) != 0) {
                modulation = (int8_t)rom->data[0x3244 + (local_frame & 7u)];
            }
            frequency = (uint16_t)(base_frequency + modulation) & 0x07ffu;
            if (channel == 1) {
                frame->square1_frequency = frequency;
                frame->flags |= ALLSTAR_ROM_SFX_CHANNEL_1;
                if (local_frame == 0) frame->flags |= ALLSTAR_ROM_SFX_TRIGGER_1;
            } else {
                frame->square2_frequency = frequency;
                frame->flags |= ALLSTAR_ROM_SFX_CHANNEL_2;
                if (local_frame == 0) frame->flags |= ALLSTAR_ROM_SFX_TRIGGER_2;
            }
        }
        output_frame += duration;
    }
    if (cursor >= rom->size || rom->data[cursor++] != 0xff) return false;
    if (output_frame > program->frame_count)
        program->frame_count = (uint8_t)output_frame;
    *next_stream = cursor;
    return true;
}

/* Program $0B, selected by rim command $09, is a DMG noise-channel stream.
   $3327 installs NR41/NR42 from descriptor bytes +3/+4, masks NR43's
   trigger marker to seven bits, and writes descriptor +6 to NR44. */
static bool decode_rom_noise_stream(const AllStarRom *rom,
                                    size_t stream,
                                    AllStarRomSfxProgram *program,
                                    size_t *next_stream) {
    size_t cursor = stream;
    size_t output_frame = 0;
    uint8_t instrument;
    size_t descriptor;
    if (!rom || !program || !next_stream || stream >= rom->size) return false;
    instrument = rom->data[cursor++];
    if ((instrument & 0x80) == 0) return false;
    descriptor = 0x3888u + (size_t)(instrument & 0x7f) * 8u;
    if (descriptor + 7 >= rom->size) return false;
    program->noise_length = rom->data[descriptor + 3];
    program->noise_envelope = rom->data[descriptor + 4];
    program->noise_control = rom->data[descriptor + 6];

    while (cursor < rom->size && rom->data[cursor] != 0xff) {
        uint8_t note;
        uint8_t duration_code;
        uint8_t duration;
        uint8_t polynomial;
        size_t local_frame;
        if (cursor + 1 >= rom->size) return false;
        note = rom->data[cursor++];
        duration_code = rom->data[cursor++];
        if ((size_t)duration_code + 0x312bu >= rom->size) return false;
        duration = rom->data[0x312b + duration_code];
        if (duration == 0 || output_frame + duration >
                ALLSTAR_ROM_SFX_MAX_FRAMES) return false;
        /* Free Throw net command $08 treats its note byte as successive
           NR43 clock-shift values ($10,$20,$30,$40). Other decoded noise
           streams use the descriptor's fixed polynomial, as $09 does. */
        polynomial = program->command == 0x08
            ? (uint8_t)((note - 1u) << 4)
            : (uint8_t)(rom->data[descriptor + 5] & 0x7f);
        for (local_frame = 0; local_frame < duration; local_frame++) {
            AllStarRomSfxFrame *frame =
                &program->frames[output_frame + local_frame];
            frame->noise_polynomial = polynomial;
            frame->flags |= ALLSTAR_ROM_SFX_CHANNEL_4;
            if (local_frame == 0) frame->flags |= ALLSTAR_ROM_SFX_TRIGGER_4;
        }
        output_frame += duration;
    }
    if (cursor >= rom->size || rom->data[cursor++] != 0xff) return false;
    program->frame_count = (uint8_t)output_frame;
    *next_stream = cursor;
    return true;
}

static bool extract_gameplay_audio(AllStarAssetPack *pack,
                                   const AllStarRom *rom) {
    static const uint8_t commands[ALLSTAR_ROM_SFX_PROGRAM_COUNT] =
        {0x0d, 0x05, 0x0c, 0x0f, 0x0e, 0x09, 0x04, 0x08, 0x0a, 0x07, 0x02};
    size_t i;
    if (!pack || !rom || rom->size <= 0x3fa5) return false;
    memset(pack->rom_sfx_programs, 0, sizeof(pack->rom_sfx_programs));
    for (i = 0; i < ALLSTAR_ROM_SFX_PROGRAM_COUNT; i++) {
        AllStarRomSfxProgram *program = &pack->rom_sfx_programs[i];
        uint8_t command = commands[i];
        uint16_t mapping = rom_word(rom, 0x2fb0 + command * 2u);
        uint8_t program_id = (uint8_t)(mapping & 0xff);
        uint16_t stream = rom_word(rom, 0x3849 + program_id * 2u);
        size_t next_stream;
        program->command = command;
        program->program_id = program_id;
        program->priority_frames = (uint8_t)(mapping >> 8);
        program->stream_pointer_1 = stream;
        program->source_checksum = fnv1a_bytes(
            rom->data + 0x2fb0, 0x3fa6 - 0x2fb0);
        if (command == 0x09 || command == 0x08) {
            if (!decode_rom_noise_stream(
                    rom, stream, program, &next_stream)) return false;
        } else if (!decode_rom_square_stream(
                       rom, stream, command == 0x0c ? 2 : 1,
                       program, &next_stream)) return false;
        if (command == 0x05 || command == 0x04 || command == 0x02) {
            program->stream_pointer_2 = (uint16_t)next_stream;
            if (!decode_rom_square_stream(
                    rom, next_stream, 2, program, &next_stream)) return false;
        }
    }
    if (pack->rom_sfx_programs[0].command != 0x0d ||
        pack->rom_sfx_programs[0].program_id != 0x11 ||
        pack->rom_sfx_programs[0].priority_frames != 0x14 ||
        pack->rom_sfx_programs[0].stream_pointer_1 != 0x3fa2 ||
        pack->rom_sfx_programs[0].frame_count != 3 ||
        pack->rom_sfx_programs[0].frames[0].square1_frequency != 0x07ba ||
        pack->rom_sfx_programs[0].frames[1].square1_frequency != 0x07bb ||
        pack->rom_sfx_programs[0].frames[2].square1_frequency != 0x07bc ||
        pack->rom_sfx_programs[1].command != 0x05 ||
        pack->rom_sfx_programs[1].program_id != 0x0c ||
        pack->rom_sfx_programs[1].priority_frames != 0x64 ||
        pack->rom_sfx_programs[1].stream_pointer_1 != 0x3ef6 ||
        pack->rom_sfx_programs[1].stream_pointer_2 != 0x3f00 ||
        pack->rom_sfx_programs[1].frame_count != 72 ||
        pack->rom_sfx_programs[1].frames[0].square1_frequency != 0x060b ||
        pack->rom_sfx_programs[1].frames[0].square2_frequency != 0x0563 ||
        pack->rom_sfx_programs[1].frames[16].square1_frequency != 0x0672 ||
        pack->rom_sfx_programs[1].frames[16].square2_frequency != 0x060b ||
        pack->rom_sfx_programs[1].frames[24].square1_frequency != 0x06b2 ||
        pack->rom_sfx_programs[1].frames[24].square2_frequency != 0x0672 ||
        pack->rom_sfx_programs[1].frames[71].square1_frequency != 0x06b1 ||
        pack->rom_sfx_programs[1].frames[71].square2_frequency != 0x0671 ||
        pack->rom_sfx_programs[2].command != 0x0c ||
        pack->rom_sfx_programs[2].program_id != 0x02 ||
        pack->rom_sfx_programs[2].priority_frames != 0x13 ||
        pack->rom_sfx_programs[2].stream_pointer_1 != 0x3d7f ||
        pack->rom_sfx_programs[2].frame_count != 6 ||
        pack->rom_sfx_programs[2].square2_duty_length != 0x7a ||
        pack->rom_sfx_programs[2].square2_envelope != 0xf1 ||
        pack->rom_sfx_programs[2].frames[0].square2_frequency != 0x0000 ||
        pack->rom_sfx_programs[3].command != 0x0f ||
        pack->rom_sfx_programs[3].program_id != 0x07 ||
        pack->rom_sfx_programs[3].priority_frames != 0x19 ||
        pack->rom_sfx_programs[3].stream_pointer_1 != 0x3ebc ||
        pack->rom_sfx_programs[3].frame_count != 24 ||
        pack->rom_sfx_programs[3].square1_sweep != 0x08 ||
        pack->rom_sfx_programs[3].square1_duty_length != 0x88 ||
        pack->rom_sfx_programs[3].square1_envelope != 0xf1 ||
        pack->rom_sfx_programs[3].frames[0].square1_frequency != 0x07b1 ||
        pack->rom_sfx_programs[4].command != 0x0e ||
        pack->rom_sfx_programs[4].program_id != 0x12 ||
        pack->rom_sfx_programs[4].priority_frames != 0x32 ||
        pack->rom_sfx_programs[4].stream_pointer_1 != 0x3fa6 ||
        pack->rom_sfx_programs[4].frame_count != 48 ||
        pack->rom_sfx_programs[4].square1_sweep != 0x80 ||
        pack->rom_sfx_programs[4].square1_duty_length != 0xba ||
        pack->rom_sfx_programs[4].square1_envelope != 0xf2 ||
        pack->rom_sfx_programs[4].frames[0].square1_frequency != 0x0783 ||
        pack->rom_sfx_programs[4].frames[6].square1_frequency != 0x0791 ||
        pack->rom_sfx_programs[4].frames[12].square1_frequency != 0x079d ||
        pack->rom_sfx_programs[4].frames[24].square1_frequency != 0x07ad ||
        pack->rom_sfx_programs[5].command != 0x09 ||
        pack->rom_sfx_programs[5].program_id != 0x0b ||
        pack->rom_sfx_programs[5].priority_frames != 0x23 ||
        pack->rom_sfx_programs[5].stream_pointer_1 != 0x3ef2 ||
        pack->rom_sfx_programs[5].frame_count != 24 ||
        pack->rom_sfx_programs[5].noise_length != 0xeb ||
        pack->rom_sfx_programs[5].noise_envelope != 0xf2 ||
        pack->rom_sfx_programs[5].noise_control != 0xbf ||
        pack->rom_sfx_programs[5].frames[0].noise_polynomial != 0x5a ||
        (pack->rom_sfx_programs[5].frames[0].flags &
            (ALLSTAR_ROM_SFX_CHANNEL_4 | ALLSTAR_ROM_SFX_TRIGGER_4)) !=
            (ALLSTAR_ROM_SFX_CHANNEL_4 | ALLSTAR_ROM_SFX_TRIGGER_4) ||
        pack->rom_sfx_programs[6].command != 0x04 ||
        pack->rom_sfx_programs[6].program_id != 0x0a ||
        pack->rom_sfx_programs[6].priority_frames != 0x1e ||
        pack->rom_sfx_programs[6].stream_pointer_1 != 0x3ed4 ||
        pack->rom_sfx_programs[6].stream_pointer_2 != 0x3ee0 ||
        pack->rom_sfx_programs[6].frame_count != 30 ||
        pack->rom_sfx_programs[6].square1_sweep != 0x08 ||
        pack->rom_sfx_programs[6].square1_duty_length != 0x08 ||
        pack->rom_sfx_programs[6].square1_envelope != 0xa2 ||
        pack->rom_sfx_programs[6].square2_duty_length != 0x48 ||
        pack->rom_sfx_programs[6].square2_envelope != 0xa2 ||
        pack->rom_sfx_programs[6].frames[0].square1_frequency != 0x07c1 ||
        pack->rom_sfx_programs[6].frames[0].square2_frequency != 0x07be ||
        pack->rom_sfx_programs[7].command != 0x08 ||
        pack->rom_sfx_programs[7].program_id != 0x05 ||
        pack->rom_sfx_programs[7].priority_frames != 0x23 ||
        pack->rom_sfx_programs[7].stream_pointer_1 != 0x3eac ||
        pack->rom_sfx_programs[7].frame_count != 57 ||
        pack->rom_sfx_programs[7].noise_length != 0xc6 ||
        pack->rom_sfx_programs[7].noise_envelope != 0xf1 ||
        pack->rom_sfx_programs[7].noise_control != 0xa0 ||
        pack->rom_sfx_programs[7].frames[0].noise_polynomial != 0x10 ||
        pack->rom_sfx_programs[8].command != 0x0a ||
        pack->rom_sfx_programs[8].program_id != 0x0d ||
        pack->rom_sfx_programs[8].priority_frames != 0x1c ||
        pack->rom_sfx_programs[8].stream_pointer_1 != 0x3f0a ||
        pack->rom_sfx_programs[8].frame_count != 12 ||
        pack->rom_sfx_programs[8].square1_sweep != 0xff ||
        pack->rom_sfx_programs[8].square1_duty_length != 0x7f ||
        pack->rom_sfx_programs[8].square1_envelope != 0xf1 ||
        pack->rom_sfx_programs[8].frames[0].square1_frequency != 0 ||
        pack->rom_sfx_programs[9].command != 0x07 ||
        pack->rom_sfx_programs[9].program_id != 0x06 ||
        pack->rom_sfx_programs[9].priority_frames != 0x2a ||
        pack->rom_sfx_programs[9].stream_pointer_1 != 0x3eb6 ||
        pack->rom_sfx_programs[9].frame_count != 42 ||
        pack->rom_sfx_programs[9].square1_sweep != 0x88 ||
        pack->rom_sfx_programs[9].square1_duty_length != 0x40 ||
        pack->rom_sfx_programs[9].square1_envelope != 0xf2 ||
        pack->rom_sfx_programs[9].frames[0].square1_frequency != 0x0783 ||
        pack->rom_sfx_programs[9].frames[6].square1_frequency != 0x079d ||
        pack->rom_sfx_programs[10].command != 0x02 ||
        pack->rom_sfx_programs[10].program_id != 0x08 ||
        pack->rom_sfx_programs[10].priority_frames != 0xaa ||
        pack->rom_sfx_programs[10].stream_pointer_1 != 0x3ec0 ||
        pack->rom_sfx_programs[10].stream_pointer_2 != 0x3ec4 ||
        pack->rom_sfx_programs[10].frame_count != 144 ||
        pack->rom_sfx_programs[10].square1_sweep != 0x88 ||
        pack->rom_sfx_programs[10].square1_duty_length != 0x00 ||
        pack->rom_sfx_programs[10].square1_envelope != 0xff ||
        pack->rom_sfx_programs[10].square2_duty_length != 0x3f ||
        pack->rom_sfx_programs[10].square2_envelope != 0x6f ||
        pack->rom_sfx_programs[10].frames[0].square1_frequency != 0x065b ||
        pack->rom_sfx_programs[10].frames[0].square2_frequency != 0x0641)
        return false;
    pack->header.audio_sequence_count = ALLSTAR_ROM_SFX_PROGRAM_COUNT;
    pack->header.rom_sfx_program_count = ALLSTAR_ROM_SFX_PROGRAM_COUNT;
    pack->header.feature_flags |= ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO;
    return true;
}

typedef struct {
    uint16_t control_base;
    uint16_t control_position;
    uint16_t stream_base;
    uint16_t stream_position;
    uint16_t descriptor;
    uint8_t duration;
    uint8_t note;
    uint8_t previous_tie;
    uint8_t loop_target;
    uint8_t loop_count;
    uint8_t modulation_phase;
    uint8_t active;
    uint8_t triggered;
} RomMusicTrackState;

typedef struct {
    RomMusicTrackState tracks[4];
    uint8_t countdown;
} RomMusicSequencerState;

static bool music_state_equal(const RomMusicSequencerState *left,
                              const RomMusicSequencerState *right) {
    size_t channel;
    if (!left || !right || left->countdown != right->countdown) return false;
    for (channel = 0; channel < 4; channel++) {
        const RomMusicTrackState *a = &left->tracks[channel];
        const RomMusicTrackState *b = &right->tracks[channel];
        if (a->control_base != b->control_base ||
            a->control_position != b->control_position ||
            a->stream_base != b->stream_base ||
            a->stream_position != b->stream_position ||
            a->descriptor != b->descriptor ||
            a->duration != b->duration || a->note != b->note ||
            a->previous_tie != b->previous_tie ||
            a->loop_target != b->loop_target ||
            a->loop_count != b->loop_count ||
            a->modulation_phase != b->modulation_phase ||
            a->active != b->active || a->triggered != b->triggered)
            return false;
    }
    return true;
}

static bool music_start_stream(const AllStarRom *rom,
                               RomMusicTrackState *track,
                               uint16_t program_pointer,
                               uint16_t offset_pointer,
                               uint8_t stream_index) {
    size_t offset_address =
        (size_t)offset_pointer + (size_t)stream_index * 2u;
    uint16_t relative;
    if (!rom || !track || offset_address + 1 >= rom->size) return false;
    relative = rom_word(rom, offset_address);
    if ((size_t)program_pointer + relative >= rom->size) return false;
    track->stream_base = (uint16_t)(program_pointer + relative);
    track->stream_position = 0;
    return true;
}

static bool music_next_note(const AllStarRom *rom,
                            RomMusicTrackState *track,
                            uint16_t program_pointer,
                            uint16_t offset_pointer) {
    unsigned guard = 0;
    if (!rom || !track) return false;
    while (track->active && guard++ < 1024u) {
        if (track->stream_base != 0) {
            size_t cursor = (size_t)track->stream_base +
                track->stream_position;
            uint8_t value;
            uint8_t duration_flags;
            uint8_t duration;
            if (cursor >= rom->size) return false;
            value = rom->data[cursor];
            if (value == 0xff) {
                track->stream_base = 0;
                track->stream_position = 0;
                continue;
            }
            if ((value & 0x80) != 0) {
                size_t descriptor =
                    0x3888u + (size_t)(value & 0x7f) * 8u;
                if (descriptor + 7 >= rom->size) return false;
                track->descriptor = (uint16_t)descriptor;
                track->stream_position++;
                continue;
            }
            if (cursor + 1 >= rom->size || track->descriptor == 0)
                return false;
            duration_flags = rom->data[cursor + 1];
            if (0x312bu + (duration_flags & 0x1fu) >= rom->size)
                return false;
            duration = rom->data[0x312b + (duration_flags & 0x1f)];
            if (duration == 0) return false;
            track->stream_position += 2;
            track->note = value == 0 ? 0 : (uint8_t)(
                value + rom->data[track->descriptor + 1]);
            track->triggered =
                value != 0 && track->previous_tie == 0;
            if (track->triggered) track->modulation_phase = 0;
            track->previous_tie = duration_flags & 0x40;
            track->duration = (uint8_t)(duration - 1u);
            return true;
        } else {
            size_t cursor = (size_t)track->control_base +
                track->control_position;
            uint8_t command;
            if (cursor >= rom->size) return false;
            command = rom->data[cursor];
            if (command == 0) {
                track->active = 0;
                track->note = 0;
                return true;
            }
            if (command == 1) {
                if (cursor + 1 >= rom->size) return false;
                track->control_position += 2;
                if (!music_start_stream(
                        rom, track, program_pointer, offset_pointer,
                        rom->data[cursor + 1])) return false;
                continue;
            }
            if (command == 2) {
                if (cursor + 1 >= rom->size) return false;
                track->control_position = rom->data[cursor + 1];
                continue;
            }
            if (command == 3) {
                if (cursor + 2 >= rom->size) return false;
                if (track->loop_count == 0) {
                    track->loop_target = rom->data[cursor + 1];
                    track->loop_count = rom->data[cursor + 2];
                    track->control_position = track->loop_target;
                } else if (track->loop_count == 1) {
                    track->loop_count = 0;
                    track->control_position += 3;
                } else {
                    track->loop_count--;
                    track->control_position = track->loop_target;
                }
                continue;
            }
            return false;
        }
    }
    return guard < 1024u;
}

static bool music_update_track(const AllStarRom *rom,
                               RomMusicTrackState *track,
                               uint16_t program_pointer,
                               uint16_t offset_pointer) {
    if (!track || !track->active) return true;
    if (track->duration != 0) {
        track->duration--;
        if (track->descriptor != 0 &&
            (rom->data[track->descriptor + 7] & 0x80) != 0)
            track->modulation_phase =
                (uint8_t)((track->modulation_phase + 1u) & 0x0fu);
        return true;
    }
    return music_next_note(
        rom, track, program_pointer, offset_pointer);
}

static uint16_t music_track_frequency(const AllStarRom *rom,
                                      const RomMusicTrackState *track) {
    int frequency;
    if (!rom || !track || track->note == 0 || track->descriptor == 0 ||
        0x31c6u + track->note >= rom->size ||
        0x3159u + track->note >= rom->size) return 0;
    frequency = rom->data[0x31c6 + track->note] |
        ((int)rom->data[0x3159 + track->note] << 8);
    if ((rom->data[track->descriptor + 7] & 0x80) != 0)
        frequency += (int8_t)rom->data[
            0x3244 + track->modulation_phase];
    return (uint16_t)frequency & 0x07ffu;
}

/*
 * $35B6 routes a starting voice: it takes bits 2-3 of the instrument
 * descriptor's first byte, clears that voice's two NR51 bits, and ORs in the
 * entry from the voice's four-entry table.  All four tables agree on the
 * meaning of the code -- 0 neither side, 1 right, 2 left, 3 both -- so one
 * shifted mask covers them.  $3587 ANDs the same two bits back off when the
 * voice rests, which is why a resting voice contributes nothing here.
 */
uint8_t allstar_asset_pack_rom_music_voice_panning(uint8_t descriptor_flags,
                                                   int voice) {
    uint8_t code = (uint8_t)((descriptor_flags >> 2) & 0x03u);
    if (voice < 0 || voice > 3) return 0;
    /* $3777[code] for voice 0; the other tables are the same bits shifted. */
    return (uint8_t)(((code & 0x01u) | ((code & 0x02u) << 3)) << voice);
}

static uint8_t music_voice_panning(const AllStarRom *rom,
                                   const RomMusicTrackState *track,
                                   int voice) {
    if (!rom || !track || track->descriptor == 0) return 0;
    return allstar_asset_pack_rom_music_voice_panning(
        rom->data[track->descriptor], voice);
}

static bool music_snapshot(const AllStarRom *rom,
                           const RomMusicSequencerState *state,
                           AllStarRomMusicFrame *frame) {
    const RomMusicTrackState *square1 = &state->tracks[0];
    const RomMusicTrackState *square2 = &state->tracks[1];
    const RomMusicTrackState *wave = &state->tracks[2];
    const RomMusicTrackState *noise = &state->tracks[3];
    if (!rom || !state || !frame) return false;
    memset(frame, 0, sizeof(*frame));
    if (square1->active && square1->note != 0) {
        frame->flags |= ALLSTAR_ROM_MUSIC_SQUARE1;
        if (square1->triggered)
            frame->flags |= ALLSTAR_ROM_MUSIC_TRIGGER1;
        frame->square1_frequency = music_track_frequency(rom, square1);
        frame->square1_sweep = rom->data[square1->descriptor + 2];
        frame->square1_duty_length = rom->data[square1->descriptor + 3];
        frame->square1_envelope = rom->data[square1->descriptor + 4];
    }
    if (square2->active && square2->note != 0) {
        frame->flags |= ALLSTAR_ROM_MUSIC_SQUARE2;
        if (square2->triggered)
            frame->flags |= ALLSTAR_ROM_MUSIC_TRIGGER2;
        frame->square2_frequency = music_track_frequency(rom, square2);
        frame->square2_duty_length = rom->data[square2->descriptor + 3];
        frame->square2_envelope = rom->data[square2->descriptor + 4];
    }
    if (wave->active && wave->note != 0) {
        frame->flags |= ALLSTAR_ROM_MUSIC_WAVE;
        if (wave->triggered)
            frame->flags |= ALLSTAR_ROM_MUSIC_TRIGGER_WAVE;
        frame->wave_frequency = music_track_frequency(rom, wave);
        frame->wave_table = rom->data[wave->descriptor + 2] & 0x0f;
        frame->wave_output_level = rom->data[wave->descriptor + 4];
    }
    if (square1->active && square1->note != 0)
        frame->panning |= music_voice_panning(rom, square1, 0);
    if (square2->active && square2->note != 0)
        frame->panning |= music_voice_panning(rom, square2, 1);
    if (wave->active && wave->note != 0)
        frame->panning |= music_voice_panning(rom, wave, 2);
    if (noise->active && noise->note != 0)
        frame->panning |= music_voice_panning(rom, noise, 3);
    if (noise->active && noise->note != 0) {
        size_t polynomial = 0x3233u + noise->note;
        if (polynomial >= rom->size) return false;
        frame->flags |= ALLSTAR_ROM_MUSIC_NOISE;
        if (noise->triggered)
            frame->flags |= ALLSTAR_ROM_MUSIC_TRIGGER_NOISE;
        frame->noise_length = rom->data[noise->descriptor + 3];
        frame->noise_envelope = rom->data[noise->descriptor + 4];
        frame->noise_polynomial = (uint8_t)(
            (rom->data[noise->descriptor + 5] & 0x0f) |
            rom->data[polynomial]);
        frame->noise_control = rom->data[noise->descriptor + 6];
    }
    return true;
}

static bool extract_title_music(AllStarAssetPack *pack,
                                const AllStarRom *rom) {
    static const uint16_t channel_tables[4] = {
        0x378b, 0x37b1, 0x37d7, 0x37fd
    };
    AllStarRomMusicProgram *program;
    RomMusicSequencerState state;
    RomMusicSequencerState *history;
    size_t frame;
    size_t channel;
    size_t loop_frame = ALLSTAR_ROM_MUSIC_MAX_FRAMES;
    if (!pack || !rom || rom->size <= 0x3fc1) return false;
    program = &pack->rom_music_programs[0];
    memset(program, 0, sizeof(*program));
    memset(&state, 0, sizeof(state));
    program->song_id = 1;
    program->program_pointer = rom_word(rom, 0x3849 + 2);
    program->offset_pointer = rom_word(rom, 0x3823 + 2);
    program->update_skip = rom->data[0x386f + 1];
    program->source_checksum = fnv1a_bytes(
        rom->data + 0x3111, 0x3fc2 - 0x3111);
    memcpy(program->wave_tables, rom->data + 0x3fb2,
           sizeof(program->wave_tables));
    state.countdown = 1;
    for (channel = 0; channel < 4; channel++) {
        RomMusicTrackState *track = &state.tracks[channel];
        track->control_base = rom_word(
            rom, channel_tables[channel] + 2);
        track->active = 1;
        if (!music_next_note(rom, track, program->program_pointer,
                             program->offset_pointer)) return false;
    }
    history = (RomMusicSequencerState *)calloc(
        ALLSTAR_ROM_MUSIC_MAX_FRAMES + 1u, sizeof(*history));
    if (!history) return false;
    history[0] = state;
    if (!music_snapshot(rom, &state, &program->frames[0])) {
        free(history);
        return false;
    }
    for (frame = 1; frame < ALLSTAR_ROM_MUSIC_MAX_FRAMES; frame++) {
        for (channel = 0; channel < 4; channel++)
            state.tracks[channel].triggered = 0;
        if (program->update_skip == 0) {
            for (channel = 0; channel < 4; channel++) {
                if (!music_update_track(
                        rom, &state.tracks[channel],
                        program->program_pointer, program->offset_pointer)) {
                    free(history);
                    return false;
                }
            }
        } else {
            state.countdown--;
            if (state.countdown == 0) {
                state.countdown = program->update_skip;
            } else {
                for (channel = 0; channel < 4; channel++) {
                    if (!music_update_track(
                            rom, &state.tracks[channel],
                            program->program_pointer,
                            program->offset_pointer)) {
                        free(history);
                        return false;
                    }
                }
            }
        }
        if (!music_snapshot(rom, &state, &program->frames[frame])) {
            free(history);
            return false;
        }
        for (loop_frame = 0; loop_frame < frame; loop_frame++) {
            if (music_state_equal(&state, &history[loop_frame])) break;
        }
        if (loop_frame < frame) {
            program->frame_count = (uint16_t)frame;
            program->loop_frame = (uint16_t)loop_frame;
            break;
        }
        history[frame] = state;
    }
    free(history);
    if (program->song_id != 1 || program->program_pointer != 0x3b25 ||
        program->offset_pointer != 0x3aab || program->update_skip != 7 ||
        program->frame_count != 3360 || program->loop_frame != 1568 ||
        program->source_checksum != 0x7ae8b9d0u ||
        program->frames[0].square1_frequency != 0x069e ||
        program->frames[0].square2_frequency != 0x0627 ||
        program->frames[0].wave_frequency != 0x053b ||
        program->frames[0].panning != 0xed ||
        (program->frames[0].flags &
            (ALLSTAR_ROM_MUSIC_SQUARE1 | ALLSTAR_ROM_MUSIC_TRIGGER1 |
             ALLSTAR_ROM_MUSIC_SQUARE2 | ALLSTAR_ROM_MUSIC_TRIGGER2 |
             ALLSTAR_ROM_MUSIC_WAVE | ALLSTAR_ROM_MUSIC_TRIGGER_WAVE |
             ALLSTAR_ROM_MUSIC_NOISE | ALLSTAR_ROM_MUSIC_TRIGGER_NOISE)) !=
            0xff) return false;
    pack->header.rom_music_program_count =
        ALLSTAR_ROM_MUSIC_PROGRAM_COUNT;
    pack->header.feature_flags |= ALLSTAR_ASSET_FEATURE_ROM_MUSIC;
    return true;
}

static bool validate_gameplay_audio(const AllStarAssetPack *pack) {
    const AllStarRomSfxProgram *movement;
    const AllStarRomSfxProgram *score;
    const AllStarRomSfxProgram *dribble;
    const AllStarRomSfxProgram *navigation;
    const AllStarRomSfxProgram *confirm;
    const AllStarRomSfxProgram *rim;
    const AllStarRomSfxProgram *foul;
    const AllStarRomSfxProgram *free_throw_net;
    const AllStarRomSfxProgram *free_throw_contact;
    const AllStarRomSfxProgram *horse_letter;
    const AllStarRomSfxProgram *accuracy_result;
    if (!pack || pack->header.rom_sfx_program_count !=
            ALLSTAR_ROM_SFX_PROGRAM_COUNT) return false;
    movement = &pack->rom_sfx_programs[0];
    score = &pack->rom_sfx_programs[1];
    dribble = &pack->rom_sfx_programs[2];
    navigation = &pack->rom_sfx_programs[3];
    confirm = &pack->rom_sfx_programs[4];
    rim = &pack->rom_sfx_programs[5];
    foul = &pack->rom_sfx_programs[6];
    free_throw_net = &pack->rom_sfx_programs[7];
    free_throw_contact = &pack->rom_sfx_programs[8];
    horse_letter = &pack->rom_sfx_programs[9];
    accuracy_result = &pack->rom_sfx_programs[10];
    return movement->command == 0x0d && movement->program_id == 0x11 &&
        movement->priority_frames == 0x14 && movement->frame_count == 3 &&
        movement->stream_pointer_1 == 0x3fa2 &&
        movement->frames[0].square1_frequency == 0x07ba &&
        movement->frames[1].square1_frequency == 0x07bb &&
        movement->frames[2].square1_frequency == 0x07bc &&
        score->command == 0x05 && score->program_id == 0x0c &&
        score->priority_frames == 0x64 && score->frame_count == 72 &&
        score->stream_pointer_1 == 0x3ef6 &&
        score->stream_pointer_2 == 0x3f00 &&
        score->frames[0].square1_frequency == 0x060b &&
        score->frames[0].square2_frequency == 0x0563 &&
        score->frames[16].square1_frequency == 0x0672 &&
        score->frames[16].square2_frequency == 0x060b &&
        score->frames[24].square1_frequency == 0x06b2 &&
        score->frames[24].square2_frequency == 0x0672 &&
        score->frames[71].square1_frequency == 0x06b1 &&
        score->frames[71].square2_frequency == 0x0671 &&
        movement->source_checksum != 0 &&
        dribble->command == 0x0c && dribble->program_id == 0x02 &&
        dribble->priority_frames == 0x13 && dribble->frame_count == 6 &&
        dribble->stream_pointer_1 == 0x3d7f &&
        dribble->square2_duty_length == 0x7a &&
        dribble->square2_envelope == 0xf1 &&
        dribble->frames[0].square2_frequency == 0 &&
        navigation->command == 0x0f && navigation->program_id == 0x07 &&
        navigation->priority_frames == 0x19 &&
        navigation->frame_count == 24 &&
        navigation->stream_pointer_1 == 0x3ebc &&
        navigation->frames[0].square1_frequency == 0x07b1 &&
        confirm->command == 0x0e && confirm->program_id == 0x12 &&
        confirm->priority_frames == 0x32 && confirm->frame_count == 48 &&
        confirm->stream_pointer_1 == 0x3fa6 &&
        confirm->frames[0].square1_frequency == 0x0783 &&
        confirm->frames[6].square1_frequency == 0x0791 &&
        confirm->frames[12].square1_frequency == 0x079d &&
        confirm->frames[24].square1_frequency == 0x07ad &&
        movement->source_checksum == score->source_checksum &&
        movement->source_checksum == dribble->source_checksum &&
        movement->source_checksum == navigation->source_checksum &&
        movement->source_checksum == confirm->source_checksum &&
        rim->command == 0x09 && rim->program_id == 0x0b &&
        rim->priority_frames == 0x23 && rim->frame_count == 24 &&
        rim->stream_pointer_1 == 0x3ef2 &&
        rim->noise_length == 0xeb && rim->noise_envelope == 0xf2 &&
        rim->noise_control == 0xbf &&
        rim->frames[0].noise_polynomial == 0x5a &&
        (rim->frames[0].flags &
            (ALLSTAR_ROM_SFX_CHANNEL_4 | ALLSTAR_ROM_SFX_TRIGGER_4)) ==
            (ALLSTAR_ROM_SFX_CHANNEL_4 | ALLSTAR_ROM_SFX_TRIGGER_4) &&
        movement->source_checksum == rim->source_checksum &&
        foul->command == 0x04 && foul->program_id == 0x0a &&
        foul->priority_frames == 0x1e && foul->frame_count == 30 &&
        foul->stream_pointer_1 == 0x3ed4 &&
        foul->stream_pointer_2 == 0x3ee0 &&
        foul->square1_sweep == 0x08 &&
        foul->square1_duty_length == 0x08 &&
        foul->square1_envelope == 0xa2 &&
        foul->square2_duty_length == 0x48 &&
        foul->square2_envelope == 0xa2 &&
        foul->frames[0].square1_frequency == 0x07c1 &&
        foul->frames[0].square2_frequency == 0x07be &&
        movement->source_checksum == foul->source_checksum &&
        free_throw_net->command == 0x08 &&
        free_throw_net->program_id == 0x05 &&
        free_throw_net->priority_frames == 0x23 &&
        free_throw_net->frame_count == 57 &&
        free_throw_net->stream_pointer_1 == 0x3eac &&
        free_throw_net->noise_length == 0xc6 &&
        free_throw_net->noise_envelope == 0xf1 &&
        free_throw_net->noise_control == 0xa0 &&
        free_throw_net->frames[0].noise_polynomial == 0x10 &&
        free_throw_net->frames[3].noise_polynomial == 0x20 &&
        free_throw_net->frames[6].noise_polynomial == 0x30 &&
        free_throw_net->frames[9].noise_polynomial == 0x40 &&
        free_throw_contact->command == 0x0a &&
        free_throw_contact->program_id == 0x0d &&
        free_throw_contact->priority_frames == 0x1c &&
        free_throw_contact->frame_count == 12 &&
        free_throw_contact->stream_pointer_1 == 0x3f0a &&
        free_throw_contact->square1_sweep == 0xff &&
        free_throw_contact->square1_duty_length == 0x7f &&
        free_throw_contact->square1_envelope == 0xf1 &&
        free_throw_contact->frames[0].square1_frequency == 0 &&
        movement->source_checksum == free_throw_net->source_checksum &&
        movement->source_checksum == free_throw_contact->source_checksum &&
        horse_letter->command == 0x07 &&
        horse_letter->program_id == 0x06 &&
        horse_letter->priority_frames == 0x2a &&
        horse_letter->stream_pointer_1 == 0x3eb6 &&
        horse_letter->frame_count == 42 &&
        horse_letter->square1_sweep == 0x88 &&
        horse_letter->square1_duty_length == 0x40 &&
        horse_letter->square1_envelope == 0xf2 &&
        horse_letter->frames[0].square1_frequency == 0x0783 &&
        horse_letter->frames[6].square1_frequency == 0x079d &&
        movement->source_checksum == horse_letter->source_checksum &&
        accuracy_result->command == 0x02 &&
        accuracy_result->program_id == 0x08 &&
        accuracy_result->priority_frames == 0xaa &&
        accuracy_result->stream_pointer_1 == 0x3ec0 &&
        accuracy_result->stream_pointer_2 == 0x3ec4 &&
        accuracy_result->frame_count == 144 &&
        accuracy_result->square1_sweep == 0x88 &&
        accuracy_result->square1_duty_length == 0x00 &&
        accuracy_result->square1_envelope == 0xff &&
        accuracy_result->square2_duty_length == 0x3f &&
        accuracy_result->square2_envelope == 0x6f &&
        accuracy_result->frames[0].square1_frequency == 0x065b &&
        accuracy_result->frames[0].square2_frequency == 0x0641 &&
        movement->source_checksum == accuracy_result->source_checksum;
}

static bool validate_title_music(const AllStarAssetPack *pack) {
    const AllStarRomMusicProgram *program;
    if (!pack || pack->header.rom_music_program_count !=
            ALLSTAR_ROM_MUSIC_PROGRAM_COUNT) return false;
    program = &pack->rom_music_programs[0];
    return program->song_id == 1 && program->update_skip == 7 &&
        program->frame_count == 3360 && program->loop_frame == 1568 &&
        program->program_pointer == 0x3b25 &&
        program->offset_pointer == 0x3aab &&
        program->source_checksum == 0x7ae8b9d0u &&
        program->frames[0].square1_frequency == 0x069e &&
        program->frames[0].square2_frequency == 0x0627 &&
        program->frames[0].wave_frequency == 0x053b &&
        program->frames[0].flags == 0xff;
}

void allstar_asset_pack_init_default(AllStarAssetPack *pack) {
    if (!pack) return;
    memset(pack, 0, sizeof(AllStarAssetPack));

    pack->header.magic = ALLSTAR_ASSET_MAGIC;
    pack->header.version = ALLSTAR_ASSET_VERSION;
    pack->header.tile_count = 128;
    pack->header.player_count = ALLSTAR_DEFAULT_ROSTER_COUNT;
    pack->header.audio_sequence_count = 0;
    pack->header.animation_action_count =
        ALLSTAR_ROM_ANIMATION_ACTION_COUNT;
    pack->header.feature_flags = 0;
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
    if (!extract_one_on_one_art(pack, rom)) {
        fprintf(stderr, "[AssetPack] Invalid One-on-One graphics streams\n");
        return false;
    }
    if (!extract_free_throw_art(pack, rom)) {
        fprintf(stderr, "[AssetPack] Invalid Free Throw graphics streams\n");
        return false;
    }
    if (!extract_gameplay_audio(pack, rom)) {
        fprintf(stderr, "[AssetPack] Invalid One-on-One $3014 audio streams\n");
        return false;
    }
    if (!extract_title_music(pack, rom)) {
        fprintf(stderr, "[AssetPack] Invalid title music stream\n");
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
           "(%u tiles, %u players, %u animation actions, One-on-One/Free Throw art, "
           "%u ROM sound programs, %u ROM songs)\n",
           rom->header.title, pack->header.tile_count,
           pack->header.player_count, pack->header.animation_action_count,
           pack->header.rom_sfx_program_count,
           pack->header.rom_music_program_count);

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
               pack->header.animation_action_count ||
        fwrite(pack->player_source_tiles, sizeof(AllStarTile),
               pack->header.player_source_tile_count, f) !=
               pack->header.player_source_tile_count ||
        fwrite(pack->player_frames, sizeof(AllStarRomPlayerFrame),
               pack->header.player_frame_count, f) !=
               pack->header.player_frame_count ||
        fwrite(pack->ball_source_tiles, sizeof(AllStarTile),
               pack->header.ball_source_tile_count, f) !=
               pack->header.ball_source_tile_count ||
        fwrite(pack->ball_oam_pairs, sizeof(AllStarRomOamPair),
               pack->header.ball_oam_pair_count, f) !=
               pack->header.ball_oam_pair_count ||
        fwrite(pack->court_tiles, sizeof(AllStarTile),
               pack->header.court_tile_count, f) !=
               pack->header.court_tile_count ||
        fwrite(pack->net_tiles, sizeof(AllStarTile),
               pack->header.net_tile_count, f) !=
               pack->header.net_tile_count ||
        fwrite(pack->free_throw_bg_tiles, sizeof(AllStarTile),
               ALLSTAR_FREE_THROW_BG_TILE_COUNT, f) !=
               ALLSTAR_FREE_THROW_BG_TILE_COUNT ||
        fwrite(pack->free_throw_obj_tiles, sizeof(AllStarTile),
               ALLSTAR_FREE_THROW_OBJ_TILE_COUNT, f) !=
               ALLSTAR_FREE_THROW_OBJ_TILE_COUNT ||
        fwrite(&pack->free_throw_reticle_tile, sizeof(AllStarTile), 1, f) != 1 ||
        fwrite(pack->free_throw_tilemap, 1,
               sizeof(pack->free_throw_tilemap), f) !=
               sizeof(pack->free_throw_tilemap) ||
        fwrite(pack->free_throw_pose_maps, 1,
               sizeof(pack->free_throw_pose_maps), f) !=
               sizeof(pack->free_throw_pose_maps) ||
        fwrite(pack->free_throw_net_maps, 1,
               sizeof(pack->free_throw_net_maps), f) !=
               sizeof(pack->free_throw_net_maps) ||
        fwrite(pack->free_throw_ball_maps, 1,
               sizeof(pack->free_throw_ball_maps), f) !=
               sizeof(pack->free_throw_ball_maps) ||
        fwrite(pack->rom_sfx_programs, sizeof(AllStarRomSfxProgram),
               pack->header.rom_sfx_program_count, f) !=
               pack->header.rom_sfx_program_count ||
        fwrite(pack->rom_music_programs, sizeof(AllStarRomMusicProgram),
               pack->header.rom_music_program_count, f) !=
               pack->header.rom_music_program_count) {
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
        fprintf(stderr, "[AssetPack] Could not open asset pack: %s\n", filepath);
        allstar_asset_pack_init_default(pack);
        return false;
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
    if (((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) != 0 &&
         (pack->header.player_source_tile_count !=
              ALLSTAR_PLAYER_SOURCE_TILE_COUNT ||
          pack->header.player_frame_count != ALLSTAR_PLAYER_FRAME_COUNT ||
          pack->header.ball_source_tile_count !=
              ALLSTAR_BALL_SOURCE_TILE_COUNT ||
          pack->header.ball_oam_pair_count != ALLSTAR_BALL_OAM_PAIR_COUNT ||
          pack->header.court_tile_count != ALLSTAR_COURT_TILE_COUNT ||
          pack->header.net_tile_count != ALLSTAR_NET_TILE_COUNT)) ||
        ((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) == 0 &&
         (pack->header.player_source_tile_count != 0 ||
          pack->header.player_frame_count != 0 ||
          pack->header.ball_source_tile_count != 0 ||
          pack->header.ball_oam_pair_count != 0 ||
          pack->header.court_tile_count != 0 ||
           pack->header.net_tile_count != 0)) ||
        ((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO) != 0 &&
         (pack->header.rom_sfx_program_count !=
              ALLSTAR_ROM_SFX_PROGRAM_COUNT ||
          pack->header.audio_sequence_count !=
              ALLSTAR_ROM_SFX_PROGRAM_COUNT)) ||
        ((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO) == 0 &&
         pack->header.rom_sfx_program_count != 0) ||
        ((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_ROM_MUSIC) != 0 &&
         pack->header.rom_music_program_count !=
             ALLSTAR_ROM_MUSIC_PROGRAM_COUNT) ||
        ((pack->header.feature_flags &
             ALLSTAR_ASSET_FEATURE_ROM_MUSIC) == 0 &&
         pack->header.rom_music_program_count != 0)) {
        fprintf(stderr, "[AssetPack] Invalid One-on-One asset counts\n");
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
            pack->header.animation_action_count ||
        fread(pack->player_source_tiles, sizeof(AllStarTile),
              pack->header.player_source_tile_count, f) !=
            pack->header.player_source_tile_count ||
        fread(pack->player_frames, sizeof(AllStarRomPlayerFrame),
              pack->header.player_frame_count, f) !=
            pack->header.player_frame_count ||
        fread(pack->ball_source_tiles, sizeof(AllStarTile),
              pack->header.ball_source_tile_count, f) !=
            pack->header.ball_source_tile_count ||
        fread(pack->ball_oam_pairs, sizeof(AllStarRomOamPair),
              pack->header.ball_oam_pair_count, f) !=
            pack->header.ball_oam_pair_count ||
        fread(pack->court_tiles, sizeof(AllStarTile),
              pack->header.court_tile_count, f) !=
              pack->header.court_tile_count ||
        fread(pack->net_tiles, sizeof(AllStarTile),
              pack->header.net_tile_count, f) !=
              pack->header.net_tile_count ||
        fread(pack->free_throw_bg_tiles, sizeof(AllStarTile),
              ALLSTAR_FREE_THROW_BG_TILE_COUNT, f) !=
              ALLSTAR_FREE_THROW_BG_TILE_COUNT ||
        fread(pack->free_throw_obj_tiles, sizeof(AllStarTile),
              ALLSTAR_FREE_THROW_OBJ_TILE_COUNT, f) !=
              ALLSTAR_FREE_THROW_OBJ_TILE_COUNT ||
        fread(&pack->free_throw_reticle_tile, sizeof(AllStarTile), 1, f) != 1 ||
        fread(pack->free_throw_tilemap, 1,
              sizeof(pack->free_throw_tilemap), f) !=
              sizeof(pack->free_throw_tilemap) ||
        fread(pack->free_throw_pose_maps, 1,
              sizeof(pack->free_throw_pose_maps), f) !=
              sizeof(pack->free_throw_pose_maps) ||
        fread(pack->free_throw_net_maps, 1,
              sizeof(pack->free_throw_net_maps), f) !=
              sizeof(pack->free_throw_net_maps) ||
        fread(pack->free_throw_ball_maps, 1,
              sizeof(pack->free_throw_ball_maps), f) !=
              sizeof(pack->free_throw_ball_maps) ||
        fread(pack->rom_sfx_programs, sizeof(AllStarRomSfxProgram),
              pack->header.rom_sfx_program_count, f) !=
              pack->header.rom_sfx_program_count ||
        fread(pack->rom_music_programs, sizeof(AllStarRomMusicProgram),
              pack->header.rom_music_program_count, f) !=
              pack->header.rom_music_program_count) {
        fprintf(stderr, "[AssetPack] Truncated asset payload\n");
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    if ((pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO) != 0 &&
        !validate_gameplay_audio(pack)) {
        fprintf(stderr, "[AssetPack] Invalid decoded One-on-One audio\n");
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }
    if ((pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ROM_MUSIC) != 0 &&
        !validate_title_music(pack)) {
        fprintf(stderr, "[AssetPack] Invalid decoded title music\n");
        fclose(f);
        allstar_asset_pack_init_default(pack);
        return false;
    }

    fclose(f);
    pack->is_loaded = true;
    printf("[AssetPack] Loaded asset pack: %u tiles, %u players, "
           "%u animation actions, %u ROM sound programs, %u ROM songs\n",
           pack->header.tile_count, pack->header.player_count,
           pack->header.animation_action_count,
           pack->header.rom_sfx_program_count,
           pack->header.rom_music_program_count);
    return true;
}
