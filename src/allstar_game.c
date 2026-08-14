#include "allstar_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool allstar_game_init(AllStarGame *game, const char *asset_pack_path) {
    if (!game) return false;
    memset(game, 0, sizeof(AllStarGame));

    /* Initialize Renderer */
    game->renderer = (AllStarRenderer*)malloc(sizeof(AllStarRenderer));
    if (!allstar_renderer_init(game->renderer, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT)) {
        fprintf(stderr, "[Game] Failed to initialize renderer\n");
        return false;
    }

    /* Initialize Asset Pack */
    game->asset_pack = (AllStarAssetPack*)malloc(sizeof(AllStarAssetPack));
    if (asset_pack_path) {
        allstar_asset_pack_load_file(game->asset_pack, asset_pack_path);
    } else {
        allstar_asset_pack_init_default(game->asset_pack);
    }

    /* Initialize Roster */
    allstar_roster_load_from_asset_pack(&game->roster, game->asset_pack);

    /* Initialize Audio & Controls */
    allstar_audio_init(&game->audio);
    allstar_input_init(&game->input);

    game->is_running = true;
    game->selected_player_1 = 0;
    game->selected_player_2 = 1;
    game->selected_mode = 0;

    /* Start in Intro Scene */
    allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);

    return true;
}

void allstar_game_shutdown(AllStarGame *game) {
    if (!game) return;

    if (game->active_scene) {
        if (game->active_scene->destroy) game->active_scene->destroy(game->active_scene);
        game->active_scene = NULL;
    }

    if (game->renderer) {
        allstar_renderer_free(game->renderer);
        free(game->renderer);
        game->renderer = NULL;
    }

    if (game->asset_pack) {
        free(game->asset_pack);
        game->asset_pack = NULL;
    }

    game->is_running = false;
}

void allstar_game_change_scene(AllStarGame *game, AllStarSceneId scene_id) {
    if (!game) return;

    if (game->active_scene) {
        if (game->active_scene->destroy) game->active_scene->destroy(game->active_scene);
        game->active_scene = NULL;
    }

    switch (scene_id) {
        case ALLSTAR_SCENE_INTRO:
            game->active_scene = allstar_scene_create_intro();
            break;
        case ALLSTAR_SCENE_MENU:
            game->active_scene = allstar_scene_create_menu();
            break;
        case ALLSTAR_SCENE_SETTINGS:
            game->active_scene = allstar_scene_create_settings();
            break;
        case ALLSTAR_SCENE_ROSTER_SELECT:
            game->active_scene = allstar_scene_create_roster_select();
            break;
        case ALLSTAR_SCENE_ONE_ON_ONE:
            game->active_scene = allstar_scene_create_one_on_one();
            break;
        case ALLSTAR_SCENE_THREE_POINT:
            game->active_scene = allstar_scene_create_three_point();
            break;
        case ALLSTAR_SCENE_FREE_THROW:
            game->active_scene = allstar_scene_create_free_throw();
            break;
        case ALLSTAR_SCENE_HORSE:
            game->active_scene = allstar_scene_create_horse();
            break;
        case ALLSTAR_SCENE_TOURNAMENT:
            game->active_scene = allstar_scene_create_tournament();
            break;
        default:
            game->active_scene = allstar_scene_create_intro();
            break;
    }

    if (game->active_scene && game->active_scene->init) {
        game->active_scene->init(game->active_scene, game);
    }
}

void allstar_game_tick(AllStarGame *game, float dt) {
    if (!game || !game->is_running) return;

    if (game->active_scene && game->active_scene->update) {
        game->active_scene->update(game->active_scene, game, &game->input, dt);
    }

    allstar_audio_update(&game->audio, dt);

    if (game->active_scene && game->active_scene->draw && game->renderer) {
        game->active_scene->draw(game->active_scene, game, game->renderer);
    }

    if (game->renderer) {
        allstar_renderer_present(game->renderer);
    }
}
