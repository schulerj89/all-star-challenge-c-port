#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_settings_art.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int mode;
    int cursor_row;
    int play_to;      /* 0 = Time, 1..99 = Score */
    int skill_level;  /* 1, 2, 3 */
    int winners_outs; /* 0 = NO, 1 = YES */
    int time_limit;   /* 0 = 02:00, 1 = 03:00, 2 = 05:00 */
    int num_throws;   /* 0 = 5, 1 = 10, 2 = 15, 3 = 20 */
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
    data->play_to = 0;
    data->skill_level = 1;
    data->winners_outs = 0;
    data->time_limit = 0;
    data->num_throws = 0;
    data->timer = 0.0f;
    allstar_audio_play_bgm(&game->audio, ALLSTAR_BGM_MENU);
}

static void settings_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneSettingsData *data = (SceneSettingsData*)scene->user_data;
    data->timer += dt;

    int max_rows = 4;
    if (data->mode == 1) max_rows = 1; /* Free Throws */
    else if (data->mode == 2) max_rows = 1; /* Horse */
    else if (data->mode == 3) max_rows = 3; /* Accuracy */

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
        if (data->mode == 0 || data->mode == 4) { /* One on One / Tournament */
            switch (data->cursor_row) {
                case 0: /* Play to: Time -> 99 -> 98 ... -> 01 -> Time */
                    if (data->play_to > 1) data->play_to--;
                    else if (data->play_to == 1) data->play_to = 0;
                    else data->play_to = 99;
                    break;
                case 1: data->skill_level = (data->skill_level == 1) ? 3 : (data->skill_level - 1); break;
                case 2: data->winners_outs ^= 1; break;
                case 3: data->time_limit = (data->time_limit == 0) ? 2 : (data->time_limit - 1); break;
                default: break;
            }
        } else if (data->mode == 1) { /* Free Throws */
            data->num_throws = (data->num_throws == 0) ? 3 : (data->num_throws - 1);
        } else if (data->mode == 3) { /* Accuracy */
            data->time_limit ^= 1;
        }
    }

    if (trigger_right) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_MOVE);
        if (data->mode == 0 || data->mode == 4) { /* One on One / Tournament */
            switch (data->cursor_row) {
                case 0: /* Play to: Time -> 01 -> 02 ... -> 99 -> Time */
                    if (data->play_to == 0) data->play_to = 1;
                    else if (data->play_to < 99) data->play_to++;
                    else data->play_to = 0;
                    break;
                case 1: data->skill_level = (data->skill_level % 3) + 1; break;
                case 2: data->winners_outs ^= 1; break;
                case 3: data->time_limit = (data->time_limit + 1) % 3; break;
                default: break;
            }
        } else if (data->mode == 1) { /* Free Throws */
            data->num_throws = (data->num_throws + 1) % 4;
        } else if (data->mode == 3) { /* Accuracy */
            data->time_limit ^= 1;
        }
    }

    /* Press Start / A to advance to Player Select */
    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) || allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_game_change_scene(game, ALLSTAR_SCENE_ROSTER_SELECT);
    }
}

static void settings_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneSettingsData *data = (SceneSettingsData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Pick authentic background: Jordan for One-on-One/Tournament, Worthy for others */
    const uint8_t *bg = (data->mode == 0 || data->mode == 4) ? ALLSTAR_SETTINGS_JORDAN_BG : ALLSTAR_SETTINGS_WORTHY_BG;
    for (int y = 0; y < 144; y++) {
        for (int x = 0; x < 160; x++) {
            uint8_t shade = bg[y * 160 + x];
            allstar_renderer_set_pixel(renderer, x, y, shade);
        }
    }

    /* Draw dynamic option values */
    if (data->mode == 0 || data->mode == 4) {
        char play_to_str[16];
        if (data->play_to == 0) {
            snprintf(play_to_str, sizeof(play_to_str), "Time ");
        } else {
            snprintf(play_to_str, sizeof(play_to_str), "%02d   ", data->play_to);
        }

        char skill_str[4];
        snprintf(skill_str, sizeof(skill_str), "%d", data->skill_level);
        const char *winners_str = (data->winners_outs == 0) ? "NO " : "YES";
        const char *time_str = (data->time_limit == 0) ? "02:00" : ((data->time_limit == 1) ? "03:00" : "05:00");

        allstar_renderer_draw_text(renderer, play_to_str, 120, 72, 3);
        allstar_renderer_draw_text(renderer, skill_str, 120, 80, 3);
        allstar_renderer_draw_text(renderer, winners_str, 120, 88, 3);
        allstar_renderer_draw_text(renderer, time_str, 120, 96, 3);
    } else if (data->mode == 1) {
        static const char *THROWS[4] = { " 5", "10", "15", "20" };
        allstar_renderer_draw_text(renderer, THROWS[data->num_throws % 4], 120, 80, 3);
    } else if (data->mode == 3) {
        const char *time_str = (data->time_limit == 0) ? "02:00" : "01:00";
        allstar_renderer_draw_text(renderer, time_str, 120, 80, 3);
    }

    /* Draw Basketball cursor aligned with text rows */
    static const int JORDAN_CURSOR_Y[4] = { 72, 80, 88, 96 };
    int cur_y = JORDAN_CURSOR_Y[data->cursor_row % 4];
    if (data->mode == 1) cur_y = 80;
    else if (data->mode == 3) cur_y = 64 + (data->cursor_row * 8);

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
