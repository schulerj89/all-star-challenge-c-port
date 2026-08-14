#include "allstar_ai.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void allstar_ai_init(AllStarAIController *ai, const AllStarPlayerStats *stats) {
    if (!ai) return;
    memset(ai, 0, sizeof(AllStarAIController));
    ai->state = ALLSTAR_AI_STATE_IDLE;
    ai->decision_timer = 0.0f;
    ai->aggression = stats ? ((float)stats->defense / 100.0f) : 0.8f;
    ai->reaction_speed = stats ? ((float)stats->speed / 100.0f) : 0.85f;
}

void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu, const AllStarPlayerState *human, const AllStarBall *ball, float dt) {
    if (!ai || !cpu || !human || !ball) return;

    ai->decision_timer -= dt;
    if (ai->decision_timer <= 0.0f) {
        ai->decision_timer = 0.20f; /* Recalculate tactics 5 times per second */

        if (cpu->has_ball) {
            /* Offensive logic */
            float dist_to_human = sqrtf((cpu->x - human->x) * (cpu->x - human->x) + (cpu->y - human->y) * (cpu->y - human->y));
            if (cpu->y <= 120.0f && (dist_to_human > 18.0f || (rand() % 100 < 30))) {
                ai->state = ALLSTAR_AI_STATE_PULL_UP_JUMPER;
            } else {
                ai->state = ALLSTAR_AI_STATE_DRIVE_TO_HOOP;
            }
        } else if (ball->in_flight || (!human->has_ball && !cpu->has_ball)) {
            ai->state = ALLSTAR_AI_STATE_REBOUND;
        } else {
            /* Defensive logic */
            ai->state = ALLSTAR_AI_STATE_DEFEND_PERIMETER;
        }
    }

    /* Reset transient flags */
    cpu->is_defending = false;
    cpu->is_jumping = false;

    /* Execute AI movement according to state */
    float move_speed = 65.0f * ai->reaction_speed;
    switch (ai->state) {
        case ALLSTAR_AI_STATE_DEFEND_PERIMETER: {
            /* Stay tightly between human player and basket */
            float target_x = human->x;
            float target_y = human->y - 18.0f;
            if (target_y < 88.0f) target_y = 88.0f;

            float dx = target_x - cpu->x;
            float dy = target_y - cpu->y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 2.0f) {
                cpu->x += (dx / len) * move_speed * dt;
                cpu->y += (dy / len) * move_speed * dt;
            }

            /* Contest or steal */
            if (human->is_shooting) {
                cpu->is_jumping = true;
            } else if (len < 16.0f) {
                cpu->is_defending = true;
            }
            break;
        }
        case ALLSTAR_AI_STATE_DRIVE_TO_HOOP: {
            float dx = 80.0f - cpu->x;
            float dy = 96.0f - cpu->y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 3.0f) {
                cpu->x += (dx / len) * move_speed * dt;
                cpu->y += (dy / len) * move_speed * dt;
            }
            break;
        }
        case ALLSTAR_AI_STATE_PULL_UP_JUMPER: {
            /* CPU sets shooting flag */
            if (cpu->has_ball && !cpu->is_shooting) {
                cpu->is_shooting = true;
            }
            break;
        }
        case ALLSTAR_AI_STATE_REBOUND: {
            float dx = ball->x - cpu->x;
            float dy = ball->y - cpu->y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 2.0f) {
                cpu->x += (dx / len) * move_speed * dt;
                cpu->y += (dy / len) * move_speed * dt;
            }
            break;
        }
        default:
            break;
    }

    /* Court bounds clamping */
    if (cpu->x < 20.0f) cpu->x = 20.0f;
    if (cpu->x > 140.0f) cpu->x = 140.0f;
    if (cpu->y < 88.0f) cpu->y = 88.0f;
    if (cpu->y > 136.0f) cpu->y = 136.0f;
}
