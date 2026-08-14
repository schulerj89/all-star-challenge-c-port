#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    float timer;
    int selected_option; /* 0 = 1 PLAYER, 1 = 2 PLAYERS */
} SceneIntroData;

static void intro_init(AllStarScene *scene, AllStarGame *game) {
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer = 0.0f;
    data->selected_option = 0;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_TITLE);
}

static void intro_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer += dt;

    /* Left / Right / Select toggles 1 PLAYER vs 2 PLAYERS */
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_SELECT)) {
        data->selected_option = (data->selected_option == 0) ? 1 : 0;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
    }

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        /* Go to authentic Select Game Menu */
        allstar_game_change_scene(game, ALLSTAR_SCENE_MENU);
    }
}

#include "allstar_title_art.h"

static void intro_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Render Authentic NBA All-Star Challenge Title Screen Extracted from ROM */
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            uint8_t shade = ALLSTAR_TITLE_BITMAP[y * 160 + x];
            allstar_renderer_set_pixel(renderer, x, y, shade);
        }
    }

    /* Basketball Cursor at ROM Table 0x2AC4 Coordinates */
    int cursor_x = (data->selected_option == 0) ? 43 : 109;
    allstar_renderer_draw_cursor(renderer, cursor_x, 124);
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
