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

#include "allstar_court_art.h"

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
    if (!renderer) return;

    /* Hardwood Floor Background */
    allstar_renderer_draw_rect_fill(renderer, 0, 16, 160, 128, 0);

    /* Outer Court Perimeter Boundaries */
    allstar_renderer_draw_line(renderer, 8, 18, 151, 18, 3);   /* Baseline */
    allstar_renderer_draw_line(renderer, 8, 18, 8, 138, 3);    /* Left Sideline */
    allstar_renderer_draw_line(renderer, 151, 18, 151, 138, 3);/* Right Sideline */
    allstar_renderer_draw_line(renderer, 8, 138, 151, 138, 3); /* Half-Court Line */

    /* Center Court Circle at Half Court Line */
    for (int deg = 0; deg < 180; deg += 10) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 16.0f);
        int cy = 138 - (int)(sinf(rad) * 8.0f);
        allstar_renderer_set_pixel(renderer, cx, cy, 3);
    }

    /* The Key / Lane (32x44) */
    allstar_renderer_draw_rect_fill(renderer, 64, 18, 32, 44, 1);
    allstar_renderer_draw_rect_outline(renderer, 64, 18, 32, 44, 3);

    /* Free Throw Circle */
    for (int deg = 0; deg < 360; deg += 12) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 16.0f);
        int cy = 62 + (int)(sinf(rad) * 9.0f);
        allstar_renderer_set_pixel(renderer, cx, cy, 3);
    }

    /* 3-Point Arc */
    for (int deg = 0; deg < 180; deg += 3) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 56.0f);
        int cy = 25 + (int)(sinf(rad) * 52.0f);
        if (cx >= 10 && cx <= 149 && cy <= 136) {
            allstar_renderer_set_pixel(renderer, cx, cy, 3);
        }
    }

    /* Basketball Hoop */
    allstar_renderer_draw_hoop(renderer, 80, 25);
}

void allstar_renderer_draw_ball_ex(AllStarRenderer *renderer, int32_t x, int32_t y, int32_t z, float spin_time) {
    if (!renderer) return;

    /* Dynamic Floor Shadow */
    if (z > 2) {
        int shadow_w = (z > 20) ? 2 : 3;
        for (int sx = -shadow_w; sx <= shadow_w; sx++) {
            uint8_t shade = (abs(sx) == shadow_w) ? 1 : 2;
            allstar_renderer_set_pixel(renderer, x + sx, y, shade);
        }
    }

    /* Ball Position with Z elevation */
    int draw_y = y - (int)(z * 0.5f);
    int frame = ((int)(spin_time * 10.0f)) & 3;

    /* Render 8x8 Basketball Sprite */
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            uint8_t shade = ALLSTAR_BALL_FRAMES[frame][r][c];
            if (shade != 0) {
                allstar_renderer_set_pixel(renderer, x - 4 + c, draw_y - 4 + r, shade);
            }
        }
    }
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

void allstar_renderer_draw_player_ex(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, uint8_t skin_tone, bool has_ball, bool is_shooting, bool is_defending, float anim_time, bool facing_left) {
    if (!renderer) return;

    /* Dynamic Floor Shadow beneath feet */
    for (int sx = -6; sx <= 6; sx++) {
        uint8_t shade = (abs(sx) >= 5) ? 1 : 2;
        allstar_renderer_set_pixel(renderer, x + sx, y + 1, shade);
    }

    /* Select 16x28 Sprite Frame */
    const uint8_t (*sprite)[16] = ALLSTAR_SPRITE_IDLE;
    int jump_y_offset = 0;

    if (is_shooting) {
        sprite = ALLSTAR_SPRITE_SHOOT_APEX;
        jump_y_offset = -8;
    } else if (is_defending) {
        sprite = ALLSTAR_SPRITE_DEFEND;
    } else if (has_ball) {
        int frame_idx = ((int)(anim_time * 6.0f)) % 2;
        sprite = (frame_idx == 0) ? ALLSTAR_SPRITE_DRIBBLE_A : ALLSTAR_SPRITE_DRIBBLE_B;
    } else {
        int frame_idx = ((int)(anim_time * 6.0f)) % 2;
        sprite = (frame_idx == 0) ? ALLSTAR_SPRITE_RUN_A : ALLSTAR_SPRITE_RUN_B;
    }

    /* Skin tone shade selection: 0x90 = Dark (Shade 2), 0x91 = Light (Shade 1) */
    uint8_t skin_shade = (skin_tone == 0x90) ? 2 : 1;
    uint8_t jersey_shade = is_p1 ? 3 : 1;

    /* Render 16x28 Sprite */
    int top_y = y - 26 + jump_y_offset;
    for (int r = 0; r < 28; r++) {
        int py = top_y + r;
        for (int c = 0; c < 16; c++) {
            int src_c = facing_left ? (15 - c) : c;
            uint8_t raw = sprite[r][src_c];
            if (raw == 0) continue;

            uint8_t final_shade = 3;
            if (raw == 1) final_shade = skin_shade;
            else if (raw == 2) final_shade = jersey_shade;
            else if (raw == 3) final_shade = 3;

            allstar_renderer_set_pixel(renderer, x - 8 + c, py, final_shade);
        }
    }

    /* If holding ball on court (and not shooting in air) */
    if (has_ball && !is_shooting) {
        int ball_x = x + (facing_left ? -9 : 9);
        int bounce_y = y - 2 + (((int)(anim_time * 6.0f) % 2) ? 3 : -1);
        allstar_renderer_draw_ball_ex(renderer, ball_x, bounce_y, 0, anim_time);
    }
}

void allstar_renderer_draw_player(AllStarRenderer *renderer, int32_t x, int32_t y, bool is_p1, bool has_ball, bool is_shooting, float anim_time) {
    allstar_renderer_draw_player_ex(renderer, x, y, is_p1, is_p1 ? 0x90 : 0x91, has_ball, is_shooting, false, anim_time, false);
}

void allstar_renderer_present(AllStarRenderer *renderer) {
    (void)renderer;
}
