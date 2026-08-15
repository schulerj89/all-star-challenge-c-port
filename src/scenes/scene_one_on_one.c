#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

typedef struct {
    AllStarPlayerState p1;
    AllStarPlayerState p2;
    AllStarBall ball;
    AllStarAIController ai;
    AllStarOneOnOneShotAttempt shot_attempt;
    AllStarOneOnOneRecoveryState recovery;
    float p1_shot_animation_clock;
    float p2_shot_animation_clock;
    uint8_t p1_shot_action;
    uint8_t p2_shot_action;
    float anim_timer;
} SceneOneOnOneData;

static void one_on_one_reset_possession(SceneOneOnOneData *data,
                                        AllStarGame *game,
                                        bool p1_possession) {
    allstar_one_on_one_match_take_possession(
        &game->one_on_one, p1_possession ? 1 : 2, true);
    data->p1.x = 80.0f;
    data->p2.x = 80.0f;
    data->p1.y = p1_possession ? 130.0f : 105.0f;
    data->p2.y = p1_possession ? 105.0f : 130.0f;
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
    allstar_one_on_one_shot_reset(&data->shot_attempt);
    allstar_physics_init_ball(&data->ball);
}

/* ROM bank 1 $7C58/$7EA9/$7F37: shot setup, vector scaling, and release origin. */
static void one_on_one_launch_shot(SceneOneOnOneData *data,
                                   AllStarGame *game,
                                   int shooter) {
    AllStarPlayerState *player = shooter == 1 ? &data->p1 : &data->p2;
    uint32_t roster_index = shooter == 1
        ? game->selected_player_1 : game->selected_player_2;
    const AllStarPlayerStats *stats = allstar_roster_get_player(
        &game->roster, roster_index);
    float dist = sqrtf(
        (player->x - ALLSTAR_ONE_ON_ONE_HOOP_X) *
            (player->x - ALLSTAR_ONE_ON_ONE_HOOP_X) +
        (player->y - ALLSTAR_ONE_ON_ONE_HOOP_Y) *
            (player->y - ALLSTAR_ONE_ON_ONE_HOOP_Y));
    int point_value = dist > 45.0f ? 3 : 2;
    int rating = point_value == 3
        ? (stats ? stats->shooting_3pt : 75)
        : (stats ? stats->shooting_2pt : 85);
    float target_offset = 0.0f;
    float release_x = player->x;
    float release_y = player->y;
    float release_z = ALLSTAR_BALL_RELEASE_HEIGHT;
    uint8_t shot_variant = allstar_one_on_one_rom_shot_variant(
        player->x, player->y);
    uint8_t shot_action = shot_variant == 1
        ? ALLSTAR_ROM_SHOT_ACTION_A : ALLSTAR_ROM_SHOT_ACTION_B;
    uint8_t shot_phase = shooter == 1
        ? data->shot_attempt.rom_phase : 2;
    AllStarOneOnOneReleaseOffset release_offset;

    if (rand() % 100 >= rating) {
        int miss = 8 + rand() % 9;
        target_offset = (rand() & 1) ? (float)miss : (float)-miss;
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
            (int)player->y - 40, (int)player->y, shot_phase,
            release_offset.height_offset);
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
    allstar_physics_shoot_ball_from_height(
        &data->ball, release_x, release_y, release_z,
        ALLSTAR_ONE_ON_ONE_HOOP_X + target_offset,
        ALLSTAR_ONE_ON_ONE_HOOP_Y, ALLSTAR_HOOP_HEIGHT,
        shooter, point_value);
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

    allstar_audio_stop_bgm(&game->audio);
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
}

