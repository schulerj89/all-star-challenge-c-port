#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>

#define RACK_COUNT 5
#define BALLS_PER_RACK 5

static const AllStarVec2 RACK_POSITIONS[RACK_COUNT] = {
    {30.0f, 60.0f},  /* Left corner */
    {45.0f, 90.0f},  /* Left wing */
    {80.0f, 105.0f}, /* Top of the key */
    {115.0f, 90.0f}, /* Right wing */
    {130.0f, 60.0f}  /* Right corner */
};

typedef struct {
    int current_rack;
    int ball_in_rack;
    int score;
    float time_remaining;
    float meter_val;
    float meter_dir;
    bool meter_active;
    AllStarBall active_ball;
} SceneThreePointData;

static void three_point_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneThreePointData *data = (SceneThreePointData*)scene->user_data;
    data->current_rack = 0;
    data->ball_in_rack = 0;
    data->score = 0;
    data->time_remaining = 60.0f;
    data->meter_val = 0.0f;
    data->meter_dir = 1.0f;
    data->meter_active = true;
    allstar_physics_init_ball(&data->active_ball);
}

static void three_point_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneThreePointData *data = (SceneThreePointData*)scene->user_data;
    data->time_remaining -= dt;

    if (data->time_remaining <= 0.0f) {
        data->time_remaining = 0.0f;
        data->meter_active = false;
        return;
    }

    /* Shot gauge oscillation */
    if (data->meter_active) {
        data->meter_val += data->meter_dir * 120.0f * dt;
        if (data->meter_val >= 100.0f) {
            data->meter_val = 100.0f;
            data->meter_dir = -1.0f;
        } else if (data->meter_val <= 0.0f) {
            data->meter_val = 0.0f;
            data->meter_dir = 1.0f;
        }

        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            AllStarVec2 rack = RACK_POSITIONS[data->current_rack];
            int pts = (data->ball_in_rack == 4) ? 2 : 1; /* Money ball = 2 pts */

            allstar_physics_shoot_ball(&data->active_ball, rack.x, rack.y, 80.0f, 24.0f, 50.0f, 1, pts);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);

            /* Timing window accuracy */
            if (data->meter_val >= 40.0f && data->meter_val <= 60.0f) {
                data->score += pts;
                allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
            }

            data->ball_in_rack++;
            if (data->ball_in_rack >= BALLS_PER_RACK) {
                data->ball_in_rack = 0;
                data->current_rack++;
                if (data->current_rack >= RACK_COUNT) {
                    data->meter_active = false;
                }
            }
        }
    }

    allstar_physics_update_ball(&data->active_ball, dt);
}

static void three_point_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneThreePointData *data = (SceneThreePointData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_text(renderer, "3 POINT SHOOTOUT", 16, 4, 3);

    /* Draw 3-point Arc */
    for (int r = 0; r < RACK_COUNT; r++) {
        AllStarVec2 pos = RACK_POSITIONS[r];
        uint8_t shade = (r == data->current_rack) ? 3 : 1;
        allstar_renderer_set_pixel(renderer, (int)pos.x, (int)pos.y, shade);
        allstar_renderer_set_pixel(renderer, (int)pos.x + 1, (int)pos.y, shade);
    }

    /* Draw Shot Meter */
    if (data->meter_active) {
        allstar_renderer_draw_text(renderer, "TIMING", 16, 120, 2);
        for (int x = 0; x < 50; x++) {
            allstar_renderer_set_pixel(renderer, 60 + x, 122, 1);
            allstar_renderer_set_pixel(renderer, 60 + x, 123, 1);
        }
        int cur_x = 60 + (int)(data->meter_val * 0.5f);
        allstar_renderer_set_pixel(renderer, cur_x, 120, 3);
        allstar_renderer_set_pixel(renderer, cur_x, 121, 3);
        allstar_renderer_set_pixel(renderer, cur_x, 122, 3);
        allstar_renderer_set_pixel(renderer, cur_x, 123, 3);
        allstar_renderer_set_pixel(renderer, cur_x, 124, 3);
    }

    /* HUD */
    char buf[32];
    snprintf(buf, sizeof(buf), "PTS:%02d TIME:%02d", data->score, (int)data->time_remaining);
    allstar_renderer_draw_text(renderer, buf, 16, 20, 3);
}

static void three_point_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_three_point(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_THREE_POINT;
    scene->user_data = calloc(1, sizeof(SceneThreePointData));
    scene->init = three_point_init;
    scene->update = three_point_update;
    scene->draw = three_point_draw;
    scene->destroy = three_point_destroy;
    return scene;
}
