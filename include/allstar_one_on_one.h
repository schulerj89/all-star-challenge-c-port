#ifndef ALLSTAR_ONE_ON_ONE_H
#define ALLSTAR_ONE_ON_ONE_H

#include "allstar_types.h"

#define ALLSTAR_ONE_ON_ONE_RESULT_FRAMES 960
#define ALLSTAR_ONE_ON_ONE_RESULT_SECONDS (ALLSTAR_ONE_ON_ONE_RESULT_FRAMES / 60.0f)
#define ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES 240
#define ALLSTAR_ONE_ON_ONE_OVERTIME_SECONDS (ALLSTAR_ONE_ON_ONE_OVERTIME_FRAMES / 60.0f)

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
    ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME_NOTICE = (1 << 4)
} AllStarOneOnOneEvent;

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
int allstar_one_on_one_next_possession_after_score(
    const AllStarOneOnOneMatch *match,
    int shooter);
int allstar_one_on_one_compare_scores(uint16_t p1_score, uint16_t p2_score);
bool allstar_one_on_one_result_can_dismiss(uint8_t buttons_pressed);
bool allstar_one_on_one_overtime_can_dismiss(uint8_t buttons_pressed);

#endif /* ALLSTAR_ONE_ON_ONE_H */
