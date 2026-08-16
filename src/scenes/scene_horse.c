#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_horse.h"
#include "allstar_physics.h"
#include "allstar_rng.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HORSE_START_X 88.0f
#define HORSE_START_Y 96.0f
#define HORSE_SHOT_RESOLVE_FRAMES 180
#define HORSE_BETWEEN_TURN_FRAMES 192
#define HORSE_CPU_MOVE_PERIOD 3
#define HORSE_CPU_GATHER_FRAMES 20

typedef enum {
    HORSE_PHASE_CONTROL = 0,
    HORSE_PHASE_CPU_APPROACH,
    HORSE_PHASE_MATCH_APPROACH,
    HORSE_PHASE_GATHER,
    HORSE_PHASE_FLIGHT,
    HORSE_PHASE_TURN_WAIT,
    HORSE_PHASE_RESULT
} SceneHorsePhase;

typedef struct {
    AllStarHorseState mode;
    AllStarRomRng rng;
    AllStarPlayerState player;
    AllStarBall ball;
    AllStarRomAnimationState animation;
    AllStarOneOnOneShotAttempt shot;
    SceneHorsePhase phase;
    float fixed_accumulator;
    float anim_time;
    float shot_animation_clock;
    uint16_t phase_frames;
    uint16_t shot_frames;
    uint16_t made_frame;
    uint8_t shot_action;
    uint8_t target_x;
    uint8_t target_y;
    uint8_t input_direction;
    uint8_t previous_direction;
    uint32_t last_events;
    bool horizontal_flip;
    bool shot_made;
    bool marker_visible;
    bool rim_audio_played;
    bool contact_audio_played;
    bool net_audio_played;
    bool score_audio_played;
} SceneHorseData;

static bool horse_player_is_cpu(const SceneHorseData *data) {
    /* Cartridge one-human setup ($FF91=1): P2 is CPU, but mode 2 still
       selects two distinct roster records before entering gameplay. */
    return data->mode.current_player == 2;
}

static uint8_t horse_direction(uint8_t held) {
    return held & (ALLSTAR_BTN_RIGHT | ALLSTAR_BTN_LEFT |
                   ALLSTAR_BTN_UP | ALLSTAR_BTN_DOWN);
}

static void horse_reset_active_player(SceneHorseData *data) {
    memset(&data->player, 0, sizeof(data->player));
    data->player.x = HORSE_START_X;
    data->player.y = HORSE_START_Y;
    data->player.has_ball = true;
    data->shot_action = 0;
    data->shot_animation_clock = 0.0f;
    data->horizontal_flip = allstar_one_on_one_rom_shot_horizontal_flip_7138(
        data->player.x);
    data->input_direction = 0;
    data->previous_direction = ALLSTAR_BTN_UP;
    allstar_one_on_one_rom_animation_init_6a8c(&data->animation, 0x0b);
    allstar_one_on_one_shot_reset(&data->shot);
    allstar_physics_init_ball(&data->ball);
}

static void horse_choose_cpu_target(SceneHorseData *data) {
    allstar_horse_cpu_spot_6cab(
        allstar_rom_rng_current(&data->rng), data->mode.cpu_spot_index,
        &data->target_x, &data->target_y);
    data->mode.cpu_spot_index = (uint8_t)(
        (data->mode.cpu_spot_index + 1) % ALLSTAR_HORSE_CPU_SPOTS_PER_GROUP);
}

static void horse_begin_turn(SceneHorseData *data) {
    horse_reset_active_player(data);
    data->phase_frames = 0;
    data->shot_frames = 0;
    data->shot_made = false;
    data->made_frame = 0;
    data->rim_audio_played = false;
    data->contact_audio_played = false;
    data->net_audio_played = false;
    data->score_audio_played = false;
    data->marker_visible = false;
    if (allstar_horse_current_is_matcher(&data->mode)) {
        data->target_x = data->mode.saved_x;
        data->target_y = data->mode.saved_y;
        data->phase = HORSE_PHASE_MATCH_APPROACH;
        data->marker_visible = true;
    } else if (horse_player_is_cpu(data)) {
        horse_choose_cpu_target(data);
        data->phase = HORSE_PHASE_CPU_APPROACH;
    } else {
        data->phase = HORSE_PHASE_CONTROL;
    }
}

