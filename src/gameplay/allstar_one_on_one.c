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

/* $7F37 stores ball ground Y as player +$15 minus two while held or minus
   four in an active shot. Ball height is that ground byte minus player
   visual Y (+$05) and the selected signed table offset. */
int allstar_one_on_one_rom_release_height(
    int player_visual_y,
    int player_ground_y,
    uint8_t shot_phase,
    int height_offset) {
    int ball_ground_y = player_ground_y - (shot_phase == 0 ? 2 : 4);
    return ball_ground_y - (player_visual_y + height_offset);
}

/* Bank 1 $6A8C record advancement. Actions $0A and $12 share twelve
   duration records totaling 67 frames; $0A ends on display frame $0C,
   while $12 remains on $0B. Active shot phases override the record frame
   with $12, $13, or $14 exactly as $6B34-$6B5E does. */
bool allstar_one_on_one_rom_shot_animation_frame(
    uint8_t action,
    uint8_t shot_phase,
    uint16_t elapsed_frames,
    uint8_t *display_frame) {
    static const uint8_t durations[12] = {
        6, 6, 6, 6, 6, 6, 1, 6, 6, 6, 6, 6
    };
    static const uint8_t frames_a[12] = {
        0x08, 0x09, 0x09, 0x09, 0x09, 0x0a,
        0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0c
    };
    static const uint8_t frames_b[12] = {
        0x08, 0x09, 0x09, 0x09, 0x09, 0x0a,
        0x0a, 0x0b, 0x0b, 0x0b, 0x0b, 0x0b
    };
    const uint8_t *frames;
    uint16_t record_start = 0;
    size_t record;

    if (!display_frame || elapsed_frames >=
            ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_FRAMES || shot_phase > 3) {
        return false;
    }
    if (action == ALLSTAR_ROM_SHOT_ACTION_A) frames = frames_a;
    else if (action == ALLSTAR_ROM_SHOT_ACTION_B) frames = frames_b;
    else return false;

    if (shot_phase != 0) {
        *display_frame = (uint8_t)(0x12 + shot_phase - 1);
        return true;
    }

    for (record = 0; record < 12; record++) {
        if (elapsed_frames < record_start + durations[record]) {
            *display_frame = frames[record];
            return true;
        }
        record_start = (uint16_t)(record_start + durations[record]);
    }
    return false;
}

void allstar_one_on_one_rom_animation_init_6a8c(
    AllStarRomAnimationState *state, uint8_t action) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->action = action;
    state->timer = 1;
}

void allstar_one_on_one_rom_animation_set_action_6a8c(
    AllStarRomAnimationState *state, uint8_t action) {
    if (!state || action >= ALLSTAR_ROM_ANIMATION_ACTION_COUNT ||
        state->action == action) return;
    state->action = action;
    state->record_index = 0;
    state->timer = 1;
    state->finished = false;
    state->new_frame = false;
}

/* Bank 1 $6A8C->$6C59: decrement +$04 first, select action through the
   $6C60 pointer map, index three-byte records by +$03, and implement the
   normal/loop/transition/end control bytes. Movement and shot-only side
   effects remain in their native gameplay helpers; this function owns the
   exact record, duration, frame, and action-transition state. */
bool allstar_one_on_one_rom_animation_tick_6a8c(
    const AllStarAssetPack *pack, AllStarRomAnimationState *state) {
    const AllStarRomAnimationAction *action;
    const AllStarRomAnimationRecord *record;

    if (!pack || !state ||
        pack->header.animation_action_count !=
            ALLSTAR_ROM_ANIMATION_ACTION_COUNT ||
        state->action >= ALLSTAR_ROM_ANIMATION_ACTION_COUNT) return false;

    state->new_frame = false;
    if (state->timer > 0) state->timer--;
    if (state->timer != 0) return true;

    action = &pack->animation_actions[state->action];
    if (state->record_index >= action->record_count) return false;
    record = &action->records[state->record_index];

    if (record->control == 0xff) {
        state->finished = true;
        state->record_index = 0;
        state->timer = 1;
        return true;
    }
    if ((record->control & 0x01) != 0) {
        state->record_index = 0;
        state->timer = 1;
        return true;
    }
    if ((record->control & 0x02) != 0) {
        if (record->value >= ALLSTAR_ROM_ANIMATION_ACTION_COUNT) return false;
        state->action = record->value;
        state->record_index = 0;
        state->timer = 1;
        state->finished = false;
        return true;
    }

    if (record->value == 0) return false;
    state->timer = record->value;
    state->display_frame = record->display_frame;
    state->new_frame = true;
    state->record_index++;
    return true;
}

