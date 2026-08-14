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
    int p1_score;
    int p2_score;
    float game_timer;
    float shot_clock;
    float anim_timer;
} SceneOneOnOneData;

static void one_on_one_init(AllStarScene *scene, AllStarGame *game) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    data->p1.x = 80.0f;
    data->p1.y = 130.0f;
    data->p1.has_ball = true;
    data->p1.is_shooting = false;

    data->p2.x = 80.0f;
    data->p2.y = 105.0f;
    data->p2.has_ball = false;
    data->p2.is_shooting = false;

    allstar_physics_init_ball(&data->ball);
    const AllStarPlayerStats *cpu_stats = allstar_roster_get_player(&game->roster, game->selected_player_2);
    allstar_ai_init(&data->ai, cpu_stats);

    data->p1_score = 0;
    data->p2_score = 0;
    data->game_timer = 120.0f;
    data->shot_clock = 24.0f;
    data->anim_timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_GAMEPLAY);
}

static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    data->anim_timer += dt;
    data->game_timer -= dt;
    data->shot_clock -= dt;
    if (data->shot_clock <= 0.0f) data->shot_clock = 24.0f;
    if (data->game_timer <= 0.0f) data->game_timer = 0.0f;

    /* Human Player (P1) Movement */
    float speed = 75.0f;
    bool moved = false;
    if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT))  { data->p1.x -= speed * dt; moved = true; }
    if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) { data->p1.x += speed * dt; moved = true; }
    if (allstar_input_is_held(input, ALLSTAR_BTN_UP))    { data->p1.y -= speed * dt; moved = true; }
    if (allstar_input_is_held(input, ALLSTAR_BTN_DOWN))  { data->p1.y += speed * dt; moved = true; }

    if (moved && data->p1.has_ball && (int)(data->anim_timer * 4.0f) % 2 == 0) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
    }

    /* Court Bounds Clamping (160x144 half-court) */
    if (data->p1.x < 18.0f) data->p1.x = 18.0f;
    if (data->p1.x > 142.0f) data->p1.x = 142.0f;
    if (data->p1.y < 80.0f) data->p1.y = 80.0f;
    if (data->p1.y > 138.0f) data->p1.y = 138.0f;

    /* Shooting mechanics */
    if (data->p1.has_ball && allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        data->p1.has_ball = false;
        data->p1.is_shooting = true;

        float dist = sqrtf((data->p1.x - 80.0f) * (data->p1.x - 80.0f) + (data->p1.y - 27.0f) * (data->p1.y - 27.0f));
        int pt_val = (dist > 54.0f) ? 3 : 2;

        const AllStarPlayerStats *p1_stats = allstar_roster_get_player(&game->roster, game->selected_player_1);
        int rating = (pt_val == 3) ? (p1_stats ? p1_stats->shooting_3pt : 75) : (p1_stats ? p1_stats->shooting_2pt : 85);

        /* Accuracy jitter: high rating shoots straight to basket */
        float target_offset = ((float)(rand() % 100) > (float)rating) ? ((float)(rand() % 14) - 7.0f) : 0.0f;
        allstar_physics_shoot_ball(&data->ball, data->p1.x, data->p1.y, 80.0f + target_offset, 27.0f, 48.0f, 1, pt_val);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
    }

    /* CPU AI update */
    allstar_ai_update(&data->ai, &data->p2, &data->p1, &data->ball, dt);

    /* Ball physics & collision */
    allstar_physics_update_ball(&data->ball, dt);

    if (!data->ball.in_flight && !data->p1.has_ball && !data->p2.has_ball) {
        /* Loose ball recovery */
        float d1 = sqrtf((data->p1.x - data->ball.x) * (data->p1.x - data->ball.x) + (data->p1.y - data->ball.y) * (data->p1.y - data->ball.y));
        float d2 = sqrtf((data->p2.x - data->ball.x) * (data->p2.x - data->ball.x) + (data->p2.y - data->ball.y) * (data->p2.y - data->ball.y));

        if (d1 < 10.0f) {
            data->p1.has_ball = true;
            data->p1.is_shooting = false;
            allstar_physics_init_ball(&data->ball);
        } else if (d2 < 10.0f) {
            data->p2.has_ball = true;
            data->p2.is_shooting = false;
            allstar_physics_init_ball(&data->ball);
        }
    }

    if (allstar_physics_check_basket(&data->ball, 80.0f, 27.0f, 16.0f)) {
        data->ball.made_basket = true;
        if (data->ball.shooter_id == 1) {
            data->p1_score += data->ball.point_value;
            /* Switch possession to P2 */
            data->p2.x = 80.0f;
            data->p2.y = 130.0f;
            data->p2.has_ball = true;
            data->p2.is_shooting = false;
            data->p1.x = 80.0f;
            data->p1.y = 105.0f;
            data->p1.has_ball = false;
            data->p1.is_shooting = false;
        } else {
            data->p2_score += data->ball.point_value;
            /* Switch possession to P1 */
            data->p1.x = 80.0f;
            data->p1.y = 130.0f;
            data->p1.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.x = 80.0f;
            data->p2.y = 105.0f;
            data->p2.has_ball = false;
            data->p2.is_shooting = false;
        }
        allstar_physics_init_ball(&data->ball);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
    }
}

