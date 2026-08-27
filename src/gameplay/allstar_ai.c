#include "allstar_ai.h"
#include "allstar_one_on_one.h"
#include <math.h>
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
    ai->rom_direction_reload = 8;
    ai->rom_direction_hysteresis = 8;
}

/* ROM $1FFA maps skill levels 1/2/3 to $74BB direction-hysteresis reloads
   of 8/4/1 controller calls.  $7170 itself still runs every gameplay call. */
void allstar_ai_set_skill(AllStarAIController *ai, uint8_t skill_level) {
    static const float ROM_SKILL_DELAYS[3] = {
        8.0f / 60.0f, 4.0f / 60.0f, 1.0f / 60.0f
    };
    if (!ai) return;
    if (skill_level < 1 || skill_level > 3) skill_level = 1;
    ai->rom_skill_level = skill_level;
    ai->decision_interval = ROM_SKILL_DELAYS[skill_level - 1];
    ai->rom_direction_reload = (uint8_t)(skill_level == 1 ? 8 :
        (skill_level == 2 ? 4 : 1));
    ai->rom_direction_hysteresis = ai->rom_direction_reload;
    if (ai->decision_timer > ai->decision_interval) {
        ai->decision_timer = ai->decision_interval;
    }
}

void allstar_ai_set_rom_profile(AllStarAIController *ai, uint8_t roster_index) {
    if (!ai) return;
    ai->rom_roster_index = roster_index;
    ai->rom_shot_profile = allstar_one_on_one_rom_shot_profile(roster_index);
}

static uint8_t allstar_ai_rom_skill_value_761b(
        uint8_t skill, const uint8_t values[3]) {
    if (skill < 1 || skill > 3) skill = 1;
    return values[skill - 1];
}

bool allstar_ai_rom_inside_07b4(float center_x, float ground_y,
                                uint8_t margin) {
    if ((uint8_t)ground_y >= (uint8_t)(0x5c + margin)) return false;
    if ((uint8_t)center_x >= 0x54) {
        return (uint8_t)((uint8_t)center_x - margin) < 0x54;
    }
    return (uint8_t)((uint8_t)center_x + margin - 1u) >= 0x54;
}

static void allstar_ai_rom_select_route_732c(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    allstar_ai_rom_route_target_732c(
        context->cpu_roster_index, context->random_route,
        context->random_position, &ai->rom_target_x, &ai->rom_target_y);
    ai->rom_initial_target_active = 0;
    ai->rom_force_route = 0;
    if (ai->rom_target_x == 0x54 && ai->rom_target_y == 0x5d) {
        ai->rom_special_frames = 1;
    } else {
        ai->rom_special_frames = 0;
        ai->rom_offense_stage = 1;
    }
}

static void allstar_ai_rom_arm_gather_755d(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    ai->rom_offense_stage = 2;
    ai->rom_stored_shot_random = context->random_current;
    ai->rom_new_input = 0x01;
}

/* $74BB owns both the asymmetric four-pixel dead zone and the 8/4/1
   direction-change hysteresis.  Arrival also advances the offense state. */
static void allstar_ai_rom_target_74bb(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context,
        uint8_t target_x, uint8_t target_y) {
    uint8_t direction = (uint8_t)(ai->rom_held_input |
        allstar_ai_rom_direction_74bb(
            context->cpu_center_x, context->cpu_ground_y,
            target_x, target_y));
    ai->rom_arrived = 0;

    if ((direction & 0xf0u) != 0) {
        if (direction != ai->rom_accepted_direction) {
            ai->rom_direction_hysteresis--;
            if (ai->rom_direction_hysteresis != 0) {
                ai->rom_held_input = ai->rom_accepted_direction;
                return;
            }
            ai->rom_direction_hysteresis = ai->rom_direction_reload;
            ai->rom_accepted_direction = direction;
        }
        ai->rom_held_input = direction;
        return;
    }
    if (direction != 0 && (direction & 0x02u) == 0) {
        if (direction != ai->rom_accepted_direction) {
            ai->rom_direction_hysteresis--;
            if (ai->rom_direction_hysteresis != 0) {
                ai->rom_held_input = ai->rom_accepted_direction;
                return;
            }
            ai->rom_direction_hysteresis = ai->rom_direction_reload;
            ai->rom_accepted_direction = direction;
        }
        ai->rom_held_input = direction;
        return;
    }

    ai->rom_held_input = direction;
    ai->rom_arrived = 1;
    if (context->game_mode == 2 && context->mode2_state == 2) {
        ai->rom_mode2_arrival = 2;
    }
    if (ai->rom_contact_hold_frames == 0) ai->rom_defense_offset_frames = 0;
    ai->rom_drive_b_frames = 0;
    ai->rom_contact_offense_count = 0;
    if (ai->rom_initial_target_active != 0) {
        ai->rom_initial_target_active = 0;
        allstar_ai_rom_select_route_732c(ai, context);
        allstar_ai_rom_target_74bb(
            ai, context, ai->rom_target_x, ai->rom_target_y);
        return;
    }
    if ((uint8_t)(ai->rom_offense_stage - 1u) == 0) {
        allstar_ai_rom_arm_gather_755d(ai, context);
    }
}

