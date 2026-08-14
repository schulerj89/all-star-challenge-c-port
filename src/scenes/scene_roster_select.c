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
    } else if (!data->p2_selected && game->selected_mode == 0) {
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

static void draw_stat_bar(AllStarRenderer *renderer, const char *label, int val, int x, int y) {
    allstar_renderer_draw_text(renderer, label, x, y, 3);
    /* Bar background */
    allstar_renderer_draw_rect_fill(renderer, x + 36, y + 1, 40, 6, 1);
    allstar_renderer_draw_rect_outline(renderer, x + 36, y + 1, 40, 6, 3);
    /* Filled segment */
    int fill_w = (val * 38) / 100;
    if (fill_w > 0) {
        allstar_renderer_draw_rect_fill(renderer, x + 37, y + 2, fill_w, 4, 3);
    }
}

static void roster_select_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Header */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 18, 3);
    allstar_renderer_draw_text(renderer, data->p1_selected ? "SELECT OPPONENT" : "SELECT PLAYER", 24, 5, 0);

    int active_cursor = data->p1_selected ? data->p2_cursor : data->p1_cursor;
    const AllStarPlayerStats *p = allstar_roster_get_player(&game->roster, active_cursor);
    if (p) {
        /* Player Card Box */
        allstar_renderer_draw_rect_fill(renderer, 8, 24, 144, 98, 1);
        allstar_renderer_draw_rect_outline(renderer, 8, 24, 144, 98, 3);

        /* Player Name Banner */
        allstar_renderer_draw_rect_fill(renderer, 10, 26, 140, 16, 2);
        char name_buf[32];
        snprintf(name_buf, sizeof(name_buf), "#%d %s", p->number, p->name);
        allstar_renderer_draw_text(renderer, name_buf, 14, 30, 0);

        /* Team */
        allstar_renderer_draw_text(renderer, p->team, 14, 46, 3);

        /* Stats Bars */
        draw_stat_bar(renderer, "SPD", p->speed, 14, 58);
        draw_stat_bar(renderer, "3PT", p->shooting_3pt, 14, 68);
        draw_stat_bar(renderer, "2PT", p->shooting_2pt, 14, 78);
        draw_stat_bar(renderer, "DEF", p->defense, 14, 88);

        /* Animated Player Preview */
        allstar_renderer_draw_player(renderer, 126, 75, !data->p1_selected, true, false, 0.0f);
    }

    /* Footer Navigation */
    allstar_renderer_draw_text(renderer, "< LEFT/RIGHT > PICK", 8, 128, 2);
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
