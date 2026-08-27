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

/* $414B..$4292 */
int allstar_select_card_layout(AllStarSelectCardOp *out, int max) {
    static const AllStarSelectCardOp OPS[ALLSTAR_SELECT_CARD_OPS] = {
        { ALLSTAR_SELECT_CARD_RULE_TOP,    ALLSTAR_SELECT_CARD_RULE,   0xFFu, 0x00u, 0x00u, 1u,  0u },
        { ALLSTAR_SELECT_CARD_FRAME,       ALLSTAR_SELECT_CARD_ROW,    0xFFu, 0x00u, 0x01u, 16u, 0u },
        { ALLSTAR_SELECT_CARD_RULE_BOTTOM, ALLSTAR_SELECT_CARD_RULE,   0xFFu, 0x00u, 0x11u, 1u,  0u },
        { ALLSTAR_SELECT_CARD_BLOCK_A,     ALLSTAR_SELECT_TILE_ARRAY,  0xFFu, 0x04u, 0x01u, 6u,  4u },
        { ALLSTAR_SELECT_CARD_BLOCK_B,     ALLSTAR_SELECT_TILE_ARRAY,  0xFFu, 0x0Cu, 0x02u, 4u,  4u },
        { ALLSTAR_SELECT_CARD_NAME_FIELD,  0u, ALLSTAR_SELECT_FIELD_NAME,   0x01u, 0x08u, 1u, 0u },
        { ALLSTAR_SELECT_CARD_SURNAME,     0u, 0xFFu,                       0x01u, 0x08u, 1u, 0u },
        { ALLSTAR_SELECT_CARD_HEIGHT,      ALLSTAR_SELECT_LABEL_HEIGHT, ALLSTAR_SELECT_FIELD_HEIGHT, 0x02u, 0x0Bu, 1u, 0u },
        { ALLSTAR_SELECT_CARD_WEIGHT,      ALLSTAR_SELECT_LABEL_WEIGHT, ALLSTAR_SELECT_FIELD_WEIGHT, 0x02u, 0x0Du, 1u, 0u },
        { ALLSTAR_SELECT_CARD_PPG,         ALLSTAR_SELECT_LABEL_PPG,    ALLSTAR_SELECT_FIELD_PPG,    0x02u, 0x0Fu, 1u, 0u }
    };
    int count = ALLSTAR_SELECT_CARD_OPS;
    int i;
    if (!out || max <= 0) return 0;
    if (count > max) count = max;
    for (i = 0; i < count; i++) out[i] = OPS[i];
    return count;
}

/* $418D */
uint16_t allstar_select_portrait_slot(uint8_t roster_id) {
    return (uint16_t)(ALLSTAR_SELECT_PORTRAIT_TABLE + (uint16_t)(roster_id * 2u));
}

/*
 * $4199..$41C5.
 *
 * The ROM's decrement loop at $41C2 always runs $18 times from blank_index + 1,
 * so for the values $42A2 actually holds ($00, $14, $17) it writes past the end
 * of the 24-byte array into whatever follows $C1B8.  Only the first 24 entries
 * are ever drawn, so this clamps at the array instead of reproducing the
 * overrun; every tile the screen shows is identical either way.
 */
void allstar_select_punch_tiles(uint8_t blank_index, uint8_t *tiles) {
    uint8_t i;
    if (!tiles) return;

    for (i = 0; i < ALLSTAR_SELECT_TILE_COUNT; i++) {               /* $4199-$41A3 */
        tiles[i] = (uint8_t)(i + 1u);
    }
    if (blank_index == 0xFFu) return;                               /* $41B0-$41B2 */
    if (blank_index >= ALLSTAR_SELECT_TILE_COUNT) return;

    tiles[blank_index] = 0;                                         /* $41BE */
    for (i = (uint8_t)(blank_index + 1u); i < ALLSTAR_SELECT_TILE_COUNT; i++) {
        tiles[i] = (uint8_t)(tiles[i] - 1u);                        /* $41C2-$41C5 */
    }
}

/* $41E0-$41E6 */
uint8_t allstar_select_tile_base(uint8_t blank_index) {
    return (blank_index == 0xFFu) ? 25u : 24u;
}

/* $41E7-$41F6 */
uint16_t allstar_select_mark_offset(const uint8_t *stream, uint16_t length, uint8_t roster_id) {
    uint16_t i = 0;
    uint8_t remaining = roster_id;

    if (!stream) return 0;
    while (remaining > 0 && i < length) {                           /* $41F0-$41F6 */
        if (stream[i] == ALLSTAR_SELECT_MARK_DELIM) remaining--;
        i++;
    }
    return i;
}

/* $41F8..$4215 */
void allstar_select_build_tiles(const uint8_t *marks, uint8_t mark_count,
                                uint8_t base, uint8_t *tiles) {
    uint8_t running = 0;
    uint8_t next_mark = 0;
    uint8_t i;

    if (!tiles) return;
    for (i = 0; i < ALLSTAR_SELECT_TILE_COUNT; i++) {
        if (marks && next_mark < mark_count && marks[next_mark] == i) {
            next_mark++;                                            /* $4205-$4208 */
            tiles[i] = 0;
        } else {
            tiles[i] = (uint8_t)(running + base);                   /* $420A-$420B */
            running++;                                              /* $420D */
        }
    }
}

/* $2EA3 */
void allstar_select_fill(uint8_t *slots, uint8_t count, uint8_t value) {
    uint8_t i;
    if (!slots) return;
    for (i = 0; i < count; i++) slots[i] = value;
}

