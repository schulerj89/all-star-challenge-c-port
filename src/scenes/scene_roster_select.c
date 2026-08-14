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
            case 4: allstar_game_change_scene(game, ALLSTAR_SCENE_TOURNAMENT); break;
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

#include "allstar_player_art.h"

static void roster_select_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Draw Authentic Star Border */
    for (int x = 0; x < 160; x += 8) {
        allstar_renderer_draw_text(renderer, "*", x, 0, 3);
        allstar_renderer_draw_text(renderer, "*", x, 136, 3);
    }
    for (int y = 8; y < 136; y += 8) {
        allstar_renderer_draw_text(renderer, "*", 0, y, 3);
        allstar_renderer_draw_text(renderer, "*", 152, y, 3);
    }

    int active_cursor = data->p1_selected ? data->p2_cursor : data->p1_cursor;
    const AllStarPlayerStats *p = allstar_roster_get_player(&game->roster, active_cursor);
    if (p) {
        /* Authentic Full Player Face Portrait (32x48 at X=32, Y=8) */
        for (int py = 0; py < 48; py++) {
            for (int px = 0; px < 32; px++) {
                uint8_t shade = ALLSTAR_PLAYER_PORTRAITS[active_cursor % 27][py * 32 + px];
                allstar_renderer_set_pixel(renderer, 32 + px, 8 + py, shade);
            }
        }

        /* Authentic Team Basketball Logo (32x32 at X=96, Y=16) */
        for (int py = 0; py < 32; py++) {
            for (int px = 0; px < 32; px++) {
                uint8_t shade = ALLSTAR_PLAYER_LOGOS[active_cursor % 27][py * 32 + px];
                allstar_renderer_set_pixel(renderer, 96 + px, 16 + py, shade);
            }
        }

        /* Full Player Name at Y=64 */
        char full_name[64];
        snprintf(full_name, sizeof(full_name), "%s %s", p->first_name, p->last_name);
        int name_len = (int)strlen(full_name);
        int name_x = 80 - (name_len * 4);
        if (name_x < 12) name_x = 12;
        allstar_renderer_draw_text(renderer, full_name, name_x, 64, 3);

        /* Authentic Height / Weight / PPG Table at Y=88, 104, 120 */
        allstar_renderer_draw_text(renderer, "HEIGHT", 16, 88, 3);
        allstar_renderer_draw_text(renderer, ":", 84, 88, 3);
        allstar_renderer_draw_text(renderer, p->height_str, 104, 88, 3);

        allstar_renderer_draw_text(renderer, "WEIGHT", 16, 104, 3);
        allstar_renderer_draw_text(renderer, ":", 84, 104, 3);
        allstar_renderer_draw_text(renderer, p->weight_str, 104, 104, 3);

        allstar_renderer_draw_text(renderer, "PPG AVG", 16, 120, 3);
        allstar_renderer_draw_text(renderer, ":", 84, 120, 3);
        allstar_renderer_draw_text(renderer, p->ppg_str, 104, 120, 3);
    }
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
