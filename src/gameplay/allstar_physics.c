#include "allstar_physics.h"
#include <math.h>
#include <string.h>

/* ROM $7BE8 subtracts $000F from 8.8 vertical velocity every 60 Hz frame. */
#define ROM_GRAVITY_PER_FRAME (15.0f / 256.0f)
#define ROM_GRAVITY \
    (ROM_GRAVITY_PER_FRAME * ALLSTAR_PHYSICS_FRAMES_PER_SECOND * \
     ALLSTAR_PHYSICS_FRAMES_PER_SECOND)
#define BALL_GROUND_RESTITUTION 0.5f
#define BALL_GROUND_DRAG 0.8f
#define BALL_STOP_VELOCITY 10.0f
#define RIM_RADIUS 5.0f
#define ROM_BACK_COURT_RETURN_Y_VELOCITY ((40.0f / 256.0f) * 60.0f)
#define ROM_BACK_COURT_RETURN_X_VELOCITY ((36.0f / 256.0f) * 60.0f)

void allstar_physics_init_ball(AllStarBall *ball) {
    if (!ball) return;
    memset(ball, 0, sizeof(AllStarBall));
}

/* ROM bank 1 $7BE8 integrates three 8.8 axes once per frame. */
void allstar_physics_update_ball(AllStarBall *ball, float dt) {
    if (!ball || !ball->in_flight || dt <= 0.0f) return;

    ball->step_accumulator += dt;
    while (ball->step_accumulator + 0.000001f >=
           ALLSTAR_PHYSICS_STEP_SECONDS) {
        float height_delta;
        float crossing_ratio;
        ball->previous_x = ball->x;
        ball->previous_y = ball->y;
        ball->previous_z = ball->z;
        ball->x += ball->vx * ALLSTAR_PHYSICS_STEP_SECONDS;
        ball->y += ball->vy * ALLSTAR_PHYSICS_STEP_SECONDS;
        ball->z += ball->vz * ALLSTAR_PHYSICS_STEP_SECONDS;
        ball->vz -= ROM_GRAVITY * ALLSTAR_PHYSICS_STEP_SECONDS;
        ball->step_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        if (ball->step_accumulator < 0.0f) ball->step_accumulator = 0.0f;

        if (ball->previous_z >= ball->target_z &&
            ball->z <= ball->target_z && ball->z < ball->previous_z) {
            height_delta = ball->previous_z - ball->z;
            crossing_ratio = height_delta > 0.0f
                ? (ball->previous_z - ball->target_z) / height_delta : 1.0f;
            ball->target_crossing_x = ball->previous_x +
                (ball->x - ball->previous_x) * crossing_ratio;
            ball->target_crossing_y = ball->previous_y +
                (ball->y - ball->previous_y) * crossing_ratio;
            ball->target_plane_crossed = true;
        }

        /* The ROM's ground/contact dispatcher is not recovered; keep a
           deterministic native rebound after the traced flight integration. */
        if (ball->z <= 0.0f) {
            ball->z = 0.0f;
            ball->vz = -ball->vz * BALL_GROUND_RESTITUTION;
            ball->vx *= BALL_GROUND_DRAG;
            ball->vy *= BALL_GROUND_DRAG;

            if (fabsf(ball->vz) < BALL_STOP_VELOCITY) {
                ball->vz = 0.0f;
                ball->in_flight = false;
                ball->step_accumulator = 0.0f;
                break;
            }
        }
    }
}

/* Fixed-bank $1CED calls $1F4D at x<$0A, x>=$A0, or y>=$97.  The
   dispatcher then handles y<$5C as a back-court/backboard return rather
   than an out-of-bounds turnover. */
uint32_t allstar_physics_apply_rom_court_contacts(AllStarBall *ball) {
    uint32_t contacts = ALLSTAR_BALL_CONTACT_NONE;
    if (!ball || !ball->in_flight) return contacts;

    if (ball->x < ALLSTAR_ROM_BALL_MIN_X ||
        ball->x >= ALLSTAR_ROM_BALL_MAX_X ||
        ball->y >= ALLSTAR_ROM_BALL_MAX_Y) {
        ball->vx = 0.0f;
        ball->vy = 0.0f;
        contacts |= ALLSTAR_BALL_CONTACT_DEAD_BOUNDARY;
    }

    if (ball->y < ALLSTAR_ROM_BACK_COURT_Y) {
        ball->y = ALLSTAR_ROM_BACK_COURT_RETURN_Y;
        ball->vy = ROM_BACK_COURT_RETURN_Y_VELOCITY;
        if (ball->vx > 0.0f) {
            ball->vx = ROM_BACK_COURT_RETURN_X_VELOCITY;
        } else if (ball->vx < 0.0f) {
            ball->vx = -ROM_BACK_COURT_RETURN_X_VELOCITY;
        }
        contacts |= ALLSTAR_BALL_CONTACT_BACK_COURT;
    }

    return contacts;
}

/* $7EA9 uses a <<3 displacement scale for the normal shot vector, which
   reaches its target in 256/8 = 32 frames in the ROM's 8.8 state. */
void allstar_physics_shoot_ball(AllStarBall *ball, float start_x, float start_y,
                                float target_x, float target_y, float target_z,
                                int shooter_id, int point_value) {
    float flight_time;
    if (!ball) return;
    ball->x = start_x;
    ball->y = start_y;
    ball->z = ALLSTAR_BALL_RELEASE_HEIGHT;
    ball->previous_x = ball->x;
    ball->previous_y = ball->y;
    ball->previous_z = ball->z;
    ball->step_accumulator = 0.0f;
    ball->target_z = target_z;
    ball->target_plane_crossed = false;
    ball->shooter_id = shooter_id;
    ball->point_value = point_value;
    ball->in_flight = true;
    ball->made_basket = false;

    flight_time = ALLSTAR_SHOT_FLIGHT_FRAMES * ALLSTAR_PHYSICS_STEP_SECONDS;
    ball->vx = (target_x - start_x) / flight_time;
    ball->vy = (target_y - start_y) / flight_time;
    /* Discrete integration updates position before subtracting gravity. */
    ball->vz = (target_z - ball->z) / flight_time +
        ROM_GRAVITY * ALLSTAR_PHYSICS_STEP_SECONDS *
        (ALLSTAR_SHOT_FLIGHT_FRAMES - 1) * 0.5f;
}

void allstar_physics_launch_shot(AllStarBall *ball, float start_x, float start_y,
                                 float target_x, float target_y, float target_z,
                                 int shooter_id, int point_value) {
    allstar_physics_shoot_ball(ball, start_x, start_y, target_x, target_y,
                               target_z, shooter_id, point_value);
}

bool allstar_physics_check_basket(AllStarBall *ball, float hoop_x,
                                  float hoop_y, float hoop_z) {
    float dx;
    float dy;
    if (!ball || !ball->in_flight || ball->made_basket ||
        !ball->target_plane_crossed) {
        return false;
    }

    ball->target_plane_crossed = false;
    if (fabsf(ball->target_z - hoop_z) > 0.001f) return false;
    dx = ball->target_crossing_x - hoop_x;
    dy = ball->target_crossing_y - hoop_y;
    if (dx * dx + dy * dy > RIM_RADIUS * RIM_RADIUS) return false;

    ball->made_basket = true;
    return true;
}
