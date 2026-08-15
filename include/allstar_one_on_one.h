#ifndef ALLSTAR_ONE_ON_ONE_H
#define ALLSTAR_ONE_ON_ONE_H

#include "allstar_types.h"

#define ALLSTAR_ONE_ON_ONE_RESULT_SECONDS 16.0f

typedef enum {
    ALLSTAR_ONE_ON_ONE_PLAYING = 0,
    ALLSTAR_ONE_ON_ONE_RESULT,
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
    ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE = (1 << 3)
} AllStarOneOnOneEvent;

typedef struct {
    int p1_score;
    int p2_score;
    int play_to;
    int period;
    int winner;
    float game_clock;
    float shot_clock;
    float period_seconds;
    float shot_clock_seconds;
    float result_clock;
    bool p1_possession;
    AllStarOneOnOnePhase phase;
    AllStarOneOnOneEndReason end_reason;
} AllStarOneOnOneMatch;

void allstar_one_on_one_match_init(AllStarOneOnOneMatch *match,
                                   float period_seconds,
                                   float shot_clock_seconds,
                                   int play_to);
uint32_t allstar_one_on_one_match_tick(AllStarOneOnOneMatch *match, float dt);
uint32_t allstar_one_on_one_match_add_score(AllStarOneOnOneMatch *match,
                                            int player,
                                            int points);
uint32_t allstar_one_on_one_match_dismiss_result(AllStarOneOnOneMatch *match);
void allstar_one_on_one_match_reset_shot_clock(AllStarOneOnOneMatch *match);

#endif /* ALLSTAR_ONE_ON_ONE_H */
