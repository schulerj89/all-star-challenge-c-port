#include "allstar_renderer.h"
#include "allstar_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Built-in Palette Presets */
static const AllStarPalette PALETTES[ALLSTAR_PALETTE_COUNT] = {
    /* ALLSTAR_PALETTE_DMG_ORIGINAL (Authentic Green Dot Matrix LCD) */
    {
        {
            0xFF9BBC0F, /* 0: Lightest green */
            0xFF8BAC0F, /* 1: Light green */
            0xFF306230, /* 2: Dark green */
            0xFF0F380F  /* 3: Darkest green */
        }
    },
    /* ALLSTAR_PALETTE_POCKET_BW (Game Boy Pocket Crisp Grayscale) */
    {
        {
            0xFFFFFFFF, /* 0: White */
            0xFFAAAAAA, /* 1: Light Gray */
            0xFF555555, /* 2: Dark Gray */
            0xFF000000  /* 3: Black */
        }
    },
    /* ALLSTAR_PALETTE_MODERN_VIBRANT (Modern High Contrast) */
    {
        {
            0xFFE0F8D0, /* 0: Pale Yellow-Green */
            0xFF88C070, /* 1: Bright Green */
            0xFF346856, /* 2: Deep Teal */
            0xFF081820  /* 3: Midnight */
        }
    }
};

bool allstar_renderer_init(AllStarRenderer *renderer, uint32_t width, uint32_t height) {
    if (!renderer) return false;
    memset(renderer, 0, sizeof(AllStarRenderer));

    renderer->width = width ? width : ALLSTAR_GB_WIDTH;
    renderer->height = height ? height : ALLSTAR_GB_HEIGHT;
    renderer->pixels = (AllStarColor*)calloc(renderer->width * renderer->height, sizeof(AllStarColor));
    if (!renderer->pixels) return false;

    renderer->palette_style = ALLSTAR_PALETTE_DMG_ORIGINAL;
    renderer->current_palette = PALETTES[renderer->palette_style];
    renderer->bg_enabled = true;
    renderer->sprites_enabled = true;
    renderer->win_enabled = false;

    return true;
}

void allstar_renderer_set_asset_pack(AllStarRenderer *renderer,
                                     const AllStarAssetPack *asset_pack) {
    if (renderer) renderer->asset_pack = asset_pack;
}

void allstar_renderer_free(AllStarRenderer *renderer) {
    if (renderer && renderer->pixels) {
        free(renderer->pixels);
        renderer->pixels = NULL;
    }
}

void allstar_renderer_set_palette_style(AllStarRenderer *renderer, AllStarPaletteStyle style) {
    if (!renderer) return;
    if (style >= ALLSTAR_PALETTE_COUNT) style = ALLSTAR_PALETTE_DMG_ORIGINAL;
    renderer->palette_style = style;
    renderer->current_palette = PALETTES[style];
}

void allstar_renderer_cycle_palette(AllStarRenderer *renderer) {
    if (!renderer) return;
    AllStarPaletteStyle next_style = (AllStarPaletteStyle)((renderer->palette_style + 1) % ALLSTAR_PALETTE_COUNT);
    allstar_renderer_set_palette_style(renderer, next_style);
}

void allstar_renderer_clear(AllStarRenderer *renderer, uint8_t shade_index) {
    if (!renderer || !renderer->pixels) return;
    AllStarColor color = renderer->current_palette.shades[shade_index & 3];
    size_t total_pixels = renderer->width * renderer->height;
    for (size_t i = 0; i < total_pixels; i++) {
        renderer->pixels[i] = color;
    }
    renderer->sprite_count = 0;
}

void allstar_renderer_set_pixel(AllStarRenderer *renderer, int32_t x, int32_t y, uint8_t shade_index) {
    if (!renderer || !renderer->pixels) return;
    if (x < 0 || (uint32_t)x >= renderer->width || y < 0 || (uint32_t)y >= renderer->height) return;
    renderer->pixels[y * renderer->width + x] = renderer->current_palette.shades[shade_index & 3];
}

