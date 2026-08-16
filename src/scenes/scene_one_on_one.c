#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    AllStarPlayerState p1;
    AllStarPlayerState p2;
    AllStarBall ball;
    AllStarAIController ai;
    AllStarOneOnOneShotAttempt shot_attempt;
    AllStarOneOnOneRecoveryState recovery;
    AllStarOneOnOneScorePresentation score_presentation;
    AllStarRomFoulPresentation foul_presentation;
    uint32_t pending_score_events;
    float p1_shot_animation_clock;
    float p2_shot_animation_clock;
    uint8_t p1_shot_action;
    uint8_t p2_shot_action;
    uint8_t p1_steal_latch_frames;
    uint8_t p2_steal_latch_frames;
    uint16_t p1_defense_jump_elapsed_frames;
    uint16_t p2_defense_jump_elapsed_frames;
    bool p1_defense_jump_active;
    bool p2_defense_jump_active;
    float defense_step_accumulator;
    AllStarRomAnimationState p1_animation;
    AllStarRomAnimationState p2_animation;
    uint8_t p1_input_direction;
    uint8_t p2_input_direction;
    uint8_t p1_previous_direction;
    uint8_t p2_previous_direction;
    bool p1_horizontal_flip;
    bool p2_horizontal_flip;
    bool take_back_required;
    uint8_t take_back_violation_pending;
    AllStarRomPlayerContactState p1_contact;
    AllStarRomPlayerContactState p2_contact;
    float animation_step_accumulator;
    float anim_timer;
    uint32_t rim_audio_events;
    uint32_t steal_transfer_events;
    uint32_t foul_events;
} SceneOneOnOneData;

static bool one_on_one_action_uses_dribble_ball_6f2a(uint8_t action) {
    return action == 0x01 || action == 0x04 || action == 0x08 ||
           action == 0x0b || action == 0x10 || action == 0x13;
}

static bool one_on_one_action_is_defense_jump_70fd(uint8_t action) {
    return action == 0x05 || action == 0x0c || action == 0x14;
}

static AllStarRomContactEvent one_on_one_tick_rom_animations(
    SceneOneOnOneData *data,
    AllStarGame *game,
    float dt,
    int *contact_offender) {
    data->animation_step_accumulator += dt;
    while (data->animation_step_accumulator >=
           ALLSTAR_PHYSICS_STEP_SECONDS) {
        AllStarRomContactEvent contact_event;
        data->animation_step_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        contact_event = allstar_one_on_one_rom_contact_tick_2c50(
            data->p1.has_ball || data->p2.has_ball,
            game->one_on_one.p1_possession ? 1 : 2,
            &data->p1_contact, &data->p2_contact,
            data->p1_animation.action, data->p2_animation.action,
            data->p1.x, data->p1.y, data->p2.x, data->p2.y,
            contact_offender);
        if (contact_event != ALLSTAR_ROM_CONTACT_NONE) return contact_event;
        if (!data->p1.is_shooting) {
            if (allstar_one_on_one_rom_select_movement_action_782e(
                &data->p1_animation,
                data->p1_input_direction, 0,
                data->p1_previous_direction, !data->p1.has_ball,
                data->p1_steal_latch_frames > 0,
                &data->p1_horizontal_flip)) {
                /* $78DD->$78E0 selects command $0D on an action change. */
                allstar_audio_play_sfx(
                    &game->audio, ALLSTAR_SFX_SHOE_SQUEAK);
            }
        }
        if (allstar_one_on_one_rom_animation_tick_6a8c(
                game->asset_pack, &data->p1_animation)) {
            bool moved = false;
            data->p1.anim_frame = data->p1_animation.display_frame;
            if (data->p1_animation.new_frame) {
                moved = allstar_one_on_one_rom_player_move_6b72(
                    data->p1_input_direction, 0,
                    &data->p1.x, &data->p1.y,
                    data->p2.x, data->p2.y,
                    &data->p1_contact.blocked_contact);
            }
            (void)moved;
            /* $6FD2->$6FE5 checks player +$03 after $6A8C and selects
               command $0C on every update for the six-frame record 6. */
            if (data->p1.has_ball && data->p1_animation.record_index == 6 &&
                one_on_one_action_uses_dribble_ball_6f2a(
                    data->p1_animation.action))
                allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
            /* $6A8C:$6B34-$6B59 replaces the newly loaded record's normal
               display frame with $12/$13/$14 while player +$13 is active.
               Phase two is the A-then-B close-range dunk/drop pose. */
            if (data->p1.is_shooting && data->shot_attempt.shooter == 1 &&
                data->shot_attempt.rom_phase != 0) {
                allstar_one_on_one_rom_shot_animation_frame(
                    data->p1_shot_action, data->shot_attempt.rom_phase, 0,
                    &data->p1.anim_frame);
            }
        }
        if (!data->p2.is_shooting) {
            if (allstar_one_on_one_rom_select_movement_action_782e(
                &data->p2_animation,
                data->p2_input_direction, 0,
                data->p2_previous_direction, !data->p2.has_ball,
                data->p2_steal_latch_frames > 0,
                &data->p2_horizontal_flip)) {
                allstar_audio_play_sfx(
                    &game->audio, ALLSTAR_SFX_SHOE_SQUEAK);
            }
        }
        if (allstar_one_on_one_rom_animation_tick_6a8c(
                game->asset_pack, &data->p2_animation)) {
            bool moved = false;
            data->p2.anim_frame = data->p2_animation.display_frame;
            if (data->p2_animation.new_frame) {
                moved = allstar_one_on_one_rom_player_move_6b72(
                    data->p2_input_direction, 0,
                    &data->p2.x, &data->p2.y,
                    data->p1.x, data->p1.y,
                    &data->p2_contact.blocked_contact);
            }
            (void)moved;
            if (data->p2.has_ball && data->p2_animation.record_index == 6 &&
                one_on_one_action_uses_dribble_ball_6f2a(
                    data->p2_animation.action))
                allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
        }
    }
    return ALLSTAR_ROM_CONTACT_NONE;
}

static void one_on_one_reset_defense(SceneOneOnOneData *data) {
    data->p1_steal_latch_frames = 0;
    data->p2_steal_latch_frames = 0;
    data->p1_defense_jump_elapsed_frames = 0;
    data->p2_defense_jump_elapsed_frames = 0;
    data->p1_defense_jump_active = false;
    data->p2_defense_jump_active = false;
    data->defense_step_accumulator = 0.0f;
}

static void one_on_one_tick_defense(SceneOneOnOneData *data, float dt) {
    data->defense_step_accumulator += dt;
    while (data->defense_step_accumulator >= ALLSTAR_PHYSICS_STEP_SECONDS) {
        data->defense_step_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        if (data->p1_steal_latch_frames > 0) data->p1_steal_latch_frames--;
        if (data->p2_steal_latch_frames > 0) data->p2_steal_latch_frames--;
        if (data->p1_defense_jump_active &&
            ++data->p1_defense_jump_elapsed_frames >=
                ALLSTAR_ROM_DEFENSE_JUMP_FRAMES) {
            data->p1_defense_jump_active = false;
            data->p1_defense_jump_elapsed_frames = 0;
        }
        if (data->p2_defense_jump_active &&
            ++data->p2_defense_jump_elapsed_frames >=
                ALLSTAR_ROM_DEFENSE_JUMP_FRAMES) {
            data->p2_defense_jump_active = false;
            data->p2_defense_jump_elapsed_frames = 0;
        }
    }
}

