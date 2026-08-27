#ifndef ALLSTAR_COURT_STATE_H
#define ALLSTAR_COURT_STATE_H

#include "allstar_types.h"

/*
 * Court-wide state shared by every gameplay mode: sprite priority against the
 * rim ($1C1D/$1C32/$1C3E), the rim-cue selector ($1F3E/$1F5B), the pause toggle
 * ($2BC6), and the two message triggers at $2C72 and $2C95.
 *
 * Like the shot-result table, none of this is mode-specific -- a divergence
 * here is visible in One-on-One, Free Throw and H-O-R-S-E at once.
 */

/* ---- $1C1D..$1C60: OAM priority against the backboard ---- */

#define ALLSTAR_COURT_OAM_GROUPS    4
#define ALLSTAR_COURT_GROUP_SPRITES 4
#define ALLSTAR_COURT_PRIORITY_Y    0x58u  /* $1C3F compares against this */
#define ALLSTAR_COURT_ATTR_OFFSET   3      /* $1C43 steps to the attribute */
#define ALLSTAR_COURT_SPRITE_STRIDE 4

/* $1C1D/$1C23/$1C29/$1C2F: the four sprite groups it walks. */
const uint16_t* allstar_court_oam_groups(int *count);

/*
 * $1C32..$1C60.  A group whose Y has reached $58 draws behind the background,
 * and a held ball ($C12B) forces every group behind it regardless of Y.
 */
bool allstar_court_sprite_behind(uint8_t group_y, uint8_t ball_held);

/* ---- $1F3E / $1F5B: the rim-cue selector ---- */

#define ALLSTAR_COURT_CUE_SELECT_VALUE 0x06u  /* $1F49 writes this to $C12A */
#define ALLSTAR_COURT_CUE_ID           0x0Cu  /* $1F5B loads C with this    */

/* $1F3E: either flag being set switches the rim cue to its short form. */
bool allstar_court_cue_select(uint8_t flag_a, uint8_t flag_b, uint8_t *selector);

uint8_t allstar_court_cue_id(void);

/* ---- $2BC6: the pause toggle ---- */

#define ALLSTAR_COURT_PAUSE_BUTTON   0x08u   /* $FFAE bit 3, Start          */
#define ALLSTAR_COURT_PAUSE_REQUEST  0xCCu   /* $C18E, sent over the link   */
#define ALLSTAR_COURT_PAUSE_SOUND    0x01u
#define ALLSTAR_COURT_PAUSE_MESSAGE  0x069Eu
#define ALLSTAR_COURT_ROLE_PLAYER_2  0x03u   /* $C199                       */

/* The six flags $2BCB..$2BE6 all have to be clear for Start to register. */
typedef struct {
    uint8_t c185;
    uint8_t c16f;
    uint8_t c12e;
    uint8_t ffeb;
    uint8_t c174;
    uint8_t ffec;
} AllStarCourtPauseGates;

typedef enum {
    ALLSTAR_COURT_PAUSE_IGNORED = 0,  /* a gate blocked it, or no Start     */
    ALLSTAR_COURT_PAUSE_REQUESTED,    /* $2BF9, a link request was posted   */
    ALLSTAR_COURT_PAUSE_ENTERED,      /* $2C0C                              */
    ALLSTAR_COURT_PAUSE_LEFT          /* $2C22                              */
} AllStarCourtPauseResult;

typedef struct {
    AllStarCourtPauseResult result;
    bool posts_link_request;  /* $C18E receives $CC        */
    bool consumes_input;      /* $2C00 clears $FFAE        */
    uint8_t sound;            /* $01 on entering pause     */
    uint16_t message;         /* $069E                     */
    bool toggles_objects;     /* mode $01 hides and shows OBJ */
} AllStarCourtPause;

/*
 * $2BC6..$2C43.  In a link game the cartridge that is not player 2 posts $CC
 * into $C18E instead of pausing immediately, and player 2 consumes the button
 * so only one side drives the toggle.
 */
void allstar_court_pause(uint8_t new_buttons, const AllStarCourtPauseGates *gates,
                         uint8_t link_game, uint8_t role, uint8_t pending_request,
                         uint8_t mode, uint8_t *paused, AllStarCourtPause *out);

/* ---- $2C72 and $2C95: the two message triggers ---- */

#define ALLSTAR_COURT_EXPIRY_MESSAGE    0x068Du
#define ALLSTAR_COURT_VIOLATION_MESSAGE 0x0649u
#define ALLSTAR_COURT_VIOLATION_SUBSTATE 0x0Cu

typedef struct {
    bool fires;
    uint8_t owner;      /* $FFD0 receives B */
    uint16_t message;
} AllStarCourtExpiry;

/* $2C72: a zero 16-bit counter flags $C131 and posts the expiry message. */
void allstar_court_expiry(uint16_t counter, uint8_t owner, AllStarCourtExpiry *out);

typedef struct {
    bool fires;
    uint16_t message;
    bool unwinds_caller;  /* $2CB9 pops a frame before jumping to $05A3 */
} AllStarCourtViolation;

/*
 * $2C95.  The action has to be $03, $0A or $12, its sub-state has to be $0C,
 * and the player has to hold possession in $FFCF.  The routine then discards a
 * return address before tail-jumping into $05A3, so the message writer returns
 * past its own caller.
 */
void allstar_court_violation(uint8_t action, uint8_t substate, uint8_t player,
                             uint8_t possession, AllStarCourtViolation *out);

#endif /* ALLSTAR_COURT_STATE_H */
