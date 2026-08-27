#include "allstar_cpu_target.h"

/* $7185 */
static const uint16_t CPU_MODE_TABLE[ALLSTAR_CPU_MODE_SLOTS] = {
    0x7190u, 0x718Fu, 0x74A8u, 0x718Fu, 0x7190u
};

/* The three-byte tables $761B indexes. */
static const uint8_t THRESHOLD_7626[ALLSTAR_CPU_SKILLS] = { 0x1Bu, 0x10u, 0x07u };
static const uint8_t THRESHOLD_7629[ALLSTAR_CPU_SKILLS] = { 0x19u, 0x50u, 0x96u };
static const uint8_t THRESHOLD_762C[ALLSTAR_CPU_SKILLS] = { 0x04u, 0x19u, 0x46u };
static const uint8_t THRESHOLD_7635[ALLSTAR_CPU_SKILLS] = { 0x1Eu, 0x14u, 0x04u };

/* $731C and $7324, four (coarse, fine) pairs each. */
static const AllStarCpuTarget SPOTS_LOW[ALLSTAR_CPU_SPOTS] = {
    { 0x68u, 0x10u }, { 0x8Cu, 0x1Cu }, { 0x7Cu, 0x10u }, { 0x98u, 0x40u }
};
static const AllStarCpuTarget SPOTS_HIGH[ALLSTAR_CPU_SPOTS] = {
    { 0x68u, 0x94u }, { 0x8Cu, 0x8Cu }, { 0x7Cu, 0x90u }, { 0x98u, 0x78u }
};

const uint16_t* allstar_cpu_mode_table(int *count) {
    if (count) *count = ALLSTAR_CPU_MODE_SLOTS;
    return CPU_MODE_TABLE;
}

/* $761B..$7625 */
uint8_t allstar_cpu_threshold(const uint8_t *table, uint8_t skill) {
    if (!table) return 0;
    if (skill == 0 || skill > ALLSTAR_CPU_SKILLS) return table[0];
    return table[skill - 1u];                                       /* $761D dec a */
}

const uint8_t* allstar_cpu_threshold_table(uint16_t address) {
    switch (address) {
    case 0x7626u: return THRESHOLD_7626;
    case 0x7629u: return THRESHOLD_7629;
    case 0x762Cu: return THRESHOLD_762C;
    case 0x7635u: return THRESHOLD_7635;
    default:      return NULL;
    }
}

/* $7190..$7197 */
AllStarCpuEntry allstar_cpu_entry(uint8_t possession, uint8_t context) {
    return (possession == context) ? ALLSTAR_CPU_SAME_POSSESSION
                                   : ALLSTAR_CPU_NEW_POSSESSION;
}

/* $719A..$71AC */
const uint16_t* allstar_cpu_cleared_state(int *count) {
    static const uint16_t CLEARED[ALLSTAR_CPU_CLEARED] = {
        0xC0F7u, 0xC0FAu, 0xC0F9u, 0xC0F8u, 0xC100u, 0xC106u
    };
    if (count) *count = ALLSTAR_CPU_CLEARED;
    return CLEARED;
}

/* $7237..$7288 */
void allstar_cpu_step_target(uint8_t field_06, uint8_t field_15, uint8_t direction,
                             AllStarCpuTarget *out) {
    uint8_t coarse;
    uint8_t fine;

    if (!out) return;
    coarse = (uint8_t)(field_06 + 0x08u);                           /* $7242-$7245 */
    fine = (uint8_t)(field_15 + 0x04u);                             /* $724A-$724E */

    if ((direction & 0x01u) != 0) {                                 /* $7257 */
        coarse = (uint8_t)(coarse + ALLSTAR_CPU_STEP_LARGE);        /* $7278 */
    } else if ((direction & 0x02u) != 0) {                          /* $725B */
        /* $726F-$7275: the subtract clamps at zero instead of wrapping. */
        coarse = (coarse < ALLSTAR_CPU_STEP_LARGE)
                     ? 0u
                     : (uint8_t)(coarse - ALLSTAR_CPU_STEP_LARGE);
    } else if ((direction & 0x04u) != 0) {                          /* $725F */
        fine = (uint8_t)(fine - ALLSTAR_CPU_STEP_SMALL);            /* $7269 */
    } else {
        fine = (uint8_t)(fine + ALLSTAR_CPU_STEP_SMALL);            /* $7263 */
    }

    out->field_06 = coarse;
    out->field_15 = fine;
}

