#ifndef ALLSTAR_TYPES_H
#define ALLSTAR_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ALLSTAR_GB_WIDTH  160
#define ALLSTAR_GB_HEIGHT 144

/* RGBA 32-bit color */
typedef uint32_t AllStarColor;

static inline AllStarColor allstar_make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

/* Palette Styles */
typedef enum {
    ALLSTAR_PALETTE_DMG_ORIGINAL = 0, /* Classic Green LCD */
    ALLSTAR_PALETTE_POCKET_BW,        /* Game Boy Pocket Grayscale */
    ALLSTAR_PALETTE_MODERN_VIBRANT,   /* High contrast modern */
    ALLSTAR_PALETTE_COUNT
} AllStarPaletteStyle;

/* Standard 4-shade grayscale DMG palette colors */
typedef struct {
    AllStarColor shades[4];
} AllStarPalette;

/* 2D Vector */
typedef struct {
    float x;
    float y;
} AllStarVec2;

typedef struct {
    int32_t x;
    int32_t y;
} AllStarPoint;

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} AllStarRect;

/* 8x8 Tile (2bpp decoded to 8bpp indices 0..3) */
typedef struct {
    uint8_t pixels[64];
} AllStarTile;

/* Sprite representation matching DMG OAM structure */
typedef struct {
    int16_t x;
    int16_t y;
    uint8_t tile_index;
    uint8_t flags; /* bit 7: priority, bit 6: Y-flip, bit 5: X-flip, bit 4: palette */
} AllStarSprite;

/* Input Button Mask */
typedef enum {
    ALLSTAR_BTN_RIGHT  = (1 << 0),
    ALLSTAR_BTN_LEFT   = (1 << 1),
    ALLSTAR_BTN_UP     = (1 << 2),
    ALLSTAR_BTN_DOWN   = (1 << 3),
    ALLSTAR_BTN_A      = (1 << 4),
    ALLSTAR_BTN_B      = (1 << 5),
    ALLSTAR_BTN_SELECT = (1 << 6),
    ALLSTAR_BTN_START  = (1 << 7)
} AllStarButtonMask;

typedef struct {
    uint8_t buttons_held;
    uint8_t buttons_pressed;
    uint8_t buttons_released;
} AllStarInput;

#endif /* ALLSTAR_TYPES_H */
