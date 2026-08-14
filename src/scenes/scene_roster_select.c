#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_player_art.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum {
    ROSTER_STATE_SELECT_P1 = 0,
    ROSTER_STATE_SELECT_OPPONENT,
    ROSTER_STATE_MATCHUP_VS
} RosterSelectState;

typedef struct {
    RosterSelectState state;
    int p1_cursor;
    int p2_cursor;
    float timer;
} SceneRosterSelectData;

static void roster_select_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    data->state = ROSTER_STATE_SELECT_P1;
    data->p1_cursor = 0;
    data->p2_cursor = 1;
    data->timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_TITLE);
}

static void roster_select_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    data->timer += dt;
    size_t count = game->roster.count ? game->roster.count : ALLSTAR_PORTRAIT_COUNT;

    if (data->state == ROSTER_STATE_SELECT_P1) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
            data->p1_cursor = (int)((data->p1_cursor + count - 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
            data->p1_cursor = (int)((data->p1_cursor + 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) || allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
            game->selected_player_1 = (uint32_t)data->p1_cursor;
            data->p2_cursor = (int)((data->p1_cursor + 1) % count);

            if (game->selected_mode == 0 || game->selected_mode == 3 || game->selected_mode == 4) {
                data->state = ROSTER_STATE_SELECT_OPPONENT;
            } else {
                data->state = ROSTER_STATE_MATCHUP_VS;
            }
        }
    } else if (data->state == ROSTER_STATE_SELECT_OPPONENT) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
            data->p2_cursor = (int)((data->p2_cursor + count - 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
            data->p2_cursor = (int)((data->p2_cursor + 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) || allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
            game->selected_player_2 = (uint32_t)data->p2_cursor;
            data->state = ROSTER_STATE_MATCHUP_VS;
        }
    } else if (data->state == ROSTER_STATE_MATCHUP_VS) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) || allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
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
}

static void draw_player_card(AllStarRenderer *renderer, AllStarGame *game, int player_idx, const char *header_title) {
    /* Draw Authentic Star Border */
    for (int x = 0; x < 160; x += 8) {
        allstar_renderer_draw_text(renderer, "*", x, 0, 3);
        allstar_renderer_draw_text(renderer, "*", x, 136, 3);
    }
    for (int y = 0; y < 144; y += 8) {
        allstar_renderer_draw_text(renderer, "*", 0, y, 3);
        allstar_renderer_draw_text(renderer, "*", 152, y, 3);
    }

    if (header_title) {
        int title_len = (int)strlen(header_title);
        int title_x = (160 - title_len * 8) / 2;
        allstar_renderer_draw_text(renderer, header_title, title_x, 4, 3);
    }

    const AllStarPlayerStats *stats = allstar_roster_get_player(&game->roster, (size_t)player_idx);
    int p_idx = player_idx % ALLSTAR_PORTRAIT_COUNT;

    /* Render 32x48 Face Portrait at (32, 14) */
    const uint8_t *face = ALLSTAR_PLAYER_PORTRAITS[p_idx];
    for (int fy = 0; fy < 48; fy++) {
        for (int fx = 0; fx < 32; fx++) {
            uint8_t shade = face[fy * 32 + fx];
            allstar_renderer_set_pixel(renderer, 32 + fx, 14 + fy, shade);
        }
    }

    /* Render 32x32 Team Logo at (96, 22) */
    const uint8_t *logo = ALLSTAR_PLAYER_LOGOS[p_idx];
    for (int ly = 0; ly < 32; ly++) {
        for (int lx = 0; lx < 32; lx++) {
            uint8_t shade = logo[ly * 32 + lx];
            allstar_renderer_set_pixel(renderer, 96 + lx, 22 + ly, shade);
        }
    }

    /* Player Name */
    char name_buf[64];
    if (stats) {
        snprintf(name_buf, sizeof(name_buf), "%s", stats->name);
    } else {
        snprintf(name_buf, sizeof(name_buf), "PLAYER %d", player_idx + 1);
    }
    int name_len = (int)strlen(name_buf);
    int name_x = (160 - name_len * 8) / 2;
    if (name_x < 8) name_x = 8;
    allstar_renderer_draw_text(renderer, name_buf, name_x, 68, 3);

    /* Attributes aligned with Game Boy ROM rows */
    char h_buf[32], w_buf[32], p_buf[32];
    if (stats) {
        snprintf(h_buf, sizeof(h_buf), "HEIGHT   :  %s", stats->height_str);
        snprintf(w_buf, sizeof(w_buf), "WEIGHT   :  %s", stats->weight_str);
        snprintf(p_buf, sizeof(p_buf), "PPG AVG  :  %s", stats->ppg_str);
    } else {
        snprintf(h_buf, sizeof(h_buf), "HEIGHT   :  6'6\"");
        snprintf(w_buf, sizeof(w_buf), "WEIGHT   :  215");
        snprintf(p_buf, sizeof(p_buf), "PPG AVG  :  25.0");
    }

    allstar_renderer_draw_text(renderer, h_buf, 16, 88, 3);
    allstar_renderer_draw_text(renderer, w_buf, 16, 104, 3);
    allstar_renderer_draw_text(renderer, p_buf, 16, 120, 3);
}

