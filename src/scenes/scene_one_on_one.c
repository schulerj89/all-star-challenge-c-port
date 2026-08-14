#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include <stdlib.h>
#include <stdio.h>
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
    data->p1.y = 90.0f;
    data->p1.has_ball = true;
    data->p1.is_shooting = false;

    data->p2.x = 80.0f;
    data->p2.y = 55.0f;
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
}

static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    data->anim_timer += dt;
    data->game_timer -= dt;
    data->shot_clock -= dt;

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
    if (data->p1.x < 16.0f) data->p1.x = 16.0f;
    if (data->p1.x > 144.0f) data->p1.x = 144.0f;
    if (data->p1.y < 34.0f) data->p1.y = 34.0f;
    if (data->p1.y > 132.0f) data->p1.y = 132.0f;

    /* Shooting mechanics */
    if (data->p1.has_ball && allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        data->p1.has_ball = false;
        data->p1.is_shooting = true;

        float dist = sqrtf((data->p1.x - 80.0f) * (data->p1.x - 80.0f) + (data->p1.y - 26.0f) * (data->p1.y - 26.0f));
        int pt_val = (dist > 52.0f) ? 3 : 2;

        const AllStarPlayerStats *p1_stats = allstar_roster_get_player(&game->roster, game->selected_player_1);
        int rating = (pt_val == 3) ? (p1_stats ? p1_stats->shooting_3pt : 75) : (p1_stats ? p1_stats->shooting_2pt : 85);

        /* Accuracy jitter: high rating shoots straight to basket */
        float target_offset = ((float)(rand() % 100) > (float)rating) ? ((float)(rand() % 14) - 7.0f) : 0.0f;
        allstar_physics_shoot_ball(&data->ball, data->p1.x, data->p1.y, 80.0f + target_offset, 26.0f, 48.0f, 1, pt_val);
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

    if (allstar_physics_check_basket(&data->ball, 80.0f, 26.0f, 16.0f)) {
        data->ball.made_basket = true;
        if (data->ball.shooter_id == 1) {
            data->p1_score += data->ball.point_value;
            /* Switch possession to P2 */
            data->p2.x = 80.0f;
            data->p2.y = 95.0f;
            data->p2.has_ball = true;
            data->p2.is_shooting = false;
            data->p1.x = 80.0f;
            data->p1.y = 65.0f;
            data->p1.has_ball = false;
            data->p1.is_shooting = false;
        } else {
            data->p2_score += data->ball.point_value;
            /* Switch possession to P1 */
            data->p1.x = 80.0f;
            data->p1.y = 95.0f;
            data->p1.has_ball = true;
            data->p1.is_shooting = false;
            data->p2.x = 80.0f;
            data->p2.y = 65.0f;
            data->p2.has_ball = false;
            data->p2.is_shooting = false;
        }
        allstar_physics_init_ball(&data->ball);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
    }
}

static void one_on_one_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Top Scoreboard Bar */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    char hud_buf[32];
    snprintf(hud_buf, sizeof(hud_buf), "1P %02d  %02d:%02d  %02d 2P",
             data->p1_score, (int)data->game_timer / 60, (int)data->game_timer % 60, data->p2_score);
    allstar_renderer_draw_text(renderer, hud_buf, 10, 4, 0);

    /* Half-Court Floor (Light Shade) */
    allstar_renderer_draw_rect_fill(renderer, 10, 20, 140, 118, 0);

    /* Court Perimeter Outline */
    allstar_renderer_draw_rect_outline(renderer, 10, 20, 140, 118, 2);
    allstar_renderer_draw_line(renderer, 10, 137, 150, 137, 3); /* Half court line */

    /* The Key / Paint */
    allstar_renderer_draw_rect_fill(renderer, 60, 20, 40, 45, 1);
    allstar_renderer_draw_rect_outline(renderer, 60, 20, 40, 45, 2);

    /* Free Throw Circle */
    for (int deg = 0; deg < 180; deg += 10) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 20.0f);
        int cy = 65 + (int)(sinf(rad) * 12.0f);
        allstar_renderer_set_pixel(renderer, cx, cy, 2);
    }

    /* 3-Point Arc */
    for (int deg = 0; deg < 180; deg += 4) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 55.0f);
        int cy = 20 + (int)(sinf(rad) * 50.0f);
        if (cx >= 12 && cx <= 148 && cy <= 136) {
            allstar_renderer_set_pixel(renderer, cx, cy, 2);
        }
    }

    /* Backboard, Rim & Net */
    allstar_renderer_draw_line(renderer, 70, 22, 90, 22, 3); /* Backboard */
    allstar_renderer_draw_line(renderer, 76, 26, 84, 26, 3); /* Rim */
    allstar_renderer_draw_line(renderer, 77, 27, 83, 30, 2); /* Net */
    allstar_renderer_draw_line(renderer, 83, 27, 77, 30, 2);

    /* Draw CPU Player (P2) */
    allstar_renderer_draw_player(renderer, (int32_t)data->p2.x, (int32_t)data->p2.y, false, data->p2.has_ball, data->p2.is_shooting, data->anim_timer);

    /* Draw Human Player (P1) */
    allstar_renderer_draw_player(renderer, (int32_t)data->p1.x, (int32_t)data->p1.y, true, data->p1.has_ball, data->p1.is_shooting, data->anim_timer);

    /* Draw Ball */
    if (data->ball.in_flight) {
        allstar_renderer_draw_ball(renderer, (int32_t)data->ball.x, (int32_t)data->ball.y, (int32_t)data->ball.z);
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
