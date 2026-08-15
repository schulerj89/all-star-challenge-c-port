#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_settings_art.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int mode;
    int cursor_row;
    float timer;
    float hold_left;
    float hold_right;
    float hold_up;
    float hold_down;
} SceneSettingsData;

static void settings_init(AllStarScene *scene, AllStarGame *game) {
    SceneSettingsData *data = (SceneSettingsData*)scene->user_data;
    memset(data, 0, sizeof(SceneSettingsData));
    data->mode = (int)game->selected_mode;
    data->cursor_row = 0;
    data->timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_TITLE);
}

static void settings_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneSettingsData *data = (SceneSettingsData*)scene->user_data;
    data->timer += dt;

    int max_rows = 4;
    if (data->mode == ALLSTAR_MODE_FREE_THROW) max_rows = 1;
    else if (data->mode == ALLSTAR_MODE_HORSE) max_rows = 1;
    else if (data->mode == ALLSTAR_MODE_ACCURACY) max_rows = 3;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_UP)) {
        data->cursor_row = (data->cursor_row + max_rows - 1) % max_rows;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
    }
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_DOWN)) {
        data->cursor_row = (data->cursor_row + 1) % max_rows;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
    }

    /* Auto-repeat for Left / Right button holding */
    bool trigger_left = false;
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_LEFT)) {
        trigger_left = true;
        data->hold_left = 0.0f;
    } else if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT)) {
        data->hold_left += dt;
        if (data->hold_left >= 0.25f) {
            trigger_left = true;
            data->hold_left -= 0.05f;
        }
    } else {
        data->hold_left = 0.0f;
    }

    bool trigger_right = false;
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_RIGHT)) {
        trigger_right = true;
        data->hold_right = 0.0f;
    } else if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) {
        data->hold_right += dt;
        if (data->hold_right >= 0.25f) {
            trigger_right = true;
            data->hold_right -= 0.05f;
        }
    } else {
        data->hold_right = 0.0f;
    }

    /* Left / Right toggles current setting */
    if (trigger_left) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        if (data->mode == ALLSTAR_MODE_ONE_ON_ONE || data->mode == ALLSTAR_MODE_TOURNAMENT) {
            switch (data->cursor_row) {
                case 0: /* Play to: Time -> 99 -> 98 ... -> 01 -> Time */
                    if (game->settings.play_to > 1) game->settings.play_to--;
                    else if (game->settings.play_to == 1) game->settings.play_to = 0;
                    else game->settings.play_to = 99;
                    break;
                case 1: game->settings.skill_level = game->settings.skill_level == 1
                            ? 3 : (uint8_t)(game->settings.skill_level - 1); break;
                case 2: game->settings.winners_outs = !game->settings.winners_outs; break;
                case 3: game->settings.game_minutes = allstar_game_settings_cycle_time(
                            game->settings.game_minutes, -1); break;
                default: break;
            }
        } else if (data->mode == ALLSTAR_MODE_FREE_THROW) {
            game->settings.free_throw_attempts = allstar_game_settings_cycle_throws(
                game->settings.free_throw_attempts, -1);
        } else if (data->mode == ALLSTAR_MODE_ACCURACY) {
            if (data->cursor_row < 2) {
                /* ROM $24C4 toggles complementary $FF9A/$FF9B together. */
                game->settings.accuracy_computer_positions =
                    !game->settings.accuracy_computer_positions;
            } else {
                game->settings.game_minutes = allstar_game_settings_cycle_time(
                    game->settings.game_minutes, -1);
            }
        }
    }

    if (trigger_right) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        if (data->mode == ALLSTAR_MODE_ONE_ON_ONE || data->mode == ALLSTAR_MODE_TOURNAMENT) {
            switch (data->cursor_row) {
                case 0: /* Play to: Time -> 01 -> 02 ... -> 99 -> Time */
                    if (game->settings.play_to == 0) game->settings.play_to = 1;
                    else if (game->settings.play_to < 99) game->settings.play_to++;
                    else game->settings.play_to = 0;
                    break;
                case 1: game->settings.skill_level =
                            (uint8_t)((game->settings.skill_level % 3) + 1); break;
                case 2: game->settings.winners_outs = !game->settings.winners_outs; break;
                case 3: game->settings.game_minutes = allstar_game_settings_cycle_time(
                            game->settings.game_minutes, 1); break;
                default: break;
            }
        } else if (data->mode == ALLSTAR_MODE_FREE_THROW) {
            game->settings.free_throw_attempts = allstar_game_settings_cycle_throws(
                game->settings.free_throw_attempts, 1);
        } else if (data->mode == ALLSTAR_MODE_ACCURACY) {
            if (data->cursor_row < 2) {
                /* New positions and computer positions are mutually exclusive. */
                game->settings.accuracy_computer_positions =
                    !game->settings.accuracy_computer_positions;
            } else {
                game->settings.game_minutes = allstar_game_settings_cycle_time(
                    game->settings.game_minutes, 1);
            }
        }
    }

    /* Press Start / A to advance to Player Select */
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) || allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_game_change_scene(game, ALLSTAR_SCENE_ROSTER_SELECT);
    }
}