/* ROM: $0B80 match loop, $0C00 score ending, $0FDE clock ending. */
static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    uint32_t events;
    int recovering_player;
    data->anim_timer += dt;

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

    one_on_one_update_shot_animations(data, dt);

    events = allstar_one_on_one_shot_tick(&data->shot_attempt, dt);
    if (events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) {
        one_on_one_launch_shot(data, game, 1);
        allstar_one_on_one_rom_shot_animation_frame(
            data->p1_shot_action, data->shot_attempt.rom_phase, 0,
            &data->p1.anim_frame);
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
        float speed = 75.0f;
        bool moved = false;
        if (data->shot_attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER) {
            if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT))  { data->p1.x -= speed * dt; moved = true; }
            if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) { data->p1.x += speed * dt; moved = true; }
            if (allstar_input_is_held(input, ALLSTAR_BTN_UP))    { data->p1.y -= speed * dt; moved = true; }
            if (allstar_input_is_held(input, ALLSTAR_BTN_DOWN))  { data->p1.y += speed * dt; moved = true; }
        }
        if (moved && data->p1.has_ball && (int)(data->anim_timer * 4.0f) % 2 == 0) {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
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
    }

    allstar_ai_update(&data->ai, &data->p2, &data->p1, &data->ball, dt);
    if (data->p2.has_ball && data->p2.is_shooting && !data->ball.in_flight) {
        one_on_one_launch_shot(data, game, 2);
    }

    allstar_physics_update_ball(&data->ball, dt);
    if (allstar_physics_check_basket(&data->ball,
                                     ALLSTAR_ONE_ON_ONE_HOOP_X,
                                     ALLSTAR_ONE_ON_ONE_HOOP_Y,
                                     ALLSTAR_HOOP_HEIGHT)) {
        int shooter = data->ball.shooter_id;
        int points = data->ball.point_value;
        events = allstar_one_on_one_match_add_score(&game->one_on_one, shooter, points);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
        if (events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) {
            one_on_one_handle_lifecycle_events(data, game, events);
        } else {
            int next_possession = allstar_one_on_one_next_possession_after_score(
                &game->one_on_one, shooter);
            one_on_one_reset_possession(data, game, next_possession == 1);
        }
        return;
    }

    allstar_physics_apply_rom_court_contacts(&data->ball);

    recovering_player = allstar_one_on_one_rom_recovery_dispatch(
        &data->recovery,
        data->p1.has_ball || data->p2.has_ball,
        false,
        data->ball.z,
        false,
        false,
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
            bool reset_shot_clock = !game->one_on_one.p1_possession;
            data->p1.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.is_shooting = false;
            allstar_one_on_one_match_take_possession(
                &game->one_on_one, 1, reset_shot_clock);
            allstar_physics_init_ball(&data->ball);
    } else if (recovering_player == 2) {
            bool reset_shot_clock = game->one_on_one.p1_possession;
            data->p2.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.is_shooting = false;
            allstar_one_on_one_match_take_possession(
                &game->one_on_one, 2, reset_shot_clock);
            allstar_physics_init_ball(&data->ball);
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
    bool p1_facing_left;
    bool p2_facing_left;

    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_RESULT) {
        one_on_one_draw_result(game, renderer);
        return;
    }
    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_OVERTIME) {
        one_on_one_draw_overtime(renderer);
        return;
    }

    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_court(renderer);
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

    p1_skin = s1 ? s1->skin_tone : 0x90;
    p2_skin = s2 ? s2->skin_tone : 0x91;
    p1_facing_left = data->p1.x > data->p2.x;
    p2_facing_left = data->p2.x > data->p1.x;
    allstar_renderer_draw_player_ex(renderer, (int32_t)data->p2.x, (int32_t)data->p2.y,
        false, p2_skin, data->p2.has_ball, data->p2.is_shooting,
        !data->p2.has_ball && data->p1.has_ball, data->anim_timer, p2_facing_left);
    allstar_renderer_draw_player_ex(renderer, (int32_t)data->p1.x, (int32_t)data->p1.y,
        true, p1_skin, data->p1.has_ball, data->p1.is_shooting,
        !data->p1.has_ball && data->p2.has_ball, data->anim_timer, p1_facing_left);
    if (data->ball.in_flight || (!data->p1.has_ball && !data->p2.has_ball)) {
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