static void horse_init(AllStarScene *scene, AllStarGame *game) {
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    memset(data, 0, sizeof(*data));
    allstar_horse_init_0cdf(&data->mode);
    allstar_rom_rng_init(&data->rng, 0xe018);
    horse_begin_turn(data);
    /* Mode 2 entry $0CDF clears $DD73. */
    allstar_audio_stop_bgm(&game->audio);
}

static bool horse_move_toward_target(SceneHorseData *data) {
    uint8_t direction = 0;
    bool blocked = false;
    bool moved;
    if (data->player.x < data->target_x) direction |= ALLSTAR_BTN_RIGHT;
    else if (data->player.x > data->target_x) direction |= ALLSTAR_BTN_LEFT;
    if (data->player.y < data->target_y) direction |= ALLSTAR_BTN_DOWN;
    else if (data->player.y > data->target_y) direction |= ALLSTAR_BTN_UP;
    data->input_direction = direction;
    if (direction == 0) return true;
    moved = allstar_one_on_one_rom_player_move_6b72(
        direction, 0x02, &data->player.x, &data->player.y,
        0.0f, 0.0f, &blocked);
    /* $6DB7 includes raw marker centers $0C/$A0, four pixels beyond the
       ordinary $6B72 player-center endpoints. $7AFD ultimately stores the
       matching +$06 field directly, so finish that final four-pixel step. */
    if (!moved) {
        if (fabsf(data->player.x - data->target_x) <= 4.0f)
            data->player.x = data->target_x;
        if (fabsf(data->player.y - data->target_y) <= 4.0f)
            data->player.y = data->target_y;
    }
    (void)blocked;
    return data->player.x == data->target_x &&
           data->player.y == data->target_y;
}

static void horse_begin_gather(SceneHorseData *data, int player) {
    uint8_t variant = allstar_one_on_one_rom_shot_variant(
        data->player.x, data->player.y);
    data->shot_action = variant == 1
        ? ALLSTAR_ROM_SHOT_ACTION_A : ALLSTAR_ROM_SHOT_ACTION_B;
    data->horizontal_flip = allstar_one_on_one_rom_shot_horizontal_flip_7138(
        data->player.x);
    data->player.is_shooting = true;
    data->player.is_jumping = true;
    allstar_one_on_one_shot_reset(&data->shot);
    allstar_one_on_one_shot_press(&data->shot, player);
    allstar_one_on_one_rom_animation_set_action_6a8c(
        &data->animation, data->shot_action);
    data->shot_animation_clock = ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
    data->phase = HORSE_PHASE_GATHER;
    data->phase_frames = 0;
}

