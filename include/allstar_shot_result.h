#ifndef ALLSTAR_SHOT_RESULT_H
#define ALLSTAR_SHOT_RESULT_H

#include "allstar_types.h"

/*
 * Rim and backboard outcomes, ported from the $1AF9 dispatch table, its eight
 * small handlers at $1B3F..$1BBC, and the bounce routine at $1E74.
 *
 * This is shared engine code: One-on-One, Free Throw and H-O-R-S-E all reach it
 * through the same table, so a divergence here shows up in three modes at once.
 * $1E74 is also where points are scored, which makes it the single most
 * load-bearing routine outside the match driver itself.
 *
 * Ball state, all little-endian 16-bit unless noted:
 *   $C0A0/$C0A1  horizontal velocity, signed
 *   $C0A3        height, single byte
 *   $C0A8/$C0A9  vertical velocity, signed
 *   $C0AB        set while the ball is in a state that suppresses hard damping
 *   $C164        bounce counter used by the settle path
 *   $C168        fifteen-frame rim timer
 *   $C129        rim-hang steps remaining
 *   $C12A        selects the three-step or two-step rim cue
 *   $C17A        which player shot; $02 means player 2
 *   $C133/$C135  the two BCD score words
 *   $FFD4        one-shot request for the heavy damping value
 *   $FFD7        set when the shot is worth three
 */

#define ALLSTAR_SHOT_RESULT_SLOTS   11
#define ALLSTAR_SHOT_VELOCITY       0xC0A0u
#define ALLSTAR_SHOT_HEIGHT         0xC0A3u
#define ALLSTAR_SHOT_VERTICAL       0xC0A8u
#define ALLSTAR_SHOT_BOUNCE_COUNT   0xC164u
#define ALLSTAR_SHOT_RIM_TIMER      0xC168u
#define ALLSTAR_SHOT_RIM_RELOAD     0x0Fu
#define ALLSTAR_SHOT_RIM_REMAINING  0xC129u
#define ALLSTAR_SHOT_SCORE_1        0xC133u
#define ALLSTAR_SHOT_SCORE_2        0xC135u
#define ALLSTAR_SHOT_SETTLE_VERTICAL 0x0080u
#define ALLSTAR_SHOT_HEIGHT_DROP    3
#define ALLSTAR_SHOT_SOUND_SCORE    0x05u
#define ALLSTAR_SHOT_SOUND_RIM      0x08u
#define ALLSTAR_SHOT_SOUND_CUE      0x0Cu   /* $1C12 */

/* Damping added to the reversed vertical velocity at $1EBE. */
#define ALLSTAR_SHOT_DAMP_NORMAL    (-57)
#define ALLSTAR_SHOT_DAMP_FREETHROW (-250)
#define ALLSTAR_SHOT_DAMP_HEAVY     (-300)

/* $1AF9: eleven slots, two of which share $1E74. */
const uint16_t* allstar_shot_result_table(int *count);

typedef enum {
    ALLSTAR_SHOT_ROUTE_BOUNCE = 0,  /* jp $1E74            */
    ALLSTAR_SHOT_ROUTE_SETTLE,      /* the $1B5D path      */
    ALLSTAR_SHOT_ROUTE_CUE          /* $1C12 then $1C0F    */
} AllStarShotRoute;

typedef struct {
    int16_t velocity;          /* what the handler stores in $C0A0 */
    AllStarShotRoute route;
} AllStarShotHandler;

/* $1B3F/$1B45/$1B53/$1B59/$1B93/$1B99/$1BA7/$1BAD. */
bool allstar_shot_handler(uint16_t entry, AllStarShotHandler *out);

typedef struct {
    bool counts_bounce;    /* only a negative velocity reaches $1B68 */
    bool lowers_height;    /* from the second bounce onward          */
    bool resets_vertical;  /* the $1B7E branch                       */
} AllStarShotSettle;

/*
 * $1B64..$1B90.  A rightward bounce always resets the vertical velocity to
 * $0080 and clears the drift words.  A leftward bounce counts up in $C164, and
 * from the second one on it drops the height by three and exits through $1C05
 * instead.
 */
void allstar_shot_settle(int16_t velocity, uint8_t *bounce_count, uint8_t *height,
                         AllStarShotSettle *out);

/* $1E7A..$1E8C: two's-complement reversal of the vertical velocity. */
int16_t allstar_shot_reverse(int16_t vertical);

/*
 * $1E8F..$1EBC.  Free Throw with $C0AB clear damps hard; a pending $FFD4
 * request outside Free Throw damps harder still and consumes the flag.
 */
int16_t allstar_shot_damping(uint8_t mode, uint8_t suppress, uint8_t heavy_request,
                             bool *clears_request, bool *sets_c128);

typedef enum {
    ALLSTAR_SHOT_TICK_WAIT = 0,   /* $1ED0, the timer has not expired */
    ALLSTAR_SHOT_TICK_IDLE,       /* $1ED7, nothing left to advance   */
    ALLSTAR_SHOT_TICK_ADVANCE     /* $1ED8, one rim step consumed     */
} AllStarShotTick;

/* $1ECC..$1EDA */
AllStarShotTick allstar_shot_tick(uint8_t *timer, uint8_t *remaining);

typedef enum {
    ALLSTAR_SHOT_NOTHING = 0,
    ALLSTAR_SHOT_RIM_CUE,   /* $1F26, sound $08 */
    ALLSTAR_SHOT_SCORE      /* $1F06, sound $05 then $290B */
} AllStarShotOutcome;

typedef struct {
    AllStarShotOutcome outcome;
    uint8_t sound;
    uint16_t score_address;  /* $C133 or $C135 */
    uint8_t points;          /* two, or three when $FFD7 is set */
} AllStarShotScore;

/* $1EF4..$1F29 */
void allstar_shot_outcome(uint8_t remaining, uint8_t cue_select, uint8_t shooter,
                          uint8_t three_point, AllStarShotScore *out);

#endif /* ALLSTAR_SHOT_RESULT_H */