/* Bank 1 $782E runs immediately before $6A8C. It only replaces an eligible
   action when +$04 is about to expire, prefers the +$14 direction override
   over +$07 input, falls back to +$10's prior direction while idle, and
   mirrors player +$02 bit 4. Player +$09 is zero while holding the ball. */
bool allstar_one_on_one_rom_select_movement_action_782e(
    AllStarRomAnimationState *state,
    uint8_t input_direction,
    uint8_t override_direction,
    uint8_t previous_direction,
    bool without_ball,
    bool reaction_locked,
    bool *horizontal_flip) {
    uint8_t direction;
    uint8_t action;

    if (!state || state->timer != 1 || reaction_locked ||
        !allstar_one_on_one_rom_action_eligible_0a78(state->action)) {
        return false;
    }

    direction = override_direction != 0
        ? override_direction : input_direction;
    if ((direction & ALLSTAR_BTN_RIGHT) != 0) {
        if (horizontal_flip) *horizontal_flip = true;
        action = without_ball ? 0x11 : 0x10;
    } else if ((direction & ALLSTAR_BTN_LEFT) != 0) {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x11 : 0x10;
    } else if ((direction & ALLSTAR_BTN_UP) != 0) {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x09 : 0x08;
    } else if ((direction & ALLSTAR_BTN_DOWN) != 0) {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x02 : 0x01;
    } else if ((previous_direction & ALLSTAR_BTN_RIGHT) != 0) {
        if (horizontal_flip) *horizontal_flip = true;
        action = without_ball ? 0x15 : 0x13;
    } else if ((previous_direction & ALLSTAR_BTN_LEFT) != 0) {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x15 : 0x13;
    } else if ((previous_direction & ALLSTAR_BTN_UP) != 0) {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x0d : 0x0b;
    } else {
        if (horizontal_flip) *horizontal_flip = false;
        action = without_ball ? 0x06 : 0x04;
    }

    allstar_one_on_one_rom_animation_set_action_6a8c(state, action);
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

/* $07B4 tests three expanding rectangles around the hoop. $7EC4 then
   recognizes six-by-three exact corner coordinates as class four; every
   other position outside the rectangles is class three. Native X is the
   player center, eight pixels beyond ROM field +$06. */
uint8_t allstar_one_on_one_rom_shot_distance_class(float player_center_x,
                                                   float player_ground_y) {
    static const uint8_t margins[3] = {24, 40, 56};
    static const uint8_t corner_x[6] = {4, 8, 12, 148, 144, 140};
    static const uint8_t corner_y[3] = {156, 152, 148};
    int raw_x = (int)player_center_x - (int)ALLSTAR_ROM_PLAYER_X_TO_CENTER;
    int raw_y = (int)player_ground_y;
    size_t i;
    size_t j;

    for (i = 0; i < 3; i++) {
        int margin = margins[i];
        if (raw_y < 92 + margin && raw_x + 8 >= 85 - margin &&
            raw_x + 8 < 84 + margin) {
            return (uint8_t)i;
        }
    }
    for (i = 0; i < sizeof(corner_x); i++) {
        for (j = 0; j < sizeof(corner_y); j++) {
            if (raw_x == corner_x[i] && raw_y == corner_y[j]) return 4;
        }
    }
    return 3;
}

/* $2F40 maps the roster byte at player +$0F to one of three launch-table
   profiles. */
uint8_t allstar_one_on_one_rom_shot_profile(uint8_t roster_index) {
    static const uint8_t profile_zero[] = {2, 13, 11, 5, 15};
    static const uint8_t profile_one[] = {25, 17, 18, 26, 4, 1, 8, 9, 14, 6};
    size_t i;
    for (i = 0; i < sizeof(profile_zero); i++) {
        if (roster_index == profile_zero[i]) return 0;
    }
    for (i = 0; i < sizeof(profile_one); i++) {
        if (roster_index == profile_one[i]) return 1;
    }
    return 2;
}

/* $7C58 indexes profile -> distance class -> the twelve-entry player-pose
   row. Class zero uses a zero high byte; classes one through four use one. */
int16_t allstar_one_on_one_rom_shot_vertical_velocity(
    uint8_t roster_index, uint8_t distance_class, uint8_t pose_index) {
    static const uint8_t low[3][5][12] = {
        {
            {0xb0,0xb2,0xb4,0xb7,0xe9,0xd5,0xc5,0xf0,0xc5,0xd5,0xe9,0xb7},
            {0xba,0xbc,0xbe,0xc0,0xc2,0xd4,0xcc,0xe4,0xcc,0xd4,0xc2,0xc0},
            {0xa0,0xa0,0xa0,0xa2,0xa4,0xa8,0xcd,0xc8,0xd0,0xa8,0xa4,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xb8,0xc8,0xa8,0xa4,0xa0,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xc8,0xa8,0xa4,0xa0,0xa0}
        },
        {
            {0xb0,0xb2,0xb4,0xb7,0xe9,0xd5,0xd0,0xf0,0xd0,0xd5,0xe9,0xb7},
            {0xba,0xbc,0xbe,0xc0,0xc2,0xd4,0xcc,0xe4,0xcc,0xd4,0xc2,0xc0},
            {0xa0,0xa0,0xa0,0xa2,0xa4,0xa8,0xcd,0xc8,0xd0,0xa8,0xa4,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xc8,0xa8,0xa4,0xa0,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xb4,0xa8,0xa4,0xa0,0xa0}
        },
        {
            {0xb0,0xb2,0xb4,0xb7,0xe9,0xc3,0xd0,0xf4,0xcc,0xc3,0xe9,0xb7},
            {0xba,0xbc,0xbe,0xc0,0xc2,0xd4,0xd8,0xe4,0xd8,0xd4,0xc2,0xc0},
            {0xa0,0xa0,0xa0,0xa2,0xa4,0xa8,0xcd,0xd8,0xcd,0xa8,0xa4,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xc8,0xa8,0xa4,0xa0,0xa0},
            {0x90,0x94,0x98,0x9c,0xa0,0xa4,0xa8,0xb4,0xa8,0xa4,0xa0,0xa0}
        }
    };
    uint8_t profile = allstar_one_on_one_rom_shot_profile(roster_index);
    if (distance_class > 4 || pose_index > 11) return 0;
    return (int16_t)((distance_class == 0 ? 0 : 0x0100) |
                     low[profile][distance_class][pose_index]);
}

/* $78E9/$794B checks the held ball against the $798B two-point region,
   stores the inverted result in $FFD6, and $7C58 copies it to $FFD7.
   $1F23 awards two points for zero and three for nonzero. */
int allstar_one_on_one_rom_point_value(float ball_x, float ball_y) {
    static const uint8_t region[][3] = {
        {0x5c,0x12,0x91}, {0x60,0x12,0x91}, {0x64,0x12,0x91},
        {0x68,0x12,0x91}, {0x6c,0x12,0x91}, {0x70,0x12,0x91},
        {0x74,0x12,0x91}, {0x78,0x12,0x91},
        {0x7c,0x16,0x8d}, {0x80,0x16,0x8d},
        {0x84,0x1a,0x8a}, {0x88,0x1e,0x85},
        {0x8c,0x22,0x81}, {0x90,0x2e,0x75}
    };
    int x = (int)ball_x;
    int y = (int)ball_y;
    size_t row;
    for (row = 0; row < sizeof(region) / sizeof(region[0]); row++) {
        if (y <= region[row][0]) {
            return region[row][1] < x && x <= region[row][2] ? 2 : 3;
        }
    }
    return 3;
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

/* Fixed bank $0A78 returns Z for the eight actions that protect a player from
   the $2B14 steal transfer and participate in shot/jump handling. */
bool allstar_one_on_one_rom_action_eligible_0a78(uint8_t action) {
    static const uint8_t protected_actions[] = {
        0x03, 0x0a, 0x12, 0x05, 0x0c, 0x14, 0x0e, 0x16
    };
    size_t i;
    for (i = 0; i < sizeof(protected_actions); i++) {
        if (action == protected_actions[i]) return false;
    }
    return true;
}

/* Bank 1 $70FD keeps defensive jumps in the actor's eight-action family. */
uint8_t allstar_one_on_one_rom_defense_jump_action_70fd(uint8_t action) {
    if (action < 0x08) return 0x05;
    if (action < 0x10) return 0x0c;
    return 0x14;
}

/* $2B14 selects the steal animation from the actor's eight-action family. */
uint8_t allstar_one_on_one_rom_steal_action_2b14(uint8_t action) {
    if (action < 0x08) return 0x07;
    if (action < 0x10) return 0x0f;
    return 0x17;
}

/* $2B14: an active owner must be vulnerable, the defender must overlap the
   held ball through $077D, and the two direction masks must oppose on either
   axis. The caller owns the B/latch gate that reaches this routine. */
bool allstar_one_on_one_rom_steal_contact_2b14(
    bool possession_active,
    uint8_t ballhandler_action,
    float defender_x,
    float defender_ground_y,
    float ball_x,
    float ball_y,
    uint8_t defender_direction,
    uint8_t ballhandler_direction) {
    uint8_t combined_direction;
    if (!possession_active ||
        !allstar_one_on_one_rom_action_eligible_0a78(ballhandler_action) ||
        !allstar_one_on_one_player_can_pick_up_ball(
            defender_x,
            defender_ground_y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y,
            ball_x, ball_y)) {
        return false;
    }
    combined_direction = (uint8_t)(defender_direction |
                                   ballhandler_direction);
    return (combined_direction & 0x03) == 0x03 ||
           (combined_direction & 0x0c) == 0x0c;
}

/* Bank 1 $6C4D supplies signed visual-Y deltas to the twelve six-frame
   records shared by jump actions $05/$0C/$14. These are the cumulative
   upward displacements used by $2B6C's ground-minus-visual-Y reach test. */
float allstar_one_on_one_rom_jump_height_6c4d(uint16_t elapsed_frames) {
    static const uint8_t cumulative_height[12] = {
        0, 9, 16, 21, 24, 26, 26, 24, 19, 12, 4, 0
    };
    size_t record;
    if (elapsed_frames >= ALLSTAR_ROM_DEFENSE_JUMP_FRAMES) return 0.0f;
    record = elapsed_frames / 6;
    return (float)cumulative_height[record];
}

/* $2B6C->$2B88: while no one owns the ball, $077D supplies the planar gate
   and the ball must be strictly above reach-8 and at or below reach. $2B88
   then rejects $FFF8, so this is an airborne post-contact rebound catch,
   not a live block. The ROM contains no separate goaltending branch. */
bool allstar_one_on_one_rom_jump_recovery_2b6c(
    bool possession_active,
    bool first_contact_locked,
    float player_x,
    float player_ground_y,
    float player_jump_height,
    float ball_x,
    float ball_y,
    float ball_height) {
    float reach = ALLSTAR_ROM_PLAYER_BODY_HEIGHT + player_jump_height;
    if (possession_active || first_contact_locked ||
        !allstar_one_on_one_player_can_pick_up_ball(
            player_x,
            player_ground_y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y,
            ball_x, ball_y)) {
        return false;
    }
    return ball_height > reach - ALLSTAR_ROM_JUMP_CATCH_BAND &&
           ball_height <= reach;
}

/* Bank 1 $6BAD/$6BBA bounds field +$06 to 8..148 and $6BC7/$6BD4
   bounds field +$15 to 98..152. Native X stores +$06 plus eight. */
void allstar_one_on_one_rom_clamp_player_court(float *player_center_x,
                                               float *player_ground_y) {
    if (!player_center_x || !player_ground_y) return;
    if (*player_center_x < ALLSTAR_ONE_ON_ONE_PLAYER_MIN_X)
        *player_center_x = ALLSTAR_ONE_ON_ONE_PLAYER_MIN_X;
    if (*player_center_x > ALLSTAR_ONE_ON_ONE_PLAYER_MAX_X)
        *player_center_x = ALLSTAR_ONE_ON_ONE_PLAYER_MAX_X;
    if (*player_ground_y < ALLSTAR_ONE_ON_ONE_PLAYER_MIN_Y)
        *player_ground_y = ALLSTAR_ONE_ON_ONE_PLAYER_MIN_Y;
    if (*player_ground_y > ALLSTAR_ONE_ON_ONE_PLAYER_MAX_Y)
        *player_ground_y = ALLSTAR_ONE_ON_ONE_PLAYER_MAX_Y;
}

/* Fixed bank $2AE2/$2B07/$2B88: the cooldown decrements before all early
   exits, player 1 is tested before player 2, and an award reloads $C12D
   with 20 frames. $FFEB blocks during counted waits; $FFE2 blocks a pending
   score event; $FFE7 blocks play transitions; $FFF8 blocks until the first
   ground contact. */
int allstar_one_on_one_rom_recovery_dispatch(
    AllStarOneOnOneRecoveryState *state,
    bool possession_active,
    bool counted_wait_locked,
    float ball_height,
    bool score_event_locked,
    bool transition_locked,
    bool flight_locked,
    bool p1_action_eligible,
    bool p1_collision,
    bool p2_action_eligible,
    bool p2_collision) {
    int recovering_player = 0;

    if (!state) return 0;
    if (state->cooldown_frames > 0) state->cooldown_frames--;

    if (possession_active || counted_wait_locked ||
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

    if (score_event_locked || transition_locked || flight_locked ||
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
    return allstar_one_on_one_shot_input(attempt, player, true, false);
}

/* ROM bank 1 $702D uses new-A at player +$11 for immediate release and
 * held-B at +$12 for a one-frame $C16A phase-advance release. */
uint32_t allstar_one_on_one_shot_input(AllStarOneOnOneShotAttempt *attempt,
                                       int player,
                                       bool a_pressed,
                                       bool b_held) {
    if (!attempt || (player != 1 && player != 2)) {
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
    }

    if (attempt->phase == ALLSTAR_ONE_ON_ONE_SHOT_IDLE) {
        if (!a_pressed) return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
        attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_GATHER;
        attempt->shooter = player;
        attempt->gather_clock = ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS;
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_GATHER;
    }

    if (attempt->phase == ALLSTAR_ONE_ON_ONE_SHOT_GATHER &&
        attempt->shooter == player) {
        if (a_pressed && attempt->rom_phase == 0) {
            attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_RELEASED;
            return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE;
        }
        if (b_held && attempt->rom_phase == 0) {
            attempt->rom_phase = 1;
            attempt->release_latch_frames = 1;
        }
    }

    return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
}

uint32_t allstar_one_on_one_shot_tick(AllStarOneOnOneShotAttempt *attempt,
                                      float dt) {
    if (!attempt || attempt->phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER ||
        dt <= 0.0f) {
        return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE;
    }

    if (attempt->rom_phase != 0 && attempt->release_latch_frames > 0) {
        attempt->release_latch_frames--;
        if (attempt->release_latch_frames == 0) {
            attempt->rom_phase++;
            attempt->phase = ALLSTAR_ONE_ON_ONE_SHOT_RELEASED;
            return ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE;
        }
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