static void settings_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneSettingsData *data = (SceneSettingsData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Pick authentic background: Jordan for One-on-One/Tournament, Worthy for others */
    const uint8_t *bg = (data->mode == ALLSTAR_MODE_ONE_ON_ONE || data->mode == ALLSTAR_MODE_TOURNAMENT) ? ALLSTAR_SETTINGS_JORDAN_BG : ALLSTAR_SETTINGS_WORTHY_BG;
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            uint8_t shade = bg[y * 160 + x];
            allstar_renderer_set_pixel(renderer, x, y, shade);
        }
    }

    /* Draw dynamic option values */
    if (data->mode == ALLSTAR_MODE_ONE_ON_ONE || data->mode == ALLSTAR_MODE_TOURNAMENT) {
        char play_to_str[16];
        if (game->settings.play_to == 0) {
            snprintf(play_to_str, sizeof(play_to_str), "Time ");
        } else {
            snprintf(play_to_str, sizeof(play_to_str), "%02u   ",
                     (unsigned)game->settings.play_to);
        }

        char skill_str[4];
        char time_str[8];
        snprintf(skill_str, sizeof(skill_str), "%u", (unsigned)game->settings.skill_level);
        const char *winners_str = game->settings.winners_outs ? "YES" : "NO ";
        snprintf(time_str, sizeof(time_str), "%02u:00",
                 (unsigned)game->settings.game_minutes);

        /* Clear only the exact tile character slots (8x8 per character) to shade 0 */
        allstar_renderer_draw_rect_fill(renderer, 120, 72, 32, 8, 0);
        allstar_renderer_draw_text(renderer, play_to_str, 120, 72, 3);

        allstar_renderer_draw_rect_fill(renderer, 120, 80, 8, 8, 0);
        allstar_renderer_draw_text(renderer, skill_str, 120, 80, 3);

        allstar_renderer_draw_rect_fill(renderer, 120, 88, 24, 8, 0);
        allstar_renderer_draw_text(renderer, winners_str, 120, 88, 3);

        allstar_renderer_draw_rect_fill(renderer, 120, 96, 40, 8, 0);
        allstar_renderer_draw_text(renderer, time_str, 120, 96, 3);
    } else if (data->mode == ALLSTAR_MODE_FREE_THROW) {
        char throws[4];
        snprintf(throws, sizeof(throws), "%2u",
                 (unsigned)game->settings.free_throw_attempts);
        allstar_renderer_draw_rect_fill(renderer, 120, 80, 16, 8, 0);
        allstar_renderer_draw_text(renderer, throws, 120, 80, 3);
    } else if (data->mode == ALLSTAR_MODE_ACCURACY) {
        char time_str[8];
        allstar_renderer_clear(renderer, 0);
        allstar_renderer_draw_text(renderer, "ACCURACY SETTINGS", 8, 32, 3);
        allstar_renderer_draw_text(renderer, "NEW POS.", 16, 64, 3);
        allstar_renderer_draw_text(renderer,
            game->settings.accuracy_computer_positions ? "NO" : "YES", 128, 64, 3);
        allstar_renderer_draw_text(renderer, "COMPUTER POS.", 16, 72, 3);
        allstar_renderer_draw_text(renderer,
            game->settings.accuracy_computer_positions ? "YES" : "NO", 128, 72, 3);
        allstar_renderer_draw_text(renderer, "TIME LIMIT", 16, 80, 3);
        snprintf(time_str, sizeof(time_str), "%02u:00",
                 (unsigned)game->settings.game_minutes);
        allstar_renderer_draw_text(renderer, time_str, 112, 80, 3);
    }

    /* Draw Basketball cursor aligned with text rows */
    static const int JORDAN_CURSOR_Y[4] = { 72, 80, 88, 96 };
    int cur_y = JORDAN_CURSOR_Y[data->cursor_row % 4];
    if (data->mode == ALLSTAR_MODE_FREE_THROW) cur_y = 80;
    else if (data->mode == ALLSTAR_MODE_ACCURACY) cur_y = 64 + (data->cursor_row * 8);

    allstar_renderer_draw_cursor(renderer, 0, cur_y);
}

static void settings_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_settings(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_SETTINGS;
    scene->user_data = calloc(1, sizeof(SceneSettingsData));
    scene->init = settings_init;
    scene->update = settings_update;
    scene->draw = settings_draw;
    scene->destroy = settings_destroy;
    return scene;
}
