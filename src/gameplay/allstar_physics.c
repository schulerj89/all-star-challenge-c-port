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

static int16_t allstar_physics_to_rom_velocity(float velocity_per_second) {
    return (int16_t)lroundf(
        velocity_per_second * 256.0f / ALLSTAR_PHYSICS_FRAMES_PER_SECOND);
}

static float allstar_physics_from_rom_velocity(int16_t velocity_per_frame) {
    return (float)velocity_per_frame * ALLSTAR_PHYSICS_FRAMES_PER_SECOND /
        256.0f;
}

static uint16_t allstar_physics_to_rom_position(float position) {
    return (uint16_t)(int16_t)lroundf(position * 256.0f);
}

static float allstar_physics_from_rom_unsigned_position(uint16_t position) {
    return (float)position / 256.0f;
}

static float allstar_physics_from_rom_signed_position(uint16_t position) {
    return (float)(int16_t)position / 256.0f;
}

void allstar_physics_init_ball(AllStarBall *ball) {
    if (!ball) return;
    memset(ball, 0, sizeof(AllStarBall));
}

/* Bank 1 $7BE8 operates directly on three 8.8 velocity/position pairs.
   Gravity ($FFF1 == -15) is applied before integration. While the height
   integer byte is zero, each nonzero planar velocity receives a raw +/-2
   impulse toward the opposite sign; the original operation can cross zero. */
void allstar_physics_rom_step_7be8(AllStarRomBallStepState *state) {
    if (!state) return;

    if (state->gravity_delay_frames > 0) {
        state->gravity_delay_frames--;
    } else {
        state->vz = (int16_t)(state->vz - 15);
    }

    if ((state->z >> 8) == 0) {
        if (state->vx != 0) {
            state->vx = (int16_t)(state->vx +
                (state->vx < 0 ? 2 : -2));
        }
        if (state->vy != 0) {
            state->vy = (int16_t)(state->vy +
                (state->vy < 0 ? 2 : -2));
        }
    }

    state->x = (uint16_t)(state->x + (uint16_t)state->vx);
    state->y = (uint16_t)(state->y + (uint16_t)state->vy);
    state->z = (uint16_t)(state->z + (uint16_t)state->vz);
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

        if (!ball->rom_step_state_valid) {
            ball->rom_step_state.vx = allstar_physics_to_rom_velocity(ball->vx);
            ball->rom_step_state.vy = allstar_physics_to_rom_velocity(ball->vy);
            ball->rom_step_state.vz = allstar_physics_to_rom_velocity(ball->vz);
            ball->rom_step_state.x = allstar_physics_to_rom_position(ball->x);
            ball->rom_step_state.y = allstar_physics_to_rom_position(ball->y);
            ball->rom_step_state.z = allstar_physics_to_rom_position(ball->z);
            ball->rom_step_state.gravity_delay_frames = 0;
            ball->rom_step_state_valid = true;
        }
        allstar_physics_rom_step_7be8(&ball->rom_step_state);
        ball->vx = allstar_physics_from_rom_velocity(ball->rom_step_state.vx);
        ball->vy = allstar_physics_from_rom_velocity(ball->rom_step_state.vy);
        ball->vz = allstar_physics_from_rom_velocity(ball->rom_step_state.vz);
        ball->x = allstar_physics_from_rom_unsigned_position(ball->rom_step_state.x);
        ball->y = allstar_physics_from_rom_unsigned_position(ball->rom_step_state.y);
        ball->z = allstar_physics_from_rom_signed_position(ball->rom_step_state.z);
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
            /* $1E77 clears $FFF8 at the first ground contact, enabling the
               later $2AE2 low-ball recovery checks while bounces continue. */
            ball->recoverable = true;
            ball->vz = -ball->vz * BALL_GROUND_RESTITUTION;
            ball->vx *= BALL_GROUND_DRAG;
            ball->vy *= BALL_GROUND_DRAG;
            ball->rom_step_state_valid = false;

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
        if (ball->rom_step_state_valid) {
            ball->rom_step_state.vx = 0;
            ball->rom_step_state.vy = 0;
        }
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
        ball->rom_step_state_valid = false;
        contacts |= ALLSTAR_BALL_CONTACT_BACK_COURT;
    }

    return contacts;
}

/* $7EA9 uses a <<3 displacement scale for the normal shot vector, which
   reaches its target in 256/8 = 32 frames in the ROM's 8.8 state. */
void allstar_physics_shoot_ball(AllStarBall *ball, float start_x, float start_y,
                                float target_x, float target_y, float target_z,
                                int shooter_id, int point_value) {
    allstar_physics_shoot_ball_from_height(
        ball, start_x, start_y, ALLSTAR_BALL_RELEASE_HEIGHT,
        target_x, target_y, target_z, shooter_id, point_value);
}

void allstar_physics_shoot_ball_from_height(
    AllStarBall *ball, float start_x, float start_y, float start_z,
    float target_x, float target_y, float target_z,
    int shooter_id, int point_value) {
    int32_t start_x_raw;
    int32_t start_y_raw;
    int32_t start_z_raw;
    int32_t target_x_raw;
    int32_t target_y_raw;
    int32_t target_z_raw;
    int32_t gravity_sum;
    if (!ball) return;
    ball->x = start_x;
    ball->y = start_y;
    ball->z = start_z;
    ball->previous_x = ball->x;
    ball->previous_y = ball->y;
    ball->previous_z = ball->z;
    ball->step_accumulator = 0.0f;
    ball->target_z = target_z;
    ball->target_plane_crossed = false;
    ball->recoverable = false;
    ball->shooter_id = shooter_id;
    ball->point_value = point_value;
    ball->in_flight = true;
    ball->made_basket = false;

    start_x_raw = (int32_t)lroundf(start_x * 256.0f);
    start_y_raw = (int32_t)lroundf(start_y * 256.0f);
    start_z_raw = (int32_t)lroundf(ball->z * 256.0f);
    target_x_raw = (int32_t)lroundf(target_x * 256.0f);
    target_y_raw = (int32_t)lroundf(target_y * 256.0f);
    target_z_raw = (int32_t)lroundf(target_z * 256.0f);
    gravity_sum = 15 * ALLSTAR_SHOT_FLIGHT_FRAMES *
        (ALLSTAR_SHOT_FLIGHT_FRAMES + 1) / 2;

    ball->rom_step_state.x = (uint16_t)start_x_raw;
    ball->rom_step_state.y = (uint16_t)start_y_raw;
    ball->rom_step_state.z = (uint16_t)start_z_raw;
    ball->rom_step_state.vx = (int16_t)(
        (target_x_raw - start_x_raw) / ALLSTAR_SHOT_FLIGHT_FRAMES);
    ball->rom_step_state.vy = (int16_t)(
        (target_y_raw - start_y_raw) / ALLSTAR_SHOT_FLIGHT_FRAMES);
    ball->rom_step_state.vz = (int16_t)(
        (target_z_raw - start_z_raw + gravity_sum) /
        ALLSTAR_SHOT_FLIGHT_FRAMES);
    ball->rom_step_state.gravity_delay_frames = 0;
    ball->rom_step_state_valid = true;
    ball->vx = allstar_physics_from_rom_velocity(ball->rom_step_state.vx);
    ball->vy = allstar_physics_from_rom_velocity(ball->rom_step_state.vy);
    ball->vz = allstar_physics_from_rom_velocity(ball->rom_step_state.vz);
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
