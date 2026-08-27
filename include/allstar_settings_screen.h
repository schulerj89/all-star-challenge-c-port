#ifndef ALLSTAR_SETTINGS_SCREEN_H
#define ALLSTAR_SETTINGS_SCREEN_H

#include "allstar_types.h"

/*
 * The settings screen cursor, ported from $231E and its handlers at $232B,
 * $2330, $235F, $236A and $2374.
 *
 * $231E is the **fourth** mode-indexed dispatcher in the cartridge, alongside
 * $10E1, $2578 and bank 1 $7182.  Each mode gets its own cursor table and its
 * own row count, and the two four-row modes wrap with `and $03` while the
 * three-row mode wraps with explicit compares.
 *
 * Input uses the cartridge's packing: bit 6 is Up, bit 7 is Down.
 */

#define ALLSTAR_SETTINGS_SLOTS     5
#define ALLSTAR_SETTINGS_UP_MASK   0x40u  /* $233A bit 6 */
#define ALLSTAR_SETTINGS_DOWN_MASK 0x80u  /* $233E bit 7 */
#define ALLSTAR_SETTINGS_CURSOR    0xFF9Cu

/* $2321, indexed by $FF8F. */
const uint16_t* allstar_settings_table(int *count);

typedef struct {
    uint16_t cursor_table;  /* the (y,x) pairs handed to $2A9E */
    uint8_t rows;           /* how many entries that table holds */
    uint16_t row_handlers;  /* the list $24E0 dispatches through */
    bool wraps_with_mask;   /* four-row modes use `and $03` */
} AllStarSettingsScreen;

/* Which table, how many rows, and how the wrap is done, for one mode. */
bool allstar_settings_screen(uint8_t mode, AllStarSettingsScreen *out);

typedef enum {
    ALLSTAR_SETTINGS_IDLE = 0,
    ALLSTAR_SETTINGS_MOVED
} AllStarSettingsResult;

/*
 * $2333..$234E for the four-row modes and $237C..$239B for the three-row one.
 * Up is tested before Down, and a mode with a single row still consumes the
 * press without moving.
 */
AllStarSettingsResult allstar_settings_step(uint8_t mode, uint8_t new_buttons,
                                            uint8_t *cursor);

#endif /* ALLSTAR_SETTINGS_SCREEN_H */
