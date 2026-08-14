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

void allstar_renderer_draw_ball(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z) {
    int draw_y = y - (int)(z * 0.45f);

    /* Ball Shadow on floor */
    if (z > 2) {
        allstar_renderer_set_pixel(renderer, x - 1, y, 1);
        allstar_renderer_set_pixel(renderer, x, y, 2);
        allstar_renderer_set_pixel(renderer, x + 1, y, 1);
    }

    /* 6x6 Round Basketball Sprite */
    static const uint8_t BALL_SPRITE[6][6] = {
        {0, 2, 3, 3, 2, 0},
        {2, 3, 2, 2, 3, 2},
        {3, 2, 3, 3, 2, 3},
        {3, 2, 3, 3, 2, 3},
        {2, 3, 2, 2, 3, 2},
        {0, 2, 3, 3, 2, 0}
    };

    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 6; c++) {
            uint8_t shade = BALL_SPRITE[r][c];
            if (shade) {
                allstar_renderer_set_pixel(renderer, x - 3 + c, draw_y - 3 + r, shade);
            }
        }
    }
}

void allstar_renderer_draw_player(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, bool has_ball, bool is_shooting, float anim_time) {
    (void)anim_time;
    uint8_t skin_shade = 2;
    uint8_t jersey_shade = is_p1 ? 3 : 1;
    uint8_t outline_shade = 3;

    /* Shadow */
    allstar_renderer_set_pixel(renderer, x - 3, y + 8, 1);
    allstar_renderer_set_pixel(renderer, x - 2, y + 8, 2);
    allstar_renderer_set_pixel(renderer, x - 1, y + 8, 2);
    allstar_renderer_set_pixel(renderer, x, y + 8, 2);
    allstar_renderer_set_pixel(renderer, x + 1, y + 8, 2);
    allstar_renderer_set_pixel(renderer, x + 2, y + 8, 2);
    allstar_renderer_set_pixel(renderer, x + 3, y + 8, 1);

    /* Head */
    allstar_renderer_draw_rect_fill(renderer, x - 2, y - 10, 5, 5, skin_shade);
    allstar_renderer_draw_rect_outline(renderer, x - 2, y - 10, 5, 5, outline_shade);

    /* Torso / Jersey */
    allstar_renderer_draw_rect_fill(renderer, x - 3, y - 5, 7, 7, jersey_shade);
    allstar_renderer_draw_rect_outline(renderer, x - 3, y - 5, 7, 7, outline_shade);

    /* Legs / Shorts */
    allstar_renderer_draw_rect_fill(renderer, x - 3, y + 2, 3, 5, is_p1 ? 3 : 2);
    allstar_renderer_draw_rect_fill(renderer, x + 1, y + 2, 3, 5, is_p1 ? 3 : 2);

    /* Arms holding ball or defending */
    if (has_ball) {
        if (is_shooting) {
            allstar_renderer_draw_rect_fill(renderer, x - 1, y - 14, 3, 3, skin_shade);
            allstar_renderer_draw_ball(renderer, x, y - 16, 0);
        } else {
            allstar_renderer_draw_ball(renderer, x + 5, y - 2, 0);
        }
    } else {
        /* Defensive stance arms */
        allstar_renderer_set_pixel(renderer, x - 4, y - 3, skin_shade);
        allstar_renderer_set_pixel(renderer, x - 5, y - 4, skin_shade);
        allstar_renderer_set_pixel(renderer, x + 4, y - 3, skin_shade);
        allstar_renderer_set_pixel(renderer, x + 5, y - 4, skin_shade);
    }
}

void allstar_renderer_present(AllStarRenderer *renderer) {
    (void)renderer;
}