/* Shared bank 1 $7C58/$7EA9/$7F37 shot construction used by modes 0/2. */
static void horse_launch_shot(SceneHorseData *data, AllStarGame *game) {
    uint8_t variant = allstar_one_on_one_rom_shot_variant(
        data->player.x, data->player.y);
    uint8_t distance = allstar_one_on_one_rom_shot_distance_class(
        data->player.x, data->player.y);
    uint8_t launch_index = allstar_one_on_one_rom_shot_record_index(
        data->shot.rom_elapsed_frames);
    uint8_t roster_index = (uint8_t)(data->mode.current_player == 1
        ? game->selected_player_1 : game->selected_player_2);
    float release_x = data->player.x;
    float release_y = data->player.y;
    float release_z = (float)allstar_one_on_one_rom_shot_release_height(
        launch_index);
    AllStarOneOnOneReleaseOffset offset;
    if (allstar_one_on_one_rom_release_offset(
            data->shot_action, data->shot.rom_phase, variant,
            data->horizontal_flip, &offset)) {
        release_x += (float)offset.x_offset - ALLSTAR_ROM_PLAYER_X_TO_CENTER;
        release_y += (float)offset.ground_y_offset;
    }
    data->player.has_ball = false;
    data->player.is_jumping = false;
    allstar_physics_shoot_ball_rom_7c58(
        &data->ball, release_x, release_y, release_z,
        ALLSTAR_ONE_ON_ONE_HOOP_X, ALLSTAR_ONE_ON_ONE_HOOP_Y,
        distance,
        allstar_one_on_one_rom_shot_vertical_velocity(
            roster_index, distance, launch_index),
        data->shot.rom_phase, data->mode.current_player, 0);
    data->ball.rom_player_shot_phase_active = data->player.is_shooting;
    /* $0E36 runs immediately after the release latch for a caller. */
    if (!allstar_horse_current_is_matcher(&data->mode))
        allstar_horse_save_spot_0e36(
            &data->mode, data->player.x, data->player.y);
    data->phase = HORSE_PHASE_FLIGHT;
    data->phase_frames = 0;
    data->shot_frames = 0;
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
}

static void horse_tick_animation(SceneHorseData *data, AllStarGame *game) {
    if (data->phase == HORSE_PHASE_CONTROL ||
        data->phase == HORSE_PHASE_CPU_APPROACH ||
        data->phase == HORSE_PHASE_MATCH_APPROACH) {
        if (allstar_one_on_one_rom_select_movement_action_782e(
                &data->animation, data->input_direction, 0,
                data->previous_direction, false, false,
                &data->horizontal_flip)) {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOE_SQUEAK);
        }
    }
    if (allstar_one_on_one_rom_animation_tick_6a8c(
            game->asset_pack, &data->animation)) {
        data->player.anim_frame = data->animation.display_frame;
        if (data->player.has_ball && data->animation.record_index == 6)
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
    }
    if (data->shot_animation_clock > 0.0f) {
        uint16_t elapsed;
        data->shot_animation_clock -= ALLSTAR_PHYSICS_STEP_SECONDS;
        if (data->shot_animation_clock < 0.0f)
            data->shot_animation_clock = 0.0f;
        elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
             data->shot_animation_clock) * 60.0f);
        allstar_one_on_one_rom_shot_animation_frame(
            data->shot_action, data->shot.rom_phase, elapsed,
            &data->player.anim_frame);
    }
}

static void horse_resolve_attempt(SceneHorseData *data,
                                  AllStarGame *game) {
    data->last_events = allstar_horse_resolve_shot_0d57(
        &data->mode, data->shot_made, data->player.x, data->player.y);
    if ((data->last_events & ALLSTAR_HORSE_EVENT_LETTER) != 0)
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_HORSE_LETTER);
    data->player.is_shooting = false;
    data->phase_frames = 0;
    data->phase = data->mode.complete
        ? HORSE_PHASE_RESULT : HORSE_PHASE_TURN_WAIT;
}

