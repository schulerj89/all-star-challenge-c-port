#include "allstar_court_state.h"

/* $1C1D, $1C23, $1C29, $1C2F */
static const uint16_t COURT_OAM_GROUPS[ALLSTAR_COURT_OAM_GROUPS] = {
    0xC000u, 0xC010u, 0xC020u, 0xC030u
};

const uint16_t* allstar_court_oam_groups(int *count) {
    if (count) *count = ALLSTAR_COURT_OAM_GROUPS;
    return COURT_OAM_GROUPS;
}

/*
 * $1C32..$1C60.  $1C3E clears bit 7 on four attribute bytes when the group sits
 * above $58 and sets it otherwise; $1C32 then overrides to "behind" whenever
 * $C12B says the ball is held.
 */
bool allstar_court_sprite_behind(uint8_t group_y, uint8_t ball_held) {
    if (ball_held != 0) return true;                                /* $1C37-$1C3C */
    return group_y >= ALLSTAR_COURT_PRIORITY_Y;                     /* $1C3F-$1C41 */
}

/* $1F3E */
bool allstar_court_cue_select(uint8_t flag_a, uint8_t flag_b, uint8_t *selector) {
    if (flag_a == 0 && flag_b == 0) return false;                   /* $1F40-$1F46 */
    if (selector) *selector = ALLSTAR_COURT_CUE_SELECT_VALUE;       /* $1F47-$1F49 */
    return true;
}

/* $1F5B */
uint8_t allstar_court_cue_id(void) {
    return ALLSTAR_COURT_CUE_ID;
}

/* $2BC6..$2C43 */
void allstar_court_pause(uint8_t new_buttons, const AllStarCourtPauseGates *gates,
                         uint8_t link_game, uint8_t role, uint8_t pending_request,
                         uint8_t mode, uint8_t *paused, AllStarCourtPause *out) {
    if (!out) return;
    out->result = ALLSTAR_COURT_PAUSE_IGNORED;
    out->posts_link_request = false;
    out->consumes_input = false;
    out->sound = 0;
    out->message = 0;
    out->toggles_objects = false;

    if ((new_buttons & ALLSTAR_COURT_PAUSE_BUTTON) == 0) return;    /* $2BC6-$2BCA */
    if (!gates) return;
    if (gates->c185 != 0 || gates->c16f != 0 || gates->c12e != 0 ||
        gates->ffeb != 0 || gates->c174 != 0 || gates->ffec != 0) {
        return;                                                     /* $2BCB-$2BE6 */
    }

    if (link_game != 0) {                                           /* $2BE7-$2BEB */
        if (role == ALLSTAR_COURT_ROLE_PLAYER_2) {                  /* $2BED-$2BF2 */
            out->consumes_input = true;                             /* $2C00 */
        } else {
            if (pending_request != 0) return;                       /* $2BF4-$2BF8 */
            out->posts_link_request = true;                         /* $2BF9 */
            out->result = ALLSTAR_COURT_PAUSE_REQUESTED;
            return;
        }
    }

    if (!paused) return;
    *paused = (uint8_t)(*paused ^ 0x01u);                           /* $2C03-$2C07 */
    out->toggles_objects = (mode == 0x01u);                         /* $2C11/$2C2A */

    if (*paused != 0) {                                             /* $2C09-$2C0A */
        out->result = ALLSTAR_COURT_PAUSE_ENTERED;
        out->sound = ALLSTAR_COURT_PAUSE_SOUND;                     /* $2C0C */
        out->message = ALLSTAR_COURT_PAUSE_MESSAGE;                 /* $2C1C */
        return;
    }
    out->result = ALLSTAR_COURT_PAUSE_LEFT;                         /* $2C22 */
}

/* $2C72 */
void allstar_court_expiry(uint16_t counter, uint8_t owner, AllStarCourtExpiry *out) {
    if (!out) return;
    out->fires = false;
    out->owner = 0;
    out->message = 0;
    if (counter != 0) return;                                       /* $2C72-$2C74 */
    out->fires = true;
    out->owner = owner;                                             /* $2C75-$2C76 */
    out->message = ALLSTAR_COURT_EXPIRY_MESSAGE;                    /* $2C7D */
}

/* $2C95 */
void allstar_court_violation(uint8_t action, uint8_t substate, uint8_t player,
                             uint8_t possession, AllStarCourtViolation *out) {
    if (!out) return;
    out->fires = false;
    out->message = 0;
    out->unwinds_caller = false;

    if (action != 0x03u && action != 0x0Au && action != 0x12u) {    /* $2C98-$2CA3 */
        return;
    }
    if (substate != ALLSTAR_COURT_VIOLATION_SUBSTATE) return;       /* $2CA4-$2CAB */
    if (possession != player) return;                               /* $2CAC-$2CB3 */

    out->fires = true;
    out->message = ALLSTAR_COURT_VIOLATION_MESSAGE;                 /* $2CB6 */
    out->unwinds_caller = true;                                     /* $2CB9 */
}
