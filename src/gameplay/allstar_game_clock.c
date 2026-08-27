#include "allstar_game_clock.h"

/* $79EE..$79F6 */
bool allstar_clock_suppressed(uint8_t freeze, uint8_t halt) {
    return (freeze != 0) || (halt != 0);
}

/* $79F7..$7A29 */
void allstar_clock_ownership(uint8_t mode, uint8_t possession, uint8_t enable,
                             AllStarClockOwnership *out) {
    uint8_t bits;

    if (!out) return;

    if (mode == 0x03u || possession == 0) {                         /* $79FC-$7A03 */
        out->resets_player_1 = true;                                /* $7A09 */
        out->resets_player_2 = true;                                /* $7A0C */
        bits = 0x00u;                                               /* $7A05 */
    } else if (possession == 0x02u) {                               /* $7A11-$7A13 */
        out->resets_player_1 = true;                                /* $7A22 */
        out->resets_player_2 = false;
        bits = ALLSTAR_CLOCK_ENABLE_PLAYER_2;                       /* $7A1E */
    } else {
        out->resets_player_1 = false;
        out->resets_player_2 = true;                                /* $7A19 */
        bits = ALLSTAR_CLOCK_ENABLE_PLAYER_1;                       /* $7A15 */
    }

    /* $7A25-$7A29: keep bit 0, replace bits 1 and 2. */
    out->enable = (uint8_t)((enable & ALLSTAR_CLOCK_ENABLE_GAME) | bits);
}

/* $7A2A..$7A2F */
bool allstar_clock_tick(uint8_t *counter) {
    if (!counter) return false;
    *counter = (uint8_t)(*counter - 1u);                            /* $7A2D */
    if (*counter != 0) return false;                                /* $7A2E */
    *counter = ALLSTAR_CLOCK_TICK_RELOAD;                           /* $7A2F */
    return true;
}

/* $7A38..$7A5A */
bool allstar_clock_warns(uint8_t mode, uint16_t game_clock) {
    uint8_t seconds = (uint8_t)(game_clock & 0xFFu);
    uint8_t minutes = (uint8_t)(game_clock >> 8);

    if (mode == 0x01u || mode == 0x02u) return false;                /* $7A3A-$7A40 */
    if (minutes != 0) return false;                                  /* $7A49-$7A4B */
    if (seconds >= ALLSTAR_CLOCK_WARN_BELOW) return false;           /* $7A4E-$7A50 */
    if (seconds == 0x01u) return false;                              /* $7A52-$7A53 */
    return true;                                                     /* $7A55 */
}

/*
 * $7A7E and $7A86 both reach `daa` with carry clear, and neither operand can be
 * zero on its path, so the only adjustment that ever fires is the half-carry
 * subtract of six.
 */
static uint8_t clock_bcd_dec(uint8_t value) {
    bool half_borrow = ((value & 0x0Fu) == 0x00u);
    value = (uint8_t)(value - 1u);
    if (half_borrow) value = (uint8_t)(value - 0x06u);
    return value;
}

/* $7A71..$7A8F */
uint16_t allstar_clock_decrement(uint16_t clock) {
    uint8_t seconds = (uint8_t)(clock & 0xFFu);
    uint8_t minutes = (uint8_t)(clock >> 8);

    if ((uint8_t)(seconds | minutes) == 0) return clock;             /* $7A77-$7A78 */

    if (seconds != 0) {                                              /* $7A7B-$7A7C */
        seconds = clock_bcd_dec(seconds);                            /* $7A7E-$7A80 */
    } else {
        seconds = ALLSTAR_CLOCK_SECONDS_WRAP;                        /* $7A83 */
        minutes = clock_bcd_dec(minutes);                            /* $7A85-$7A88 */
    }
    return (uint16_t)(((uint16_t)minutes << 8) | seconds);           /* $7A89-$7A8D */
}
