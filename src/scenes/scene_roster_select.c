#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_player_art.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* Live bank-2 $40F4 trace: the final accepted-player command $0E starts
   35 frames before the first bank-1 $702D gameplay update. */
#define ALLSTAR_ROM_MATCHUP_TO_GAMEPLAY_SECONDS (35.0f / 60.0f)

typedef enum {
    ROSTER_STATE_SPLASH_P1 = 0,
    ROSTER_STATE_SELECT_P1,
    ROSTER_STATE_SPLASH_OPPONENT,
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
    data->state = ROSTER_STATE_SPLASH_P1;
    data->p1_cursor = 0;
    data->p2_cursor = 1;
    data->timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_TITLE);
}

static void roster_select_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    data->timer += dt;
    size_t count = game->roster.count ? game->roster.count : ALLSTAR_PORTRAIT_COUNT;

    if (data->state == ROSTER_STATE_SPLASH_P1) {
        if (data->timer >= 0.8f ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            data->state = ROSTER_STATE_SELECT_P1;
            data->timer = 0.0f;
        }
    } else if (data->state == ROSTER_STATE_SELECT_P1) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
            data->p1_cursor = (int)((data->p1_cursor + count - 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
            data->p1_cursor = (int)((data->p1_cursor + 1) % count);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        }
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) || allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
            /* Bank 2 $40F4 uses command $0E/program $12 for an accepted
               player. Command $0F/program $07 is the navigation cue. */
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
            game->selected_player_1 = (uint32_t)data->p1_cursor;
            data->p2_cursor = 0; /* Reset back to first index player */
            data->timer = 0.0f;

            if (allstar_game_mode_requires_opponent(game->selected_mode)) {
                data->state = ROSTER_STATE_SPLASH_OPPONENT;
            } else {
                data->state = ROSTER_STATE_MATCHUP_VS;
            }
        }
    } else if (data->state == ROSTER_STATE_SPLASH_OPPONENT) {
        if (data->timer >= 0.8f ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            data->state = ROSTER_STATE_SELECT_OPPONENT;
            data->timer = 0.0f;
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
            allstar_audio_stop_bgm(&game->audio);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
            game->selected_player_2 = (uint32_t)data->p2_cursor;
            data->state = ROSTER_STATE_MATCHUP_VS;
            data->timer = 0.0f;
        }
    } else if (data->state == ROSTER_STATE_MATCHUP_VS) {
        if (data->timer >= ALLSTAR_ROM_MATCHUP_TO_GAMEPLAY_SECONDS ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            allstar_game_change_scene(game, allstar_game_mode_scene(game->selected_mode));
        }
    }
}

static void draw_splash_screen(AllStarRenderer *renderer, const char *line1, const char *line2, const char *line3) {
    if (line3) {
        if (line1) {
            int len1 = (int)strlen(line1);
            int x1 = (160 - len1 * 8) / 2;
            allstar_renderer_draw_text(renderer, line1, x1, 48, 3);
        }
        if (line2) {
            int len2 = (int)strlen(line2);
            int x2 = (160 - len2 * 8) / 2;
            allstar_renderer_draw_text(renderer, line2, x2, 64, 3);
        }
        int len3 = (int)strlen(line3);
        int x3 = (160 - len3 * 8) / 2;
        allstar_renderer_draw_text(renderer, line3, x3, 80, 3);
    } else {
        if (line1) {
            int len1 = (int)strlen(line1);
            int x1 = (160 - len1 * 8) / 2;
            allstar_renderer_draw_text(renderer, line1, x1, 56, 3);
        }
        if (line2) {
            int len2 = (int)strlen(line2);
            int x2 = (160 - len2 * 8) / 2;
            allstar_renderer_draw_text(renderer, line2, x2, 72, 3);
        }
    }
}

static void draw_player_card(AllStarRenderer *renderer, AllStarGame *game, int player_idx) {
    /* Draw Authentic Star Border */
    for (int x = 0; x < 160; x += 8) {
        allstar_renderer_draw_text(renderer, "*", x, 0, 3);
        allstar_renderer_draw_text(renderer, "*", x, 136, 3);
    }
    for (int y = 0; y < 144; y += 8) {
        allstar_renderer_draw_text(renderer, "*", 0, y, 3);
        allstar_renderer_draw_text(renderer, "*", 152, y, 3);
    }

    const AllStarPlayerStats *stats = allstar_roster_get_player(&game->roster, (size_t)player_idx);
    int p_idx = player_idx % ALLSTAR_PORTRAIT_COUNT;

    /* Render 32x48 Face Portrait at (32, 12) */
    const uint8_t *face = ALLSTAR_PLAYER_PORTRAITS[p_idx];
    for (int fy = 0; fy < 48; fy++) {
        for (int fx = 0; fx < 32; fx++) {
            uint8_t shade = face[fy * 32 + fx];
            allstar_renderer_set_pixel(renderer, 32 + fx, 12 + fy, shade);
        }
    }

    /* Render 32x32 Team Logo at (96, 20) */
    const uint8_t *logo = ALLSTAR_PLAYER_LOGOS[p_idx];
    for (int ly = 0; ly < 32; ly++) {
        for (int lx = 0; lx < 32; lx++) {
            uint8_t shade = logo[ly * 32 + lx];
            allstar_renderer_set_pixel(renderer, 96 + lx, 20 + ly, shade);
        }
    }

    /* Player Name at Y=66 */
    char name_buf[64];
    if (stats) {
        snprintf(name_buf, sizeof(name_buf), "%s", stats->name);
    } else {
        snprintf(name_buf, sizeof(name_buf), "PLAYER %d", player_idx + 1);
    }
    int name_len = (int)strlen(name_buf);
    int name_x = (160 - name_len * 8) / 2;
    if (name_x < 8) name_x = 8;
    allstar_renderer_draw_text(renderer, name_buf, name_x, 66, 3);

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
    const AllStarPlayerStats *s1 = allstar_roster_get_player(&game->roster, (size_t)p1_idx);
    const AllStarPlayerStats *s2 = allstar_roster_get_player(&game->roster, (size_t)p2_idx);

    const char *name1 = s1 ? s1->last_name : "PLAYER";
    const char *name2 = s2 ? s2->last_name : "OPPONENT";

    /* Player 1 Last Name centered at Y = 48 */
    int len1 = (int)strlen(name1);
    int x1 = (160 - len1 * 8) / 2;
    if (x1 < 4) x1 = 4;
    allstar_renderer_draw_text(renderer, name1, x1, 48, 3);

    /* VS centered at Y = 68 */
    allstar_renderer_draw_text(renderer, "VS", 72, 68, 3);

    /* Player 2 / Opponent Last Name centered at Y = 88 */
    int len2 = (int)strlen(name2);
    int x2 = (160 - len2 * 8) / 2;
    if (x2 < 4) x2 = 4;
    allstar_renderer_draw_text(renderer, name2, x2, 88, 3);
}

static void roster_select_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneRosterSelectData *data = (SceneRosterSelectData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    if (data->state == ROSTER_STATE_SPLASH_P1) {
        draw_splash_screen(renderer, "SELECT", "PLAYER", NULL);
    } else if (data->state == ROSTER_STATE_SELECT_P1) {
        draw_player_card(renderer, game, data->p1_cursor);
    } else if (data->state == ROSTER_STATE_SPLASH_OPPONENT) {
        draw_splash_screen(renderer, "SELECT", "YOUR", "OPPONENT");
    } else if (data->state == ROSTER_STATE_SELECT_OPPONENT) {
        draw_player_card(renderer, game, data->p2_cursor);
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
