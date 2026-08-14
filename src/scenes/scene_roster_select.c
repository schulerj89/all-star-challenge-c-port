#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int p1_cursor;
    int p2_cursor;
    bool p1_selected;
    bool p2_selected;
} SceneRosterSelectData;

static void roster_select_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    data->p1_cursor = 0;
    data->p2_cursor = 1;
    data->p1_selected = false;
    data->p2_selected = false;
}

static void roster_select_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    (void)dt;
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    size_t count = game->roster.count ? game->roster.count : 1;

    if (!data->p1_selected) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
            data->p1_cursor = (int)((data->p1_cursor + count - 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
            data->p1_cursor = (int)((data->p1_cursor + 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            data->p1_selected = true;
            game->selected_player_1 = (uint32_t)data->p1_cursor;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        }
    } else if (!data->p2_selected && game->selected_mode == 0) { /* 1 on 1 requires P2 */
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
            data->p2_cursor = (int)((data->p2_cursor + count - 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
            data->p2_cursor = (int)((data->p2_cursor + 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            data->p2_selected = true;
            game->selected_player_2 = (uint32_t)data->p2_cursor;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        }
    }

    /* Transition to selected game mode */
    if (data->p1_selected && (data->p2_selected || game->selected_mode != 0)) {
        switch (game->selected_mode) {
            case 0: allstar_game_change_scene(game, ALLSTAR_SCENE_ONE_ON_ONE); break;
            case 1: allstar_game_change_scene(game, ALLSTAR_SCENE_THREE_POINT); break;
            case 2: allstar_game_change_scene(game, ALLSTAR_SCENE_FREE_THROW); break;
            case 3: allstar_game_change_scene(game, ALLSTAR_SCENE_HORSE); break;
            default: allstar_game_change_scene(game, ALLSTAR_SCENE_ONE_ON_ONE); break;
        }
    }
}

static void roster_select_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_text(renderer, "PLAYER SELECT", 28, 8, 3);

    const AllStarPlayerStats *p1 = allstar_roster_get_player(&game->roster, data->p1_cursor);
    if (p1) {
        allstar_renderer_draw_text(renderer, "1P:", 8, 32, 3);
        allstar_renderer_draw_text(renderer, p1->name, 36, 32, 3);
        allstar_renderer_draw_text(renderer, p1->team, 36, 44, 2);

        char stats_buf[32];
        snprintf(stats_buf, sizeof(stats_buf), "SPD:%d 3PT:%d", p1->speed, p1->shooting_3pt);
        allstar_renderer_draw_text(renderer, stats_buf, 16, 60, 2);
        snprintf(stats_buf, sizeof(stats_buf), "DEF:%d 2PT:%d", p1->defense, p1->shooting_2pt);
        allstar_renderer_draw_text(renderer, stats_buf, 16, 72, 2);
    }

    if (game->selected_mode == 0) {
        const AllStarPlayerStats *p2 = allstar_roster_get_player(&game->roster, data->p2_cursor);
        if (p2) {
            allstar_renderer_draw_text(renderer, "2P:", 8, 92, 3);
            allstar_renderer_draw_text(renderer, p2->name, 36, 92, 3);
            allstar_renderer_draw_text(renderer, p2->team, 36, 104, 2);
        }
    }

    allstar_renderer_draw_text(renderer, "PRESS A TO PICK", 20, 128, 1);
}

static void roster_select_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_roster_select(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_ROSTER_SELECT;
    scene->user_data = calloc(1, sizeof(SceneRosterSelectData));
    scene->init = roster_select_init;
    scene->update = roster_select_update;
    scene->draw = roster_select_draw;
    scene->destroy = roster_select_destroy;
    return scene;
}
