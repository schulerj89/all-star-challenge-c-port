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
    uint8_t rom_action_index;
    uint8_t rom_skill_level;
    uint8_t rom_target_x;
    uint8_t rom_target_y;
} AllStarAIController;

void allstar_ai_init(AllStarAIController *ai, const AllStarPlayerStats *stats);
void allstar_ai_set_skill(AllStarAIController *ai, uint8_t skill_level);
void allstar_ai_set_rom_profile(AllStarAIController *ai, uint8_t roster_index);
uint8_t allstar_ai_rom_direction_74bb(float current_x, float current_y,
                                     uint8_t target_x, uint8_t target_y);
void allstar_ai_rom_offense_target_72ea(uint8_t ball_x, uint8_t random_byte,
                                       uint8_t *target_x, uint8_t *target_y);
bool allstar_ai_rom_should_shoot_756c(uint8_t profile,
                                     uint8_t distance_class,
                                     uint8_t action_index,
                                     uint8_t skill_level,
                                     uint8_t profile_random,
                                     uint8_t skill_random);
bool allstar_ai_rom_should_contest_71ee(float cpu_x, float cpu_y,
                                       bool opponent_shot_in_flight);
void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu, const AllStarPlayerState *human, const AllStarBall *ball, float dt);

#endif /* ALLSTAR_AI_H */
