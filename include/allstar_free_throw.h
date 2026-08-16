#ifndef ALLSTAR_FREE_THROW_H
#define ALLSTAR_FREE_THROW_H

#include "allstar_physics.h"
#include "allstar_types.h"

#define ALLSTAR_FREE_THROW_PRESENTATION_FRAMES 292

typedef enum {
    ALLSTAR_FREE_THROW_AIMING = 0,
    ALLSTAR_FREE_THROW_PRESENTATION,
    ALLSTAR_FREE_THROW_RESULT
} AllStarFreeThrowPhase;

typedef enum {
    ALLSTAR_FREE_THROW_EVENT_NONE = 0,
    ALLSTAR_FREE_THROW_EVENT_RELEASE = (1u << 0),
    ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT = (1u << 1), /* command $0A/$0C */
    ALLSTAR_FREE_THROW_EVENT_RIM = (1u << 2),          /* command $09 */
    ALLSTAR_FREE_THROW_EVENT_MAKE = (1u << 3),
    ALLSTAR_FREE_THROW_EVENT_NET = (1u << 4),          /* command $08 */
    ALLSTAR_FREE_THROW_EVENT_SCORE = (1u << 5),        /* command $05 */
    ALLSTAR_FREE_THROW_EVENT_NEXT_ATTEMPT = (1u << 6),
    ALLSTAR_FREE_THROW_EVENT_RESULT = (1u << 7)
} AllStarFreeThrowEvent;

typedef struct {
    AllStarFreeThrowPhase phase;
    AllStarRomBallStepState ball;
    uint16_t aim_x;
    uint16_t aim_y;
    int16_t aim_vx;
    int16_t aim_vy;
    uint8_t previous_aim_x;
    uint8_t previous_aim_y;
    uint8_t aim_timer;
    uint8_t attempts_limit;
    uint8_t attempts_remaining;
    uint8_t attempts_taken;
    uint8_t makes;
    uint8_t player_profile;
    uint8_t net_state;
    uint8_t net_timer;
    uint8_t contact_cooldown;
    uint8_t center_latch;
    uint8_t ricochet_count;
    uint16_t presentation_frame;
    bool physics_enabled;
    bool made_current;
    bool score_pending;
} AllStarFreeThrowState;

/* Free Throw entry/reset path: fixed bank $0C8E -> $17AA -> $18E7. */
void allstar_free_throw_init(AllStarFreeThrowState *state,
                             uint8_t attempts, uint8_t rng,
                             uint8_t roster_id);
void allstar_free_throw_reset_attempt_17aa(AllStarFreeThrowState *state,
                                           uint8_t rng);

/* Per-frame mode-1 path beneath fixed-bank $100F. */
uint32_t allstar_free_throw_tick_100f(AllStarFreeThrowState *state,
                                     uint8_t held_buttons,
                                     uint8_t pressed_buttons,
                                     uint8_t rng);

/* Pure ROM translations used by deterministic regression tests. */
void allstar_free_throw_aim_init_18e7(AllStarFreeThrowState *state,
                                      uint8_t rng);
void allstar_free_throw_aim_input_1942(AllStarFreeThrowState *state,
                                       uint8_t held_buttons);
void allstar_free_throw_aim_step_1986(AllStarFreeThrowState *state);
bool allstar_free_throw_launch_1caa_7c58(AllStarFreeThrowState *state,
                                        uint8_t rng);
uint8_t allstar_free_throw_player_profile_2f40(uint8_t roster_id);
void allstar_free_throw_ball_screen_1884(const AllStarFreeThrowState *state,
                                         int *screen_x, int *screen_y);

/* Deterministic proof hook: changes only the visible 8.8 target. */
void allstar_free_throw_set_test_aim(AllStarFreeThrowState *state,
                                     uint8_t x, uint8_t y);

#endif /* ALLSTAR_FREE_THROW_H */
