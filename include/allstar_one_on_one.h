#ifndef ALLSTAR_ONE_ON_ONE_H
#define ALLSTAR_ONE_ON_ONE_H

#include "allstar_types.h"
#include "allstar_asset_pack.h"

#define ALLSTAR_ONE_ON_ONE_RESULT_FRAMES 960
#define ALLSTAR_ONE_ON_ONE_RESULT_SECONDS (ALLSTAR_ONE_ON_ONE_RESULT_FRAMES / 60.0f)
#define ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES 240
#define ALLSTAR_ONE_ON_ONE_OVERTIME_SECONDS (ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES / 60.0f)
/* Action $0A/$12 reaches its terminal $0D transition after 67 frames. */
#define ALLSTAR_ONE_ON_ONE_SHOT_GATHER_FRAMES 67
#define ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS \
    (ALLSTAR_ONE_ON_ONE_SHOT_GATHER_FRAMES / 60.0f)
#define ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_FRAMES 67
#define ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS \
    (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_FRAMES / 60.0f)

/* $077D compares these player reference coordinates to the loose ball. */
#define ALLSTAR_ONE_ON_ONE_PICKUP_X_RADIUS 12.0f
#define ALLSTAR_ONE_ON_ONE_PICKUP_Y_RADIUS 8.0f
#define ALLSTAR_ROM_RECOVERY_MAX_HEIGHT 24.0f
#define ALLSTAR_ROM_RECOVERY_COOLDOWN_FRAMES 20

/* $2B14 uses two animation records as its repeat latch. The steal action's
   first record lasts one setup frame plus fifteen held frames. */
#define ALLSTAR_ROM_STEAL_LATCH_FRAMES 16
/* Actions $05/$0C/$14 share twelve six-frame jump records. */
#define ALLSTAR_ROM_DEFENSE_JUMP_FRAMES 72
#define ALLSTAR_ROM_PLAYER_BODY_HEIGHT 40.0f
#define ALLSTAR_ROM_JUMP_CATCH_BAND 8.0f

/* Native x is the player center ($+06 + 8); native y is ground field $+15. */
#define ALLSTAR_ONE_ON_ONE_PLAYER_MIN_X 16.0f
#define ALLSTAR_ONE_ON_ONE_PLAYER_MAX_X 156.0f
#define ALLSTAR_ONE_ON_ONE_PLAYER_MIN_Y 98.0f
#define ALLSTAR_ONE_ON_ONE_PLAYER_MAX_Y 152.0f
#define ALLSTAR_ROM_PLAYER_X_TO_CENTER 8.0f
#define ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y -2.0f
#define ALLSTAR_ONE_ON_ONE_HOOP_X 84.0f
#define ALLSTAR_ONE_ON_ONE_HOOP_Y 92.0f

#define ALLSTAR_ROM_SHOT_ACTION_A 0x0a
#define ALLSTAR_ROM_SHOT_ACTION_B 0x12

typedef enum {
    ALLSTAR_ONE_ON_ONE_PLAYING = 0,
    ALLSTAR_ONE_ON_ONE_RESULT,
    ALLSTAR_ONE_ON_ONE_OVERTIME,
    ALLSTAR_ONE_ON_ONE_COMPLETE
} AllStarOneOnOnePhase;

typedef enum {
    ALLSTAR_ONE_ON_ONE_END_NONE = 0,
    ALLSTAR_ONE_ON_ONE_END_TIME,
    ALLSTAR_ONE_ON_ONE_END_SCORE
} AllStarOneOnOneEndReason;

typedef enum {
    ALLSTAR_ONE_ON_ONE_EVENT_NONE = 0,
    ALLSTAR_ONE_ON_ONE_EVENT_SHOT_CLOCK = (1 << 0),
    ALLSTAR_ONE_ON_ONE_EVENT_RESULT = (1 << 1),
    ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME = (1 << 2),
    ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE = (1 << 3),
    ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME_NOTICE = (1 << 4),
    ALLSTAR_ONE_ON_ONE_EVENT_TRAVELING = (1 << 5)
} AllStarOneOnOneEvent;

typedef enum {
    ALLSTAR_ONE_ON_ONE_SHOT_IDLE = 0,
    ALLSTAR_ONE_ON_ONE_SHOT_GATHER,
    ALLSTAR_ONE_ON_ONE_SHOT_RELEASED
} AllStarOneOnOneShotPhase;

typedef enum {
    ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE = 0,
    ALLSTAR_ONE_ON_ONE_SHOT_EVENT_GATHER = (1 << 0),
    ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE = (1 << 1),
    ALLSTAR_ONE_ON_ONE_SHOT_EVENT_TRAVELING = (1 << 2)
} AllStarOneOnOneShotEvent;

typedef struct {
    AllStarOneOnOneShotPhase phase;
    int shooter;
    float gather_clock;
    uint8_t rom_phase;
    uint8_t release_latch_frames;
} AllStarOneOnOneShotAttempt;

typedef struct {
    int x_offset;
    int ground_y_offset;
    int height_offset;
} AllStarOneOnOneReleaseOffset;

typedef struct {
    uint8_t cooldown_frames;
} AllStarOneOnOneRecoveryState;

typedef struct {
    uint8_t action;
    uint8_t display_frame;
    uint8_t record_index;
    uint8_t timer;
    bool finished;
    bool new_frame;
} AllStarRomAnimationState;

