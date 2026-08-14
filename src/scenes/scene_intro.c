#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    float timer;
    float ball_y;
    float ball_vy;
} SceneIntroData;

static void intro_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer = 0.0f;
    data->ball_y = 10.0f;
    data->ball_vy = 0.0f;
}

static void intro_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer += dt;

    /* Bouncing ball animation */
    data->ball_vy += 120.0f * dt;
    data->ball_y += data->ball_vy * dt;
    if (data->ball_y >= 100.0f) {
        data->ball_y = 100.0f;
        data->ball_vy = -75.0f;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
    }

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        allstar_game_change_scene(game, ALLSTAR_SCENE_MENU);
    }
}

static void intro_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Decorative Court Lines */
    allstar_renderer_draw_line(renderer, 0, 115, 160, 115, 2);
    allstar_renderer_draw_line(renderer, 0, 117, 160, 117, 1);

    /* Game Boy Header Presentation */
    allstar_renderer_draw_text(renderer, "BEAM SOFTWARE", 28, 12, 2);
    allstar_renderer_draw_text(renderer, "PRESENTS", 48, 24, 1);

    /* Title Box */
    allstar_renderer_draw_rect_fill(renderer, 10, 36, 140, 42, 3);
    allstar_renderer_draw_rect_outline(renderer, 12, 38, 136, 38, 0);
    allstar_renderer_draw_text(renderer, "NBA ALL-STAR", 32, 44, 0);
    allstar_renderer_draw_text(renderer, "CHALLENGE", 44, 58, 0);

    /* Bouncing Ball */
    allstar_renderer_draw_ball(renderer, 80, (int)data->ball_y, 0);

    /* Pulsing Press Start */
    if ((int)(data->timer * 3.0f) % 2 == 0) {
        allstar_renderer_draw_text_box(renderer, "PRESS START", 36, 122, 3, 0, 2);
    }
}

static void intro_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_intro(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_INTRO;
    scene->user_data = calloc(1, sizeof(SceneIntroData));
    scene->init = intro_init;
    scene->update = intro_update;
    scene->draw = intro_draw;
    scene->destroy = intro_destroy;
    return scene;
}
