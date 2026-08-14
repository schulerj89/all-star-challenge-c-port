#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

typedef struct {
    float timer;
    float ball_y;
    float ball_vy;
} SceneIntroData;

static void intro_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer = 0.0f;
    data->ball_y = 10.0f;
    data->ball_vy = 0.0f;
}

static void intro_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    data->timer += dt;

    /* Bouncing ball animation */
    data->ball_vy += 120.0f * dt;
    data->ball_y += data->ball_vy * dt;
    if (data->ball_y >= 100.0f) {
        data->ball_y = 100.0f;
        data->ball_vy = -75.0f;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
    }

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_START) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        allstar_game_change_scene(game, ALLSTAR_SCENE_MENU);
    }
}

#include "allstar_title_art.h"

static void intro_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneIntroData *data = (SceneIntroData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    if (data->timer < 1.0f) {
        /* Authentic Beam Software Copyright Screen */
        allstar_renderer_draw_text(renderer, "(C)1990 NBA", 36, 20, 3);
        allstar_renderer_draw_text(renderer, "PROPERTIES INC.", 20, 32, 3);

        allstar_renderer_draw_text(renderer, "(C)1990 LJN. LTD", 16, 56, 3);
        allstar_renderer_draw_text(renderer, "LICENSED BY NINTENDO", 4, 76, 3);

        allstar_renderer_draw_text(renderer, "PROGRAMMED BY", 28, 106, 3);
        allstar_renderer_draw_text(renderer, "BEAM SOFTWARE.", 24, 120, 3);
    } else {
        /* Authentic NBA All-Star Challenge Title Screen Extracted from ROM */
        for (int y = 0; y < 144; y++) {
            for (int x = 0; x < 160; x++) {
                uint8_t shade = ALLSTAR_TITLE_BITMAP[y * 160 + x];
                allstar_renderer_set_pixel(renderer, x, y, shade);
            }
        }

        /* Pulsing "PRESS START" and Animated Cursor */
        if ((int)(data->timer * 3.0f) % 2 == 0) {
            allstar_renderer_draw_ball(renderer, 46, 128, 0);
        }
    }
}

static void intro_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_intro(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_INTRO;
    scene->user_data = calloc(1, sizeof(SceneIntroData));
    scene->init = intro_init;
    scene->update = intro_update;
    scene->draw = intro_draw;
    scene->destroy = intro_destroy;
    return scene;
}
