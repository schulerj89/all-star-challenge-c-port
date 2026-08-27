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

/* ---- $73C9..$74A7: the exits $7190 jumps to ---- */

#define ALLSTAR_CPU_BALL_COARSE   0xC0A3u  /* $7476 */
#define ALLSTAR_CPU_BALL_FINE     0xC0A7u
#define ALLSTAR_CPU_BALL_MIN      0x28u    /* $7489 */
#define ALLSTAR_CPU_FACE_COMMIT   0x32u    /* $7468 writes this to $C0F9 */
#define ALLSTAR_CPU_FACE_ROLL_MAX 0x07u    /* $7445 */
#define ALLSTAR_CPU_RELEASE_STATE 0x0Du    /* $7416 */
#define ALLSTAR_CPU_RELEASE_ROLL  0x0Au    /* $741C */
#define ALLSTAR_CPU_REQUEST_BIT   0x01u    /* $7499 */

typedef enum {
    ALLSTAR_CPU_STEER_BALL = 0,   /* $7476 aims at $C0A3/$C0A7 */
    ALLSTAR_CPU_STEER_TARGET      /* $749E aims at $C102/$C101 */
} AllStarCpuSteer;

/* Which pair $74BB is handed.  $749E is what consumes the stored target. */
void allstar_cpu_steer_source(AllStarCpuSteer which, uint16_t *coarse, uint16_t *fine);

/*
 * $7481..$7495.  Three gates stand between chasing the ball and asking for an
 * action: $C0FD must be set, $C0AB must have reached $28, and the roll in
 * $FFFE must come in under the skill-scaled threshold from $7629.
 */
bool allstar_cpu_requests_action(uint8_t gate, uint8_t ball_state,
                                 uint8_t roll, uint8_t threshold);

/* $7496..$749D: the request byte is the $C0FE base with bit 0 forced on. */
uint8_t allstar_cpu_action_request(uint8_t base);

typedef struct {
    uint8_t facing;         /* $01 when the opponent is below us, else $02 */
    uint8_t commit_frames;  /* $C0F9 */
} AllStarCpuFacing;

/*
 * $7443..$7473.  A roll of $07 or more skips this entirely and just steers.
 * Otherwise the two coarse fields are compared and the loser's side decides
 * which way we face, held for $32 frames.
 */
bool allstar_cpu_face_opponent(uint8_t roll, uint8_t own_coarse, uint8_t other_coarse,
                               AllStarCpuFacing *out);

typedef enum {
    ALLSTAR_CPU_HOLD_RUNNING = 0,  /* $7437, still committed */
    ALLSTAR_CPU_HOLD_EXPIRED       /* $7443, free to choose again */
} AllStarCpuHold;

/* $7431..$7441: the $C0F9 commit counts down before anything else happens. */
AllStarCpuHold allstar_cpu_hold(uint8_t *frames);

/*
 * $7411..$742D.  The release fires immediately when the record field at $0F is
 * not $0D and the roll is under $0A; otherwise it waits for the $C0FF counter
 * to reach one.
 */
bool allstar_cpu_release(uint8_t field_0f, uint8_t roll, uint8_t counter);

/* ---- $73C9..$7410: the head that chooses among the exits ---- */

#define ALLSTAR_CPU_HEAD_SHOOT_STATE  0x02u  /* $73CC, $C0FA           */
#define ALLSTAR_CPU_HEAD_COMMIT_TIMER 0x2Au  /* $7401, into $C0FF      */
#define ALLSTAR_CPU_HEAD_RELEASE_AT   0x25u  /* $740D                  */
#define ALLSTAR_CPU_HEAD_LANE_Y       0x60u  /* $73E0, field +$15      */
#define ALLSTAR_CPU_HEAD_LANE_ROLL    0x30u  /* $73E6, $FFFB           */
#define ALLSTAR_CPU_HEAD_OUTER_MARGIN 0x1Eu  /* $73EA                  */
#define ALLSTAR_CPU_HEAD_INNER_MARGIN 0x1Au  /* $73F1                  */
#define ALLSTAR_CPU_HEAD_WIDE_MARGIN  0x12u  /* $73FA                  */

typedef enum {
    ALLSTAR_CPU_HEAD_SHOOT = 0,     /* $73CE, on to the $756C decision  */
    ALLSTAR_CPU_HEAD_DEFEND,        /* $73D5 and $73FF, on to $742E     */
    ALLSTAR_CPU_HEAD_COMMIT,        /* $7406, on to $7496               */
    ALLSTAR_CPU_HEAD_COUNTDOWN,     /* $7420, the timer is still running */
    ALLSTAR_CPU_HEAD_RELEASE_CHECK  /* $7411, it hit exactly $25        */
} AllStarCpuHeadRoute;

typedef struct {
    AllStarCpuHeadRoute route;
    uint8_t timer;   /* $C0FF on return */
} AllStarCpuHead;

/*
 * $73C9.  The entry to everything above: it reads the shoot state in $C0FA and
 * the commit timer in $C0FF and picks which of the exits runs.
 *
 * With the timer at one it decides whether to commit, and the test worth
 * noticing is an annulus: from the exact lane row $60, with a low roll, the
 * actor must be inside the $1E box but OUTSIDE the $1A one.  Miss any of that
 * and it falls back to the plain $12 box; miss that too and it defends.
 */
void allstar_cpu_head_73c9(uint8_t shoot_state, uint8_t timer,
                           float center_x, float ground_y, uint8_t roll,
                           AllStarCpuHead *out);

#endif /* ALLSTAR_CPU_TARGET_H */
