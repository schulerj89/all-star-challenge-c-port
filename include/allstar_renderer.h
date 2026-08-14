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
void allstar_renderer_set_pixel(AllStarRenderer *renderer, int32_t x, int32_t y, uint8_t shade_index);
void allstar_renderer_draw_tile(AllStarRenderer *renderer, const AllStarTile *tile, int32_t x, int32_t y, bool flip_x, bool flip_y);
void allstar_renderer_draw_tilemap(AllStarRenderer *renderer, const uint8_t *map, const AllStarTile *tile_bank, int32_t map_w, int32_t map_h, int32_t scroll_x, int32_t scroll_y);
void allstar_renderer_draw_text(AllStarRenderer *renderer, const char *text, int32_t x, int32_t y, uint8_t shade);
void allstar_renderer_draw_sprite(AllStarRenderer *renderer, const AllStarTile *tile_bank, const AllStarSprite *sprite);
void allstar_renderer_present(AllStarRenderer *renderer);

#endif /* ALLSTAR_RENDERER_H */
