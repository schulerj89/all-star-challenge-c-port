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
    float anim_timer;
} SceneOneOnOneData;

static void one_on_one_reset_possession(SceneOneOnOneData *data,
                                        AllStarGame *game,
                                        bool p1_possession) {
    game->one_on_one.p1_possession = p1_possession;
    data->p1.x = 80.0f;
    data->p2.x = 80.0f;
    data->p1.y = p1_possession ? 130.0f : 105.0f;
    data->p2.y = p1_possession ? 105.0f : 130.0f;
    data->p1.has_ball = p1_possession;
    data->p2.has_ball = !p1_possession;
    data->p1.is_shooting = false;
    data->p2.is_shooting = false;
    allstar_physics_init_ball(&data->ball);
    allstar_one_on_one_match_reset_shot_clock(&game->one_on_one);
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
        allstar_physics_init_ball(&data->ball);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_BUZZER);
    }
    if (events & ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME) {
        const AllStarPlayerStats *cpu_stats = allstar_roster_get_player(
            &game->roster, game->selected_player_2);
        one_on_one_reset_possession(data, game, true);
        allstar_ai_init(&data->ai, cpu_stats);
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
                                  game->one_on_one_time_seconds,
                                  game->one_on_one_shot_clock_seconds,
                                  game->one_on_one_play_to);
    allstar_physics_init_ball(&data->ball);
    one_on_one_reset_possession(data, game, true);

    cpu_stats = allstar_roster_get_player(&game->roster, game->selected_player_2);
    allstar_ai_init(&data->ai, cpu_stats);

    allstar_audio_stop_bgm(&game->audio);
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_WHISTLE);
}