static void one_on_one_reset_possession(SceneOneOnOneData *data,
                                        AllStarGame *game,
                                        bool p1_possession) {
    AllStarRomInboundPlacement placement;
    allstar_one_on_one_match_take_possession(
        &game->one_on_one, p1_possession ? 1 : 2, true);
    allstar_one_on_one_rom_inbound_placement_20f7(
        p1_possession ? 1 : 2, &placement);
    data->p1.x = placement.p1_center_x;
    data->p2.x = placement.p2_center_x;
    data->p1.y = placement.p1_ground_y;
    data->p2.y = placement.p2_ground_y;
    data->p1.has_ball = p1_possession;
    data->p2.has_ball = !p1_possession;
    data->p1.is_shooting = false;
    data->p2.is_shooting = false;
    data->p1.is_jumping = false;
    data->p2.is_jumping = false;
    data->p1_shot_animation_clock = 0.0f;
    data->p2_shot_animation_clock = 0.0f;
    data->p1_shot_action = 0;
    data->p2_shot_action = 0;
    allstar_one_on_one_rom_animation_init_6a8c(
        &data->p1_animation, p1_possession ? 0x13 : 0x0d);
    allstar_one_on_one_rom_animation_init_6a8c(
        &data->p2_animation, p1_possession ? 0x0d : 0x13);
    data->p1_input_direction = 0;
    data->p2_input_direction = 0;
    data->p1_previous_direction = p1_possession
        ? ALLSTAR_BTN_RIGHT : ALLSTAR_BTN_UP;
    data->p2_previous_direction = p1_possession
        ? ALLSTAR_BTN_UP : ALLSTAR_BTN_RIGHT;
    data->p1_horizontal_flip = p1_possession;
    data->p2_horizontal_flip = !p1_possession;
    memset(&data->p1_contact, 0, sizeof(data->p1_contact));
    memset(&data->p2_contact, 0, sizeof(data->p2_contact));
    data->animation_step_accumulator = 0.0f;
    data->take_back_required = false;
    data->take_back_violation_pending = 0;
    one_on_one_reset_defense(data);
    allstar_one_on_one_shot_reset(&data->shot_attempt);
    allstar_physics_init_ball(&data->ball);
}

static uint8_t one_on_one_direction_from_delta(float dx, float dy) {
    uint8_t direction = 0;
    if (dx > 0.001f) direction |= ALLSTAR_BTN_RIGHT;
    else if (dx < -0.001f) direction |= ALLSTAR_BTN_LEFT;
    if (dy < -0.001f) direction |= ALLSTAR_BTN_UP;
    else if (dy > 0.001f) direction |= ALLSTAR_BTN_DOWN;
    return direction;
}

static void one_on_one_take_live_possession(SceneOneOnOneData *data,
                                             AllStarGame *game,
                                             int player,
                                             bool preserve_animation) {
    bool reset_shot_clock =
        (player == 1) != game->one_on_one.p1_possession;
    allstar_one_on_one_match_take_possession(
        &game->one_on_one, player, reset_shot_clock);
    data->p1.has_ball = player == 1;
    data->p2.has_ball = player == 2;
    data->p1.is_shooting = false;
    data->p2.is_shooting = false;
    data->p1_shot_animation_clock = 0.0f;
    data->p2_shot_animation_clock = 0.0f;
    data->p1_shot_action = 0;
    data->p2_shot_action = 0;
    if (!preserve_animation) {
        allstar_one_on_one_rom_animation_set_action_6a8c(
            &data->p1_animation, player == 1 ? 0x13 : 0x0d);
        allstar_one_on_one_rom_animation_set_action_6a8c(
            &data->p2_animation, player == 2 ? 0x13 : 0x0d);
    }
    /* $2B88 changes only possession/role globals. It does not touch either
       player's current +$07 direction, stored +$10 direction, facing bit,
       or animation record. In particular, a $2B6C airborne rebound keeps
       running $6BF9->$6B72 with the direction latched when the jump began. */
    if (!preserve_animation) {
        data->p1_input_direction = 0;
        data->p2_input_direction = 0;
        data->p1_previous_direction = player == 1
            ? ALLSTAR_BTN_RIGHT : ALLSTAR_BTN_UP;
        data->p2_previous_direction = player == 2
            ? ALLSTAR_BTN_RIGHT : ALLSTAR_BTN_UP;
        data->p1_horizontal_flip = player == 1;
        data->p2_horizontal_flip = player == 2;
    }
    data->recovery.cooldown_frames = ALLSTAR_ROM_RECOVERY_COOLDOWN_FRAMES;
    allstar_one_on_one_shot_reset(&data->shot_attempt);
    /* $2B88 changes owner without clearing $C0A3/$C0A7. $6F2A replaces
       those coordinates once the preserved action returns to a held-ball
       family, so retain the recovery point in the meantime. */
    data->ball.in_flight = false;
    data->ball.recoverable = false;
    data->ball.made_basket = false;
    data->ball.vx = data->ball.vy = data->ball.vz = 0.0f;
    data->ball.rom_step_state.vx = 0;
    data->ball.rom_step_state.vy = 0;
    data->ball.rom_step_state.vz = 0;
    data->take_back_required = reset_shot_clock;
    data->take_back_violation_pending = 0;
}

static void one_on_one_update_take_back_78e9(SceneOneOnOneData *data) {
    const AllStarPlayerState *owner;
    const AllStarRomAnimationState *animation;
    AllStarRomHeldBallPresentation held_ball;
    bool direction_bit4;
    float ball_x;
    float ball_y;
    if (!data || !data->take_back_required) return;
    owner = data->p1.has_ball ? &data->p1 :
        (data->p2.has_ball ? &data->p2 : NULL);
    if (!owner) return;
    animation = data->p1.has_ball ? &data->p1_animation : &data->p2_animation;
    direction_bit4 = data->p1.has_ball
        ? data->p1_horizontal_flip : data->p2_horizontal_flip;
    allstar_renderer_rom_dribble_ball_6f2a(
        (int32_t)owner->x, (int32_t)owner->y,
        animation->action, animation->record_index,
        direction_bit4, &held_ball);
    ball_x = held_ball.visible ? (float)held_ball.ball_x : data->ball.x;
    ball_y = held_ball.visible ? (float)held_ball.ball_y : data->ball.y;
    if (allstar_one_on_one_rom_take_back_cleared_78e9(ball_x, ball_y)) {
        data->take_back_required = false;
    }
}

