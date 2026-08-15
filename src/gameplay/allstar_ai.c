#include "allstar_ai.h"
#include "allstar_one_on_one.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void allstar_ai_init(AllStarAIController *ai, const AllStarPlayerStats *stats) {
    if (!ai) return;
    memset(ai, 0, sizeof(AllStarAIController));
    ai->state = ALLSTAR_AI_STATE_IDLE;
    ai->decision_timer = 0.0f;
    ai->decision_interval = 8.0f / 60.0f;
    ai->aggression = stats ? ((float)stats->defense / 100.0f) : 0.8f;
    ai->reaction_speed = stats ? ((float)stats->speed / 100.0f) : 0.85f;
    ai->rom_shot_profile = 2;
    ai->rom_action_index = 2;
    ai->rom_skill_level = 1;
}

/* ROM $1FFA maps skill levels 1/2/3 to update delays of 8/4/1 frames. */
void allstar_ai_set_skill(AllStarAIController *ai, uint8_t skill_level) {
    static const float ROM_SKILL_DELAYS[3] = {
        8.0f / 60.0f, 4.0f / 60.0f, 1.0f / 60.0f
    };
    if (!ai) return;
    if (skill_level < 1 || skill_level > 3) skill_level = 1;
    ai->rom_skill_level = skill_level;
    ai->decision_interval = ROM_SKILL_DELAYS[skill_level - 1];
    if (ai->decision_timer > ai->decision_interval) {
        ai->decision_timer = ai->decision_interval;
    }
}

void allstar_ai_set_rom_profile(AllStarAIController *ai, uint8_t roster_index) {
    if (!ai) return;
    ai->rom_shot_profile = allstar_one_on_one_rom_shot_profile(roster_index);
}

/* $74BB emits direction bits only when an axis is more than four pixels
   from its target. Native X already includes player +$06's eight-pixel bias. */
uint8_t allstar_ai_rom_direction_74bb(float current_x, float current_y,
                                     uint8_t target_x, uint8_t target_y) {
    int dx = (int)current_x - (int)target_x;
    int dy = (int)current_y - (int)target_y;
    uint8_t direction = 0;
    if (dy > 3) direction |= 0x40;
    else if (dy < -4) direction |= 0x80;
    if (dx > 3) direction |= 0x20;
    else if (dx < -4) direction |= 0x10;
    return direction;
}

/* $72EA selects one of four targets from the side-specific tables at
   $731C/$7324 using random bins $00..$2F, $30..$6F, $70..$AF, $B0..$FF. */
void allstar_ai_rom_offense_target_72ea(uint8_t ball_x, uint8_t random_byte,
                                       uint8_t *target_x, uint8_t *target_y) {
    static const uint8_t targets[2][4][2] = {
        {{0x10,0x68},{0x1c,0x8c},{0x10,0x7c},{0x40,0x98}},
        {{0x94,0x68},{0x8c,0x8c},{0x90,0x7c},{0x78,0x98}}
    };
    uint8_t side = ball_x < 0x54 ? 0 : 1;
    uint8_t index = random_byte < 0x30 ? 0 :
        (random_byte < 0x70 ? 1 : (random_byte < 0xb0 ? 2 : 3));
    if (target_x) *target_x = targets[side][index][0];
    if (target_y) *target_y = targets[side][index][1];
}

/* $756C uses profile thresholds $B0/$60/$40, requires the current action
   to match distance classes 0/1/2/far as 4/5/6/7, then falls back to the
   skill thresholds $1A/$0C/$06 for actions 5..7. */
bool allstar_ai_rom_should_shoot_756c(uint8_t profile,
                                     uint8_t distance_class,
                                     uint8_t action_index,
                                     uint8_t skill_level,
                                     uint8_t profile_random,
                                     uint8_t skill_random) {
    static const uint8_t profile_threshold[3] = {0xb0,0x60,0x40};
    static const uint8_t skill_threshold[3] = {0x1a,0x0c,0x06};
    uint8_t expected_action;
    if (profile > 2) profile = 2;
    if (skill_level < 1 || skill_level > 3) skill_level = 1;
    expected_action = distance_class < 3
        ? (uint8_t)(4 + distance_class) : 7;
    if (profile_random < profile_threshold[profile]) {
        return action_index == expected_action;
    }
    return skill_random >= skill_threshold[skill_level - 1] &&
           action_index >= 5 && action_index < 8;
}

