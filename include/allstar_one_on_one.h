#ifndef ALLSTAR_ONE_ON_ONE_H
#define ALLSTAR_ONE_ON_ONE_H

#include "allstar_types.h"

#define ALLSTAR_ONE_ON_ONE_RESULT_FRAMES 960
#define ALLSTAR_ONE_ON_ONE_RESULT_SECONDS (ALLSTAR_ONE_ON_ONE_RESULT_FRAMES / 60.0f)
#define ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES 240
#define ALLSTAR_ONE_ON_ONE_OVERTIME_SECONDS (ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES / 60.0f)
/* Provisional input window; the action-$0A/$12 table itself totals 67 frames. */
#define ALLSTAR_ONE_ON_ONE_SHOT_GATHER_FRAMES 30
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
uint8_t allstar_one_on_one_rom_shot_variant(float player_center_x,
                                            float player_ground_y);
bool allstar_one_on_one_player_can_pick_up_ball(float player_reference_x,
                                                float player_reference_y,
                                                float ball_x,
                                                float ball_y);
void allstar_one_on_one_rom_clamp_player_court(float *player_center_x,
                                               float *player_ground_y);
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
    bool p2_collision);
void allstar_one_on_one_shot_reset(AllStarOneOnOneShotAttempt *attempt);
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
