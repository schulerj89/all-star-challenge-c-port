#include "allstar_ai.h"
#include <math.h>
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
        ai->decision_timer = 0.25f; /* Recalculate tactics 4 times per second */

        if (cpu->has_ball) {
            /* Offensive logic */
            float dist_to_human = sqrtf((cpu->x - human->x) * (cpu->x - human->x) + (cpu->y - human->y) * (cpu->y - human->y));
            if (dist_to_human > 24.0f) {
                ai->state = ALLSTAR_AI_STATE_PULL_UP_JUMPER;
            } else {
                ai->state = ALLSTAR_AI_STATE_DRIVE_TO_HOOP;
            }
        } else if (ball->in_flight) {
            ai->state = ALLSTAR_AI_STATE_REBOUND;
        } else {
            /* Defensive logic */
            ai->state = ALLSTAR_AI_STATE_DEFEND_PERIMETER;
        }
    }

    /* Execute AI movement according to state */
    float move_speed = 60.0f * ai->reaction_speed;
    switch (ai->state) {
        case ALLSTAR_AI_STATE_DEFEND_PERIMETER: {
            /* Stay between human player and basket (basket at x=80, y=20) */
            float target_x = (human->x + 80.0f) * 0.5f;
            float target_y = (human->y + 20.0f) * 0.5f;
            float dx = target_x - cpu->x;
            float dy = target_y - cpu->y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 2.0f) {
                cpu->x += (dx / len) * move_speed * dt;
                cpu->y += (dy / len) * move_speed * dt;
            }
            break;
        }
        case ALLSTAR_AI_STATE_DRIVE_TO_HOOP: {
            float dx = 80.0f - cpu->x;
            float dy = 30.0f - cpu->y;
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 4.0f) {
                cpu->x += (dx / len) * move_speed * dt;
                cpu->y += (dy / len) * move_speed * dt;
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
}
