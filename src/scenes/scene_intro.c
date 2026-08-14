#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    float timer;
    int phase;
} SceneIntroData;

static void intro_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer = 0.0f;
    data->phase = 0;
}

static void intro_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer += dt;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_A) ||
        data->timer > 4.0f) {
        allstar_game_change_scene(game, ALLSTAR_SCENE_MENU);
    }
}

static void intro_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)scene;
    (void)game;
    allstar_renderer_clear(renderer, 0);

    /* Draw Authentic Game Boy Intro Presentation */
    allstar_renderer_draw_text(renderer, "BEAM SOFTWARE", 32, 40, 3);
    allstar_renderer_draw_text(renderer, "PRESENTS", 48, 56, 2);
    allstar_renderer_draw_text(renderer, "NBA ALL-STAR", 36, 80, 3);
    allstar_renderer_draw_text(renderer, "CHALLENGE", 44, 96, 3);
    allstar_renderer_draw_text(renderer, "PRESS START", 36, 120, 1);
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
