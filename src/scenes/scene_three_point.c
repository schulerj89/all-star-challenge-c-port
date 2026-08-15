#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define RACK_COUNT 5
#define BALLS_PER_RACK 5

/* ROM $6CA2 chooses computer-generated positions when $FF9A is set. */
static const AllStarVec2 COMPUTER_POSITIONS[RACK_COUNT] = {
    {30.0f, 60.0f},  /* Left corner */
    {45.0f, 90.0f},  /* Left wing */
    {80.0f, 105.0f}, /* Top of the key */
    {115.0f, 90.0f}, /* Right wing */
    {130.0f, 60.0f}  /* Right corner */
};

static const AllStarVec2 NEW_POSITION_TEMPLATE[RACK_COUNT] = {
    {24.0f, 82.0f},
    {52.0f, 112.0f},
    {80.0f, 88.0f},
    {108.0f, 112.0f},
    {136.0f, 82.0f}
};

typedef struct {
    int current_rack;
    int ball_in_rack;
    int score;
    float time_remaining;
    float meter_val;
    float meter_dir;
    bool meter_active;
    bool computer_positions;
    AllStarVec2 positions[RACK_COUNT];
    AllStarBall active_ball;
} SceneThreePointData;

static void three_point_init(AllStarScene *scene, AllStarGame *game) {
    SceneThreePointData *data = (SceneThreePointData*)scene->user_data;
    data->current_rack = 0;
    data->ball_in_rack = 0;
    data->score = 0;
    data->time_remaining = allstar_game_settings_time_seconds(&game->settings);
    data->meter_val = 0.0f;
    data->meter_dir = 1.0f;
    data->meter_active = true;
    data->computer_positions = game->settings.accuracy_computer_positions;
    memcpy(data->positions,
           data->computer_positions ? COMPUTER_POSITIONS : NEW_POSITION_TEMPLATE,
           sizeof(data->positions));
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
            AllStarVec2 rack = data->positions[data->current_rack];
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

    /* Top HUD Bar */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    char buf[32];
    snprintf(buf, sizeof(buf), "ACC   PTS:%02d  TIME:%02d", data->score, (int)data->time_remaining);
    allstar_renderer_draw_text(renderer, buf, 6, 4, 0);

    /* Court Key & 3-Point Arc */
    allstar_renderer_draw_rect_fill(renderer, 64, 18, 32, 40, 1);
    allstar_renderer_draw_rect_outline(renderer, 64, 18, 32, 40, 2);

    /* 3-Point Arc */
    for (int deg = 0; deg < 180; deg += 4) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 55.0f);
        int cy = 20 + (int)(sinf(rad) * 50.0f);
        if (cx >= 12 && cx <= 148 && cy <= 110) {
            allstar_renderer_set_pixel(renderer, cx, cy, 2);
        }
    }

    /* Hoop & Backboard */
    allstar_renderer_draw_line(renderer, 72, 20, 88, 20, 3);
    allstar_renderer_draw_line(renderer, 76, 24, 84, 24, 3);
    allstar_renderer_draw_line(renderer, 78, 25, 82, 28, 2);

    /* 5 Ball Racks */
    for (int r = 0; r < RACK_COUNT; r++) {
        AllStarVec2 pos = data->positions[r];
        int rx = (int)pos.x;
        int ry = (int)pos.y;

        /* Rack Box */
        allstar_renderer_draw_rect_fill(renderer, rx - 6, ry - 3, 12, 6, 1);
        allstar_renderer_draw_rect_outline(renderer, rx - 6, ry - 3, 12, 6, 3);

        /* Balls in Rack */
        int balls_left = (r == data->current_rack) ? (BALLS_PER_RACK - data->ball_in_rack) : (r > data->current_rack ? 5 : 0);
        for (int b = 0; b < balls_left && b < 4; b++) {
            allstar_renderer_set_pixel(renderer, rx - 4 + b * 2, ry - 1, (b == 4 || (r == data->current_rack && data->ball_in_rack == 4)) ? 3 : 2);
        }
    }

    /* Active Shooter */
    if (data->current_rack < RACK_COUNT) {
        AllStarVec2 cur_pos = data->positions[data->current_rack];
        allstar_renderer_draw_player(renderer, (int)cur_pos.x + 8, (int)cur_pos.y + 4, true, data->meter_active, false, 0.0f);
    }

    /* Active Ball in flight */
    if (data->active_ball.in_flight) {
        allstar_renderer_draw_ball(renderer, (int)data->active_ball.x, (int)data->active_ball.y, (int)data->active_ball.z);
    }

    /* Timing Meter Box */
    allstar_renderer_draw_rect_fill(renderer, 0, 116, 160, 28, 1);
    allstar_renderer_draw_line(renderer, 0, 116, 160, 116, 3);
    allstar_renderer_draw_text(renderer, "SHOT TIMING", 8, 120, 3);

    /* Meter Track */
    allstar_renderer_draw_rect_fill(renderer, 10, 130, 140, 8, 0);
    allstar_renderer_draw_rect_outline(renderer, 10, 130, 140, 8, 3);

    /* Sweet Spot (Green Target Zone) */
    allstar_renderer_draw_rect_fill(renderer, 66, 131, 28, 6, 2);

    /* Moving Needle */
    int needle_x = 10 + (int)(data->meter_val * 1.36f);
    allstar_renderer_draw_line(renderer, needle_x, 128, needle_x, 139, 3);
    allstar_renderer_draw_line(renderer, needle_x - 1, 130, needle_x + 1, 130, 3);
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