static bool one_on_one_try_steal(SceneOneOnOneData *data,
                                 AllStarGame *game,
                                 int defender) {
    AllStarPlayerState *defending_player =
        defender == 1 ? &data->p1 : &data->p2;
    AllStarPlayerState *ballhandler =
        defender == 1 ? &data->p2 : &data->p1;
    uint8_t *latch = defender == 1
        ? &data->p1_steal_latch_frames : &data->p2_steal_latch_frames;
    uint8_t ballhandler_action;
    uint8_t defender_direction;
    uint8_t ballhandler_direction;
    uint8_t steal_action;
    AllStarRomAnimationState *handler_animation;
    AllStarRomHeldBallPresentation held_ball;
    bool handler_flip;
    float live_ball_x;
    float live_ball_y;

    if (*latch != 0 || defending_player->has_ball ||
        !ballhandler->has_ball) {
        return false;
    }
    *latch = ALLSTAR_ROM_STEAL_LATCH_FRAMES;
    defending_player->is_defending = true;
    steal_action = allstar_one_on_one_rom_steal_action_2b14(
        defender == 1 ? data->p1_animation.action
                      : data->p2_animation.action);
    {
        AllStarRomAnimationState *animation = defender == 1
            ? &data->p1_animation : &data->p2_animation;
        allstar_one_on_one_rom_animation_set_action_6a8c(
            animation, steal_action);
    }
    handler_animation = defender == 1
        ? &data->p2_animation : &data->p1_animation;
    handler_flip = defender == 1
        ? data->p2_horizontal_flip : data->p1_horizontal_flip;
    ballhandler_action = ballhandler->is_shooting
        ? (defender == 1 ? data->p2_shot_action : data->p1_shot_action)
        : handler_animation->action;
    if (ballhandler->is_shooting && ballhandler_action == 0) {
        ballhandler_action = ALLSTAR_ROM_SHOT_ACTION_A;
    }
    defender_direction = defender == 1
        ? data->p1_previous_direction : data->p2_previous_direction;
    ballhandler_direction = defender == 1
        ? data->p2_previous_direction : data->p1_previous_direction;
    allstar_renderer_rom_dribble_ball_6f2a(
        (int32_t)ballhandler->x, (int32_t)ballhandler->y,
        handler_animation->action, handler_animation->record_index,
        handler_flip, &held_ball);
    live_ball_x = held_ball.visible
        ? (float)held_ball.ball_x : data->ball.x;
    live_ball_y = held_ball.visible
        ? (float)held_ball.ball_y : data->ball.y;
    if (!allstar_one_on_one_rom_steal_contact_2b14(
            true, ballhandler_action,
            defending_player->x, defending_player->y,
            live_ball_x, live_ball_y,
            defender_direction, ballhandler_direction)) {
        return false;
    }
    /* $2B88 rejects the transfer while the shared $C12D recovery cooldown
       is nonzero and otherwise leaves both animation records untouched. */
    if (data->recovery.cooldown_frames != 0) return false;
    one_on_one_take_live_possession(data, game, defender, true);
    allstar_one_on_one_rom_animation_set_action_6a8c(
        defender == 1 ? &data->p1_animation : &data->p2_animation,
        steal_action);
    data->steal_transfer_events++;
    return true;
}

static bool one_on_one_try_jump_recovery(SceneOneOnOneData *data,
                                         AllStarGame *game,
                                         int defender) {
    AllStarPlayerState *player = defender == 1 ? &data->p1 : &data->p2;
    bool active = defender == 1
        ? data->p1_defense_jump_active : data->p2_defense_jump_active;
    uint16_t elapsed = defender == 1
        ? data->p1_defense_jump_elapsed_frames
        : data->p2_defense_jump_elapsed_frames;
    if (!active || !allstar_one_on_one_rom_jump_recovery_2b6c(
            data->p1.has_ball || data->p2.has_ball,
            !data->ball.recoverable,
            player->x, player->y,
            allstar_one_on_one_rom_jump_height_6c4d(elapsed),
            data->ball.x, data->ball.y, data->ball.z)) {
        return false;
    }
    one_on_one_take_live_possession(data, game, defender, true);
    return true;
}

/* ROM bank 1 $7C58/$7EA9/$7F37: shot setup, vector scaling, and release origin. */
static void one_on_one_launch_shot(SceneOneOnOneData *data,
                                   AllStarGame *game,
                                   int shooter) {
    AllStarPlayerState *player = shooter == 1 ? &data->p1 : &data->p2;
    uint32_t roster_index = shooter == 1
        ? game->selected_player_1 : game->selected_player_2;
    int point_value;
    uint8_t launch_index;
    float release_x = player->x;
    float release_y = player->y;
    float release_z = ALLSTAR_BALL_RELEASE_HEIGHT;
    uint8_t shot_variant = allstar_one_on_one_rom_shot_variant(
        player->x, player->y);
    uint8_t shot_action = shot_variant == 1
        ? ALLSTAR_ROM_SHOT_ACTION_A : ALLSTAR_ROM_SHOT_ACTION_B;
    uint8_t shot_phase = shooter == 1
        ? data->shot_attempt.rom_phase : 0;
    uint8_t distance_class = allstar_one_on_one_rom_shot_distance_class(
        player->x, player->y);
    AllStarOneOnOneReleaseOffset release_offset;

    /* $7C58 returns without launching while $FFD1 remains set. Both human
       and CPU therefore have to carry a changed-possession rebound outside
       $78E9's central region before a shot can leave the hand. */
    if (data->take_back_required) {
        player->is_shooting = false;
        player->is_jumping = false;
        allstar_one_on_one_shot_reset(&data->shot_attempt);
        /* $7C58 stores the current owner in $C178 and returns.  Fixed-bank
           $2C50 consumes that latch on the following update and presents
           the $067C DIDN'T CLEAR BALL violation through $05A3. */
        data->take_back_violation_pending = (uint8_t)shooter;
        return;
    }

    if (shooter == 1 && shot_phase == 0) {
        launch_index = allstar_one_on_one_rom_shot_record_index(
            data->shot_attempt.rom_elapsed_frames);
    } else {
        launch_index = data->ai.rom_action_index;
    }

    player->has_ball = false;
    player->is_shooting = true;
    player->is_jumping = false;
    if (allstar_one_on_one_rom_release_offset(
            shot_action, shot_phase, shot_variant,
            player->x > (shooter == 1 ? data->p2.x : data->p1.x),
            &release_offset)) {
        release_x += (float)release_offset.x_offset -
                     ALLSTAR_ROM_PLAYER_X_TO_CENTER;
        release_y += (float)release_offset.ground_y_offset;
        release_z = (float)allstar_one_on_one_rom_release_height(
            (int)player->y - 48, (int)player->y, shot_phase,
            release_offset.height_offset);
    }
    if (shot_phase == 0) {
        release_z = (float)allstar_one_on_one_rom_shot_release_height(
            launch_index);
    }
    if (shooter == 1) {
        data->p1_shot_action = shot_action;
        if (data->p1_shot_animation_clock <= 0.0f) {
            data->p1_shot_animation_clock =
                ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
        }
    } else {
        data->p2_shot_action = shot_action;
        if (data->p2_shot_animation_clock <= 0.0f) {
            data->p2_shot_animation_clock =
                ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
        }
    }
    point_value = allstar_one_on_one_rom_point_value(release_x, release_y);
    allstar_physics_shoot_ball_rom_7c58(
        &data->ball, release_x, release_y, release_z,
        ALLSTAR_ONE_ON_ONE_HOOP_X,
        ALLSTAR_ONE_ON_ONE_HOOP_Y, distance_class,
        allstar_one_on_one_rom_shot_vertical_velocity(
            (uint8_t)roster_index, distance_class, launch_index), shot_phase,
        shooter, point_value);
    data->ball.rom_player_shot_phase_active = player->is_shooting;
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
}