/* $728C..$72BC */
void allstar_cpu_center_target(uint8_t field_06, uint8_t field_15, AllStarCpuTarget *out) {
    uint8_t coarse = field_06;

    if (!out) return;

    if (ALLSTAR_CPU_CENTRE_LOW >= coarse) {                         /* $729E-$72A1 */
        coarse = (uint8_t)(coarse + ALLSTAR_CPU_STEP_LARGE);        /* $72AE */
    } else if (ALLSTAR_CPU_CENTRE_HIGH < coarse) {                  /* $72A3-$72A6 */
        coarse = (uint8_t)(coarse - ALLSTAR_CPU_STEP_SMALL);        /* $72B4 */
    } else {
        coarse = (uint8_t)(coarse + ALLSTAR_CPU_STEP_SMALL);        /* $72A8 */
    }

    out->field_06 = coarse;
    out->field_15 = (uint8_t)(field_15 - ALLSTAR_CPU_STEP_SMALL);   /* $72B8 */
}

/* $72EA..$72F4 */
uint16_t allstar_cpu_spot_table(uint8_t ball_height) {
    return (ball_height < ALLSTAR_CPU_SPOT_HEIGHT) ? 0x731Cu : 0x7324u;
}

/* $72F7..$7307 */
int allstar_cpu_spot_index(uint8_t value) {
    uint8_t bound = ALLSTAR_CPU_SPOT_FIRST;
    int i;
    for (i = 0; i < ALLSTAR_CPU_SPOTS - 1; i++) {
        if (value < bound) return i;                                /* $72FD-$72FE */
        bound = (uint8_t)(bound + ALLSTAR_CPU_SPOT_STEP);           /* $7300-$7303 */
    }
    return ALLSTAR_CPU_SPOTS - 1;                                   /* $7307 falls out */
}

/* $7309..$7312 */
bool allstar_cpu_spot(uint8_t ball_height, uint8_t value, AllStarCpuTarget *out) {
    const AllStarCpuTarget *table;
    int index;

    if (!out) return false;
    table = (allstar_cpu_spot_table(ball_height) == 0x731Cu) ? SPOTS_LOW : SPOTS_HIGH;
    index = allstar_cpu_spot_index(value);
    *out = table[index];
    return true;
}

/* $7476 and $749E */
void allstar_cpu_steer_source(AllStarCpuSteer which, uint16_t *coarse, uint16_t *fine) {
    if (which == ALLSTAR_CPU_STEER_BALL) {
        if (coarse) *coarse = ALLSTAR_CPU_BALL_COARSE;              /* $7476 */
        if (fine) *fine = ALLSTAR_CPU_BALL_FINE;                    /* $747A */
        return;
    }
    if (coarse) *coarse = ALLSTAR_CPU_TARGET_X;                     /* $749E */
    if (fine) *fine = ALLSTAR_CPU_TARGET_Y;                         /* $74A2 */
}

/* $7481..$7495 */
bool allstar_cpu_requests_action(uint8_t gate, uint8_t ball_state,
                                 uint8_t roll, uint8_t threshold) {
    if (gate == 0) return false;                                    /* $7484-$7485 */
    if (ball_state < ALLSTAR_CPU_BALL_MIN) return false;            /* $7489-$748B */
    return roll < threshold;                                        /* $7494-$7495 */
}

/* $7496..$749D */
uint8_t allstar_cpu_action_request(uint8_t base) {
    return (uint8_t)(base | ALLSTAR_CPU_REQUEST_BIT);
}

/* $7443..$7473 */
bool allstar_cpu_face_opponent(uint8_t roll, uint8_t own_coarse, uint8_t other_coarse,
                               AllStarCpuFacing *out) {
    if (!out) return false;
    if (roll >= ALLSTAR_CPU_FACE_ROLL_MAX) return false;            /* $7445-$7447 */

    /* $745A: `cp [hl]` compares the opponent against us. */
    out->facing = (other_coarse < own_coarse) ? 0x01u : 0x02u;
    out->commit_frames = ALLSTAR_CPU_FACE_COMMIT;                   /* $7468 */
    return true;
}

/* $7431..$7441 */
AllStarCpuHold allstar_cpu_hold(uint8_t *frames) {
    if (!frames) return ALLSTAR_CPU_HOLD_EXPIRED;
    if (*frames == 0) return ALLSTAR_CPU_HOLD_EXPIRED;              /* $7434-$7435 */
    *frames = (uint8_t)(*frames - 1u);                              /* $7437-$743A */
    return ALLSTAR_CPU_HOLD_RUNNING;
}

/* $7411..$742D */
bool allstar_cpu_release(uint8_t field_0f, uint8_t roll, uint8_t counter) {
    if (field_0f != ALLSTAR_CPU_RELEASE_STATE &&                    /* $7416-$7418 */
        roll < ALLSTAR_CPU_RELEASE_ROLL) {                          /* $741C-$741E */
        return true;                                                /* $7425 */
    }
    return counter == 1u;                                           /* $7420-$7424 */
}