static void one_on_one_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Render Authentic Basketball Court & Scoreboard */
    allstar_renderer_draw_court(renderer);
    
    const AllStarPlayerStats *s1 = allstar_roster_get_player(&game->roster, game->selected_player_1);
    const AllStarPlayerStats *s2 = allstar_roster_get_player(&game->roster, game->selected_player_2);

    /* Left Shot Clock (10, 8) */
    char shot_buf[16];
    snprintf(shot_buf, sizeof(shot_buf), "00:%02d", (int)data->shot_clock);
    allstar_renderer_draw_text(renderer, shot_buf, 10, 8, 3);

    /* Left Score (18, 24) */
    char s1_buf[16];
    snprintf(s1_buf, sizeof(s1_buf), "%03d", data->p1_score);
    allstar_renderer_draw_text(renderer, s1_buf, 18, 24, 3);

    /* Left Name Plate (0..72, 43) */
    const char *p1_name = s1 ? s1->name : "PLAYER 1";
    const char *p1_space = strchr(p1_name, ' ');
    const char *p1_last = p1_space ? (p1_space + 1) : p1_name;
    int p1_len = (int)strlen(p1_last);
    int p1_x = (72 - p1_len * 8) / 2;
    if (p1_x < 2) p1_x = 2;
    allstar_renderer_draw_text(renderer, p1_last, p1_x, 43, 3);

    /* Right Game Timer (112, 8) */
    char clk_buf[16];
    snprintf(clk_buf, sizeof(clk_buf), "%02d:%02d", (int)data->game_timer / 60, (int)data->game_timer % 60);
    allstar_renderer_draw_text(renderer, clk_buf, 112, 8, 3);

    /* Right Score (122, 24) */
    char s2_buf[16];
    snprintf(s2_buf, sizeof(s2_buf), "%03d", data->p2_score);
    allstar_renderer_draw_text(renderer, s2_buf, 122, 24, 3);

    /* Right Name Plate (88..160, 43) */
    const char *p2_name = s2 ? s2->name : "PLAYER 2";
    const char *p2_space = strchr(p2_name, ' ');
    const char *p2_last = p2_space ? (p2_space + 1) : p2_name;
    int p2_len = (int)strlen(p2_last);
    int p2_x = 88 + (72 - p2_len * 8) / 2;
    if (p2_x < 88) p2_x = 88;
    allstar_renderer_draw_text(renderer, p2_last, p2_x, 43, 3);

    uint8_t p1_skin = s1 ? s1->skin_tone : 0x90;
    uint8_t p2_skin = s2 ? s2->skin_tone : 0x91;

    bool p1_facing_left = (data->p1.x > data->p2.x);
    bool p2_facing_left = (data->p2.x > data->p1.x);

    /* Draw CPU Player (P2) */
    allstar_renderer_draw_player_ex(renderer, (int32_t)data->p2.x, (int32_t)data->p2.y,
                                     false, p2_skin, data->p2.has_ball, data->p2.is_shooting,
                                     !data->p2.has_ball && data->p1.has_ball,
                                     data->anim_timer, p2_facing_left);

    /* Draw Human Player (P1) */
    allstar_renderer_draw_player_ex(renderer, (int32_t)data->p1.x, (int32_t)data->p1.y,
                                     true, p1_skin, data->p1.has_ball, data->p1.is_shooting,
                                     !data->p1.has_ball && data->p2.has_ball,
                                     data->anim_timer, p1_facing_left);

    /* Draw Ball in flight */
    if (data->ball.in_flight) {
        allstar_renderer_draw_ball_ex(renderer, (int32_t)data->ball.x, (int32_t)data->ball.y, (int32_t)data->ball.z, data->anim_timer);
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