/* ROM: $0B80 match loop, $0C00 score ending, $0FDE clock ending. */
static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    uint32_t events;
    data->anim_timer += dt;

    events = allstar_one_on_one_match_tick(&game->one_on_one, dt);
    if (one_on_one_handle_lifecycle_events(data, game, events)) return;

    if (game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_RESULT) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_B) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
            events = allstar_one_on_one_match_dismiss_result(&game->one_on_one);
            one_on_one_handle_lifecycle_events(data, game, events);
        }
        return;
    }

    if (game->one_on_one.phase != ALLSTAR_ONE_ON_ONE_PLAYING) return;

    {
        float speed = 75.0f;
        bool moved = false;
        if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT))  { data->p1.x -= speed * dt; moved = true; }
        if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) { data->p1.x += speed * dt; moved = true; }
        if (allstar_input_is_held(input, ALLSTAR_BTN_UP))    { data->p1.y -= speed * dt; moved = true; }
        if (allstar_input_is_held(input, ALLSTAR_BTN_DOWN))  { data->p1.y += speed * dt; moved = true; }
        if (moved && data->p1.has_ball && (int)(data->anim_timer * 4.0f) % 2 == 0) {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
        }
    }

    if (data->p1.x < 18.0f) data->p1.x = 18.0f;
    if (data->p1.x > 142.0f) data->p1.x = 142.0f;
    if (data->p1.y < 88.0f) data->p1.y = 88.0f;
    if (data->p1.y > 136.0f) data->p1.y = 136.0f;

    if (data->p1.has_ball && allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        float dist;
        int pt_val;
        int rating;
        float target_offset;
        const AllStarPlayerStats *p1_stats;
        data->p1.has_ball = false;
        data->p1.is_shooting = true;
        dist = sqrtf((data->p1.x - 80.0f) * (data->p1.x - 80.0f) +
                     (data->p1.y - 82.0f) * (data->p1.y - 82.0f));
        pt_val = (dist > 45.0f) ? 3 : 2;
        p1_stats = allstar_roster_get_player(&game->roster, game->selected_player_1);
        rating = (pt_val == 3) ? (p1_stats ? p1_stats->shooting_3pt : 75)
                               : (p1_stats ? p1_stats->shooting_2pt : 85);
        target_offset = ((float)(rand() % 100) > (float)rating)
            ? ((float)(rand() % 12) - 6.0f) : 0.0f;
        allstar_physics_shoot_ball(&data->ball, data->p1.x, data->p1.y,
                                   80.0f + target_offset, 82.0f, 96.0f, 1, pt_val);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
    }

    allstar_ai_update(&data->ai, &data->p2, &data->p1, &data->ball, dt);
    if (data->p2.has_ball && data->p2.is_shooting && !data->ball.in_flight) {
        float dist;
        int pt_val;
        int rating;
        float target_offset;
        const AllStarPlayerStats *p2_stats;
        data->p2.has_ball = false;
        dist = sqrtf((data->p2.x - 80.0f) * (data->p2.x - 80.0f) +
                     (data->p2.y - 82.0f) * (data->p2.y - 82.0f));
        pt_val = (dist > 45.0f) ? 3 : 2;
        p2_stats = allstar_roster_get_player(&game->roster, game->selected_player_2);
        rating = (pt_val == 3) ? (p2_stats ? p2_stats->shooting_3pt : 75)
                               : (p2_stats ? p2_stats->shooting_2pt : 85);
        target_offset = ((float)(rand() % 100) > (float)rating)
            ? ((float)(rand() % 12) - 6.0f) : 0.0f;
        allstar_physics_shoot_ball(&data->ball, data->p2.x, data->p2.y,
                                   80.0f + target_offset, 82.0f, 96.0f, 2, pt_val);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
    }

    allstar_physics_update_ball(&data->ball, dt);
    if (!data->ball.in_flight && !data->p1.has_ball && !data->p2.has_ball) {
        float d1 = sqrtf((data->p1.x - data->ball.x) * (data->p1.x - data->ball.x) +
                         (data->p1.y - data->ball.y) * (data->p1.y - data->ball.y));
        float d2 = sqrtf((data->p2.x - data->ball.x) * (data->p2.x - data->ball.x) +
                         (data->p2.y - data->ball.y) * (data->p2.y - data->ball.y));
        if (d1 < 12.0f) {
            data->p1.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.is_shooting = false;
            game->one_on_one.p1_possession = true;
            allstar_physics_init_ball(&data->ball);
        } else if (d2 < 12.0f) {
            data->p2.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.is_shooting = false;
            game->one_on_one.p1_possession = false;
            allstar_physics_init_ball(&data->ball);
        }
    }

    if (allstar_physics_check_basket(&data->ball, 80.0f, 82.0f, 112.0f)) {
        int shooter = data->ball.shooter_id;
        int points = data->ball.point_value;
        data->ball.made_basket = true;
        events = allstar_one_on_one_match_add_score(&game->one_on_one, shooter, points);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
        if (events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) {
            one_on_one_handle_lifecycle_events(data, game, events);
        } else {
            one_on_one_reset_possession(data, game, shooter != 1);
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
    snprintf(score, sizeof(score), "%03d", game->one_on_one.p1_score);
    allstar_renderer_draw_text(renderer, score, 112, 48, 3);
    allstar_renderer_draw_text(renderer, p2_name, 24, 72, 3);
    snprintf(score, sizeof(score), "%03d", game->one_on_one.p2_score);
    allstar_renderer_draw_text(renderer, score, 112, 72, 3);
    if (game->one_on_one.winner == 0) {
        allstar_renderer_draw_text(renderer, "OVERTIME", 48, 104, 3);
    } else {
        allstar_renderer_draw_text(renderer, "A/B TO SKIP", 32, 112, 3);
    }
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

    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_court(renderer);
    s1 = allstar_roster_get_player(&game->roster, game->selected_player_1);
    s2 = allstar_roster_get_player(&game->roster, game->selected_player_2);

    snprintf(shot_buf, sizeof(shot_buf), "00:%02d", (int)game->one_on_one.shot_clock);
    allstar_renderer_draw_text(renderer, shot_buf, 8, 8, 3);
    snprintf(s1_buf, sizeof(s1_buf), "%03d", game->one_on_one.p1_score);
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
    snprintf(s2_buf, sizeof(s2_buf), "%03d", game->one_on_one.p2_score);
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
