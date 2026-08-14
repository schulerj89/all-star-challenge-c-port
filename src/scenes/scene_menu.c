#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

#define MENU_ITEM_COUNT 5

static const char *MENU_ITEMS[MENU_ITEM_COUNT] = {
    "1 ON 1",
    "3 POINT SHOOTOUT",
    "FREE THROW",
    "H - O - R - S - E",
    "TOURNAMENT"
};

typedef struct {
    int selected_index;
} SceneMenuData;

static void menu_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    data->selected_index = 0;
}

static void menu_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    (void)dt;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_UP)) {
        data->selected_index = (data->selected_index + MENU_ITEM_COUNT - 1) % MENU_ITEM_COUNT;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
    }
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_DOWN)) {
        data->selected_index = (data->selected_index + 1) % MENU_ITEM_COUNT;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
    }

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) || allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        game->selected_mode = (uint32_t)data->selected_index;
        allstar_game_change_scene(game, ALLSTAR_SCENE_ROSTER_SELECT);
    }
}

static void menu_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_text(renderer, "GAME SELECT", 36, 16, 3);

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = 44 + i * 16;
        if (i == data->selected_index) {
            allstar_renderer_draw_text(renderer, ">", 16, y, 3);
            allstar_renderer_draw_text(renderer, MENU_ITEMS[i], 28, y, 3);
        } else {
            allstar_renderer_draw_text(renderer, MENU_ITEMS[i], 28, y, 1);
        }
    }

    allstar_renderer_draw_text(renderer, "1992 LJN TOYS", 32, 128, 2);
}

static void menu_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_menu(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_MENU;
    scene->user_data = calloc(1, sizeof(SceneMenuData));
    scene->init = menu_init;
    scene->update = menu_update;
    scene->draw = menu_draw;
    scene->destroy = menu_destroy;
    return scene;
}