static void one_on_one_update_shot_animations(SceneOneOnOneData *data,
                                               float dt) {
    if (data->p1_shot_animation_clock > 0.0f) {
        uint16_t elapsed_frames;
        data->p1_shot_animation_clock -= dt;
        if (data->p1_shot_animation_clock <= 0.0f) {
            data->p1_shot_animation_clock = 0.0f;
            data->p1.is_shooting = false;
            data->p1.is_jumping = false;
            data->p1.anim_frame = 0;
            data->p1_shot_action = 0;
        } else {
            elapsed_frames = (uint16_t)(
                (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
                 data->p1_shot_animation_clock) * 60.0f);
            allstar_one_on_one_rom_shot_animation_frame(
                data->p1_shot_action,
                data->shot_attempt.shooter == 1
                    ? data->shot_attempt.rom_phase : 0,
                elapsed_frames,
                &data->p1.anim_frame);
        }
    }
    if (data->p2_shot_animation_clock > 0.0f) {
        uint16_t elapsed_frames;
        data->p2_shot_animation_clock -= dt;
        if (data->p2_shot_animation_clock <= 0.0f) {
            data->p2_shot_animation_clock = 0.0f;
            data->p2.is_shooting = false;
            data->p2.is_jumping = false;
            data->p2.anim_frame = 0;
            data->p2_shot_action = 0;
        } else {
            elapsed_frames = (uint16_t)(
                (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
                 data->p2_shot_animation_clock) * 60.0f);
            allstar_one_on_one_rom_shot_animation_frame(
                data->p2_shot_action, 0, elapsed_frames,
                &data->p2.anim_frame);
        }
    }
}

static bool one_on_one_handle_lifecycle_events(SceneOneOnOneData *data,
                                                AllStarGame *game,
                                                uint32_t events) {
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_SHOT_CLOCK) {
        one_on_one_reset_possession(data, game, game->one_on_one.p1_possession);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_BUZZER);
    }
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) {
        data->p1.has_ball = false;
        data->p2.has_ball = false;
        allstar_one_on_one_shot_reset(&data->shot_attempt);
        allstar_physics_init_ball(&data->ball);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_BUZZER);
    }
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME_NOTICE) {
        data->p1.has_ball = false;
        data->p2.has_ball = false;
        allstar_one_on_one_shot_reset(&data->shot_attempt);
        allstar_physics_init_ball(&data->ball);
    }
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME) {
        const AllStarPlayerStats *cpu_stats = allstar_roster_get_player(
            &game->roster, game->selected_player_2);
        one_on_one_reset_possession(data, game, true);
        allstar_ai_init(&data->ai, cpu_stats);
        allstar_ai_set_skill(&data->ai, game->settings.skill_level);
        allstar_ai_set_rom_profile(
            &data->ai, (uint8_t)game->selected_player_2);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
    }
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE) {
        uint32_t winner = game->one_on_one.winner == 1
            ? game->selected_player_1 : game->selected_player_2;
        game->last_match_winner = game->one_on_one.winner;
        if (game->selected_mode == ALLSTAR_MODE_TOURNAMENT &&
            game->tournament.active && game->tournament.match_in_progress) {
            allstar_tournament_record_winner(&game->tournament, winner);
            allstar_game_change_scene(game, ALLSTAR_SCENE_TOURNAMENT);
        } else {
            allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);
        }
        return true;
    }
    return false;
}

static bool one_on_one_update_score_presentation(SceneOneOnOneData *data,
                                                  AllStarGame *game,
                                                  float dt) {
    uint16_t previous_frame;
    uint32_t score_events;
    uint32_t frame;
    if (!data->score_presentation.active) return false;

    previous_frame = data->score_presentation.elapsed_frames;
    score_events = allstar_one_on_one_score_presentation_tick_0c13(
        &data->score_presentation,
        dt * ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE);

    /* $1E0E pins the made ball to $54/$5E.  The live trace holds gravity
       for 35 frames, then resumes $7BE8 and its $1E5B/$1E77 ground bounce
       path until $27C7 begins the fade. */
    for (frame = previous_frame;
         frame < data->score_presentation.elapsed_frames &&
         frame < ALLSTAR_ROM_SCORE_POSSESSION_RESET_FRAME;
         frame++) {
        allstar_physics_update_ball(
            &data->ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    }

    if (score_events & ALLSTAR_ROM_SCORE_EVENT_COMMIT) {
        data->pending_score_events = allstar_one_on_one_match_add_score(
            &game->one_on_one, data->score_presentation.shooter,
            data->score_presentation.points);
        /* $1F23 calls $2F88 command $05 when the score effect counter ends. */
        /* $1F06 selects normal score command $05. */
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SCORE_CHIME);
    }
    if (score_events & ALLSTAR_ROM_SCORE_EVENT_NET_SOUND) {
        /* $1F26 selects command $08 with the first net bend at +20. */
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
    }

    if ((score_events & ALLSTAR_ROM_SCORE_EVENT_FADE_OUT) &&
        (data->pending_score_events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT)) {
        data->score_presentation.active = false;
        one_on_one_handle_lifecycle_events(
            data, game, data->pending_score_events);
        data->pending_score_events = 0;
        return true;
    }

    if (score_events & ALLSTAR_ROM_SCORE_EVENT_RESET_POSSESSION) {
        int shooter = data->score_presentation.shooter;
        int next_possession = allstar_one_on_one_next_possession_after_score(
            &game->one_on_one, shooter);
        one_on_one_reset_possession(
            data, game, next_possession == 1);
    }

    if (score_events & ALLSTAR_ROM_SCORE_EVENT_INBOUND) {
        data->pending_score_events = 0;
        return false;
    }
    return true;
}

static bool one_on_one_update_foul_presentation(SceneOneOnOneData *data,
                                                 AllStarGame *game,
                                                 float dt) {
    uint32_t events;
    if (!data->foul_presentation.active) return false;
    events = allstar_one_on_one_foul_presentation_tick_0c49(
        &data->foul_presentation, dt);
    if (events & ALLSTAR_ROM_FOUL_EVENT_RESET_POSSESSION) {
        /* $20F7 restarts with the player opposite the offender. */
        one_on_one_reset_possession(
            data, game, data->foul_presentation.offender != 1);
    }
    return (events & ALLSTAR_ROM_FOUL_EVENT_COMPLETE) == 0;
}

static bool one_on_one_begin_take_back_violation_2c50(
        SceneOneOnOneData *data, AllStarGame *game) {
    int offender;
    if (!data || !game || data->take_back_violation_pending == 0)
        return false;
    offender = data->take_back_violation_pending;
    data->take_back_violation_pending = 0;
    allstar_one_on_one_foul_presentation_begin_05a3(
        &data->foul_presentation, ALLSTAR_ROM_CONTACT_DIDNT_CLEAR,
        offender);
    data->foul_events++;
    /* $05A3 selects command $04 for all rule popups. */
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
    return true;
}

