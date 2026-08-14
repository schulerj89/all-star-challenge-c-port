#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

#define MENU_ITEM_COUNT 5

static const char *MENU_ITEMS[MENU_ITEM_COUNT] = {
    "1 ON 1 GAME",
    "3-PT SHOOTOUT",
    "FREE THROW",
    "H - O - R - S - E",
    "TOURNAMENT"
};

typedef struct {
    int selected_index;
    float anim_timer;
} SceneMenuData;

static void menu_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    data->selected_index = 0;
    data->anim_timer = 0.0f;
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

static void menu_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneMenuData *data = (SceneMenuData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Header Bar */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 20, 3);
    allstar_renderer_draw_text(renderer, "SELECT MODE", 36, 6, 0);

    for (int i = 0; i < MENU_ITEM_COUNT; i++) {
        int y = 30 + i * 20;
        if (i == data->selected_index) {
            /* Highlight Card */
            allstar_renderer_draw_rect_fill(renderer, 10, y - 2, 140, 16, 2);
            allstar_renderer_draw_rect_outline(renderer, 10, y - 2, 140, 16, 3);
            /* Basketball icon */
            allstar_renderer_draw_ball(renderer, 20, y + 5, 0);
            allstar_renderer_draw_text(renderer, MENU_ITEMS[i], 32, y + 2, 0);
        } else {
            allstar_renderer_draw_rect_fill(renderer, 14, y - 2, 132, 16, 1);
            allstar_renderer_draw_text(renderer, MENU_ITEMS[i], 32, y + 2, 3);
        }
    }

    /* Footer Hint */
    allstar_renderer_draw_text(renderer, "PRESS A TO SELECT", 12, 132, 2);
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
