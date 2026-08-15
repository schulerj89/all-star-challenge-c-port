#ifndef ALLSTAR_RENDERER_H
#define ALLSTAR_RENDERER_H

#include "allstar_types.h"
#include "allstar_asset_pack.h"

#define ALLSTAR_MAX_SPRITES 40

typedef struct {
    uint32_t width;
    uint32_t height;
    AllStarColor *pixels;
    AllStarPalette current_palette;
    AllStarPaletteStyle palette_style;
    const AllStarAssetPack *asset_pack;
    AllStarSprite sprites[ALLSTAR_MAX_SPRITES];
    size_t sprite_count;
    uint8_t bg_scroll_x;
    uint8_t bg_scroll_y;
    uint8_t win_x;
    uint8_t win_y;
    bool win_enabled;
    bool bg_enabled;
    bool sprites_enabled;
} AllStarRenderer;

typedef struct {
    uint8_t phase;
    uint8_t adjusted_phase;
    uint8_t oam_x;
    uint8_t ball_oam_y;
    uint8_t shadow_oam_y;
    uint8_t shadow_tier;
    uint8_t ball_pair_index;
    uint8_t shadow_pair_index;
} AllStarRomBallPresentation;

typedef struct {
    bool visible;
    uint8_t ball_x;
    uint8_t ball_y;
    uint8_t ball_z;
    bool behind_owner;
} AllStarRomHeldBallPresentation;

bool allstar_renderer_init(AllStarRenderer *renderer, uint32_t width, uint32_t height);
void allstar_renderer_set_asset_pack(AllStarRenderer *renderer,
                                     const AllStarAssetPack *asset_pack);
void allstar_renderer_free(AllStarRenderer *renderer);
void allstar_renderer_clear(AllStarRenderer *renderer, uint8_t shade_index);
void allstar_renderer_set_palette_style(AllStarRenderer *renderer, AllStarPaletteStyle style);
void allstar_renderer_cycle_palette(AllStarRenderer *renderer);
void allstar_renderer_set_pixel(AllStarRenderer *renderer, int32_t x, int32_t y, uint8_t shade_index);
void allstar_renderer_draw_line(AllStarRenderer *renderer, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t shade);
void allstar_renderer_draw_rect_fill(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade);
void allstar_renderer_draw_rect_outline(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t w, int32_t h, uint8_t shade);
void allstar_renderer_draw_tile(AllStarRenderer *renderer, const AllStarTile *tile, int32_t x, int32_t y, bool flip_x, bool flip_y);
void allstar_renderer_draw_tilemap(AllStarRenderer *renderer, const uint8_t *map, const AllStarTile *tile_bank, int32_t map_w, int32_t map_h, int32_t scroll_x, int32_t scroll_y);
void allstar_renderer_draw_text(AllStarRenderer *renderer, const char *text, int32_t x, int32_t y, uint8_t shade);
void allstar_renderer_draw_text_box(AllStarRenderer *renderer, const char *text, int32_t x, int32_t y, uint8_t text_shade, uint8_t bg_shade, uint8_t border_shade);
void allstar_renderer_draw_ball(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z);
void allstar_renderer_draw_ball_ex(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z, float spin_time);
void allstar_renderer_draw_cursor(AllStarRenderer *renderer, int32_t x, int32_t y);
void allstar_renderer_draw_player(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, bool has_ball, bool is_shooting, float anim_time);
void allstar_renderer_draw_player_ex(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, uint8_t skin_tone, bool has_ball, bool is_shooting, bool is_defending, uint8_t rom_action, uint8_t rom_display_frame, float anim_time, bool facing_left);
void allstar_renderer_rom_ball_presentation_6945(
    uint8_t ball_x, uint8_t ball_y, uint8_t ball_z,
    bool behind_owner, AllStarRomBallPresentation *presentation);
void allstar_renderer_rom_held_ball_7f37(
    int32_t player_center_x, int32_t player_ground_y,
    uint8_t action, uint8_t display_frame, bool horizontal_flip,
    AllStarRomHeldBallPresentation *presentation);
bool allstar_renderer_rom_player_tiles_2945(
    const AllStarAssetPack *pack, uint8_t action, uint8_t display_frame,
    bool horizontal_flip,
    uint16_t output_tiles[ALLSTAR_PLAYER_FRAME_TILE_COUNT]);
void allstar_renderer_draw_hoop(AllStarRenderer *renderer, int32_t hoop_x, int32_t hoop_y);
void allstar_renderer_draw_court(AllStarRenderer *renderer);
void allstar_renderer_present(AllStarRenderer *renderer);

#endif /* ALLSTAR_RENDERER_H */