void allstar_renderer_draw_line(AllStarRenderer *renderer, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t shade) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;

    while (1) {
        allstar_renderer_set_pixel(renderer, x0, y0, shade);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void allstar_renderer_draw_rect_fill(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade) {
    for (int cy = y; cy < y + h; cy++) {
        for (int cx = x; cx < x + w; cx++) {
            allstar_renderer_set_pixel(renderer, cx, cy, shade);
        }
    }
}

void allstar_renderer_draw_rect_outline(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade) {
    for (int cx = x; cx < x + w; cx++) {
        allstar_renderer_set_pixel(renderer, cx, y, shade);
        allstar_renderer_set_pixel(renderer, cx, y + h - 1, shade);
    }
    for (int cy = y; cy < y + h; cy++) {
        allstar_renderer_set_pixel(renderer, x, cy, shade);
        allstar_renderer_set_pixel(renderer, x + w - 1, cy, shade);
    }
}

void allstar_renderer_draw_tile(AllStarRenderer *renderer, const AllStarTile *tile, int32_t px, int32_t py, bool flip_x, bool flip_y) {
    if (!renderer || !tile) return;
    for (int ty = 0; ty < 8; ty++) {
        int target_y = py + ty;
        if (target_y < 0 || (uint32_t)target_y >= renderer->height) continue;
        int src_y = flip_y ? (7 - ty) : ty;

        for (int tx = 0; tx < 8; tx++) {
            int target_x = px + tx;
            if (target_x < 0 || (uint32_t)target_x >= renderer->width) continue;
            int src_x = flip_x ? (7 - tx) : tx;

            uint8_t shade = tile->pixels[src_y * 8 + src_x];
            allstar_renderer_set_pixel(renderer, target_x, target_y, shade);
        }
    }
}

void allstar_renderer_draw_tilemap(AllStarRenderer *renderer, const uint8_t *map, const AllStarTile *tile_bank, int32_t map_w, int32_t map_h, int32_t scroll_x, int32_t scroll_y) {
    if (!renderer || !map || !tile_bank || !renderer->bg_enabled) return;

    int start_tile_x = scroll_x / 8;
    int start_tile_y = scroll_y / 8;
    int offset_x = -(scroll_x % 8);
    int offset_y = -(scroll_y % 8);

    for (int ty = 0; ty <= (int)(renderer->height / 8) + 1; ty++) {
        int my = (start_tile_y + ty) % map_h;
        if (my < 0) my += map_h;

        for (int tx = 0; tx <= (int)(renderer->width / 8) + 1; tx++) {
            int mx = (start_tile_x + tx) % map_w;
            if (mx < 0) mx += map_w;

            uint8_t tile_idx = map[my * map_w + mx];
            const AllStarTile *tile = &tile_bank[tile_idx];
            allstar_renderer_draw_tile(renderer, tile, offset_x + tx * 8, offset_y + ty * 8, false, false);
        }
    }
}

void allstar_renderer_draw_text(AllStarRenderer *renderer, const char *text, int32_t x, int32_t y, uint8_t shade) {
    if (!renderer || !text) return;
    int cx = x;
    int cy = y;

    while (*text) {
        char c = *text++;
        if (c == '\n') {
            cx = x;
            cy += 8;
            continue;
        }

        if (c >= 32 && c <= 126) {
            const uint8_t *glyph = ALLSTAR_FONT_8X8[c - 32];
            for (int r = 0; r < 8; r++) {
                uint8_t row_bits = glyph[r];
                for (int col = 0; col < 8; col++) {
                    if ((row_bits >> (7 - col)) & 1) {
                        allstar_renderer_set_pixel(renderer, cx + col, cy + r, shade);
                    }
                }
            }
        }
        cx += 8;
    }
}

void allstar_renderer_draw_text_box(AllStarRenderer *renderer, const char *text, int32_t x, int32_t y, uint8_t text_shade, uint8_t bg_shade, uint8_t border_shade) {
    size_t len = strlen(text);
    int box_w = (int)len * 8 + 6;
    int box_h = 14;

    allstar_renderer_draw_rect_fill(renderer, x - 3, y - 3, box_w, box_h, bg_shade);
    allstar_renderer_draw_rect_outline(renderer, x - 3, y - 3, box_w, box_h, border_shade);
    allstar_renderer_draw_text(renderer, text, x, y, text_shade);
}

void allstar_renderer_draw_sprite(AllStarRenderer *renderer, const AllStarTile *tile_bank, const AllStarSprite *sprite) {
    if (!renderer || !tile_bank || !sprite || !renderer->sprites_enabled) return;
    const AllStarTile *tile = &tile_bank[sprite->tile_index];
    bool flip_x = (sprite->flags & 0x20) != 0;
    bool flip_y = (sprite->flags & 0x40) != 0;

    for (int ty = 0; ty < 8; ty++) {
        int target_y = sprite->y + ty;
        if (target_y < 0 || (uint32_t)target_y >= renderer->height) continue;
        int src_y = flip_y ? (7 - ty) : ty;

        for (int tx = 0; tx < 8; tx++) {
            int target_x = sprite->x + tx;
            if (target_x < 0 || (uint32_t)target_x >= renderer->width) continue;
            int src_x = flip_x ? (7 - tx) : tx;

            uint8_t shade = tile->pixels[src_y * 8 + src_x];
            if (shade != 0) {
                allstar_renderer_set_pixel(renderer, target_x, target_y, shade);
            }
        }
    }
}

void allstar_renderer_draw_hoop(AllStarRenderer *renderer, int32_t hoop_x, int32_t hoop_y) {
    if (!renderer) return;

    /* Stanchion / Support Post */
    allstar_renderer_draw_line(renderer, hoop_x - 1, 6, hoop_x - 1, hoop_y - 8, 3);
    allstar_renderer_draw_line(renderer, hoop_x, 6, hoop_x, hoop_y - 8, 3);
    allstar_renderer_draw_line(renderer, hoop_x + 1, 6, hoop_x + 1, hoop_y - 8, 3);

    /* Backboard (24x9) at (hoop_x - 12, hoop_y - 9) */
    allstar_renderer_draw_rect_fill(renderer, hoop_x - 12, hoop_y - 9, 24, 9, 0);
    allstar_renderer_draw_rect_outline(renderer, hoop_x - 12, hoop_y - 9, 24, 9, 3);

    /* Inner Target Box (10x6) */
    allstar_renderer_draw_rect_outline(renderer, hoop_x - 5, hoop_y - 7, 10, 6, 3);

    /* Rim (12px) */
    allstar_renderer_draw_line(renderer, hoop_x - 6, hoop_y, hoop_x + 6, hoop_y, 3);

    /* Net (hanging down 8px) */
    for (int ny = 1; ny <= 7; ny++) {
        int w = 6 - (ny / 2);
        allstar_renderer_set_pixel(renderer, hoop_x - w, hoop_y + ny, (ny % 2 == 0) ? 3 : 2);
        allstar_renderer_set_pixel(renderer, hoop_x + w, hoop_y + ny, (ny % 2 == 0) ? 3 : 2);
        if (ny % 2 == 0) {
            allstar_renderer_set_pixel(renderer, hoop_x - (w / 2), hoop_y + ny, 2);
            allstar_renderer_set_pixel(renderer, hoop_x + (w / 2), hoop_y + ny, 2);
        }
    }
}

void allstar_renderer_draw_court(AllStarRenderer *renderer) {
    const AllStarAssetPack *pack;
    if (!renderer) return;
    pack = renderer->asset_pack;
    if (pack && (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) != 0) {
        int row;
        int column;
        /* $04B1/$050F expands 20 rows with a 32-byte VRAM stride. */
        for (row = 0; row < 18; row++) {
            for (column = 0; column < 20; column++) {
                uint8_t tile = pack->court_tilemap[row * 32 + column];
                if (tile < pack->header.court_tile_count) {
                    allstar_renderer_draw_tile(
                        renderer, &pack->court_tiles[tile],
                        column * 8, row * 8, false, false);
                }
            }
        }
        return;
    }
    /* Source-free fallback keeps development builds playable. */
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_rect_outline(renderer, 4, 48, 152, 92, 2);
    allstar_renderer_draw_line(renderer, 80, 48, 80, 140, 1);
    allstar_renderer_draw_rect_outline(renderer, 56, 48, 48, 36, 2);
}

static void allstar_renderer_draw_asset_tile_skin(
    AllStarRenderer *renderer, const AllStarTile *tile,
    int x, int y, bool flip_x, uint8_t skin_tone) {
    int ty;
    int tx;
    if (!renderer || !tile) return;
    for (ty = 0; ty < 8; ty++) {
        for (tx = 0; tx < 8; tx++) {
            int source_x = flip_x ? 7 - tx : tx;
            uint8_t shade = tile->pixels[ty * 8 + source_x];
            if (shade == 0) continue;
            if (skin_tone == 0x90 && shade == 1) shade = 2;
            allstar_renderer_set_pixel(renderer, x + tx, y + ty, shade);
        }
    }
}

/* Bank 1 $6945 selects eight phases from X&7, optionally rotates the phase
   half when the ball is behind its owner, and selects one of three $6A5C
   shadow tables from the unsigned height. */
void allstar_renderer_rom_ball_presentation_6945(
    uint8_t ball_x, uint8_t ball_y, uint8_t ball_z,
    bool behind_owner, AllStarRomBallPresentation *presentation) {
    uint8_t phase;
    uint8_t adjusted;
    uint8_t aligned_x;
    uint8_t tier;
    if (!presentation) return;
    phase = ball_x & 7;
    adjusted = phase;
    aligned_x = ball_x & 0xf8;
    if (behind_owner) {
        if (phase < 4) {
            adjusted = (uint8_t)(phase + 4);
            aligned_x = (uint8_t)(aligned_x - 4);
        } else {
            adjusted = (uint8_t)(phase - 4);
            aligned_x = (uint8_t)(aligned_x + 4);
        }
    }
    tier = ball_z >= 0x1f ? 0 : (ball_z >= 8 ? 1 : 2);
    presentation->phase = phase;
    presentation->adjusted_phase = adjusted;
    presentation->oam_x = aligned_x;
    presentation->ball_oam_y = (uint8_t)(ball_y - ball_z);
    presentation->shadow_oam_y = ball_y;
    presentation->shadow_tier = tier;
    presentation->ball_pair_index = adjusted;
    presentation->shadow_pair_index = (uint8_t)(8 + tier * 8 + adjusted);
}

/* Fixed $1ECC uses the $1F2B pointer table to replace six BG cells beginning
   at $9849 (columns 9/10, rows 2..4). The signed BG IDs $60..$70 resolve
   to the separate 17-tile bank-3 $793F stream loaded at VRAM $9600. */
void allstar_renderer_draw_court_net_1ecc(AllStarRenderer *renderer,
                                          uint8_t net_frame) {
    static const uint8_t net_tiles[3][6] = {
        {0x61, 0x62, 0x67, 0x68, 0x69, 0x6a},
        {0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70},
        {0x61, 0x62, 0x63, 0x64, 0x65, 0x66}
    };
    const AllStarAssetPack *pack;
    const uint8_t *tiles;
    int row;
    int column;
    allstar_renderer_draw_court(renderer);
    if (!renderer || net_frame == 0 || net_frame > 3) return;
    pack = renderer->asset_pack;
    if (!pack || (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) == 0 ||
        pack->header.net_tile_count != ALLSTAR_NET_TILE_COUNT) return;
    tiles = net_tiles[net_frame - 1];
    for (row = 0; row < 3; row++) {
        for (column = 0; column < 2; column++) {
            allstar_renderer_draw_tile(
                renderer, &pack->net_tiles[
                    tiles[row * 2 + column] - 0x60],
                (9 + column) * 8, (2 + row) * 8, false, false);
        }
    }
}

/* $27C7/$27CC write E4,F9,FE,FF and the reverse to BGP. Apply this after the
   background/HUD and before player OAM so the native layers preserve the
   cartridge's separate BGP and OBJ behavior. */
void allstar_renderer_apply_dmg_bgp(AllStarRenderer *renderer,
                                    uint8_t bg_palette) {
    AllStarColor remap[4];
    size_t pixel_count;
    size_t i;
    int source;
    if (!renderer || !renderer->pixels) return;
    for (source = 0; source < 4; source++) {
        uint8_t target = (uint8_t)((bg_palette >> (source * 2)) & 0x03);
        remap[source] = renderer->current_palette.shades[target];
    }
    pixel_count = renderer->width * renderer->height;
    for (i = 0; i < pixel_count; i++) {
        AllStarColor color = renderer->pixels[i];
        for (source = 0; source < 4; source++) {
            if (color == renderer->current_palette.shades[source]) {
                renderer->pixels[i] = remap[source];
                break;
            }
        }
    }
}

/* Bank 1 $7F37 attaches the ball during the shot/gather path.  $7FC7 is the
   action-$0A table {+7,-2}/{+10,-2}; $7FCB is the other-action table
   {+7,+10}/{-2,+8}. Display frames $00/$0C contain the ball in player art. */
void allstar_renderer_rom_held_ball_7f37(
    int32_t player_center_x, int32_t player_ground_y,
    uint8_t action, uint8_t display_frame, bool horizontal_flip,
    AllStarRomHeldBallPresentation *presentation) {
    static const int8_t held_offsets[2][2][2] = {
        {{7, -2}, {10, -2}},
        {{7, 10}, {-2, 8}}
    };
    uint8_t player_x;
    uint8_t ground_y;
    uint8_t visual_y;
    uint8_t action_table;
    uint8_t flip_table;
    int8_t x_offset;
    int8_t height_offset;
    if (!presentation) return;
    memset(presentation, 0, sizeof(*presentation));
    if (display_frame == 0 || display_frame == 0x0c) return;

    player_x = (uint8_t)(player_center_x - 8);
    ground_y = (uint8_t)player_ground_y;
    visual_y = (uint8_t)(ground_y - 40);
    action_table = action == 0x0a ? 0 : 1;
    flip_table = horizontal_flip ? 1 : 0;
    x_offset = held_offsets[action_table][flip_table][0];
    height_offset = held_offsets[action_table][flip_table][1];
    presentation->visible = true;
    presentation->ball_x = (uint8_t)(player_x + x_offset);
    presentation->ball_y = (uint8_t)(ground_y - 2);
    presentation->ball_z = (uint8_t)(
        (uint8_t)(ground_y - 4) -
        (uint8_t)(visual_y + height_offset));
    presentation->behind_owner =
        (presentation->ball_y < (uint8_t)(visual_y + 40)) ==
        ((player_x & 4) != 0);
}

/* Bank 1 $6F2A runs after $7F37 on every gameplay update and is therefore
   the final held/dribbling-ball placement for actions $01/$04/$08/$0B/$10/
   $13. Native x is player +$06+8; ground y is +$15; visual field +$05 is
   ground-40 outside a jump. The $6FEA table supplies the bounce Z. */
void allstar_renderer_rom_dribble_ball_6f2a(
    int32_t player_center_x, int32_t player_ground_y,
    uint8_t action, uint8_t record_index, bool direction_bit4,
    AllStarRomHeldBallPresentation *presentation) {
    static const uint8_t bounce_z[12] =
        {0x0c,0x0c,0x0c,0x08,0x04,0x00,0x04,0x08,0x0b,0x01,0x01,0x01};
    uint8_t player_x;
    uint8_t visual_y;
    int x_offset;
    int y_offset;
    if (!presentation) return;
    memset(presentation, 0, sizeof(*presentation));
    player_x = (uint8_t)(player_center_x - 8);
    visual_y = (uint8_t)(player_ground_y - 40);
    switch (action) {
        case 0x01: x_offset = 3;  y_offset = 0x28; break;
        case 0x04: x_offset = 6;  y_offset = 0x2c; break;
        case 0x08: x_offset = 13; y_offset = 0x28; break;
        case 0x0b: x_offset = 11; y_offset = 0x25; break;
        case 0x10: x_offset = direction_bit4 ? 13 : 0; y_offset = 0x28; break;
        case 0x13: x_offset = direction_bit4 ? 14 : 2; y_offset = 0x26; break;
        default: return;
    }
    presentation->visible = true;
    presentation->ball_x = (uint8_t)(player_x + x_offset);
    presentation->ball_y = (uint8_t)(visual_y + y_offset);
    presentation->ball_z = bounce_z[record_index < 12 ? record_index : 11];
    presentation->behind_owner = false;
}

/* Fixed $2933/$293D->$2945->$2A2B selects one of three action families and
   traverses each 18-index frame as nine top/bottom 8x16 sprite pairs. */
bool allstar_renderer_rom_player_tiles_2945(
    const AllStarAssetPack *pack, uint8_t action, uint8_t display_frame,
    bool horizontal_flip,
    uint16_t output_tiles[ALLSTAR_PLAYER_FRAME_TILE_COUNT]) {
    static const uint16_t tile_offsets[3] = {0, 147, 358};
    static const uint8_t frame_offsets[3] = {0, 16, 38};
    static const uint8_t frame_counts[3] = {16, 22, 22};
    size_t family;
    size_t frame_index;
    const AllStarRomPlayerFrame *frame;
    int sprite_row;
    int column;
    if (!pack || !output_tiles ||
        action >= ALLSTAR_ROM_ANIMATION_ACTION_COUNT) return false;
    family = action >> 3;
    frame_index = frame_offsets[family] +
        (display_frame % frame_counts[family]);
    frame = &pack->player_frames[frame_index];
    for (sprite_row = 0; sprite_row < 3; sprite_row++) {
        size_t source_row = (size_t)sprite_row * 6;
        size_t output_row = (size_t)sprite_row * 6;
        for (column = 0; column < 3; column++) {
            int source_column = horizontal_flip ? 2 - column : column;
            output_tiles[output_row + column * 2] =
                (uint16_t)(tile_offsets[family] +
                    frame->tile_indices[source_row + source_column]);
            output_tiles[output_row + column * 2 + 1] =
                (uint16_t)(tile_offsets[family] +
                    frame->tile_indices[source_row + 3 + source_column]);
        }
    }
    return true;
}

static void allstar_renderer_draw_oam_pair(
    AllStarRenderer *renderer, const AllStarAssetPack *pack,
    const AllStarRomOamPair *pair, uint8_t oam_x, uint8_t oam_y) {
    uint8_t ids[2];
    int column;
    if (!renderer || !pack || !pair) return;
    ids[0] = pair->left_tile;
    ids[1] = pair->right_tile;
    for (column = 0; column < 2; column++) {
        uint8_t id = ids[column];
        size_t source_index;
        if (id < 0x24 || (id & 1) != 0) continue;
        source_index = (size_t)((id - 0x24) >> 1);
        if (source_index >= pack->header.ball_source_tile_count) continue;
        allstar_renderer_draw_asset_tile_skin(
            renderer, &pack->ball_source_tiles[source_index],
            (int)oam_x - 8 + column * 8, (int)oam_y - 16,
            false, 0);
    }
}

static void allstar_renderer_draw_ball_rom_6945(
    AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z,
    float spin_time, bool behind_owner) {
    const AllStarAssetPack *pack;
    AllStarRomBallPresentation presentation;
    if (!renderer) return;
    (void)spin_time;
    pack = renderer->asset_pack;
    allstar_renderer_rom_ball_presentation_6945(
        (uint8_t)x, (uint8_t)y, (uint8_t)z,
        behind_owner, &presentation);
    if (pack && (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) != 0) {
        allstar_renderer_draw_oam_pair(
            renderer, pack,
            &pack->ball_oam_pairs[presentation.shadow_pair_index],
            presentation.oam_x, presentation.shadow_oam_y);
        allstar_renderer_draw_oam_pair(
            renderer, pack,
            &pack->ball_oam_pairs[presentation.ball_pair_index],
            presentation.oam_x, presentation.ball_oam_y);
        return;
    }
    allstar_renderer_draw_rect_fill(
        renderer, (int)presentation.oam_x - 7,
        (int)presentation.ball_oam_y - 15, 6, 6, 3);
    allstar_renderer_draw_line(renderer, x - 4, y, x + 4, y, 1);
}

void allstar_renderer_draw_ball_ex(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z, float spin_time) {
    allstar_renderer_draw_ball_rom_6945(
        renderer, x, y, z, spin_time, false);
}

void allstar_renderer_draw_ball(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z) {
    allstar_renderer_draw_ball_ex(renderer, x, y, z, 0.0f);
}

void allstar_renderer_draw_cursor(AllStarRenderer *renderer, int32_t x, int32_t y) {
    if (!renderer) return;
    static const uint8_t GB_CURSOR_SPRITE[8][8] = {
        {0, 0, 2, 2, 2, 2, 0, 0},
        {0, 2, 1, 1, 1, 1, 2, 0},
        {2, 1, 1, 2, 2, 2, 2, 2},
        {2, 1, 1, 1, 1, 1, 1, 2},
        {2, 1, 2, 2, 2, 2, 2, 2},
        {2, 1, 1, 1, 1, 1, 2, 2},
        {0, 2, 2, 2, 2, 2, 2, 0},
        {0, 0, 2, 2, 2, 2, 0, 0}
    };

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            uint8_t shade = GB_CURSOR_SPRITE[r][c];
            if (shade != 0) {
                allstar_renderer_set_pixel(renderer, x + c, y + r, shade);
            }
        }
    }
}

