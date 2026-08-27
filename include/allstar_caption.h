#ifndef ALLSTAR_CAPTION_H
#define ALLSTAR_CAPTION_H

#include "allstar_types.h"
#include "allstar_asset_pack.h"

/*
 * The screen-caption script, ported from $07DE..$0801.
 *
 * $07E3 takes a one-based layout index in A, skips that many $FF markers from
 * $0802, and then walks four-byte records -- column, row, and a pointer to a
 * tile stream -- handing each to $06C0 until the next byte is $FF.  $07DE is
 * the same thing with a $047E clear in front of it.
 *
 * Between them these draw every prompt in the game: "SELECT PLAYER", "SELECT
 * YOUR OPPONENT", "CHOOSE TEN POSITIONS", the bracket's "GAME 1".."GAME 6",
 * "SHOTS ATTEMPTED" / "SHOTS MADE", "CONGRATULATIONS YOU ARE THE CHAMPION",
 * "EXTENDED TIME n MINUTE" and "WAITING FOR PLAYER n".
 *
 * The streams are tile indices, not ASCII -- $0A15 is "PLAYER " followed by
 * tile $82 -- so they stay bytes here and $06C0 decides what they look like.
 */

#define ALLSTAR_CAPTION_CLEAR_FIRST 0x047Eu  /* $07DF */
#define ALLSTAR_CAPTION_SCRIPT_BASE 0x0802u  /* $07E4 */
#define ALLSTAR_CAPTION_TERMINATOR  0xFFu    /* $07E8, $07FD */

typedef struct {
    uint8_t row;           /* $06C0's E, scaled by the map stride */
    uint8_t column;        /* $06C0's D, the destination's low byte */
    const uint8_t *tiles;  /* into the pack's pool                */
    uint8_t length;        /* including the tile carrying bit 7   */
    uint16_t rom_pointer;  /* where the cartridge keeps it        */
} AllStarCaptionDraw;

/*
 * $07E3.  Fills `out` with the draws layout `index` performs, in ROM order,
 * and returns how many there were.  An index with no layout returns zero.
 */
int allstar_caption_layout_07e3(const AllStarAssetPack *pack, uint8_t index,
                                AllStarCaptionDraw *out, int max);

/* $07DE runs $047E first; the draws are otherwise identical. */
bool allstar_caption_clears_first_07de(uint16_t entry);

/*
 * The last tile of a stream carries bit 7.  This strips it, which is what a
 * renderer wants when it maps the stream onto the shared gameplay font.
 */
uint8_t allstar_caption_tile(uint8_t stream_byte);

#endif /* ALLSTAR_CAPTION_H */
