#include "allstar_one_on_one.h"
#include <string.h>

/* ROM $28E1: compare the two unsigned 16-bit score words. */
int allstar_one_on_one_compare_scores(uint16_t p1_score, uint16_t p2_score) {
    if (p1_score == p2_score) return 0;
    return p1_score > p2_score ? 1 : 2;
}

/* ROM $10FA/$15DF: newly pressed A or B dismisses the final score. */
bool allstar_one_on_one_result_can_dismiss(uint8_t buttons_pressed) {
    return (buttons_pressed & (ALLSTAR_BTN_A | ALLSTAR_BTN_B)) != 0;
}

/* ROM $1638: the overtime notice accepts A, but not B or Start. */
bool allstar_one_on_one_overtime_can_dismiss(uint8_t buttons_pressed) {
    return (buttons_pressed & ALLSTAR_BTN_A) != 0;
}

static void allstar_one_on_one_begin_result(AllStarOneOnOneMatch *match,
                                            AllStarOneOnOneEndReason reason) {
    match->game_clock = 0.0f;
    match->result_clock = ALLSTAR_ONE_ON_ONE_RESULT_SECONDS;
    match->end_reason = reason;
    match->phase = ALLSTAR_ONE_ON_ONE_RESULT;

    match->winner = allstar_one_on_one_compare_scores(match->p1_score,
                                                       match->p2_score);
}

static uint32_t allstar_one_on_one_advance_result(AllStarOneOnOneMatch *match) {
    if (match->winner == 0) {
        match->phase = ALLSTAR_ONE_ON_ONE_OVERTIME;
        match->result_clock = 0.0f;
        match->overtime_clock = ALLSTAR_ONE_ON_ONE_OVERTIME_SECONDS;
        return ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME_NOTICE;
    }

    match->phase = ALLSTAR_ONE_ON_ONE_COMPLETE;
    match->result_clock = 0.0f;
    return ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE;
}

static uint32_t allstar_one_on_one_begin_overtime(AllStarOneOnOneMatch *match) {
    match->period++;
    match->phase = ALLSTAR_ONE_ON_ONE_PLAYING;
    match->end_reason = ALLSTAR_ONE_ON_ONE_END_NONE;
    match->game_clock = match->period_seconds;
    match->shot_clock = match->shot_clock_seconds;
    match->overtime_clock = 0.0f;
    match->p1_possession = true;
    return ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME;
}

void allstar_one_on_one_match_init(AllStarOneOnOneMatch *match,
                                   float period_seconds,
                                   float shot_clock_seconds,
                                   int play_to,
                                   bool winners_outs) {
    if (!match) return;
    memset(match, 0, sizeof(*match));
    match->period_seconds = period_seconds > 0.0f ? period_seconds : 120.0f;
    match->shot_clock_seconds = shot_clock_seconds > 0.0f ? shot_clock_seconds : 24.0f;
    match->game_clock = match->period_seconds;
    match->shot_clock = match->shot_clock_seconds;
    match->play_to = play_to > 0 ? play_to : 0;
    match->winners_outs = winners_outs;
    match->period = 1;
    match->p1_possession = true;
    match->phase = ALLSTAR_ONE_ON_ONE_PLAYING;
}

void allstar_one_on_one_match_take_possession(AllStarOneOnOneMatch *match,
                                              int player,
                                              bool reset_shot_clock) {
    if (!match || (player != 1 && player != 2)) return;
    match->p1_possession = player == 1;
    if (reset_shot_clock) match->shot_clock = match->shot_clock_seconds;
}

uint32_t allstar_one_on_one_match_call_traveling(AllStarOneOnOneMatch *match,
                                                 int player) {
    int defender;
    if (!match || match->phase != ALLSTAR_ONE_ON_ONE_PLAYING ||
        (player != 1 && player != 2) || match->p1_possession != (player == 1)) {
        return ALLSTAR_ONE_ON_ONE_EVENT_NONE;
    }

    defender = player == 1 ? 2 : 1;
    allstar_one_on_one_match_take_possession(match, defender, true);
    return ALLSTAR_ONE_ON_ONE_EVENT_TRAVELING;
}

bool allstar_one_on_one_rom_release_offset(
    uint8_t action,
    uint8_t shot_phase,
    uint8_t shot_variant,
    bool facing_left,
    AllStarOneOnOneReleaseOffset *offset) {
    static const int8_t phase_offsets[3][2][2] = {
        {{ 8,  0}, {20, -2}},
        {{ 8, -1}, { 8,  1}},
        {{ 7,  0}, {-5, -2}}
    };
    static const int8_t held_offsets[2][2][2] = {
        {{7, -2}, {10, -2}},
        {{7, -2}, {10, -2}}
    };
    int action_index;
    int facing_index;

    if (!offset) return false;
    if (action == ALLSTAR_ROM_SHOT_ACTION_A) action_index = 0;
    else if (action == ALLSTAR_ROM_SHOT_ACTION_B) action_index = 1;
    else return false;

    if (shot_phase == 0) {
        facing_index = facing_left ? 1 : 0;
        offset->x_offset = held_offsets[action_index][facing_index][0];
        offset->height_offset = held_offsets[action_index][facing_index][1];
        offset->ground_y_offset = -2;
        return true;
    }

    if (shot_phase > 2 || shot_variant > 2) return false;
    offset->x_offset = phase_offsets[shot_variant][shot_phase - 1][0];
    offset->height_offset = phase_offsets[shot_variant][shot_phase - 1][1];
    offset->ground_y_offset = -4;
    return true;
}

