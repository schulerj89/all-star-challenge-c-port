#include "allstar_one_on_one.h"
#include <string.h>

static void allstar_one_on_one_begin_result(AllStarOneOnOneMatch *match,
                                            AllStarOneOnOneEndReason reason) {
    match->game_clock = 0.0f;
    match->result_clock = ALLSTAR_ONE_ON_ONE_RESULT_SECONDS;
    match->end_reason = reason;
    match->phase = ALLSTAR_ONE_ON_ONE_RESULT;

    if (match->p1_score > match->p2_score) match->winner = 1;
    else if (match->p2_score > match->p1_score) match->winner = 2;
    else match->winner = 0;
}

static uint32_t allstar_one_on_one_advance_result(AllStarOneOnOneMatch *match) {
    if (match->winner == 0) {
        match->period++;
        match->phase = ALLSTAR_ONE_ON_ONE_PLAYING;
        match->end_reason = ALLSTAR_ONE_ON_ONE_END_NONE;
        match->game_clock = match->period_seconds;
        match->shot_clock = match->shot_clock_seconds;
        match->result_clock = 0.0f;
        match->p1_possession = true;
        return ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME;
    }

    match->phase = ALLSTAR_ONE_ON_ONE_COMPLETE;
    match->result_clock = 0.0f;
    return ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE;
}

void allstar_one_on_one_match_init(AllStarOneOnOneMatch *match,
                                   float period_seconds,
                                   float shot_clock_seconds,
                                   int play_to) {
    if (!match) return;
    memset(match, 0, sizeof(*match));
    match->period_seconds = period_seconds > 0.0f ? period_seconds : 120.0f;
    match->shot_clock_seconds = shot_clock_seconds > 0.0f ? shot_clock_seconds : 24.0f;
    match->game_clock = match->period_seconds;
    match->shot_clock = match->shot_clock_seconds;
    match->play_to = play_to > 0 ? play_to : 0;
    match->period = 1;
    match->p1_possession = true;
    match->phase = ALLSTAR_ONE_ON_ONE_PLAYING;
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

    match->game_clock -= dt;
    if (match->game_clock <= 0.0f) {
        allstar_one_on_one_begin_result(match, ALLSTAR_ONE_ON_ONE_END_TIME);
        return ALLSTAR_ONE_ON_ONE_EVENT_RESULT;
    }

    match->shot_clock -= dt;
    if (match->shot_clock <= 0.0f) {
        match->shot_clock = match->shot_clock_seconds;
        match->p1_possession = !match->p1_possession;
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

void allstar_one_on_one_match_reset_shot_clock(AllStarOneOnOneMatch *match) {
    if (!match) return;
    match->shot_clock = match->shot_clock_seconds;
}
