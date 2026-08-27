#include "allstar_settings_screen.h"

/* $2321, indexed by $FF8F. */
static const uint16_t SETTINGS_TABLE[ALLSTAR_SETTINGS_SLOTS] = {
    0x232Bu, 0x235Fu, 0x236Au, 0x2374u, 0x2330u
};

const uint16_t* allstar_settings_table(int *count) {
    if (count) *count = ALLSTAR_SETTINGS_SLOTS;
    return SETTINGS_TABLE;
}

/*
 * $232B and $2330 both fall into the shared four-row body at $2333, differing
 * only in which cursor table they load.  $2AC8 and $2ADA happen to hold the
 * same four (y,x) pairs.  $235F and $236A each point at a two-byte table, so
 * their cursor never moves, and $2374 has three rows of its own.
 */
bool allstar_settings_screen(uint8_t mode, AllStarSettingsScreen *out) {
    if (!out) return false;
    switch (mode) {
    case 0x00u:                                                     /* $232B */
        out->cursor_table = 0x2AC8u; out->rows = 4u;
        out->row_handlers = 0x2357u; out->wraps_with_mask = true;
        return true;
    case 0x04u:                                                     /* $2330 */
        out->cursor_table = 0x2ADAu; out->rows = 4u;
        out->row_handlers = 0x2357u; out->wraps_with_mask = true;
        return true;
    case 0x01u:                                                     /* $235F */
        out->cursor_table = 0x2AD0u; out->rows = 1u;
        out->row_handlers = 0x2486u; out->wraps_with_mask = false;
        return true;
    case 0x02u:                                                     /* $236A */
        out->cursor_table = 0x2AD2u; out->rows = 1u;
        out->row_handlers = 0x230Eu; out->wraps_with_mask = false;
        return true;
    case 0x03u:                                                     /* $2374 */
        out->cursor_table = 0x2AD4u; out->rows = 3u;
        out->row_handlers = 0x23A4u; out->wraps_with_mask = false;
        return true;
    default:
        return false;
    }
}

/* $2333..$234E and $237C..$239B */
AllStarSettingsResult allstar_settings_step(uint8_t mode, uint8_t new_buttons,
                                            uint8_t *cursor) {
    AllStarSettingsScreen screen;
    bool up;
    bool down;

    if (!cursor) return ALLSTAR_SETTINGS_IDLE;
    if (!allstar_settings_screen(mode, &screen)) return ALLSTAR_SETTINGS_IDLE;

    up = (new_buttons & ALLSTAR_SETTINGS_UP_MASK) != 0;             /* $233A */
    down = (new_buttons & ALLSTAR_SETTINGS_DOWN_MASK) != 0;         /* $233E */
    if (!up && !down) return ALLSTAR_SETTINGS_IDLE;

    /* Modes $01 and $02 reach the same code with a single-row table. */
    if (screen.rows <= 1u) {
        *cursor = 0u;
        return ALLSTAR_SETTINGS_MOVED;
    }

    if (up) {                                                       /* $2347 / $2390 */
        if (screen.wraps_with_mask) {
            *cursor = (uint8_t)((*cursor - 1u) & 0x03u);            /* $234A */
        } else {
            *cursor = (*cursor == 0u) ? (uint8_t)(screen.rows - 1u) /* $2393-$2397 */
                                      : (uint8_t)(*cursor - 1u);
        }
    } else {                                                        /* $2342 / $2386 */
        if (screen.wraps_with_mask) {
            *cursor = (uint8_t)((*cursor + 1u) & 0x03u);
        } else {
            *cursor = (uint8_t)(*cursor + 1u);                      /* $2389-$238D */
            if (*cursor >= screen.rows) *cursor = 0u;
        }
    }
    return ALLSTAR_SETTINGS_MOVED;                                  /* $234E / $239B */
}
