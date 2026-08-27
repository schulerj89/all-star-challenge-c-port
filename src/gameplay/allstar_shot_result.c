#include "allstar_shot_result.h"

/* $1AF9, in slot order.  Slots 2 and 8 both land on $1E74. */
static const uint16_t SHOT_RESULT_TABLE[ALLSTAR_SHOT_RESULT_SLOTS] = {
    0x1BA7u, 0x1B3Fu, 0x1E74u, 0x1B93u, 0x1B53u, 0x1BBDu,
    0x1B59u, 0x1B99u, 0x1E74u, 0x1B45u, 0x1BADu
};

const uint16_t* allstar_shot_result_table(int *count) {
    if (count) *count = ALLSTAR_SHOT_RESULT_SLOTS;
    return SHOT_RESULT_TABLE;
}

/*
 * $1B3F..$1BBC.  Every handler is the same three instructions -- load a signed
 * horizontal velocity into $C0A0/$C0A1 -- followed by one of three exits.  The
 * values come in mirrored pairs except for the settle pair, which is
 * deliberately asymmetric.
 */
bool allstar_shot_handler(uint16_t entry, AllStarShotHandler *out) {
    if (!out) return false;
    switch (entry) {
    case 0x1B3Fu: out->velocity = -50;  out->route = ALLSTAR_SHOT_ROUTE_BOUNCE; return true;
    case 0x1B45u: out->velocity =  50;  out->route = ALLSTAR_SHOT_ROUTE_BOUNCE; return true;
    case 0x1B93u: out->velocity =  110; out->route = ALLSTAR_SHOT_ROUTE_BOUNCE; return true;
    case 0x1B99u: out->velocity = -110; out->route = ALLSTAR_SHOT_ROUTE_BOUNCE; return true;
    case 0x1B53u: out->velocity =  165; out->route = ALLSTAR_SHOT_ROUTE_SETTLE; return true;
    case 0x1B59u: out->velocity = -147; out->route = ALLSTAR_SHOT_ROUTE_SETTLE; return true;
    case 0x1BA7u: out->velocity = -110; out->route = ALLSTAR_SHOT_ROUTE_CUE;    return true;
    case 0x1BADu: out->velocity =  110; out->route = ALLSTAR_SHOT_ROUTE_CUE;    return true;
    default: return false;
    }
}

/*
 * $1B64..$1B90.  The ROM tests B, the velocity's high byte, so the branch is on
 * the sign rather than on the magnitude.
 */
void allstar_shot_settle(int16_t velocity, uint8_t *bounce_count, uint8_t *height,
                         AllStarShotSettle *out) {
    if (!out) return;
    out->counts_bounce = false;
    out->lowers_height = false;
    out->resets_vertical = false;

    if (velocity >= 0) {                                            /* $1B64-$1B66 */
        out->resets_vertical = true;                                /* $1B7E-$1B8A */
        return;
    }

    out->counts_bounce = true;                                      /* $1B68-$1B6C */
    if (bounce_count) *bounce_count = (uint8_t)(*bounce_count + 1u);

    if (bounce_count && *bounce_count >= 0x02u) {                   /* $1B6F-$1B71 */
        out->lowers_height = true;                                  /* $1B73-$1B7B */
        if (height) *height = (uint8_t)(*height - ALLSTAR_SHOT_HEIGHT_DROP);
        return;
    }
    out->resets_vertical = true;
}

/* $1E7A..$1E8C */
int16_t allstar_shot_reverse(int16_t vertical) {
    return (int16_t)(0 - (uint16_t)vertical);
}

/* $1E8F..$1EBC */
int16_t allstar_shot_damping(uint8_t mode, uint8_t suppress, uint8_t heavy_request,
                             bool *clears_request, bool *sets_c128) {
    if (clears_request) *clears_request = false;
    if (sets_c128) *sets_c128 = false;

    if (mode == 0x01u) {                                            /* $1E8F-$1E92 */
        if (sets_c128) *sets_c128 = true;                           /* $1E94 */
        if (suppress == 0) return ALLSTAR_SHOT_DAMP_FREETHROW;      /* $1E9C-$1EA2 */
        return ALLSTAR_SHOT_DAMP_NORMAL;                            /* $1EAA */
    }

    if (suppress != 0) return ALLSTAR_SHOT_DAMP_NORMAL;             /* $1EAD-$1EB1 */
    if (heavy_request == 0) return ALLSTAR_SHOT_DAMP_NORMAL;        /* $1EB3-$1EB6 */

    if (clears_request) *clears_request = true;                     /* $1EBB-$1EBC */
    return ALLSTAR_SHOT_DAMP_HEAVY;                                 /* $1EB8 */
}

/* $1ECC..$1EDA */
AllStarShotTick allstar_shot_tick(uint8_t *timer, uint8_t *remaining) {
    if (!timer || !remaining) return ALLSTAR_SHOT_TICK_WAIT;

    *timer = (uint8_t)(*timer - 1u);                                /* $1ECF */
    if (*timer != 0) return ALLSTAR_SHOT_TICK_WAIT;                 /* $1ED0 */

    *timer = ALLSTAR_SHOT_RIM_RELOAD;                               /* $1ED1 */
    if (*remaining == 0) return ALLSTAR_SHOT_TICK_IDLE;             /* $1ED7 */

    *remaining = (uint8_t)(*remaining - 1u);                        /* $1ED8 */
    return ALLSTAR_SHOT_TICK_ADVANCE;
}

/*
 * $1EF4..$1F29.  The rim cue fires once, on the step where the counter equals
 * the threshold; the score lands only when the counter has run out.
 */
void allstar_shot_outcome(uint8_t remaining, uint8_t cue_select, uint8_t shooter,
                          uint8_t three_point, AllStarShotScore *out) {
    uint8_t threshold;

    if (!out) return;
    out->outcome = ALLSTAR_SHOT_NOTHING;
    out->sound = 0;
    out->score_address = 0;
    out->points = 0;

    threshold = (cue_select == 0) ? 3u : 2u;                        /* $1EF4-$1EFC */

    if (remaining == threshold) {                                   /* $1EFE-$1F02 */
        out->outcome = ALLSTAR_SHOT_RIM_CUE;
        out->sound = ALLSTAR_SHOT_SOUND_RIM;                        /* $1F26 */
        return;
    }
    if (remaining != 0) return;                                     /* $1F04-$1F05 */

    out->outcome = ALLSTAR_SHOT_SCORE;
    out->sound = ALLSTAR_SHOT_SOUND_SCORE;                          /* $1F06 */
    out->score_address = (shooter == 0x02u) ? ALLSTAR_SHOT_SCORE_2  /* $1F0B-$1F17 */
                                            : ALLSTAR_SHOT_SCORE_1;
    out->points = (three_point != 0) ? 3u : 2u;                     /* $1F1A-$1F21 */
}
