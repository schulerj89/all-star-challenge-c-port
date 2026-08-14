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

bool allstar_renderer_init(AllStarRenderer *renderer, uint32_t width, uint32_t height);
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
void allstar_renderer_draw_player_ex(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, uint8_t skin_tone, bool has_ball, bool is_shooting, bool is_defending, float anim_time, bool facing_left);
void allstar_renderer_draw_hoop(AllStarRenderer *renderer, int32_t hoop_x, int32_t hoop_y);
void allstar_renderer_draw_court(AllStarRenderer *renderer);
void allstar_renderer_present(AllStarRenderer *renderer);

#endif /* ALLSTAR_RENDERER_H */