static void allstar_ai_rom_select_initial_72ea(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    allstar_ai_rom_offense_target_72ea(
        context->ball_x, context->random_target,
        &ai->rom_target_x, &ai->rom_target_y);
    ai->rom_initial_target_active = 1;
    allstar_ai_rom_target_74bb(
        ai, context, ai->rom_target_x, ai->rom_target_y);
}

/* Returns true when $75CD uses its stack-pop tail and the caller must stop. */
static bool allstar_ai_rom_contact_75cd(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    static const uint8_t thresholds[3] = {0xbe,0xaa,0x96};
    uint8_t threshold = allstar_ai_rom_skill_value_761b(
        context->skill_level, thresholds);
    if (!context->movement_blocked || ai->rom_contact_hold_frames != 0 ||
        context->random_current < threshold) return false;
    if (context->possession_owner == context->cpu_player) {
        ai->rom_contact_offense_count++;
        if (ai->rom_contact_offense_count == 0x0e) {
            allstar_ai_rom_arm_gather_755d(ai, context);
        } else {
            ai->rom_contact_hold_frames = 0;
            allstar_ai_rom_select_initial_72ea(ai, context);
        }
    } else {
        ai->rom_contact_saved_y = (uint8_t)context->cpu_ground_y;
        ai->rom_contact_saved_x = (uint8_t)context->cpu_center_x;
        ai->rom_contact_hold_frames = 0x0a;
        allstar_ai_rom_target_74bb(
            ai, context, ai->rom_contact_saved_x,
            ai->rom_contact_saved_y);
    }
    return true;
}

static void allstar_ai_rom_chase_ball_7476(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    static const uint8_t jump_thresholds[3] = {0x19,0x50,0x96};
    allstar_ai_rom_target_74bb(
        ai, context, context->ball_x, context->ball_y);
    if (ai->rom_arrived != 0 && context->ball_height >= 0x28 &&
        context->random_position < allstar_ai_rom_skill_value_761b(
            context->skill_level, jump_thresholds)) {
        ai->rom_new_input = (uint8_t)(ai->rom_accepted_direction | 0x01u);
    }
}