/* $71EE gates the CPU A/contest input on an opponent flight and the same
   $07B4 hoop rectangle with margin $0E. */
bool allstar_ai_rom_should_contest_71ee(float cpu_x, float cpu_y,
                                       bool opponent_shot_in_flight) {
    if (!opponent_shot_in_flight) return false;
    return cpu_y < 0x5c + 0x0e && cpu_x >= 0x55 - 0x0e &&
           cpu_x < 0x54 + 0x0e;
}

void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu, const AllStarPlayerState *human, const AllStarBall *ball, float dt) {
    if (!ai || !cpu || !human || !ball) return;

    ai->decision_timer -= dt;
    if (ai->decision_timer <= 0.0f) {
        ai->decision_timer = ai->decision_interval;

        if (cpu->has_ball) {
            uint8_t distance_class =
                allstar_one_on_one_rom_shot_distance_class(cpu->x, cpu->y);
            ai->rom_action_index = distance_class < 3
                ? (uint8_t)(4 + distance_class) : 7;
            allstar_ai_rom_offense_target_72ea(
                (uint8_t)cpu->x, (uint8_t)(rand() & 0xff),
                &ai->rom_target_x, &ai->rom_target_y);
            if (allstar_ai_rom_should_shoot_756c(
                    ai->rom_shot_profile, distance_class,
                    ai->rom_action_index, ai->rom_skill_level,
                    (uint8_t)(rand() & 0xff),
                    (uint8_t)(rand() & 0xff))) {
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
            uint8_t direction = allstar_ai_rom_direction_74bb(
                cpu->x, cpu->y, (uint8_t)human->x,
                (uint8_t)(human->y - 8.0f));
            if (direction & 0x10) cpu->x += move_speed * dt;
            if (direction & 0x20) cpu->x -= move_speed * dt;
            if (direction & 0x80) cpu->y += move_speed * dt;
            if (direction & 0x40) cpu->y -= move_speed * dt;

            if (allstar_ai_rom_should_contest_71ee(
                    cpu->x, cpu->y,
                    ball->in_flight && ball->shooter_id != 2)) {
                ai->state = ALLSTAR_AI_STATE_CONTEST_SHOT;
                cpu->is_jumping = true;
            } else if (fabsf(cpu->x - human->x) < 12.0f &&
                       fabsf(cpu->y - human->y) < 8.0f) {
                cpu->is_defending = true;
            }
            break;
        }
        case ALLSTAR_AI_STATE_CONTEST_SHOT:
            cpu->is_jumping = allstar_ai_rom_should_contest_71ee(
                cpu->x, cpu->y,
                ball->in_flight && ball->shooter_id != 2);
            if (!cpu->is_jumping) ai->state = ALLSTAR_AI_STATE_REBOUND;
            break;
        case ALLSTAR_AI_STATE_DRIVE_TO_HOOP: {
            uint8_t direction = allstar_ai_rom_direction_74bb(
                cpu->x, cpu->y, ai->rom_target_x, ai->rom_target_y);
            if (direction & 0x10) cpu->x += move_speed * dt;
            if (direction & 0x20) cpu->x -= move_speed * dt;
            if (direction & 0x80) cpu->y += move_speed * dt;
            if (direction & 0x40) cpu->y -= move_speed * dt;
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
    if (cpu->x < ALLSTAR_ONE_ON_ONE_PLAYER_MIN_X)
        cpu->x = ALLSTAR_ONE_ON_ONE_PLAYER_MIN_X;
    if (cpu->x > ALLSTAR_ONE_ON_ONE_PLAYER_MAX_X)
        cpu->x = ALLSTAR_ONE_ON_ONE_PLAYER_MAX_X;
    if (cpu->y < ALLSTAR_ONE_ON_ONE_PLAYER_MIN_Y)
        cpu->y = ALLSTAR_ONE_ON_ONE_PLAYER_MIN_Y;
    if (cpu->y > ALLSTAR_ONE_ON_ONE_PLAYER_MAX_Y)
        cpu->y = ALLSTAR_ONE_ON_ONE_PLAYER_MAX_Y;
}