static void horse_tick_fixed(SceneHorseData *data, AllStarGame *game,
                             uint8_t held, uint8_t pressed) {
    data->phase_frames++;
    allstar_rom_rng_end_frame_0714(&data->rng, 0, 0);
    if (data->phase == HORSE_PHASE_RESULT) return;

    if (data->phase == HORSE_PHASE_CONTROL) {
        bool blocked = false;
        data->input_direction = horse_direction(held);
        if (data->input_direction != 0 &&
            (data->phase_frames % 6u) == 0) {
            allstar_one_on_one_rom_player_move_6b72(
                data->input_direction, 0x02,
                &data->player.x, &data->player.y,
                0.0f, 0.0f, &blocked);
        }
        if ((pressed & ALLSTAR_BTN_A) != 0)
            horse_begin_gather(data, data->mode.current_player);
    } else if (data->phase == HORSE_PHASE_CPU_APPROACH ||
               data->phase == HORSE_PHASE_MATCH_APPROACH) {
        if ((data->phase_frames % HORSE_CPU_MOVE_PERIOD) == 0 &&
            horse_move_toward_target(data)) {
            data->player.x = data->target_x;
            data->player.y = data->target_y;
            data->marker_visible = false;
            if (horse_player_is_cpu(data)) {
                horse_begin_gather(data, data->mode.current_player);
            } else {
                data->phase = HORSE_PHASE_CONTROL;
                data->phase_frames = 0;
            }
        }
    } else if (data->phase == HORSE_PHASE_GATHER) {
        uint32_t shot_events;
        data->input_direction = horse_player_is_cpu(data)
            ? 0 : horse_direction(held);
        /* $702D remains active during gather, so the human can move before
           the second A/release exactly as in One-on-One. */
        if (!horse_player_is_cpu(data) && data->input_direction != 0 &&
            (data->phase_frames % 6u) == 0) {
            bool blocked = false;
            allstar_one_on_one_rom_player_move_6b72(
                data->input_direction, 0x02,
                &data->player.x, &data->player.y,
                0.0f, 0.0f, &blocked);
        }
        if (horse_player_is_cpu(data) &&
            data->phase_frames >= HORSE_CPU_GATHER_FRAMES) {
            data->shot.rom_elapsed_frames = data->phase_frames;
            horse_launch_shot(data, game);
        } else {
            shot_events = allstar_one_on_one_shot_input(
                &data->shot, data->mode.current_player,
                (pressed & ALLSTAR_BTN_A) != 0,
                (held & ALLSTAR_BTN_B) != 0);
            if ((shot_events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) != 0)
                horse_launch_shot(data, game);
            else {
                shot_events = allstar_one_on_one_shot_tick(
                    &data->shot, ALLSTAR_PHYSICS_STEP_SECONDS);
                if ((shot_events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) != 0)
                    horse_launch_shot(data, game);
            }
        }
    } else if (data->phase == HORSE_PHASE_FLIGHT) {
        uint32_t contacts;
        data->shot_frames++;
        allstar_physics_update_ball(&data->ball,
                                    ALLSTAR_PHYSICS_STEP_SECONDS);
        contacts = allstar_physics_apply_rom_court_contacts(&data->ball);
        if ((contacts & ALLSTAR_BALL_CONTACT_RIM_BACKBOARD) != 0 &&
            !data->rim_audio_played) {
            data->rim_audio_played = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
        }
        if ((contacts & ALLSTAR_BALL_CONTACT_BACK_COURT) != 0 &&
            !data->contact_audio_played) {
            data->contact_audio_played = true;
            /* Live mode 2 reaches command $0A at the backboard/return
               dispatcher on the matching miss (release +64). */
            allstar_audio_play_sfx(
                &game->audio, ALLSTAR_SFX_FREE_THROW_CONTACT);
        }
        if ((contacts & ALLSTAR_BALL_CONTACT_SCORE) != 0 &&
            !data->shot_made) {
            data->shot_made = true;
            data->made_frame = data->shot_frames;
        }
        if (data->shot_made && !data->net_audio_played &&
            data->shot_frames == data->made_frame + 20) {
            data->net_audio_played = true;
            /* $1F26: shared net command $08 exactly 20 frames after make. */
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_FREE_THROW_NET);
        }
        if (data->shot_made && !data->score_audio_played &&
            data->shot_frames == data->made_frame + 65) {
            data->score_audio_played = true;
            /* $1F23: shared score command $05 exactly 65 frames after make. */
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SCORE_CHIME);
        }
        if (data->shot_frames >= HORSE_SHOT_RESOLVE_FRAMES)
            horse_resolve_attempt(data, game);
    } else if (data->phase == HORSE_PHASE_TURN_WAIT &&
               data->phase_frames >= HORSE_BETWEEN_TURN_FRAMES) {
        horse_begin_turn(data);
    }
    horse_tick_animation(data, game);
    if (data->input_direction != 0)
        data->previous_direction = data->input_direction;
}