static void allstar_ai_rom_defense_7190(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    static const uint8_t steal_thresholds[3] = {0x04,0x19,0x46};
    static const uint8_t contest_thresholds[3] = {0x19,0x50,0x96};
    static const uint8_t offset_thresholds[3] = {0x1b,0x10,0x07};
    uint8_t target_x;
    uint8_t target_y;

    ai->rom_offense_active = 0;
    ai->rom_offense_stage = 0;
    ai->rom_drive_b_frames = 0;
    ai->rom_initial_target_active = 0;
    ai->rom_contact_offense_count = 0;
    ai->rom_force_route = 0;
    if (context->possession_owner == 0) {
        allstar_ai_rom_chase_ball_7476(ai, context);
        return;
    }
    if (context->ball_contact &&
        context->random_current < allstar_ai_rom_skill_value_761b(
            context->skill_level, steal_thresholds)) {
        ai->rom_new_input = 0x02;
        allstar_ai_rom_chase_ball_7476(ai, context);
        return;
    }
    if (allstar_ai_rom_contact_75cd(ai, context)) return;
    if (ai->rom_contact_hold_frames != 0) {
        ai->rom_contact_hold_frames--;
        allstar_ai_rom_target_74bb(
            ai, context, ai->rom_contact_saved_x,
            ai->rom_contact_saved_y);
        return;
    }
    if (ai->rom_defense_offset_frames != 0) {
        ai->rom_defense_offset_frames--;
        goto direction_offset;
    }
    if (context->initial_flight &&
        context->shot_owner != context->cpu_player &&
        allstar_ai_rom_inside_07b4(
            context->cpu_center_x, context->cpu_ground_y, 0x0e)) {
        ai->rom_new_input = (uint8_t)(ai->rom_accepted_direction | 0x01u);
        return;
    }
    if (!allstar_one_on_one_rom_action_eligible_0a78(
            context->opponent_action) && ai->rom_arrived != 0 &&
        context->opponent_action == 0x03 &&
        context->random_target >= allstar_ai_rom_skill_value_761b(
            context->skill_level, contest_thresholds)) {
        ai->rom_new_input = (uint8_t)(ai->rom_accepted_direction | 0x01u);
        return;
    }
    if (context->random_route < allstar_ai_rom_skill_value_761b(
            context->skill_level, offset_thresholds)) {
direction_offset:
        target_x = (uint8_t)context->opponent_center_x;
        target_y = (uint8_t)(context->opponent_ground_y + 4.0f);
        if ((context->opponent_stored_direction & 0x01u) != 0) {
            target_x = (uint8_t)(target_x + 0x10u);
        } else if ((context->opponent_stored_direction & 0x02u) != 0) {
            target_x = target_x < 0x10 ? 0 : (uint8_t)(target_x - 0x10u);
        } else if ((context->opponent_stored_direction & 0x04u) != 0) {
            target_y = (uint8_t)(target_y - 0x08u);
        } else {
            target_y = (uint8_t)(target_y + 0x08u);
        }
        ai->rom_target_x = target_x;
        ai->rom_target_y = target_y;
        ai->rom_defense_offset_frames = 0x3c;
        allstar_ai_rom_target_74bb(ai, context, target_x, target_y);
        return;
    }

    target_x = (uint8_t)(context->opponent_center_x -
        ALLSTAR_ROM_PLAYER_X_TO_CENTER);
    target_y = (uint8_t)context->opponent_ground_y;
    if (target_x <= 0x3c) target_x = (uint8_t)(target_x + 0x10u);
    else if (target_x > 0x6c) target_x = (uint8_t)(target_x - 0x08u);
    else target_x = (uint8_t)(target_x + 0x08u);
    target_y = (uint8_t)(target_y - 0x08u);
    allstar_ai_rom_target_74bb(ai, context, target_x, target_y);
}

static void allstar_ai_rom_offense_72bf(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    static const uint8_t initial_thresholds[3] = {0x1e,0x14,0x04};
    if (ai->rom_force_route != 0) {
        allstar_ai_rom_select_route_732c(ai, context);
        allstar_ai_rom_target_74bb(
            ai, context, ai->rom_target_x, ai->rom_target_y);
        return;
    }
    if (ai->rom_offense_active == 0) {
        ai->rom_contact_hold_frames = 0;
        ai->rom_offense_active = 1;
        if (ai->rom_initial_target_active != 0) {
            allstar_ai_rom_target_74bb(
                ai, context, ai->rom_target_x, ai->rom_target_y);
        } else if (!context->special_route &&
            context->random_current < allstar_ai_rom_skill_value_761b(
                context->skill_level, initial_thresholds)) {
            allstar_ai_rom_select_route_732c(ai, context);
            allstar_ai_rom_target_74bb(
                ai, context, ai->rom_target_x, ai->rom_target_y);
        } else {
            allstar_ai_rom_select_initial_72ea(ai, context);
        }
        return;
    }
    if (ai->rom_offense_stage == 2) {
        uint8_t distance_class = allstar_one_on_one_rom_shot_distance_class(
            context->cpu_center_x, context->cpu_ground_y);
        if (allstar_ai_rom_should_shoot_756c(
                context->cpu_shot_profile, distance_class,
                context->cpu_record, context->skill_level,
                ai->rom_stored_shot_random, context->random_current)) {
            ai->rom_new_input = 0x01;
        }
        return;
    }
    if (ai->rom_special_frames != 0) {
        if (ai->rom_special_frames == 1) {
            bool special = false;
            if ((uint8_t)context->cpu_ground_y == 0x60 &&
                context->random_current < 0x30 &&
                allstar_ai_rom_inside_07b4(
                    context->cpu_center_x, context->cpu_ground_y, 0x1e) &&
                !allstar_ai_rom_inside_07b4(
                    context->cpu_center_x, context->cpu_ground_y, 0x1a)) {
                special = true;
            }
            if (special || allstar_ai_rom_inside_07b4(
                    context->cpu_center_x, context->cpu_ground_y, 0x12)) {
                ai->rom_special_frames = 0x2a;
                ai->rom_new_input = (uint8_t)(
                    ai->rom_accepted_direction | 0x01u);
                return;
            }
        } else {
            ai->rom_special_frames--;
            if (ai->rom_special_frames == 0x25 &&
                context->cpu_roster_index != 0x0d &&
                context->random_current < 0x0a) {
                ai->rom_special_frames = 0;
            } else if (ai->rom_special_frames != 0) {
                ai->rom_special_frames--;
                if (ai->rom_special_frames != 0) return;
            }
            ai->rom_held_input = 0x02;
            return;
        }
    }
    if (allstar_ai_rom_contact_75cd(ai, context)) return;
    if (ai->rom_drive_b_frames != 0) {
        ai->rom_drive_b_frames--;
        ai->rom_held_input = 0x02;
        ai->rom_new_input = 0x02;
        allstar_ai_rom_target_74bb(
            ai, context, ai->rom_target_x, ai->rom_target_y);
        return;
    }
    if (context->random_route < 0x07) {
        ai->rom_drive_b_frames = 0x32;
        ai->rom_held_input = 0x02;
        ai->rom_new_input = 0x02;
        return;
    }
    allstar_ai_rom_target_74bb(
        ai, context, ai->rom_target_x, ai->rom_target_y);
}

