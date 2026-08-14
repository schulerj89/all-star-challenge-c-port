#ifndef ALLSTAR_PHYSICS_H
#define ALLSTAR_PHYSICS_H

#include "allstar_types.h"

typedef struct {
    float x;
    float y;
    float z; /* Height above court */
    float vx;
    float vy;
    float vz;
    bool in_flight;
    bool made_basket;
    int shooter_id;
    int point_value;
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

void allstar_physics_init_ball(AllStarBall *ball);
void allstar_physics_update_ball(AllStarBall *ball, float dt);
void allstar_physics_shoot_ball(AllStarBall *ball, float start_x, float start_y, float target_x, float target_y, float arc_height, int shooter_id, int point_value);
bool allstar_physics_check_basket(const AllStarBall *ball, float hoop_x, float hoop_y, float hoop_z);

#endif /* ALLSTAR_PHYSICS_H */