static void one_on_one_init(AllStarScene *scene, AllStarGame *game) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    const AllStarPlayerStats *cpu_stats;

    memset(data, 0, sizeof(*data));
    allstar_one_on_one_match_init(&game->one_on_one,
                                  allstar_game_settings_time_seconds(&game->settings),
                                  game->one_on_one_shot_clock_seconds,
                                  game->settings.play_to,
                                  game->settings.winners_outs);
    allstar_physics_init_ball(&data->ball);
    one_on_one_reset_possession(data, game, true);

    cpu_stats = allstar_roster_get_player(&game->roster, game->selected_player_2);
    allstar_ai_init(&data->ai, cpu_stats);
    allstar_ai_set_skill(&data->ai, game->settings.skill_level);
    allstar_ai_set_rom_profile(
        &data->ai, (uint8_t)game->selected_player_2);
    allstar_rom_rng_init(&game->one_on_one_rng, 0xe018);

    allstar_audio_stop_bgm(&game->audio);
    /* The live trace has no synthetic whistle at match entry. The final
       roster command $0E is still sounding across the first $702D update. */
}

/* ROM: $0B80 match loop, $0C00 score ending, $0FDE clock ending. */
static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    uint32_t events;
    uint32_t ball_contacts;
    int recovering_player;
    float p2_before_x;
    float p2_before_y;
    data->anim_timer += dt;

    if (one_on_one_update_score_presentation(data, game, dt)) return;
    if (one_on_one_update_foul_presentation(data, game, dt)) return;
    if (one_on_one_begin_take_back_violation_2c50(data, game)) return;

    events = allstar_one_on_one_match_tick(&game->one_on_one, dt);
    if (one_on_one_handle_lifecycle_events(data, game, events)) return;

    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_RESULT) {
        if (allstar_one_on_one_result_can_dismiss(input->buttons_pressed)) {
            events = allstar_one_on_one_match_dismiss_result(&game->one_on_one);
            one_on_one_handle_lifecycle_events(data, game, events);
        }
        return;
    }

    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_OVERTIME) {
        if (allstar_one_on_one_overtime_can_dismiss(input->buttons_pressed)) {
            events = allstar_one_on_one_match_dismiss_overtime(&game->one_on_one);
            one_on_one_handle_lifecycle_events(data, game, events);
        }
        return;
    }

    if (game->one_on_one.phase != ALLSTAR_ONE_ON_ONE_PLAYING) return;

    one_on_one_update_take_back_78e9(data);

    one_on_one_tick_defense(data, dt);
    one_on_one_update_shot_animations(data, dt);

    events = allstar_one_on_one_shot_tick(&data->shot_attempt, dt);
    if (events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) {
        one_on_one_launch_shot(data, game, 1);
    }
    if (events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_TRAVELING) {
        events = allstar_one_on_one_match_call_traveling(&game->one_on_one, 1);
        if (events & ALLSTAR_ONE_ON_ONE_EVENT_TRAVELING) {
            one_on_one_reset_possession(data, game, false);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
        }
        return;
    }

    {
        /* On $70FD's protected actions $05/$0C/$14, $702D branches at
           $709A before clearing or refreshing player +$07. Preserve the
           direction sampled on the jump edge so $6BF9->$6B72 can keep
           moving at record boundaries until the landing transition. */
        if (!data->p1_defense_jump_active &&
            !one_on_one_action_is_defense_jump_70fd(
                data->p1_animation.action)) {
            data->p1_input_direction = (uint8_t)(
                input->buttons_held & 0x0f);
        }
        if (data->p1_input_direction != 0) {
            data->p1_previous_direction = data->p1_input_direction;
        }
    }

    allstar_one_on_one_rom_clamp_player_court(&data->p1.x, &data->p1.y);

    if (data->p1.has_ball) {
        uint32_t shot_events = allstar_one_on_one_shot_input(
            &data->shot_attempt, 1,
            allstar_input_is_pressed(input, ALLSTAR_BTN_A),
            allstar_input_is_held(input, ALLSTAR_BTN_B));
        if (shot_events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_GATHER) {
            uint8_t shot_variant = allstar_one_on_one_rom_shot_variant(
                data->p1.x, data->p1.y);
            data->p1.is_jumping = true;
            data->p1.is_shooting = true;
            data->p1_shot_action = shot_variant == 1
                ? ALLSTAR_ROM_SHOT_ACTION_A : ALLSTAR_ROM_SHOT_ACTION_B;
            data->p1_shot_animation_clock =
                ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
            data->p1_horizontal_flip =
                allstar_one_on_one_rom_shot_horizontal_flip_7138(
                    data->p1.x);
            allstar_one_on_one_rom_animation_set_action_6a8c(
                &data->p1_animation, data->p1_shot_action);
            allstar_one_on_one_rom_shot_animation_frame(
                data->p1_shot_action, data->shot_attempt.rom_phase, 0,
                &data->p1.anim_frame);
        } else if (shot_events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) {
            one_on_one_launch_shot(data, game, 1);
        } else if (data->shot_attempt.rom_phase == 1) {
            allstar_one_on_one_rom_shot_animation_frame(
                data->p1_shot_action, data->shot_attempt.rom_phase, 0,
                &data->p1.anim_frame);
        }
    } else {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) &&
            !data->p1.is_shooting && !data->p1_defense_jump_active &&
            !one_on_one_action_is_defense_jump_70fd(
                data->p1_animation.action)) {
            data->p1_defense_jump_active = true;
            data->p1_defense_jump_elapsed_frames = 0;
            allstar_one_on_one_rom_animation_set_action_6a8c(
                &data->p1_animation,
                allstar_one_on_one_rom_defense_jump_action_70fd(
                    data->p1_animation.action));
        }
        if (allstar_input_is_held(input, ALLSTAR_BTN_B) &&
            one_on_one_try_steal(data, game, 1)) {
            return;
        }
    }

    p2_before_x = data->p2.x;
    p2_before_y = data->p2.y;
    allstar_ai_rom_contact_response_75cd(
        &data->ai, data->p2_contact.blocked_contact,
        data->p2.has_ball,
        allstar_rom_rng_current(&game->one_on_one_rng),
        (uint8_t)data->ball.x, data->p2.x, data->p2.y);
    data->ai.rom_action_index = data->p2_animation.record_index;
    allstar_ai_update(&data->ai, &data->p2, &data->p1, &data->ball,
                      allstar_rom_rng_current(&game->one_on_one_rng),
                      allstar_rom_rng_high(&game->one_on_one_rng),
                      allstar_rom_rng_alternate(&game->one_on_one_rng),
                      allstar_rom_rng_alternate_high(&game->one_on_one_rng),
                      dt);
    if (data->p2.has_ball && data->p2.is_shooting &&
        data->p2_animation.action != ALLSTAR_ROM_SHOT_ACTION_A &&
        data->p2_animation.action != ALLSTAR_ROM_SHOT_ACTION_B) {
        uint8_t shot_variant = allstar_one_on_one_rom_shot_variant(
            data->p2.x, data->p2.y);
        data->p2_shot_action = shot_variant == 1
            ? ALLSTAR_ROM_SHOT_ACTION_A : ALLSTAR_ROM_SHOT_ACTION_B;
        data->p2_shot_animation_clock =
            ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
        data->p2_horizontal_flip =
            allstar_one_on_one_rom_shot_horizontal_flip_7138(data->p2.x);
        allstar_one_on_one_rom_animation_set_action_6a8c(
            &data->p2_animation, data->p2_shot_action);
    }
    if (!data->p2_defense_jump_active &&
        !one_on_one_action_is_defense_jump_70fd(
            data->p2_animation.action)) {
        data->p2_input_direction = one_on_one_direction_from_delta(
            data->p2.x - p2_before_x, data->p2.y - p2_before_y);
    }
    {
        data->p2.x = p2_before_x;
        data->p2.y = p2_before_y;
    }
    if (data->p2_input_direction != 0) {
        data->p2_previous_direction = data->p2_input_direction;
    }
    if (data->ai.rom_steal_pressed &&
        one_on_one_try_steal(data, game, 2)) {
        return;
    }
    if (data->p2.is_jumping && !data->p2.is_shooting &&
        !data->p2_defense_jump_active &&
        !one_on_one_action_is_defense_jump_70fd(
            data->p2_animation.action)) {
        data->p2_defense_jump_active = true;
        data->p2_defense_jump_elapsed_frames = 0;
        allstar_one_on_one_rom_animation_set_action_6a8c(
            &data->p2_animation,
            allstar_one_on_one_rom_defense_jump_action_70fd(
                data->p2_animation.action));
    }
    data->p1.is_jumping = data->p1.is_shooting ||
                          data->p1_defense_jump_active;
    data->p2.is_jumping = data->p2.is_shooting ||
                          data->p2_defense_jump_active;

    /* $6FF3 processes P1 before P2 and reaches $2B6C before $7BE8 advances
       the ball. $2B88 rejects $FFF8; $1F5F rim contact preserves it, while
       $1E5B/$1E77 clears it at the first ground bounce. */
    if ((!data->p1.has_ball && !data->p2.has_ball) &&
        (one_on_one_try_jump_recovery(data, game, 1) ||
         one_on_one_try_jump_recovery(data, game, 2))) {
        return;
    }
    if (data->p2.has_ball && data->p2.is_shooting &&
        data->ai.rom_shot_release && !data->ball.in_flight) {
        one_on_one_launch_shot(data, game, 2);
    }

    allstar_physics_update_ball(&data->ball, dt);
    ball_contacts = allstar_physics_apply_rom_court_contacts(&data->ball);
    if (ball_contacts & ALLSTAR_BALL_CONTACT_RIM_BACKBOARD) {
        /* Fixed $1F5F dispatches command $09 once per accepted rim cell. */
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
        data->rim_audio_events++;
    }
    if (ball_contacts & ALLSTAR_BALL_CONTACT_SCORE) {
        allstar_one_on_one_score_presentation_begin_1e0e(
            &data->score_presentation,
            data->ball.shooter_id, data->ball.point_value);
        data->pending_score_events = 0;
        data->ball.rom_step_state.vx = 0;
        data->ball.rom_step_state.vy = 0;
        data->ball.rom_step_state.vz = -0x0018;
        data->ball.rom_step_state.x = 0x5400;
        data->ball.rom_step_state.y = 0x5e00;
        data->ball.x = 84.0f;
        data->ball.y = 94.0f;
        data->ball.step_accumulator = 0.0f;
        data->ball.rom_step_state.gravity_delay_frames = 35;
        data->ball.rom_hard_bounce_pending = true;
        data->ball.in_flight = true;
        return;
    }

    recovering_player = allstar_one_on_one_rom_recovery_dispatch(
        &data->recovery,
        data->p1.has_ball || data->p2.has_ball,
        false, /* $FFEB: the native live update is not a counted wait. */
        data->ball.z,
        data->ball.made_basket, /* $FFE2 score event; handled above. */
        game->one_on_one.phase != ALLSTAR_ONE_ON_ONE_PLAYING, /* $FFE7 */
        !data->ball.recoverable,
        !data->p1.is_shooting,
        allstar_one_on_one_player_can_pick_up_ball(
            data->p1.x,
            data->p1.y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y,
            data->ball.x, data->ball.y),
        !data->p2.is_shooting,
        allstar_one_on_one_player_can_pick_up_ball(
            data->p2.x,
            data->p2.y + ALLSTAR_ROM_PLAYER_GROUND_TO_PICKUP_Y,
            data->ball.x, data->ball.y));

    if (recovering_player == 1) {
        one_on_one_take_live_possession(data, game, 1, true);
    } else if (recovering_player == 2) {
        one_on_one_take_live_possession(data, game, 2, true);
    }

    {
        int offender = 0;
        AllStarRomContactEvent contact_event =
            one_on_one_tick_rom_animations(data, game, dt, &offender);
        if (contact_event != ALLSTAR_ROM_CONTACT_NONE && offender != 0) {
            allstar_one_on_one_foul_presentation_begin_05a3(
                &data->foul_presentation, contact_event, offender);
            data->foul_events++;
            /* $05A3 selects sound command $04 before $0C49's wait/fades. */
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
        }
    }
}

