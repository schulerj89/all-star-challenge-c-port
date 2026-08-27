#ifndef ALLSTAR_CPU_TARGET_H
#define ALLSTAR_CPU_TARGET_H

#include "allstar_types.h"

/*
 * The CPU steering update, ported from bank 1 $7182 and $7190.
 *
 * $7182 is the fourth mode-indexed dispatcher in the cartridge.  Everything
 * below it converges on one thing: choosing a target position and writing it to
 * $C101/$C102, which is the pair $7367's coordinate tables also feed.
 *
 * The entity record fields this reads are the same two the $0ADB probes move:
 * offset $06 steps in sixteens and offset $15 in eights.
 *
 * Difficulty comes from $FF97 through $761B, which indexes a three-byte table
 * by the skill level minus one.
 */

#define ALLSTAR_CPU_MODE_SLOTS   5
#define ALLSTAR_CPU_CLEARED      6
#define ALLSTAR_CPU_SKILLS       3
#define ALLSTAR_CPU_TARGET_X     0xC102u
#define ALLSTAR_CPU_TARGET_Y     0xC101u
#define ALLSTAR_CPU_STEP_LARGE   0x10u   /* $7279 / $7270 */
#define ALLSTAR_CPU_STEP_SMALL   0x08u   /* $7264 / $726A */
#define ALLSTAR_CPU_CENTRE_LOW   0x3Cu   /* $729E */
#define ALLSTAR_CPU_CENTRE_HIGH  0x6Cu   /* $72A3 */
#define ALLSTAR_CPU_HOLD_FRAMES  0x3Cu   /* $7284 writes this to $C0FB */
#define ALLSTAR_CPU_SPOT_HEIGHT  0x54u   /* $72F0 */
#define ALLSTAR_CPU_SPOT_FIRST   0x30u   /* $72F9 */
#define ALLSTAR_CPU_SPOT_STEP    0x40u   /* $7301 */
#define ALLSTAR_CPU_SPOTS        4

/* $7185, indexed by $FF8F.  Modes $01 and $03 land on the bare ret at $718F. */
const uint16_t* allstar_cpu_mode_table(int *count);

/* $761B: three thresholds per table, chosen by the skill level in $FF97. */
uint8_t allstar_cpu_threshold(const uint8_t *table, uint8_t skill);

/* The four threshold tables the routine uses, by ROM address. */
const uint8_t* allstar_cpu_threshold_table(uint16_t address);

typedef enum {
    ALLSTAR_CPU_SAME_POSSESSION = 0,  /* $7197 jumps to $72BF */
    ALLSTAR_CPU_NEW_POSSESSION        /* $719A falls through  */
} AllStarCpuEntry;

/* $7190..$7197 */
AllStarCpuEntry allstar_cpu_entry(uint8_t possession, uint8_t context);

/* $719A..$71AC: the six bytes a possession change clears. */
const uint16_t* allstar_cpu_cleared_state(int *count);

typedef struct {
    uint8_t field_06;   /* the coarse axis, stepped in sixteens */
    uint8_t field_15;   /* the fine axis, stepped in eights     */
} AllStarCpuTarget;

/*
 * $7237..$7288.  The direction byte at record + $10 is tested bit by bit, and
 * the first set bit wins; with none set the fine axis steps forward.  Stepping
 * the coarse axis down clamps at zero rather than wrapping.
 */
void allstar_cpu_step_target(uint8_t field_06, uint8_t field_15, uint8_t direction,
                             AllStarCpuTarget *out);

/*
 * $728C..$72BC.  Pulls the coarse axis toward the middle of the court: below
 * $3C it gains sixteen, above $6C it loses eight, and in between it gains
 * eight.  The fine axis always loses eight.
 */
void allstar_cpu_center_target(uint8_t field_06, uint8_t field_15, AllStarCpuTarget *out);

/* $72EA..$72F4: the ball height picks which spot table is used. */
uint16_t allstar_cpu_spot_table(uint8_t ball_height);

/* $72F7..$7307: bucket a value into one of four spots, $30 then steps of $40. */
int allstar_cpu_spot_index(uint8_t value);

/* $7309..$7312: the chosen spot, ready for $C101/$C102. */
bool allstar_cpu_spot(uint8_t ball_height, uint8_t value, AllStarCpuTarget *out);

#endif /* ALLSTAR_CPU_TARGET_H */