static void draw_matchup_vs(AllStarRenderer *renderer, AllStarGame *game, int p1_idx, int p2_idx) {
    /* Draw Star Border */
    for (int x = 0; x < 160; x += 8) {
        allstar_renderer_draw_text(renderer, "*", x, 0, 3);
        allstar_renderer_draw_text(renderer, "*", x, 136, 3);
    }
    for (int y = 0; y < 144; y += 8) {
        allstar_renderer_draw_text(renderer, "*", 0, y, 3);
        allstar_renderer_draw_text(renderer, "*", 152, y, 3);
    }

    /* Header Title */
    allstar_renderer_draw_text(renderer, "PLAYER 1  VS  CPU", 16, 8, 3);

    /* P1 Face (32x48) at (16, 22) */
    int p1_art = p1_idx % ALLSTAR_PORTRAIT_COUNT;
    const uint8_t *f1 = ALLSTAR_PLAYER_PORTRAITS[p1_art];
    for (int fy = 0; fy < 48; fy++) {
        for (int fx = 0; fx < 32; fx++) {
            allstar_renderer_set_pixel(renderer, 16 + fx, 22 + fy, f1[fy * 32 + fx]);
        }
    }

    /* Center VS */
    allstar_renderer_draw_text(renderer, "VS", 74, 42, 3);

    /* P2 Face (32x48) at (112, 22) */
    int p2_art = p2_idx % ALLSTAR_PORTRAIT_COUNT;
    const uint8_t *f2 = ALLSTAR_PLAYER_PORTRAITS[p2_art];
    for (int fy = 0; fy < 48; fy++) {
        for (int fx = 0; fx < 32; fx++) {
            allstar_renderer_set_pixel(renderer, 112 + fx, 22 + fy, f2[fy * 32 + fx]);
        }
    }

    /* P1 Team Logo (32x32) at (16, 74) */
    const uint8_t *l1 = ALLSTAR_PLAYER_LOGOS[p1_art];
    for (int ly = 0; ly < 32; ly++) {
        for (int lx = 0; lx < 32; lx++) {
            allstar_renderer_set_pixel(renderer, 16 + lx, 74 + ly, l1[ly * 32 + lx]);
        }
    }

    /* P2 Team Logo (32x32) at (112, 74) */
    const uint8_t *l2 = ALLSTAR_PLAYER_LOGOS[p2_art];
    for (int ly = 0; ly < 32; ly++) {
        for (int lx = 0; lx < 32; lx++) {
            allstar_renderer_set_pixel(renderer, 112 + lx, 74 + ly, l2[ly * 32 + lx]);
        }
    }

    /* Player Last Names Below Columns */
    const AllStarPlayerStats *s1 = allstar_roster_get_player(&game->roster, (size_t)p1_idx);
    const AllStarPlayerStats *s2 = allstar_roster_get_player(&game->roster, (size_t)p2_idx);

    const char *n1 = s1 ? s1->last_name : "PLAYER 1";
    const char *n2 = s2 ? s2->last_name : "CPU";

    int len1 = (int)strlen(n1);
    int x1 = 16 + (32 - len1 * 8) / 2;
    if (x1 < 8) x1 = 8;
    allstar_renderer_draw_text(renderer, n1, x1, 110, 3);

    int len2 = (int)strlen(n2);
    int x2 = 112 + (32 - len2 * 8) / 2;
    if (x2 < 88) x2 = 88;
    allstar_renderer_draw_text(renderer, n2, x2, 110, 3);

    /* Bottom Prompt */
    allstar_renderer_draw_text(renderer, "PRESS START", 36, 126, 3);
}

static void roster_select_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    if (data->state == ROSTER_STATE_SELECT_P1) {
        draw_player_card(renderer, game, data->p1_cursor, "SELECT PLAYER");
    } else if (data->state == ROSTER_STATE_SELECT_OPPONENT) {
        draw_player_card(renderer, game, data->p2_cursor, "SELECT OPPONENT");
    } else if (data->state == ROSTER_STATE_MATCHUP_VS) {
        draw_matchup_vs(renderer, game, data->p1_cursor, data->p2_cursor);
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