static void one_on_one_draw_result(AllStarGame *game, AllStarRenderer *renderer) {
    const AllStarPlayerStats *s1 = allstar_roster_get_player(&game->roster, game->selected_player_1);
    const AllStarPlayerStats *s2 = allstar_roster_get_player(&game->roster, game->selected_player_2);
    char score[16];
    const char *p1_name = s1 ? s1->last_name : "PLAYER 1";
    const char *p2_name = s2 ? s2->last_name : "PLAYER 2";
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_text(renderer,
        game->one_on_one.winner == 0 ? "TIE" : "FINAL", 56, 24, 3);
    allstar_renderer_draw_text(renderer, p1_name, 24, 48, 3);
    snprintf(score, sizeof(score), "%03u", (unsigned)game->one_on_one.p1_score);
    allstar_renderer_draw_text(renderer, score, 112, 48, 3);
    allstar_renderer_draw_text(renderer, p2_name, 24, 72, 3);
    snprintf(score, sizeof(score), "%03u", (unsigned)game->one_on_one.p2_score);
    allstar_renderer_draw_text(renderer, score, 112, 72, 3);
    allstar_renderer_draw_text(renderer, "A/B TO SKIP", 32, 112, 3);
}

static void one_on_one_draw_overtime(AllStarRenderer *renderer) {
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_text(renderer, "OVERTIME", 48, 56, 3);
    allstar_renderer_draw_text(renderer, "A TO SKIP", 40, 88, 3);
}

