#ifndef ALLSTAR_AI_H
#define ALLSTAR_AI_H

#include "allstar_types.h"
#include "allstar_physics.h"
#include "allstar_roster.h"

typedef enum {
    ALLSTAR_AI_STATE_IDLE,
    ALLSTAR_AI_STATE_DEFEND_PERIMETER,
    ALLSTAR_AI_STATE_CONTEST_SHOT,
    ALLSTAR_AI_STATE_DRIVE_TO_HOOP,
    ALLSTAR_AI_STATE_PULL_UP_JUMPER,
    ALLSTAR_AI_STATE_REBOUND
} AllStarAIState;

typedef struct {
    AllStarAIState state;
    float decision_timer;
    float decision_interval;
    float aggression;
    float reaction_speed;
    uint8_t rom_shot_profile;
    uint8_t rom_roster_index;
    uint8_t rom_action_index;
    uint8_t rom_skill_level;
    uint8_t rom_target_x;
    uint8_t rom_target_y;
    uint8_t rom_contact_hold_frames;
    uint8_t rom_contact_offense_count;
    uint8_t rom_contact_saved_x;
    uint8_t rom_contact_saved_y;
    uint8_t rom_offense_stage;
    uint8_t rom_stored_shot_random;
    uint8_t rom_new_input;               /* $FFD2 */
    uint8_t rom_held_input;              /* $FFD3 */
    uint8_t rom_offense_active;          /* $C0F7 */
    uint8_t rom_initial_target_active;    /* $C0F8 */
    uint8_t rom_drive_b_frames;           /* $C0F9 */
    uint8_t rom_defense_offset_frames;    /* $C0FB */
    uint8_t rom_arrived;                 /* $C0FD */
    uint8_t rom_accepted_direction;      /* $C0FE */
    uint8_t rom_special_frames;          /* $C0FF */
    uint8_t rom_direction_hysteresis;    /* $C103 */
    uint8_t rom_direction_reload;        /* $C144 */
    uint8_t rom_force_route;             /* $C106 */
    uint8_t rom_mode2_arrival;           /* $C145 */
    bool rom_steal_pressed;
    bool rom_force_shot;
    bool rom_shot_release;
    bool rom_had_possession;
} AllStarAIController;

typedef struct {
    uint8_t game_mode;             /* $FF8F */
    bool counted_wait_locked;      /* $C12C */
    bool cpu_enabled;              /* $FF90 */
    bool score_event_locked;       /* $FFE2 */
    bool special_route;            /* $FFE4 */
    bool initial_flight;           /* $FFF8 */
    uint8_t cpu_player;            /* $C127 */
    uint8_t possession_owner;      /* $FFCF */
    uint8_t shot_owner;            /* $FFD0 */
    uint8_t skill_level;           /* $FF97 */
    uint8_t random_current;        /* $FFFB */
    uint8_t random_target;         /* $FFFC */
    uint8_t random_route;          /* $FFFD */
    uint8_t random_position;       /* $FFFE */
    uint8_t ball_x;                /* $C0A3 */
    uint8_t ball_y;                /* $C0A7 */
    uint8_t ball_height;           /* $C0AB */
    bool ball_contact;             /* $077D returned Z */
    bool movement_blocked;         /* $C16B */
    uint8_t cpu_action;
    uint8_t cpu_record;
    uint8_t cpu_roster_index;
    uint8_t cpu_shot_profile;
    uint8_t cpu_stored_direction;
    float cpu_center_x;
    float cpu_ground_y;
    uint8_t opponent_action;
    uint8_t opponent_record;
    uint8_t opponent_stored_direction;
    float opponent_center_x;
    float opponent_ground_y;
    uint8_t mode2_target_x;        /* $FFDB */
    uint8_t mode2_target_y;        /* $FFDC */
    uint8_t mode2_state;           /* $C172 */
} AllStarRomCpuControllerContext;

void allstar_ai_init(AllStarAIController *ai, const AllStarPlayerStats *stats);
void allstar_ai_set_skill(AllStarAIController *ai, uint8_t skill_level);
void allstar_ai_set_rom_profile(AllStarAIController *ai, uint8_t roster_index);
uint8_t allstar_ai_rom_direction_74bb(float current_x, float current_y,
                                     uint8_t target_x, uint8_t target_y);
void allstar_ai_rom_offense_target_72ea(uint8_t ball_x, uint8_t random_byte,
                                       uint8_t *target_x, uint8_t *target_y);
/*
 * $07B4.  Returns Z when the entity is inside the lane box: the ground row
 * $15 must be above $5C + margin, and the centre must straddle $54 within the
 * margin.  The caller passes the margin in B, which is how the same routine
 * serves the $1E/$1A/$12 tests in $73DB.
 */
bool allstar_ai_rom_inside_07b4(float center_x, float ground_y,
                                uint8_t margin);

void allstar_ai_rom_route_target_732c(uint8_t roster_index,
                                     uint8_t route_random,
                                     uint8_t position_random,
                                     uint8_t *target_x,
                                     uint8_t *target_y);
bool allstar_ai_rom_should_shoot_756c(uint8_t profile,
                                     uint8_t distance_class,
                                     uint8_t animation_record,
                                     uint8_t skill_level,
                                     uint8_t profile_random,
                                     uint8_t skill_random);
bool allstar_ai_rom_should_contest_71ee(float cpu_x, float cpu_y,
                                       bool opponent_shot_in_flight);
bool allstar_ai_rom_should_steal_71b3(uint8_t skill_level,
                                     uint8_t random_byte,
                                     bool ball_contact);
bool allstar_ai_rom_contact_response_75cd(
    AllStarAIController *ai,
    bool movement_blocked,
    bool owns_ball,
    uint8_t random_byte,
    uint8_t target_random,
    uint8_t ball_x,
    float cpu_center_x,
    float cpu_ground_y);
/* Bank 1 $7170-$761A: complete CPU synthetic-input controller. */
void allstar_ai_rom_controller_7170(
    AllStarAIController *ai,
    const AllStarRomCpuControllerContext *context);
void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu,
                       const AllStarPlayerState *human,
                       const AllStarBall *ball, uint8_t rom_random_byte,
                       uint8_t target_random, uint8_t route_random,
                       uint8_t position_random,
                       float dt);

#endif /* ALLSTAR_AI_H */