typedef struct {
    uint16_t p1_score;
    uint16_t p2_score;
    int play_to;
    int period;
    int winner;
    float game_clock;
    float shot_clock;
    float period_seconds;
    float shot_clock_seconds;
    float result_clock;
    float overtime_clock;
    bool p1_possession;
    bool winners_outs;
    AllStarOneOnOnePhase phase;
    AllStarOneOnOneEndReason end_reason;
} AllStarOneOnOneMatch;

void allstar_one_on_one_match_init(AllStarOneOnOneMatch *match,
                                   float period_seconds,
                                   float shot_clock_seconds,
                                   int play_to,
                                   bool winners_outs);
uint32_t allstar_one_on_one_match_tick(AllStarOneOnOneMatch *match, float dt);
uint32_t allstar_one_on_one_match_add_score(AllStarOneOnOneMatch *match,
                                            int player,
                                            int points);
uint32_t allstar_one_on_one_match_dismiss_result(AllStarOneOnOneMatch *match);
uint32_t allstar_one_on_one_match_dismiss_overtime(AllStarOneOnOneMatch *match);
void allstar_one_on_one_match_reset_shot_clock(AllStarOneOnOneMatch *match);
void allstar_one_on_one_match_take_possession(AllStarOneOnOneMatch *match,
                                              int player,
                                              bool reset_shot_clock);
uint32_t allstar_one_on_one_match_call_traveling(AllStarOneOnOneMatch *match,
                                                 int player);
bool allstar_one_on_one_rom_release_offset(
    uint8_t action,
    uint8_t shot_phase,
    uint8_t shot_variant,
    bool facing_left,
    AllStarOneOnOneReleaseOffset *offset);
int allstar_one_on_one_rom_release_height(
    int player_visual_y,
    int player_ground_y,
    uint8_t shot_phase,
    int height_offset);
bool allstar_one_on_one_rom_shot_animation_frame(
    uint8_t action,
    uint8_t shot_phase,
    uint16_t elapsed_frames,
    uint8_t *display_frame);
void allstar_one_on_one_rom_animation_init_6a8c(
    AllStarRomAnimationState *state, uint8_t action);
void allstar_one_on_one_rom_animation_set_action_6a8c(
    AllStarRomAnimationState *state, uint8_t action);
bool allstar_one_on_one_rom_animation_tick_6a8c(
    const AllStarAssetPack *pack, AllStarRomAnimationState *state);
/* Bank 1 $782E: record-boundary movement/idle action selection. */
bool allstar_one_on_one_rom_select_movement_action_782e(
    AllStarRomAnimationState *state,
    uint8_t input_direction,
    uint8_t override_direction,
    uint8_t previous_direction,
    bool without_ball,
    bool reaction_locked,
    bool *horizontal_flip);
uint8_t allstar_one_on_one_rom_shot_variant(float player_center_x,
                                            float player_ground_y);
/* Bank 1 $07B4/$7EC4/$2F40/$7C58 launch selectors. */
uint8_t allstar_one_on_one_rom_shot_distance_class(float player_center_x,
                                                   float player_ground_y);
uint8_t allstar_one_on_one_rom_shot_profile(uint8_t roster_index);
int16_t allstar_one_on_one_rom_shot_vertical_velocity(
    uint8_t roster_index, uint8_t distance_class, uint8_t pose_index);
int allstar_one_on_one_rom_point_value(float ball_x, float ball_y);
bool allstar_one_on_one_player_can_pick_up_ball(float player_reference_x,
                                                float player_reference_y,
                                                float ball_x,
                                                float ball_y);
bool allstar_one_on_one_rom_action_eligible_0a78(uint8_t action);
uint8_t allstar_one_on_one_rom_defense_jump_action_70fd(uint8_t action);
uint8_t allstar_one_on_one_rom_steal_action_2b14(uint8_t action);
bool allstar_one_on_one_rom_steal_contact_2b14(
    bool possession_active,
    uint8_t ballhandler_action,
    float defender_x,
    float defender_ground_y,
    float ball_x,
    float ball_y,
    uint8_t defender_direction,
    uint8_t ballhandler_direction);
float allstar_one_on_one_rom_jump_height_6c4d(uint16_t elapsed_frames);
bool allstar_one_on_one_rom_jump_recovery_2b6c(
    bool possession_active,
    bool first_contact_locked,
    float player_x,
    float player_ground_y,
    float player_jump_height,
    float ball_x,
    float ball_y,
    float ball_height);
void allstar_one_on_one_rom_clamp_player_court(float *player_center_x,
                                               float *player_ground_y);
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
    bool p2_collision);
void allstar_one_on_one_shot_reset(AllStarOneOnOneShotAttempt *attempt);
uint32_t allstar_one_on_one_shot_input(AllStarOneOnOneShotAttempt *attempt,
                                       int player,
                                       bool a_pressed,
                                       bool b_held);
uint32_t allstar_one_on_one_shot_press(AllStarOneOnOneShotAttempt *attempt,
                                       int player);
uint32_t allstar_one_on_one_shot_tick(AllStarOneOnOneShotAttempt *attempt,
                                      float dt);
int allstar_one_on_one_next_possession_after_score(
    const AllStarOneOnOneMatch *match,
    int shooter);
int allstar_one_on_one_compare_scores(uint16_t p1_score, uint16_t p2_score);
bool allstar_one_on_one_result_can_dismiss(uint8_t buttons_pressed);
bool allstar_one_on_one_overtime_can_dismiss(uint8_t buttons_pressed);

#endif /* ALLSTAR_ONE_ON_ONE_H */
