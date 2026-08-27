#include "allstar_select.h"

/* $4358 and $4360, indexed by $C17F - 1. */
static const uint16_t SELECT_PASS_1[4] = { 0xC0D1u, 0xC0CBu, 0x0000u, 0xC0BFu };
static const uint16_t SELECT_PASS_2[4] = { 0xC0D2u, 0xC0CDu, 0x0000u, 0xC0C3u };

/*
 * $4000 stores the count in $C17F and reaches this decision at $400F..$401D.
 * With two players both passes run.  With one player, modes $01 and $03 jump
 * straight to the single pass at $4034 and return without the $4023 pass.
 */
bool allstar_select_runs_second_pass(uint8_t player_count, uint8_t mode) {
    if (player_count != 0x01u) return true;                         /* $4012 */
    if (mode == 0x01u || mode == 0x03u) return false;               /* $4016-$401D */
    return true;
}

/* $4045, and again at $40C8 inside $40B1's duplicate scan. */
uint16_t allstar_select_destination(uint8_t stage, uint8_t pass) {
    if (stage == 0 || stage > 4u) return 0;
    if (pass == ALLSTAR_SELECT_PASS_2) return SELECT_PASS_2[stage - 1u];
    return SELECT_PASS_1[stage - 1u];
}

/*
 * $406A.  Outside the tournament the prompt is 1, except for a one-player game
 * where player 2 is picking, which uses 2.  Inside the tournament the prompt
 * encodes both the stage and which player is choosing.
 */
uint8_t allstar_select_prompt(uint8_t mode, uint8_t stage, uint8_t picker,
                              uint8_t player_count) {
    bool first_picker = (picker == 0x01u);

    if (mode != 0x04u) {                                            /* $406C-$406E */
        if (first_picker) return 0x01u;                             /* $4070-$4076 */
        if (player_count != 0x01u) return 0x01u;                    /* $4078-$407D */
        return 0x02u;                                               /* $407F */
    }

    /* $4083..$40AD */
    if (first_picker) {
        if (stage == 0x02u) return 0x05u;
        if (stage == 0x01u) return 0x07u;
        return 0x03u;
    }
    if (stage == 0x02u) return 0x06u;
    if (stage == 0x01u) return 0x08u;
    return 0x04u;
}

/* $40B1 at $40C4: $C17F doubled, so the scan covers both passes. */
uint8_t allstar_select_scan_length(uint8_t stage) {
    return (uint8_t)(stage * 2u);
}

/* $4053 sets $C184; the $40F4 loop reads it at $4100..$410D. */
uint8_t allstar_select_buttons(uint8_t player_count, uint8_t picker,
                               uint8_t player_1_held, uint8_t player_2_held) {
    if (picker == 0x01u) return player_1_held;                      /* $4100-$4104 */
    if (player_count == 0x01u) return player_1_held;                /* $4106-$4109 */
    return player_2_held;                                           /* $410B */
}

/* $40F4's loop body, $40F7..$4149. */
AllStarSelectInput allstar_select_step(uint8_t hold_lock, uint8_t held,
                                       uint8_t length, uint8_t *index) {
    if (!index || length == 0) return ALLSTAR_SELECT_IDLE;
    if (hold_lock != 0) return ALLSTAR_SELECT_IDLE;                 /* $40F8-$40FB */

    if ((held & ALLSTAR_SELECT_CONFIRM_MASK) != 0) {                /* $410F-$4111 */
        return ALLSTAR_SELECT_CONFIRMED;
    }
    if ((held & ALLSTAR_SELECT_MOVE_MASK) == 0) {                   /* $4119-$411B */
        return ALLSTAR_SELECT_IDLE;
    }

    if ((held & ALLSTAR_SELECT_BACK_MASK) != 0) {                   /* $4127-$4129 */
        /* $4136-$4140: stepping off the leading $FF scans to the last entry. */
        *index = (*index == 0) ? (uint8_t)(length - 1u) : (uint8_t)(*index - 1u);
    } else {
        /* $412B-$4134: stepping onto the trailing $FF wraps back to $C0D9. */
        *index = (uint8_t)(*index + 1u);
        if (*index >= length) *index = 0;
    }
    return ALLSTAR_SELECT_MOVED;
}

/* $40B1 at $40BA..$40D0. */
bool allstar_select_is_duplicate(const uint8_t *picked, uint8_t scan_length, uint8_t candidate) {
    uint8_t i;
    if (!picked) return false;
    for (i = 0; i < scan_length; i++) {
        if (picked[i] == candidate) return true;                    /* $40C9-$40CB */
    }
    return false;
}