static void horse_update(AllStarScene *scene, AllStarGame *game,
                         const AllStarInput *input, float dt) {
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    AllStarInput repeated = *input;
    if (data->phase == HORSE_PHASE_RESULT &&
        ((input->buttons_pressed & ALLSTAR_BTN_A) != 0 ||
         (input->buttons_pressed & ALLSTAR_BTN_START) != 0)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);
        return;
    }
    data->fixed_accumulator += dt;
    data->anim_time += dt;
    while (data->fixed_accumulator + 0.000001f >=
           ALLSTAR_PHYSICS_STEP_SECONDS) {
        horse_tick_fixed(data, game, repeated.buttons_held,
                         repeated.buttons_pressed);
        data->fixed_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        repeated.buttons_pressed = 0;
        repeated.buttons_released = 0;
    }
}

static void horse_draw_marker_7b7a(AllStarRenderer *renderer,
                                   const SceneHorseData *data) {
    const AllStarAssetPack *pack = renderer ? renderer->asset_pack : NULL;
    int left;
    int top;
    int y;
    int x;
    if (!data->marker_visible || ((data->phase_frames / 8u) & 1u) == 0)
        return;
    left = (int)data->mode.saved_x - 8;
    top = (int)data->mode.saved_y - 16;
    if (pack && (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART) != 0) {
        /* Live VRAM proves tile $76 is source index 41 in the 42-tile
           $62A6 stream; tile $24 is the alternating blank frame. */
        const AllStarTile *tile = &pack->ball_source_tiles[41];
        for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) {
            uint8_t shade = tile->pixels[y * 8 + x];
            if (shade != 0)
                allstar_renderer_set_pixel(renderer, left + x, top + y,
                                            shade);
        }
    } else {
        allstar_renderer_draw_line(renderer, left, top, left + 7, top + 7, 3);
        allstar_renderer_draw_line(renderer, left + 7, top, left, top + 7, 3);
    }
}

static void horse_draw_result(AllStarRenderer *renderer,
                              AllStarGame *game,
                              const SceneHorseData *data) {
    const AllStarPlayerStats *winner = allstar_roster_get_player(
        &game->roster, data->mode.winner == 1
            ? game->selected_player_1 : game->selected_player_2);
    const char *name = winner ? winner->last_name : "PLAYER";
    char message[32];
    int x;
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_rect_fill(renderer, 8, 20, 144, 100, 1);
    allstar_renderer_draw_rect_outline(renderer, 8, 20, 144, 100, 3);
    allstar_renderer_draw_text(renderer, "H-O-R-S-E", 44, 32, 3);
    snprintf(message, sizeof(message), "%s WINS", name);
    x = (160 - (int)strlen(message) * 8) / 2;
    if (x < 8) x = 8;
    allstar_renderer_draw_text(renderer, message, x, 64, 3);
    allstar_renderer_draw_text(renderer, "A/START CONTINUE", 16, 96, 2);
}