static bool allstar_one_on_one_rom_position_in_table(
    uint8_t player_x,
    uint8_t player_y,
    const uint8_t table[][3],
    size_t row_count) {
    size_t row;
    for (row = 0; row < row_count; row++) {
        if (table[row][0] >= player_y) {
            return table[row][1] < player_x &&
                   table[row][2] >= player_x;
        }
    }
    return false;
}

/* Bank 1 $791D/$794B classifies the player's $+06/$+15 position through
   the wedge tables at $79B6 and $79D2, producing shot variant 0, 1, or 2. */
uint8_t allstar_one_on_one_rom_shot_variant(float player_center_x,
                                            float player_ground_y) {
    static const uint8_t left_wedge[][3] = {
        {0x60, 0x00, 0x44}, {0x64, 0x00, 0x44},
        {0x68, 0x00, 0x44}, {0x6c, 0x00, 0x40},
        {0x70, 0x00, 0x38}, {0x74, 0x00, 0x2c},
        {0x78, 0x00, 0x1c}, {0x7c, 0x00, 0x14},
        {0x80, 0x00, 0x0c}
    };
    static const uint8_t right_wedge[][3] = {
        {0x60, 0x50, 0xa0}, {0x64, 0x50, 0xa0},
        {0x68, 0x50, 0xa0}, {0x6c, 0x54, 0xa0},
        {0x70, 0x5c, 0xa0}, {0x74, 0x68, 0xa0},
        {0x78, 0x78, 0xa0}, {0x7c, 0x80, 0xa0},
        {0x80, 0x88, 0xa0}
    };
    int raw_x = (int)(player_center_x - ALLSTAR_ROM_PLAYER_X_TO_CENTER);
    int raw_y = (int)player_ground_y;
    uint8_t player_x;
    uint8_t player_y;

    if (raw_x < 0) raw_x = 0;
    if (raw_x > 255) raw_x = 255;
    if (raw_y < 0) raw_y = 0;
    if (raw_y > 255) raw_y = 255;
    player_x = (uint8_t)raw_x;
    player_y = (uint8_t)raw_y;

    if (allstar_one_on_one_rom_position_in_table(
            player_x, player_y, left_wedge,
            sizeof(left_wedge) / sizeof(left_wedge[0]))) {
        return 0;
    }
    if (allstar_one_on_one_rom_position_in_table(
            player_x, player_y, right_wedge,
            sizeof(right_wedge) / sizeof(right_wedge[0]))) {
        return 2;
    }
    return 1;
}

/* Fixed-bank $077D uses strict unsigned-distance limits of 12 by 8. */
bool allstar_one_on_one_player_can_pick_up_ball(float player_reference_x,
                                                float player_reference_y,
                                                float ball_x,
                                                float ball_y) {
    float dx = player_reference_x - ball_x;
    float dy = player_reference_y - ball_y;
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    return dx < ALLSTAR_ONE_ON_ONE_PICKUP_X_RADIUS &&
           dy < ALLSTAR_ONE_ON_ONE_PICKUP_Y_RADIUS;
}

/* Fixed bank $2AE2/$2B07/$2B88: the cooldown decrements before all early
   exits, player 1 is tested before player 2, and an award reloads $C12D
   with 20 frames. The three lock inputs correspond to $FFE2/$FFE7/$FFF8. */
int allstar_one_on_one_rom_recovery_dispatch(
    AllStarOneOnOneRecoveryState *state,
    bool possession_active,
    bool global_blocked,
    float ball_height,
    bool contact_locked,
    bool secondary_locked,
    bool flight_locked,
    bool p1_action_eligible,
    bool p1_collision,
    bool p2_action_eligible,
    bool p2_collision) {
    int recovering_player = 0;

    if (!state) return 0;
    if (state->cooldown_frames > 0) state->cooldown_frames--;

    if (possession_active || global_blocked ||
        ball_height >= ALLSTAR_ROM_RECOVERY_MAX_HEIGHT) {
        return 0;
    }

    if (p1_action_eligible && p1_collision) {
        recovering_player = 1;
    } else if (p2_action_eligible && p2_collision) {
        recovering_player = 2;
    } else {
        return 0;
    }

    if (contact_locked || secondary_locked || flight_locked ||
        state->cooldown_frames > 0) {
        return 0;
    }

    state->cooldown_frames = ALLSTAR_ROM_RECOVERY_COOLDOWN_FRAMES;
    return recovering_player;
}