void allstar_ai_rom_controller_7170(
        AllStarAIController *ai,
        const AllStarRomCpuControllerContext *context) {
    if (!ai || !context) return;
    ai->rom_new_input = 0;
    ai->rom_held_input = 0;
    ai->rom_steal_pressed = false;
    ai->rom_force_shot = false;
    ai->rom_shot_release = false;
    if (context->counted_wait_locked || !context->cpu_enabled ||
        context->score_event_locked) return;
    if (context->game_mode == 0 || context->game_mode == 4) {
        if (context->possession_owner == context->cpu_player) {
            allstar_ai_rom_offense_72bf(ai, context);
        } else {
            allstar_ai_rom_defense_7190(ai, context);
        }
    } else if (context->game_mode == 2) {
        if (!allstar_one_on_one_rom_action_eligible_0a78(
                context->cpu_action)) {
            static const uint8_t profile_thresholds[3] = {
                0xb0,0x60,0x40
            };
            uint8_t distance_class = allstar_one_on_one_rom_shot_distance_class(
                context->cpu_center_x, context->cpu_ground_y);
            uint8_t profile = context->cpu_shot_profile > 2
                ? 2 : context->cpu_shot_profile;
            uint8_t expected_record = distance_class < 3
                ? (uint8_t)(4 + distance_class) : 7;
            bool release = ai->rom_stored_shot_random <
                profile_thresholds[profile]
                ? context->cpu_record == expected_record
                : context->cpu_record >= 5 && context->cpu_record < 8;
            if (release) {
                ai->rom_new_input = 0x01;
            }
        } else if (ai->rom_mode2_arrival != 0) {
            allstar_ai_rom_arm_gather_755d(ai, context);
        } else {
            allstar_ai_rom_target_74bb(
                ai, context, context->mode2_target_x,
                context->mode2_target_y);
        }
    }
    ai->rom_steal_pressed = (ai->rom_new_input & 0x02u) != 0;
    ai->rom_force_shot = context->possession_owner == context->cpu_player &&
        (ai->rom_new_input & 0x01u) != 0 &&
        allstar_one_on_one_rom_action_eligible_0a78(context->cpu_action);
    ai->rom_shot_release = context->possession_owner == context->cpu_player &&
        (ai->rom_new_input & 0x01u) != 0 &&
        !allstar_one_on_one_rom_action_eligible_0a78(context->cpu_action);
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

/* $732C uses player +$0F to find its three-entry route family at $763B.
   $733F selects the family with $FFFD at $A2/$F0 -- about 63/30/6 per cent.
   $734C special-cases family $82 to the fixed centre target $54/$5D before the
   dispatch, because the inline pointer table at $738D only has two entries.
   $7362 subtracts $80 and $7367's `rst $10` picks between them: family $80 is
   the fourteen pairs at $7391, family $81 the fourteen at $73AD.  $736C then
   walks $FFFE in nineteen-unit steps ($13) to choose one of the fourteen.
   Both tables below are byte-for-byte the ROM's, first byte $C101 then $C102;
   they are stored here as (x, y) to match this function's outputs. */
void allstar_ai_rom_route_target_732c(uint8_t roster_index,
                                     uint8_t route_random,
                                     uint8_t position_random,
                                     uint8_t *target_x,
                                     uint8_t *target_y) {
    static const uint8_t keys[27] = {
        0x01,0x04,0x10,0x11,0x07,0x02,0x14,0x12,0x19,
        0x0a,0x1a,0x00,0x09,0x16,0x0b,0x18,0x17,0x05,
        0x0f,0x06,0x0e,0x08,0x0c,0x13,0x15,0x03,0x0d
    };
    static const uint8_t families[27][3] = {
        {1,0,2},{1,0,2},{1,0,2},{1,0,2},{2,1,0},{0,1,2},
        {1,0,2},{2,1,0},{2,1,0},{1,0,2},{2,1,0},{1,0,2},
        {0,1,2},{1,0,2},{2,1,0},{1,0,2},{1,0,2},{1,0,2},
        {2,1,0},{1,0,2},{1,0,2},{2,1,0},{2,1,0},{1,0,2},
        {1,0,2},{1,0,2},{2,1,0}
    };
    static const uint8_t routes[2][14][2] = {
        {{0x90,0x84},{0x28,0x98},{0x48,0x98},{0x0c,0x60},
         {0x9c,0x74},{0x0c,0x94},{0x70,0x98},{0x9c,0x8c},
         {0x14,0x88},{0x9c,0x60},{0x5c,0x98},{0x0c,0x70},
         {0x90,0x98},{0x54,0x98}},
        {{0x34,0x70},{0x60,0x8c},{0x28,0x84},{0x5c,0x68},
         {0x80,0x74},{0x48,0x64},{0x68,0x74},{0x80,0x64},
         {0x24,0x64},{0x50,0x88},{0x70,0x64},{0x3c,0x84},
         {0x78,0x84},{0x50,0x70}}
    };
    size_t player = 0;
    uint8_t bin = route_random < 0xa2 ? 0 :
        (route_random < 0xf0 ? 1 : 2);
    uint8_t family;
    uint8_t position = 0;
    while (player + 1 < 27 && keys[player] != roster_index) player++;
    family = families[player][bin];
    if (family == 2) {
        if (target_x) *target_x = 0x54;
        if (target_y) *target_y = 0x5d;
        return;
    }
    while (position < 13 && position_random >=
           (uint8_t)(0x13u * (position + 1u))) position++;
    if (target_x) *target_x = routes[family][position][0];
    if (target_y) *target_y = routes[family][position][1];
}

/* $756C uses profile thresholds $B0/$60/$40, requires current animation
   record +$03 to match distance classes 0/1/2/far as 4/5/6/7, then falls
   back to the skill thresholds $1A/$0C/$06 for records 5..7. */
bool allstar_ai_rom_should_shoot_756c(uint8_t profile,
                                     uint8_t distance_class,
                                     uint8_t animation_record,
                                     uint8_t skill_level,
                                     uint8_t profile_random,
                                     uint8_t skill_random) {
    static const uint8_t profile_threshold[3] = {0xb0,0x60,0x40};
    static const uint8_t skill_threshold[3] = {0x1a,0x0c,0x06};
    uint8_t expected_record;
    if (profile > 2) profile = 2;
    if (skill_level < 1 || skill_level > 3) skill_level = 1;
    expected_record = distance_class < 3
        ? (uint8_t)(4 + distance_class) : 7;
    if (profile_random < profile_threshold[profile]) {
        return animation_record == expected_record;
    }
    return skill_random >= skill_threshold[skill_level - 1] &&
           animation_record >= 5 && animation_record < 8;
}

/* $71EE gates the CPU A/contest input while the opponent's initial-flight
   flag remains active and inside the same $07B4 hoop rectangle margin $0E. */
bool allstar_ai_rom_should_contest_71ee(float cpu_x, float cpu_y,
                                       bool opponent_shot_in_flight) {
    if (!opponent_shot_in_flight) return false;
    return cpu_y < 0x5c + 0x0e && cpu_x >= 0x55 - 0x0e &&
           cpu_x < 0x54 + 0x0e;
}

/* $71B3 indexes $762C by the one-based skill byte. A CPU touching the held
   ball presses B only when the random byte is below 04/19/46. */
bool allstar_ai_rom_should_steal_71b3(uint8_t skill_level,
                                     uint8_t random_byte,
                                     bool ball_contact) {
    static const uint8_t thresholds[3] = {0x04, 0x19, 0x46};
    if (!ball_contact) return false;
    if (skill_level < 1 || skill_level > 3) skill_level = 1;
    return random_byte < thresholds[skill_level - 1];
}

/* Bank 1 $75CD consumes the latest $C16B movement-block signal.  The CPU
   ballhandler reroutes until the fourteenth qualified contact, which forces
   A; the defender saves its exact position and holds that target for ten
   countdown updates. */
bool allstar_ai_rom_contact_response_75cd(
    AllStarAIController *ai,
    bool movement_blocked,
    bool owns_ball,
    uint8_t random_byte,
    uint8_t target_random,
    uint8_t ball_x,
    float cpu_center_x,
    float cpu_ground_y) {
    static const uint8_t thresholds[3] = {0xbe, 0xaa, 0x96};
    uint8_t skill;
    if (!ai || !movement_blocked || ai->rom_contact_hold_frames != 0)
        return false;
    skill = ai->rom_skill_level;
    if (skill < 1 || skill > 3) skill = 1;
    if (random_byte < thresholds[skill - 1]) return false;

    if (owns_ball) {
        ai->rom_contact_offense_count++;
        if (ai->rom_contact_offense_count == 14) {
            ai->rom_force_shot = true;
        } else {
            allstar_ai_rom_offense_target_72ea(
                ball_x, target_random,
                &ai->rom_target_x, &ai->rom_target_y);
        }
    } else {
        ai->rom_contact_saved_y = (uint8_t)cpu_ground_y;
        ai->rom_contact_saved_x = (uint8_t)cpu_center_x;
        ai->rom_target_x = ai->rom_contact_saved_x;
        ai->rom_target_y = ai->rom_contact_saved_y;
        ai->rom_contact_hold_frames = 10;
    }
    return true;
}

void allstar_ai_update(AllStarAIController *ai, AllStarPlayerState *cpu,
                       const AllStarPlayerState *human,
                       const AllStarBall *ball, uint8_t rom_random_byte,
                       uint8_t target_random, uint8_t route_random,
                       uint8_t position_random,
                       float dt) {
    if (!ai || !cpu || !human || !ball) return;

    ai->rom_steal_pressed = false;
    ai->rom_shot_release = false;

    if (cpu->has_ball && !ai->rom_had_possession) {
        ai->rom_offense_stage = 0;
        ai->rom_contact_offense_count = 0;
        cpu->is_shooting = false;
    } else if (!cpu->has_ball && ai->rom_had_possession) {
        ai->rom_offense_stage = 0;
        cpu->is_shooting = false;
    }
    ai->rom_had_possession = cpu->has_ball;

    /* $72BF starts an offense only once per possession.  It first selects
       the side-specific $72EA target; arrival later advances through $732C
       instead of rerolling a target and shooting on every controller tick. */
    if (cpu->has_ball && ai->rom_offense_stage == 0) {
        allstar_ai_rom_offense_target_72ea(
            (uint8_t)ball->x, target_random,
            &ai->rom_target_x, &ai->rom_target_y);
        ai->rom_offense_stage = 1;
        ai->state = ALLSTAR_AI_STATE_DRIVE_TO_HOOP;
    }

    ai->decision_timer -= dt;
    if (ai->rom_force_shot && cpu->has_ball) {
        ai->rom_force_shot = false;
        ai->rom_stored_shot_random = rom_random_byte;
        ai->rom_offense_stage = 3;
        ai->state = ALLSTAR_AI_STATE_PULL_UP_JUMPER;
    } else if (ai->rom_contact_hold_frames != 0) {
        ai->state = ALLSTAR_AI_STATE_DEFEND_PERIMETER;
    } else if (ai->decision_timer <= 0.0f) {
        ai->decision_timer = ai->decision_interval;

        if (cpu->has_ball) {
            /* Offense is advanced below by the $72EA/$732C/$756C stages. */
        } else if (ball->in_flight || (!human->has_ball && !cpu->has_ball)) {
            ai->state = ALLSTAR_AI_STATE_REBOUND;
        } else {
            /* Defensive logic */
            ai->state = ALLSTAR_AI_STATE_DEFEND_PERIMETER;
            ai->rom_steal_pressed = allstar_ai_rom_should_steal_71b3(
                ai->rom_skill_level, rom_random_byte,
                allstar_one_on_one_player_can_pick_up_ball(
                    cpu->x,
                    cpu->y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y,
                    human->x,
                    human->y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y));
        }
    }

    /* Reset transient flags */
    cpu->is_defending = false;
    cpu->is_jumping = false;

    /* Execute AI movement according to state */
    float move_speed = 65.0f * ai->reaction_speed;
    switch (ai->state) {
        case ALLSTAR_AI_STATE_DEFEND_PERIMETER: {
            uint8_t direction;
            if (ai->rom_contact_hold_frames != 0) {
                direction = allstar_ai_rom_direction_74bb(
                    cpu->x, cpu->y,
                    ai->rom_contact_saved_x, ai->rom_contact_saved_y);
                ai->rom_contact_hold_frames--;
            } else {
                direction = allstar_ai_rom_direction_74bb(
                    cpu->x, cpu->y, (uint8_t)human->x,
                    (uint8_t)(human->y - 8.0f));
            }
            if (direction & 0x10) cpu->x += move_speed * dt;
            if (direction & 0x20) cpu->x -= move_speed * dt;
            if (direction & 0x80) cpu->y += move_speed * dt;
            if (direction & 0x40) cpu->y -= move_speed * dt;

            if (allstar_ai_rom_should_contest_71ee(
                    cpu->x, cpu->y,
                    ball->in_flight && !ball->recoverable &&
                    ball->shooter_id != 2)) {
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
                ball->in_flight && !ball->recoverable &&
                ball->shooter_id != 2);
            if (!cpu->is_jumping) ai->state = ALLSTAR_AI_STATE_REBOUND;
            break;
        case ALLSTAR_AI_STATE_DRIVE_TO_HOOP: {
            uint8_t direction = allstar_ai_rom_direction_74bb(
                cpu->x, cpu->y, ai->rom_target_x, ai->rom_target_y);
            if (direction == 0 && cpu->has_ball) {
                if (ai->rom_offense_stage == 1) {
                    allstar_ai_rom_route_target_732c(
                        ai->rom_roster_index, route_random, position_random,
                        &ai->rom_target_x, &ai->rom_target_y);
                    ai->rom_offense_stage = 2;
                    direction = allstar_ai_rom_direction_74bb(
                        cpu->x, cpu->y,
                        ai->rom_target_x, ai->rom_target_y);
                } else if (ai->rom_offense_stage == 2) {
                    ai->rom_stored_shot_random = rom_random_byte;
                    ai->rom_offense_stage = 3;
                    ai->state = ALLSTAR_AI_STATE_PULL_UP_JUMPER;
                    cpu->is_shooting = true;
                    break;
                }
            }
            if (direction & 0x10) cpu->x += move_speed * dt;
            if (direction & 0x20) cpu->x -= move_speed * dt;
            if (direction & 0x80) cpu->y += move_speed * dt;
            if (direction & 0x40) cpu->y -= move_speed * dt;
            break;
        }
        case ALLSTAR_AI_STATE_PULL_UP_JUMPER: {
            if (cpu->has_ball) {
                uint8_t distance_class =
                    allstar_one_on_one_rom_shot_distance_class(
                        cpu->x, cpu->y);
                cpu->is_shooting = true;
                /* $756C reads player +$03: this is the current $6A8C shot
                   record, not a synthetic distance-class action. */
                if (allstar_ai_rom_should_shoot_756c(
                        ai->rom_shot_profile, distance_class,
                        ai->rom_action_index, ai->rom_skill_level,
                        ai->rom_stored_shot_random, rom_random_byte)) {
                    ai->rom_shot_release = true;
                }
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