void allstar_renderer_draw_player_lifted_ex(AllStarRenderer *renderer,
    int32_t x, int32_t y, int32_t visual_lift,
    bool is_p1, uint8_t skin_tone, bool has_ball, bool is_shooting,
    bool is_defending, uint8_t rom_action, uint8_t rom_display_frame,
    uint8_t rom_record_index, float anim_time, bool horizontal_flip) {
    const AllStarAssetPack *pack;
    if (!renderer) return;
    (void)is_p1;
    (void)is_shooting;
    (void)is_defending;

    /* Native ground Y is player +$15. $2945's 48-pixel OBJ stack ends at
       ground-8 after the DMG OAM Y bias; anchor this optional native shadow
       directly below that baseline instead of leaving a floating gap. */
    for (int sx = -8; sx <= 8; sx++) {
        uint8_t shade = (abs(sx) >= 6) ? 1 : 2;
        allstar_renderer_set_pixel(renderer, x + sx, y - 8, shade);
    }

    pack = renderer->asset_pack;
    if (pack && (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) != 0 &&
        rom_action < ALLSTAR_ROM_ANIMATION_ACTION_COUNT) {
        uint16_t tiles[ALLSTAR_PLAYER_FRAME_TILE_COUNT];
        int sprite_row;
        int column;
        int top_x = x - 16;
        int top_y = y - 56 - visual_lift;
        if (!allstar_renderer_rom_player_tiles_2945(
                pack, rom_action, rom_display_frame,
                horizontal_flip, tiles)) return;
        for (sprite_row = 0; sprite_row < 3; sprite_row++) {
            for (column = 0; column < 3; column++) {
                size_t output = (size_t)sprite_row * 6 + column * 2;
                allstar_renderer_draw_asset_tile_skin(
                    renderer, &pack->player_source_tiles[tiles[output]],
                    top_x + column * 8, top_y + sprite_row * 16,
                    horizontal_flip, skin_tone);
                allstar_renderer_draw_asset_tile_skin(
                    renderer, &pack->player_source_tiles[tiles[output + 1]],
                    top_x + column * 8, top_y + sprite_row * 16 + 8,
                    horizontal_flip, skin_tone);
            }
        }
    } else {
        /* Source-free fallback silhouette. */
        allstar_renderer_draw_rect_fill(renderer, x - 5, y - 30 - visual_lift,
                                        10, 22, 2);
        allstar_renderer_draw_rect_fill(renderer, x - 4, y - 38 - visual_lift,
                                        8, 8,
                                        skin_tone == 0x90 ? 2 : 1);
        allstar_renderer_draw_line(renderer, x - 4, y - 8 - visual_lift,
                                   x - 7, y - visual_lift, 3);
        allstar_renderer_draw_line(renderer, x + 4, y - 8 - visual_lift,
                                   x + 7, y - visual_lift, 3);
    }

    /* If dribbling ball */
    if (has_ball) {
        AllStarRomHeldBallPresentation held_ball;
        if (rom_action == 0x0a || rom_action == 0x12) {
            allstar_renderer_rom_held_ball_7f37(
                x, y, rom_action, rom_display_frame,
                horizontal_flip, &held_ball);
        } else {
            allstar_renderer_rom_dribble_ball_6f2a(
                x, y, rom_action, rom_record_index,
                horizontal_flip, &held_ball);
        }
        if (held_ball.visible) {
            held_ball.ball_z = (uint8_t)(held_ball.ball_z + visual_lift);
            allstar_renderer_draw_ball_rom_6945(
                renderer, held_ball.ball_x, held_ball.ball_y,
                held_ball.ball_z, anim_time, held_ball.behind_owner);
        }
    }
}

void allstar_renderer_draw_player_ex(AllStarRenderer *renderer, int32_t x,
    int32_t y, bool is_p1, uint8_t skin_tone, bool has_ball,
    bool is_shooting, bool is_defending, uint8_t rom_action,
    uint8_t rom_display_frame, float anim_time, bool horizontal_flip) {
    allstar_renderer_draw_player_lifted_ex(
        renderer, x, y, 0, is_p1, skin_tone, has_ball, is_shooting,
        is_defending, rom_action, rom_display_frame, 0,
        anim_time, horizontal_flip);
}

void allstar_renderer_draw_player(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, bool has_ball, bool is_shooting, float anim_time) {
    allstar_renderer_draw_player_ex(renderer, x, y, is_p1,
        is_p1 ? 0x90 : 0x91, has_ball, is_shooting, false,
        has_ball ? 0x01 : 0x02, 0, anim_time, false);
}

void allstar_renderer_present(AllStarRenderer *renderer) {
    (void)renderer;
}