static void one_on_one_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    const AllStarPlayerStats *s1;
    const AllStarPlayerStats *s2;
    const char *p1_name;
    const char *p2_name;
    const char *p1_space;
    const char *p2_space;
    char shot_buf[16];
    char s1_buf[16];
    char clk_buf[16];
    char s2_buf[16];
    int p1_len;
    int p2_len;
    int p1_x;
    int p2_x;
    uint8_t p1_skin;
    uint8_t p2_skin;
    bool p1_sprite_flip;
    bool p2_sprite_flip;
    bool sprites_visible;
    int32_t p1_visual_lift = 0;
    int32_t p2_visual_lift = 0;
    AllStarRomNetFrame net_frame;
    bool loose_ball_visible;
    bool score_ball_behind_net;

    if (!data->score_presentation.active &&
        game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_RESULT) {
        one_on_one_draw_result(game, renderer);
        return;
    }
    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_OVERTIME) {
        one_on_one_draw_overtime(renderer);
        return;
    }

    net_frame = allstar_one_on_one_score_net_frame_1ecc(
        data->score_presentation.elapsed_frames);
    loose_ball_visible = data->ball.in_flight ||
        (!data->p1.has_ball && !data->p2.has_ball);
    /* $1E0E seeds $C12B=$23.  Until its 35-count expires, $6945 ORs OBJ
       priority bit 7 into the ball OAM, putting the score ball behind the
       nonzero net BG while player OAM remains in front. */
    score_ball_behind_net = data->score_presentation.active &&
        data->score_presentation.elapsed_frames < 35 &&
        loose_ball_visible;

    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_court(renderer);
    if (score_ball_behind_net) {
        allstar_renderer_draw_ball_ex(
            renderer, (int32_t)data->ball.x, (int32_t)data->ball.y,
            (int32_t)data->ball.z, data->anim_timer);
    }
    allstar_renderer_draw_net_overlay_1ecc(renderer, (uint8_t)net_frame);
    s1 = allstar_roster_get_player(&game->roster, game->selected_player_1);
    s2 = allstar_roster_get_player(&game->roster, game->selected_player_2);

    snprintf(shot_buf, sizeof(shot_buf), "00:%02d", (int)game->one_on_one.shot_clock);
    allstar_renderer_draw_text(renderer, shot_buf, 8, 8, 3);
    snprintf(s1_buf, sizeof(s1_buf), "%03u", (unsigned)game->one_on_one.p1_score);
    allstar_renderer_draw_text(renderer, s1_buf, 16, 24, 3);

    p1_name = s1 ? s1->name : "PLAYER 1";
    p1_space = strchr(p1_name, ' ');
    p1_name = p1_space ? p1_space + 1 : p1_name;
    p1_len = (int)strlen(p1_name);
    p1_x = (72 - p1_len * 8) / 2;
    if (p1_x < 2) p1_x = 2;
    allstar_renderer_draw_text(renderer, p1_name, p1_x, 40, 3);

    snprintf(clk_buf, sizeof(clk_buf), "%02d:%02d",
             (int)game->one_on_one.game_clock / 60,
             (int)game->one_on_one.game_clock % 60);
    allstar_renderer_draw_text(renderer, clk_buf, 112, 8, 3);
    snprintf(s2_buf, sizeof(s2_buf), "%03u", (unsigned)game->one_on_one.p2_score);
    allstar_renderer_draw_text(renderer, s2_buf, 120, 24, 3);

    p2_name = s2 ? s2->name : "PLAYER 2";
    p2_space = strchr(p2_name, ' ');
    p2_name = p2_space ? p2_space + 1 : p2_name;
    p2_len = (int)strlen(p2_name);
    p2_x = 88 + (72 - p2_len * 8) / 2;
    if (p2_x < 88) p2_x = 88;
    allstar_renderer_draw_text(renderer, p2_name, p2_x, 40, 3);

    /* $2821 changes LCDC from $87 to $85 at fade stage two. $2814's
       reverse-table completion restores the OBJ control flag for inbound. */
    sprites_visible = (!data->score_presentation.active ||
        data->score_presentation.elapsed_frames < 192) &&
        (!data->foul_presentation.active ||
         data->foul_presentation.sprites_visible);
    if (data->score_presentation.active) {
        allstar_renderer_apply_dmg_bgp(
            renderer, data->score_presentation.bg_palette);
    }
    if (data->foul_presentation.active) {
        allstar_renderer_apply_dmg_bgp(
            renderer, data->foul_presentation.bg_palette);
    }
    if (data->foul_presentation.active &&
        data->foul_presentation.message_visible) {
        const char *message;
        int message_width;
        int message_x;
        if (data->foul_presentation.violation ==
                ALLSTAR_ROM_CONTACT_CHARGING) {
            message = "CHARGING";
        } else if (data->foul_presentation.violation ==
                ALLSTAR_ROM_CONTACT_BLOCKING) {
            message = "BLOCKING";
        } else {
            message = "DIDN'T CLEAR BALL";
        }
        message_width = (int)strlen(message) * 8;
        message_x = (160 - message_width) / 2;
        allstar_renderer_draw_rect_fill(
            renderer, message_x - 8, 64, message_width + 16, 24, 0);
        allstar_renderer_draw_rect_outline(
            renderer, message_x - 8, 64, message_width + 16, 24, 3);
        allstar_renderer_draw_text(renderer, message, message_x, 72, 3);
    }
    if (!sprites_visible) return;

    p1_skin = s1 ? s1->skin_tone : 0x90;
    p2_skin = s2 ? s2->skin_tone : 0x91;
    /* $2945 copies player +$02 bit 4 into OAM X-flip bit 5. Preserve the
       bit directly for both player composition and $6F2A ball placement. */
    p1_sprite_flip = data->p1_horizontal_flip;
    p2_sprite_flip = data->p2_horizontal_flip;
    if (data->p1_shot_animation_clock > 0.0f) {
        uint16_t elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
             data->p1_shot_animation_clock) * 60.0f);
        p1_visual_lift = (int32_t)
            allstar_one_on_one_rom_shot_jump_height_6c4d(elapsed);
    }
    if (data->p2_shot_animation_clock > 0.0f) {
        uint16_t elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
             data->p2_shot_animation_clock) * 60.0f);
        p2_visual_lift = (int32_t)
            allstar_one_on_one_rom_shot_jump_height_6c4d(elapsed);
    }
    allstar_renderer_draw_player_lifted_ex(renderer,
        (int32_t)data->p2.x, (int32_t)data->p2.y, p2_visual_lift,
        false, p2_skin, data->p2.has_ball,
        data->p2.is_shooting || data->p2.is_jumping,
        data->p2_steal_latch_frames > 0 || data->p2_defense_jump_active ||
        (!data->p2.has_ball && data->p1.has_ball),
        data->p2.is_shooting ? data->p2_shot_action
                             : data->p2_animation.action,
        data->p2.anim_frame,
        data->p2_animation.record_index,
        data->anim_timer, p2_sprite_flip);
    allstar_renderer_draw_player_lifted_ex(renderer,
        (int32_t)data->p1.x, (int32_t)data->p1.y, p1_visual_lift,
        true, p1_skin, data->p1.has_ball,
        data->p1.is_shooting || data->p1.is_jumping,
        data->p1_steal_latch_frames > 0 || data->p1_defense_jump_active ||
        (!data->p1.has_ball && data->p2.has_ball),
        data->p1.is_shooting ? data->p1_shot_action
                             : data->p1_animation.action,
        data->p1.anim_frame,
        data->p1_animation.record_index,
        data->anim_timer, p1_sprite_flip);
    if (loose_ball_visible && !score_ball_behind_net) {
        allstar_renderer_draw_ball_ex(renderer, (int32_t)data->ball.x, (int32_t)data->ball.y,
                                      (int32_t)data->ball.z, data->anim_timer);
    }
}

