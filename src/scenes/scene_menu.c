#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

#define MENU_ITEM_COUNT 5

static const char *MENU_ITEMS[MENU_ITEM_COUNT] = {
    "One On One",
    "Free Throws",
    "Horse",
    "Accuracy Shootout",
    "Tournament"
};

typedef struct {
    int selected_index;
    float anim_timer;
} SceneMenuData;

static void menu_init(AllStarScene *scene, AllStarGame *game) {
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    data->selected_index = 0;
    data->anim_timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_MENU);
}

static void menu_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    data->anim_timer += dt;

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

#include "allstar_menu_art.h"

static void menu_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Render Authentic Select Game Background */
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            uint8_t shade = ALLSTAR_MENU_BACKGROUND[y * 160 + x];
            allstar_renderer_set_pixel(renderer, x, y, shade);
        }
    }

    /* Moving Basketball Cursor at ROM Table 0x2ABA Coordinates */
    static const int OPTION_Y[5] = { 48, 56, 64, 72, 80 };
    int cur_y = OPTION_Y[data->selected_index % 5];
    allstar_renderer_draw_cursor(renderer, 4, cur_y);
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
