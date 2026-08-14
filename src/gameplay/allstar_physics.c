#include "allstar_physics.h"
#include <math.h>
#include <string.h>

#define GRAVITY 180.0f

void allstar_physics_init_ball(AllStarBall *ball) {
    if (!ball) return;
    memset(ball, 0, sizeof(AllStarBall));
}

/* Ghidra: Call_001_7f37 - Ball flight trajectory & arc dynamics */
void allstar_physics_update_ball(AllStarBall *ball, float dt) {
    if (!ball || !ball->in_flight) return;

    ball->x += ball->vx * dt;
    ball->y += ball->vy * dt;
    ball->z += ball->vz * dt;
    ball->vz -= GRAVITY * dt;

    /* Ground bounce */
    if (ball->z <= 0.0f) {
        ball->z = 0.0f;
        ball->vz = -ball->vz * 0.6f; /* Energy loss on bounce */
        ball->vx *= 0.8f;
        ball->vy *= 0.8f;

        if (fabsf(ball->vz) < 10.0f) {
            ball->vz = 0.0f;
            ball->in_flight = false;
        }
    }
}

/* Ghidra: Call_001_7ea9 - Ball shot launch & parabolic velocity multiplier */
void allstar_physics_shoot_ball(AllStarBall *ball, float start_x, float start_y, float target_x, float target_y, float arc_height, int shooter_id, int point_value) {
    if (!ball) return;
    ball->x = start_x;
    ball->y = start_y;
    ball->z = 16.0f; /* Release height above ground */
    ball->shooter_id = shooter_id;
    ball->point_value = point_value;
    ball->in_flight = true;
    ball->made_basket = false;

    float flight_time = 0.85f; /* Standard flight duration to rim */
    ball->vx = (target_x - start_x) / flight_time;
    ball->vy = (target_y - start_y) / flight_time;
    ball->vz = (arc_height + 0.5f * GRAVITY * flight_time * flight_time) / flight_time;
}

void allstar_physics_launch_shot(AllStarBall *ball, float start_x, float start_y, float target_x, float target_y, float arc_height, int shooter_id, int point_value) {
    allstar_physics_shoot_ball(ball, start_x, start_y, target_x, target_y, arc_height, shooter_id, point_value);
}

/* Ghidra: Call_001_7ec4 - Rim collision & basket net detection */
bool allstar_physics_check_basket(const AllStarBall *ball, float hoop_x, float hoop_y, float hoop_z) {
    if (!ball || !ball->in_flight) return false;
    float dx = ball->x - hoop_x;
    float dy = ball->y - hoop_y;
    float dz = ball->z - hoop_z;
    float dist_xy = sqrtf(dx * dx + dy * dy);
    return (dist_xy < 8.0f) && (fabsf(dz) < 20.0f) && (ball->vz < 0.0f);
}