/*
 * $2E73 and $2E8C mark one side's slots empty with $80; $2E70 calls $2E8C and
 * then falls into $2E73, so entering there clears both sides.
 */
int allstar_select_clear_runs(uint8_t pass, AllStarSelectClearRun *out, int max) {
    static const AllStarSelectClearRun LEFT[ALLSTAR_SELECT_CLEAR_RUNS] = {
        { 0xC0BFu, 4u }, { 0xC0CBu, 2u }, { 0xC0D1u, 1u }      /* $2E73 */
    };
    static const AllStarSelectClearRun RIGHT[ALLSTAR_SELECT_CLEAR_RUNS] = {
        { 0xC0C3u, 4u }, { 0xC0CDu, 2u }, { 0xC0D2u, 1u }      /* $2E8C */
    };
    const AllStarSelectClearRun *src = (pass == ALLSTAR_SELECT_PASS_2) ? RIGHT : LEFT;
    int count = ALLSTAR_SELECT_CLEAR_RUNS;
    int i;
    if (!out || max <= 0) return 0;
    if (count > max) count = max;
    for (i = 0; i < count; i++) out[i] = src[i];
    return count;
}

/* $2DD2 */
uint16_t allstar_select_record_offset(const uint8_t *table, uint16_t length, uint8_t roster_id) {
    uint16_t i = 0;
    uint8_t remaining = roster_id;

    if (!table) return 0;
    if (remaining == 0) return 0;                                   /* $2DD7-$2DD8 */
    while (remaining > 0 && i < length) {                           /* $2DDA-$2DE0 */
        if (table[i] == ALLSTAR_SELECT_RECORD_DELIM) remaining--;
        i++;
    }
    return i;
}

/* $2DBE */
uint16_t allstar_select_record_buffer(uint8_t slot) {
    return (slot == 0x01u) ? ALLSTAR_SELECT_SLOT_1_BUFFER          /* $2DC4-$2DC7 */
                           : ALLSTAR_SELECT_SLOT_2_BUFFER;         /* $2DC9 */
}

/*
 * $2D93.  C starts at seven and climbs in sevens for at most $1A steps while it
 * stays at or below the seed, so D ends up as the step count the seed reaches.
 * If that lands on the id already held in $FFAC the seed gains $14 and the whole
 * walk restarts, which is how the CPU avoids picking the human's player.
 *
 * $2D85 is the caller: it runs this walk, copies the result into $FFAC and
 * loads it as slot 1 as well.
 *
 * The retry guard is this port's addition, not the cartridge's: $2DB0 loops with
 * no bound, so a seed that keeps colliding would spin forever. The guard only
 * engages after $1A retries, which no reachable seed reaches.
 */
uint8_t allstar_select_cpu_opponent(uint8_t seed, uint8_t taken, uint8_t *seed_out) {
    uint8_t guard = 0;

    for (;;) {
        uint8_t index = 0;                                          /* D */
        uint8_t threshold = ALLSTAR_SELECT_CPU_STRIDE;              /* C */
        uint8_t steps = ALLSTAR_SELECT_CPU_LIMIT;                   /* B */

        while (seed >= threshold && steps > 0) {                    /* $2D98-$2DA3 */
            threshold = (uint8_t)(threshold + ALLSTAR_SELECT_CPU_STRIDE);
            index++;
            steps--;
        }
        if (index != taken || guard >= ALLSTAR_SELECT_CPU_LIMIT) {  /* $2DA5-$2DA8 */
            if (seed_out) *seed_out = seed;
            return index;                                           /* $2DB2-$2DB5 */
        }
        seed = (uint8_t)(seed + ALLSTAR_SELECT_CPU_RETRY);           /* $2DAA-$2DB0 */
        guard++;
    }
}

/* $2AB5: the menu blip, sound $0F handed to $2F88. */
uint8_t allstar_select_blip_sound(void) {
    return ALLSTAR_SELECT_BLIP_SOUND;
}

/* $2DEA */
void allstar_select_prompt_shape(uint8_t prompt, uint8_t player_count, uint8_t picker,
                                 AllStarSelectPrompt *out) {
    if (!out) return;
    out->prompt_sound = prompt;                                     /* $2DEE-$2DEF */
    out->hold_frames = ALLSTAR_SELECT_PROMPT_HOLD;                  /* $2E0A */
    out->announces_player = (player_count != 0x01u);                /* $2DF2-$2DF5 */
    if (!out->announces_player) {
        out->player_sound = 0;
        return;
    }
    out->player_sound = (picker == 0x01u) ? ALLSTAR_SELECT_PROMPT_P1  /* $2DF7-$2DFF */
                                          : ALLSTAR_SELECT_PROMPT_P2;
}

/*
 * $780A.  Four nibbles, high to low, each plus the digit base.  Unlike $1726
 * this never blanks a leading zero.
 */
void allstar_select_wide_digits(uint16_t bcd_value, uint8_t *digits) {
    uint8_t high = (uint8_t)(bcd_value >> 8);
    uint8_t low = (uint8_t)(bcd_value & 0xFFu);
    if (!digits) return;
    digits[0] = (uint8_t)(((high & 0xF0u) >> 4) + ALLSTAR_SELECT_DIGIT_BASE);
    digits[1] = (uint8_t)((high & 0x0Fu) + ALLSTAR_SELECT_DIGIT_BASE);
    digits[2] = (uint8_t)(((low & 0xF0u) >> 4) + ALLSTAR_SELECT_DIGIT_BASE);
    digits[3] = (uint8_t)((low & 0x0Fu) + ALLSTAR_SELECT_DIGIT_BASE);
}