static void one_on_one_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_one_on_one(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_ONE_ON_ONE;
    scene->user_data = calloc(1, sizeof(SceneOneOnOneData));
    scene->init = one_on_one_init;
    scene->update = one_on_one_update;
    scene->draw = one_on_one_draw;
    scene->destroy = one_on_one_destroy;
    return scene;
}

bool allstar_scene_one_on_one_set_test_positions(AllStarScene *scene,
                                                 float p1_x, float p1_y,
                                                 float p2_x, float p2_y) {
    SceneOneOnOneData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_ONE_ON_ONE || !scene->user_data)
        return false;
    data = (SceneOneOnOneData*)scene->user_data;
    data->p1.x = p1_x;
    data->p1.y = p1_y;
    data->p2.x = p2_x;
    data->p2.y = p2_y;
    return true;
}

bool allstar_scene_one_on_one_set_test_possession(
        AllStarScene *scene, AllStarGame *game, int player) {
    SceneOneOnOneData *data;
    if (!scene || !game || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data || (player != 1 && player != 2)) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    one_on_one_reset_possession(data, game, player == 1);
    return true;
}

bool allstar_scene_one_on_one_get_debug_state(
        const AllStarScene *scene, AllStarOneOnOneDebugState *state) {
    const SceneOneOnOneData *data;
    if (!scene || !state || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data) return false;
    data = (const SceneOneOnOneData*)scene->user_data;
    memset(state, 0, sizeof(*state));
    state->p1_x = data->p1.x;
    state->p1_y = data->p1.y;
    state->p2_x = data->p2.x;
    state->p2_y = data->p2.y;
    state->ball_x = data->ball.x;
    state->ball_y = data->ball.y;
    state->ball_z = data->ball.z;
    state->p1_action = data->p1_animation.action;
    state->p2_action = data->p2_animation.action;
    state->p1_record = data->p1_animation.record_index;
    state->p2_record = data->p2_animation.record_index;
    state->p1_display_frame = data->p1.anim_frame;
    state->shot_phase = data->shot_attempt.rom_phase;
    state->cpu_state = (uint8_t)data->ai.state;
    state->cpu_offense_stage = data->ai.rom_offense_stage;
    state->cpu_target_x = data->ai.rom_target_x;
    state->cpu_target_y = data->ai.rom_target_y;
    state->rim_audio_events = data->rim_audio_events;
    state->steal_transfer_events = data->steal_transfer_events;
    state->foul_events = data->foul_events;
    state->foul_elapsed_frames = data->foul_presentation.elapsed_frames;
    state->foul_violation = (uint8_t)data->foul_presentation.violation;
    state->p1_has_ball = data->p1.has_ball;
    state->p2_has_ball = data->p2.has_ball;
    state->ball_in_flight = data->ball.in_flight;
    state->ball_recoverable = data->ball.recoverable;
    state->p1_defense_jump_active = data->p1_defense_jump_active;
    state->p2_defense_jump_active = data->p2_defense_jump_active;
    state->score_presentation_active = data->score_presentation.active;
    state->foul_presentation_active = data->foul_presentation.active;
    state->foul_message_visible = data->foul_presentation.message_visible;
    return true;
}

bool allstar_scene_one_on_one_set_test_ball_rom(
        AllStarScene *scene, uint16_t x, uint16_t y, uint16_t z,
        int16_t vx, int16_t vy, int16_t vz, int shooter) {
    SceneOneOnOneData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data || (shooter != 1 && shooter != 2)) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    allstar_physics_init_ball(&data->ball);
    data->p1.has_ball = false;
    data->p2.has_ball = false;
    allstar_one_on_one_rom_animation_set_action_6a8c(
        &data->p1_animation, 0x15);
    allstar_one_on_one_rom_animation_set_action_6a8c(
        &data->p2_animation, 0x0d);
    data->ball.in_flight = true;
    data->ball.shooter_id = shooter;
    data->ball.rom_step_state_valid = true;
    data->ball.rom_step_state.x = x;
    data->ball.rom_step_state.y = y;
    data->ball.rom_step_state.z = z;
    data->ball.rom_step_state.vx = vx;
    data->ball.rom_step_state.vy = vy;
    data->ball.rom_step_state.vz = vz;
    data->ball.x = (float)x / 256.0f;
    data->ball.y = (float)y / 256.0f;
    data->ball.z = (float)(int16_t)z / 256.0f;
    return true;
}

bool allstar_scene_one_on_one_set_test_player_state(
        AllStarScene *scene, int player, uint8_t action, uint8_t record,
        uint8_t previous_direction, bool horizontal_flip) {
    SceneOneOnOneData *data;
    AllStarRomAnimationState *animation;
    if (!scene || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data || (player != 1 && player != 2)) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    animation = player == 1 ? &data->p1_animation : &data->p2_animation;
    allstar_one_on_one_rom_animation_set_action_6a8c(animation, action);
    animation->record_index = record;
    if (player == 1) {
        data->p1_previous_direction = previous_direction;
        data->p1_horizontal_flip = horizontal_flip;
    } else {
        data->p2_previous_direction = previous_direction;
        data->p2_horizontal_flip = horizontal_flip;
    }
    return true;
}

bool allstar_scene_one_on_one_try_test_steal(
        AllStarScene *scene, AllStarGame *game, int defender) {
    if (!scene || !game || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data || (defender != 1 && defender != 2)) return false;
    return one_on_one_try_steal(
        (SceneOneOnOneData*)scene->user_data, game, defender);
}

bool allstar_scene_one_on_one_begin_test_foul(
        AllStarScene *scene, AllStarGame *game,
        uint8_t violation, int offender) {
    SceneOneOnOneData *data;
    if (!scene || !game || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data ||
        (violation != ALLSTAR_ROM_CONTACT_CHARGING &&
         violation != ALLSTAR_ROM_CONTACT_BLOCKING &&
         violation != ALLSTAR_ROM_CONTACT_DIDNT_CLEAR) ||
        (offender != 1 && offender != 2)) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    allstar_one_on_one_foul_presentation_begin_05a3(
        &data->foul_presentation, (AllStarRomContactEvent)violation,
        offender);
    data->foul_events++;
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
    return true;
}

bool allstar_scene_one_on_one_set_test_take_back_required(
        AllStarScene *scene, bool required) {
    SceneOneOnOneData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    data->take_back_required = required;
    data->take_back_violation_pending = 0;
    return true;
}

bool allstar_scene_one_on_one_take_test_live_possession(
        AllStarScene *scene, AllStarGame *game, int player) {
    SceneOneOnOneData *data;
    if (!scene || !game || scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        !scene->user_data || (player != 1 && player != 2)) return false;
    data = (SceneOneOnOneData*)scene->user_data;
    one_on_one_take_live_possession(data, game, player, true);
    return true;
}