static void horse_draw(AllStarScene *scene, AllStarGame *game,
                       AllStarRenderer *renderer) {
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    const AllStarPlayerStats *stats;
    uint8_t skin;
    bool loose_ball;
    bool ball_behind_net;
    uint8_t net_frame = 0;
    int32_t visual_lift = 0;
    if (data->phase == HORSE_PHASE_RESULT) {
        horse_draw_result(renderer, game, data);
        return;
    }
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_court(renderer);
    if (data->shot_made) {
        uint16_t after_make = data->shot_frames >= data->made_frame
            ? (uint16_t)(data->shot_frames - data->made_frame) : 0;
        net_frame = (uint8_t)allstar_one_on_one_score_net_frame_1ecc(
            after_make);
    }
    loose_ball = data->phase == HORSE_PHASE_FLIGHT;
    ball_behind_net = loose_ball && data->shot_made && net_frame != 0;
    if (ball_behind_net)
        allstar_renderer_draw_ball_ex(renderer,
            (int32_t)data->ball.x, (int32_t)data->ball.y,
            (int32_t)data->ball.z, data->anim_time);
    allstar_renderer_draw_net_overlay_1ecc(renderer, net_frame);
    /* $7BA8 calls $06C0 at tile coordinates (1,1) and (14,1). */
    allstar_renderer_draw_text(renderer,
        allstar_horse_letters_7bc0(data->mode.letters_remaining[0]),
        8, 8, 3);
    allstar_renderer_draw_text(renderer,
        allstar_horse_letters_7bc0(data->mode.letters_remaining[1]),
        112, 8, 3);
    horse_draw_marker_7b7a(renderer, data);

    stats = allstar_roster_get_player(&game->roster,
        data->mode.current_player == 1
            ? game->selected_player_1 : game->selected_player_2);
    skin = stats ? stats->skin_tone :
        (data->mode.current_player == 1 ? 0x90 : 0x91);
    if (data->shot_animation_clock > 0.0f) {
        uint16_t elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
             data->shot_animation_clock) * 60.0f);
        visual_lift = (int32_t)
            allstar_one_on_one_rom_shot_jump_height_6c4d(elapsed);
    }
    allstar_renderer_draw_player_lifted_ex(renderer,
        (int32_t)data->player.x, (int32_t)data->player.y, visual_lift,
        data->mode.current_player == 1, skin, data->player.has_ball,
        data->player.is_shooting, false,
        data->player.is_shooting ? data->shot_action : data->animation.action,
        data->player.anim_frame, data->animation.record_index,
        data->anim_time, data->horizontal_flip);
    if (loose_ball && !ball_behind_net)
        allstar_renderer_draw_ball_ex(renderer,
            (int32_t)data->ball.x, (int32_t)data->ball.y,
            (int32_t)data->ball.z, data->anim_time);
}

static void horse_destroy(AllStarScene *scene) {
    if (scene) {
        free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_horse(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_HORSE;
    scene->user_data = calloc(1, sizeof(SceneHorseData));
    if (!scene->user_data) { free(scene); return NULL; }
    scene->init = horse_init;
    scene->update = horse_update;
    scene->draw = horse_draw;
    scene->destroy = horse_destroy;
    return scene;
}

bool allstar_scene_horse_get_debug_state(const AllStarScene *scene,
                                         AllStarHorseDebugState *debug) {
    const SceneHorseData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_HORSE || !scene->user_data ||
        !debug) return false;
    data = (const SceneHorseData*)scene->user_data;
    memset(debug, 0, sizeof(*debug));
    debug->phase = (uint8_t)data->phase;
    debug->current_player = data->mode.current_player;
    debug->caller = data->mode.caller;
    debug->p1_letters_remaining = data->mode.letters_remaining[0];
    debug->p2_letters_remaining = data->mode.letters_remaining[1];
    debug->saved_x = data->mode.saved_x;
    debug->saved_y = data->mode.saved_y;
    debug->player_x = (uint8_t)data->player.x;
    debug->player_y = (uint8_t)data->player.y;
    debug->shot_frames = data->shot_frames;
    debug->last_events = data->last_events;
    debug->marker_visible = data->marker_visible;
    debug->ball_in_flight = data->ball.in_flight;
    debug->shot_made = data->shot_made;
    debug->complete = data->mode.complete;
    return true;
}

bool allstar_scene_horse_force_test_result(AllStarScene *scene,
                                           bool made) {
    SceneHorseData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_HORSE || !scene->user_data)
        return false;
    data = (SceneHorseData*)scene->user_data;
    data->shot_made = made;
    data->made_frame = made ? data->shot_frames : 0;
    data->shot_frames = HORSE_SHOT_RESOLVE_FRAMES - 1;
    data->phase = HORSE_PHASE_FLIGHT;
    return true;
}
