#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int attempts;
    int makes;
    float gauge_x;
    float gauge_y;
    float gauge_dx;
    float gauge_dy;
    bool shooting;
    AllStarBall ball;
} SceneFreeThrowData;

static void free_throw_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    data->attempts = 0;
    data->makes = 0;
    data->gauge_x = 50.0f;
    data->gauge_y = 50.0f;
    data->gauge_dx = 80.0f;
    data->gauge_dy = 60.0f;
    data->shooting = false;
    allstar_physics_init_ball(&data->ball);
}

static void free_throw_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;

    data->gauge_x += data->gauge_dx * dt;
    if (data->gauge_x >= 100.0f || data->gauge_x <= 0.0f) data->gauge_dx = -data->gauge_dx;

    data->gauge_y += data->gauge_dy * dt;
    if (data->gauge_y >= 100.0f || data->gauge_y <= 0.0f) data->gauge_dy = -data->gauge_dy;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        data->attempts++;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
        if (data->gauge_x >= 40.0f && data->gauge_x <= 60.0f &&
            data->gauge_y >= 40.0f && data->gauge_y <= 60.0f) {
            data->makes++;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
        } else {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
        }
    }
}

static void free_throw_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_text(renderer, "FREE THROW", 44, 8, 3);

    /* Draw Target Reticle Box */
    for (int i = 0; i < 40; i++) {
        allstar_renderer_set_pixel(renderer, 60 + i, 40, 2);
        allstar_renderer_set_pixel(renderer, 60 + i, 80, 2);
        allstar_renderer_set_pixel(renderer, 60, 40 + i, 2);
        allstar_renderer_set_pixel(renderer, 100, 40 + i, 2);
    }

    /* Draw Moving Aim Point */
    int ax = 60 + (int)(data->gauge_x * 0.4f);
    int ay = 40 + (int)(data->gauge_y * 0.4f);
    allstar_renderer_set_pixel(renderer, ax, ay, 3);
    allstar_renderer_set_pixel(renderer, ax + 1, ay, 3);
    allstar_renderer_set_pixel(renderer, ax, ay + 1, 3);
    allstar_renderer_set_pixel(renderer, ax + 1, ay + 1, 3);

    /* HUD */
    char buf[32];
    snprintf(buf, sizeof(buf), "MAKES:%d/%d", data->makes, data->attempts);
    allstar_renderer_draw_text(renderer, buf, 36, 110, 3);
}

static void free_throw_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_free_throw(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_FREE_THROW;
    scene->user_data = calloc(1, sizeof(SceneFreeThrowData));
    scene->init = free_throw_init;
    scene->update = free_throw_update;
    scene->draw = free_throw_draw;
    scene->destroy = free_throw_destroy;
    return scene;
}
