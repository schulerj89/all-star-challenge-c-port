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
