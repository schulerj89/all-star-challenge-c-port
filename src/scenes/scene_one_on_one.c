#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    AllStarPlayerState p1;
    AllStarPlayerState p2;
    AllStarBall ball;
    AllStarAIController ai;
    int p1_score;
    int p2_score;
    float game_timer;
    float shot_clock;
} SceneOneOnOneData;

static void one_on_one_init(AllStarScene *scene, AllStarGame *game) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    data->p1.x = 80.0f;
    data->p1.y = 80.0f;
    data->p1.has_ball = true;

    data->p2.x = 80.0f;
    data->p2.y = 50.0f;
    data->p2.has_ball = false;

    allstar_physics_init_ball(&data->ball);
    const AllStarPlayerStats *cpu_stats = allstar_roster_get_player(&game->roster, game->selected_player_2);
    allstar_ai_init(&data->ai, cpu_stats);

    data->p1_score = 0;
    data->p2_score = 0;
    data->game_timer = 120.0f;
    data->shot_clock = 24.0f;
}

static void one_on_one_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;

    data->game_timer -= dt;
    data->shot_clock -= dt;

    /* Human Player (P1) Movement */
    float speed = 70.0f;
    if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT))  data->p1.x -= speed * dt;
    if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) data->p1.x += speed * dt;
    if (allstar_input_is_held(input, ALLSTAR_BTN_UP))    data->p1.y -= speed * dt;
    if (allstar_input_is_held(input, ALLSTAR_BTN_DOWN))  data->p1.y += speed * dt;

    /* Court Bounds Clamping (160x144 half-court) */
    if (data->p1.x < 16.0f) data->p1.x = 16.0f;
    if (data->p1.x > 144.0f) data->p1.x = 144.0f;
    if (data->p1.y < 30.0f) data->p1.y = 30.0f;
    if (data->p1.y > 130.0f) data->p1.y = 130.0f;

    /* Shooting mechanics */
    if (data->p1.has_ball && allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        data->p1.has_ball = false;
        allstar_physics_shoot_ball(&data->ball, data->p1.x, data->p1.y, 80.0f, 24.0f, 40.0f, 1, 2);
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
    }

    /* CPU AI update */
    allstar_ai_update(&data->ai, &data->p2, &data->p1, &data->ball, dt);

    /* Ball physics */
    allstar_physics_update_ball(&data->ball, dt);
    if (allstar_physics_check_basket(&data->ball, 80.0f, 24.0f, 20.0f)) {
        data->ball.made_basket = true;
        if (data->ball.shooter_id == 1) data->p1_score += data->ball.point_value;
        else data->p2_score += data->ball.point_value;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
    }
}

static void one_on_one_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneOneOnOneData *data = (SceneOneOnOneData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Draw Half-Court Lines */
    for (int x = 20; x <= 140; x++) {
        allstar_renderer_set_pixel(renderer, x, 24, 2);  /* Baseline */
        allstar_renderer_set_pixel(renderer, x, 134, 2); /* Half-court */
    }
    for (int y = 24; y <= 134; y++) {
        allstar_renderer_set_pixel(renderer, 20, y, 2);  /* Left sideline */
        allstar_renderer_set_pixel(renderer, 140, y, 2); /* Right sideline */
    }

    /* Hoop / Rim */
    for (int x = 76; x <= 84; x++) allstar_renderer_set_pixel(renderer, x, 24, 3);

    /* Draw P1 */
    allstar_renderer_draw_text(renderer, "1P", (int32_t)data->p1.x - 6, (int32_t)data->p1.y - 12, 3);
    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            allstar_renderer_set_pixel(renderer, (int32_t)data->p1.x + dx, (int32_t)data->p1.y + dy, 3);
        }
    }

    /* Draw P2 (CPU) */
    allstar_renderer_draw_text(renderer, "2P", (int32_t)data->p2.x - 6, (int32_t)data->p2.y - 12, 2);
    for (int dx = -3; dx <= 3; dx++) {
        for (int dy = -3; dy <= 3; dy++) {
            allstar_renderer_set_pixel(renderer, (int32_t)data->p2.x + dx, (int32_t)data->p2.y + dy, 2);
        }
    }

    /* Draw Ball */
    if (data->ball.in_flight) {
        int bx = (int)data->ball.x;
        int by = (int)(data->ball.y - data->ball.z * 0.5f);
        allstar_renderer_set_pixel(renderer, bx, by, 3);
        allstar_renderer_set_pixel(renderer, bx + 1, by, 3);
        allstar_renderer_set_pixel(renderer, bx, by + 1, 3);
        allstar_renderer_set_pixel(renderer, bx + 1, by + 1, 3);
    }

    /* HUD */
    char hud_buf[32];
    snprintf(hud_buf, sizeof(hud_buf), "%02d - %02d", data->p1_score, data->p2_score);
    allstar_renderer_draw_text(renderer, hud_buf, 56, 4, 3);
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
