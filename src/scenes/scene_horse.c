#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>

static const char HORSE_LETTERS[6] = "HORSE";

typedef struct {
    int p1_letters;
    int p2_letters;
    int current_turn; /* 1 = P1, 2 = P2 */
    bool shot_called;
    float spot_x;
    float spot_y;
} SceneHorseData;

static void horse_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    data->p1_letters = 0;
    data->p2_letters = 0;
    data->current_turn = 1;
    data->shot_called = false;
    data->spot_x = 80.0f;
    data->spot_y = 70.0f;
}

static void horse_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    (void)dt;
    SceneHorseData *data = (SceneHorseData*)scene->user_data;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
        /* Toggle turn */
        data->current_turn = (data->current_turn == 1) ? 2 : 1;
    }
}

static void horse_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_text(renderer, "H-O-R-S-E", 48, 8, 3);

    /* Player 1 Letters */
    char p1_str[16] = "1P: ";
    for (int i = 0; i < data->p1_letters && i < 5; i++) {
        p1_str[4 + i] = HORSE_LETTERS[i];
    }
    p1_str[4 + data->p1_letters] = '\0';
    allstar_renderer_draw_text(renderer, p1_str, 20, 40, 3);

    /* Player 2 Letters */
    char p2_str[16] = "2P: ";
    for (int i = 0; i < data->p2_letters && i < 5; i++) {
        p2_str[4 + i] = HORSE_LETTERS[i];
    }
    p2_str[4 + data->p2_letters] = '\0';
    allstar_renderer_draw_text(renderer, p2_str, 20, 60, 3);

    /* Turn indicator */
    char turn_str[32];
    snprintf(turn_str, sizeof(turn_str), "TURN: %dP SHOOT", data->current_turn);
    allstar_renderer_draw_text(renderer, turn_str, 24, 100, 2);
}

static void horse_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_horse(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_HORSE;
    scene->user_data = calloc(1, sizeof(SceneHorseData));
    scene->init = horse_init;
    scene->update = horse_update;
    scene->draw = horse_draw;
    scene->destroy = horse_destroy;
    return scene;
}
