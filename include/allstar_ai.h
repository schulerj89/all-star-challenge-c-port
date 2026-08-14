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
    float aggression;
    float reaction_speed;
} AllStarAIController;

void allstar_ai_init(AllStarAIController *ai, const AllStarPlayerStats *stats);
void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu, const AllStarPlayerState *human, const AllStarBall *ball, float dt);

#endif /* ALLSTAR_AI_H */