void allstar_one_on_one_shot_reset(AllStarOneOnOneShotAttempt *attempt) {
    if (!attempt) return;
    memset(attempt, 0, sizeof(*attempt));
    attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_IDLE;
}

uint32_t allstar_one_on_one_shot_press(AllStarOneOnOneShotAttempt *attempt,
                                       int player) {
    if (!attempt || (player != 1 && player != 2)) {
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
    }

    if (attempt->phase == ALLSTAR_ONE_ON_ONE_SHOT_IDLE) {
        attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_GATHER;
        attempt->shooter = player;
        attempt->gather_clock = ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS;
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_GATHER;
    }

    if (attempt->phase == ALLSTAR_ONE_ON_ONE_SHOT_GATHER &&
        attempt->shooter == player) {
        attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_RELEASED;
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE;
    }

    return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
}

uint32_t allstar_one_on_one_shot_tick(AllStarOneOnOneShotAttempt *attempt,
                                      float dt) {
    if (!attempt || attempt->phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER ||
        dt <= 0.0f) {
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
    }

    attempt->gather_clock -= dt;
    if (attempt->gather_clock > 0.0f) {
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
    }

    allstar_one_on_one_shot_reset(attempt);
    return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_TRAVELING;
}

/* ROM $0C13-$0C2C conditionally reverses the possession side via $FF96. */
int allstar_one_on_one_next_possession_after_score(
    const AllStarOneOnOneMatch *match,
    int shooter) {
    if (!match || (shooter != 1 && shooter != 2)) return 0;
    if (match->winners_outs) return shooter;
    return shooter == 1 ? 2 : 1;
}

uint32_t allstar_one_on_one_match_tick(AllStarOneOnOneMatch *match, float dt) {
    uint32_t events = ALLSTAR_ONE_ON_ONE_EVENT_NONE;
    if (!match || dt <= 0.0f || match->phase == ALLSTAR_ONE_ON_ONE_COMPLETE) return events;

    if (match->phase == ALLSTAR_ONE_ON_ONE_RESULT) {
        match->result_clock -= dt;
        if (match->result_clock <= 0.0f) {
            events |= allstar_one_on_one_advance_result(match);
        }
        return events;
    }

    if (match->phase == ALLSTAR_ONE_ON_ONE_OVERTIME) {
        match->overtime_clock -= dt;
        if (match->overtime_clock <= 0.0f) {
            events |= allstar_one_on_one_begin_overtime(match);
        }
        return events;
    }

    match->game_clock -= dt;
    if (match->game_clock <= 0.0f) {
        allstar_one_on_one_begin_result(match, ALLSTAR_ONE_ON_ONE_END_TIME);
        return ALLSTAR_ONE_ON_ONE_EVENT_RESULT;
    }

    match->shot_clock -= dt;
    if (match->shot_clock <= 0.0f) {
        allstar_one_on_one_match_take_possession(
            match, match->p1_possession ? 2 : 1, true);
        events |= ALLSTAR_ONE_ON_ONE_EVENT_SHOT_CLOCK;
    }

    return events;
}

uint32_t allstar_one_on_one_match_add_score(AllStarOneOnOneMatch *match,
                                            int player,
                                            int points) {
    if (!match || match->phase != ALLSTAR_ONE_ON_ONE_PLAYING || points <= 0) {
        return ALLSTAR_ONE_ON_ONE_EVENT_NONE;
    }

    if (player == 1) match->p1_score += points;
    else if (player == 2) match->p2_score += points;
    else return ALLSTAR_ONE_ON_ONE_EVENT_NONE;

    if (match->play_to > 0 &&
        (match->p1_score >= match->play_to || match->p2_score >= match->play_to)) {
        allstar_one_on_one_begin_result(match, ALLSTAR_ONE_ON_ONE_END_SCORE);
        return ALLSTAR_ONE_ON_ONE_EVENT_RESULT;
    }

    return ALLSTAR_ONE_ON_ONE_EVENT_NONE;
}

uint32_t allstar_one_on_one_match_dismiss_result(AllStarOneOnOneMatch *match) {
    if (!match || match->phase != ALLSTAR_ONE_ON_ONE_RESULT) {
        return ALLSTAR_ONE_ON_ONE_EVENT_NONE;
    }
    return allstar_one_on_one_advance_result(match);
}

uint32_t allstar_one_on_one_match_dismiss_overtime(AllStarOneOnOneMatch *match) {
    if (!match || match->phase != ALLSTAR_ONE_ON_ONE_OVERTIME) {
        return ALLSTAR_ONE_ON_ONE_EVENT_NONE;
    }
    return allstar_one_on_one_begin_overtime(match);
}

void allstar_one_on_one_match_reset_shot_clock(AllStarOneOnOneMatch *match) {
    if (!match) return;
    match->shot_clock = match->shot_clock_seconds;
}
