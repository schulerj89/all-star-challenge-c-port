#ifndef ALLSTAR_PHYSICS_H
#define ALLSTAR_PHYSICS_H

#include "allstar_types.h"

#define ALLSTAR_PHYSICS_FRAMES_PER_SECOND 60
#define ALLSTAR_PHYSICS_STEP_SECONDS \
    (1.0f / (float)ALLSTAR_PHYSICS_FRAMES_PER_SECOND)
#define ALLSTAR_SHOT_FLIGHT_FRAMES 32
#define ALLSTAR_BALL_RELEASE_HEIGHT 16.0f
#define ALLSTAR_HOOP_HEIGHT 16.0f

/* Fixed-bank $1CED court/contact dispatcher thresholds. */
#define ALLSTAR_ROM_BALL_MIN_X 10.0f
#define ALLSTAR_ROM_BALL_MAX_X 160.0f
#define ALLSTAR_ROM_BALL_MAX_Y 151.0f
#define ALLSTAR_ROM_BACK_COURT_Y 92.0f
#define ALLSTAR_ROM_BACK_COURT_RETURN_Y 94.0f

typedef enum {
    ALLSTAR_BALL_CONTACT_NONE = 0,
    ALLSTAR_BALL_CONTACT_DEAD_BOUNDARY = (1 << 0),
    ALLSTAR_BALL_CONTACT_BACK_COURT = (1 << 1)
} AllStarBallContact;

typedef struct {
    int16_t vx;
    int16_t vy;
    int16_t vz;
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint8_t gravity_delay_frames;
} AllStarRomBallStepState;

typedef struct {
    float x;
    float y;
    float z; /* Height above court */
    float vx;
    float vy;
    float vz;
    float previous_x;
    float previous_y;
    float previous_z;
    float step_accumulator;
    float target_z;
    float target_crossing_x;
    float target_crossing_y;
    bool target_plane_crossed;
    bool recoverable;
    bool in_flight;
    bool made_basket;
    int shooter_id;
    int point_value;
    AllStarRomBallStepState rom_step_state;
    bool rom_step_state_valid;
} AllStarBall;

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    float stamina;
    uint8_t anim_frame;
    bool has_ball;
    bool is_shooting;
    bool is_jumping;
    bool is_defending;
} AllStarPlayerState;

/* ROM bank 1 $7BE8: 60 Hz fixed-point ball flight update. */
void allstar_physics_init_ball(AllStarBall *ball);
void allstar_physics_rom_step_7be8(AllStarRomBallStepState *state);
void allstar_physics_update_ball(AllStarBall *ball, float dt);

/* Fixed-bank $1CED/$1F4D: court limits and planar-velocity response. */
uint32_t allstar_physics_apply_rom_court_contacts(AllStarBall *ball);

/* ROM bank 1 $7C58/$7EA9: target displacement and launch-vector setup. */
void allstar_physics_shoot_ball(AllStarBall *ball, float start_x, float start_y,
                                float target_x, float target_y, float target_z,
                                int shooter_id, int point_value);
void allstar_physics_launch_shot(AllStarBall *ball, float start_x, float start_y,
                                 float target_x, float target_y, float target_z,
                                 int shooter_id, int point_value);

/* Native rim-plane crossing test; ROM collision/rim behavior is still untraced. */
bool allstar_physics_check_basket(AllStarBall *ball, float hoop_x,
                                  float hoop_y, float hoop_z);

#endif /* ALLSTAR_PHYSICS_H */
