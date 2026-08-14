#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>

static const char HORSE_LETTERS[6] = "HORSE";

typedef struct {
    int p1_letters;
    int p2_letters;
    int current_turn; /* 1 = P1, 2 = P2 */
    bool shot_called;
    float spot_x;
    float spot_y;
} SceneHorseData;

static void horse_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    data->p1_letters = 0;
    data->p2_letters = 0;
    data->current_turn = 1;
    data->shot_called = false;
    data->spot_x = 80.0f;
    data->spot_y = 70.0f;
}

static void horse_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneHorseData *data = (SceneHorseData*)scene->user_data;

    /* Move shooting spot */
    float speed = 70.0f;
    if (!data->shot_called) {
        if (allstar_input_is_held(input, ALLSTAR_BTN_LEFT))  data->spot_x -= speed * dt;
        if (allstar_input_is_held(input, ALLSTAR_BTN_RIGHT)) data->spot_x += speed * dt;
        if (allstar_input_is_held(input, ALLSTAR_BTN_UP))    data->spot_y -= speed * dt;
        if (allstar_input_is_held(input, ALLSTAR_BTN_DOWN))  data->spot_y += speed * dt;

        if (data->spot_x < 20.0f) data->spot_x = 20.0f;
        if (data->spot_x > 140.0f) data->spot_x = 140.0f;
        if (data->spot_y < 35.0f) data->spot_y = 35.0f;
        if (data->spot_y > 130.0f) data->spot_y = 130.0f;

        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
            data->shot_called = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);

            /* Shoot ball */
            float dist = sqrtf((data->spot_x - 80.0f) * (data->spot_x - 80.0f) + (data->spot_y - 26.0f) * (data->spot_y - 26.0f));
            int rating = (dist > 52.0f) ? 80 : 90;
            bool made = ((rand() % 100) < rating);

            if (made) {
                allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
                /* Next player must match */
                data->current_turn = (data->current_turn == 1) ? 2 : 1;
            } else {
                allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
                /* Missed shot gives letter if matching */
                if (data->current_turn == 1) data->p1_letters++;
                else data->p2_letters++;
                data->current_turn = (data->current_turn == 1) ? 2 : 1;
            }
            data->shot_called = false;
        }
    }
}

static void horse_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneHorseData *data = (SceneHorseData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Top HUD */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    allstar_renderer_draw_text(renderer, "H - O - R - S - E", 18, 4, 0);

    /* Court Layout */
    allstar_renderer_draw_rect_outline(renderer, 10, 20, 140, 118, 2);
    allstar_renderer_draw_rect_fill(renderer, 60, 20, 40, 45, 1);
    allstar_renderer_draw_rect_outline(renderer, 60, 20, 40, 45, 2);

    /* Hoop */
    allstar_renderer_draw_line(renderer, 70, 22, 90, 22, 3);
    allstar_renderer_draw_line(renderer, 76, 26, 84, 26, 3);

    /* Scoreboard Box */
    allstar_renderer_draw_rect_fill(renderer, 12, 24, 46, 28, 0);
    allstar_renderer_draw_rect_outline(renderer, 12, 24, 46, 28, 3);

    char p1_str[16] = "1P: ";
    for (int i = 0; i < data->p1_letters && i < 5; i++) p1_str[4 + i] = HORSE_LETTERS[i];
    p1_str[4 + data->p1_letters] = '\0';
    allstar_renderer_draw_text(renderer, p1_str, 14, 28, 3);

    char p2_str[16] = "2P: ";
    for (int i = 0; i < data->p2_letters && i < 5; i++) p2_str[4 + i] = HORSE_LETTERS[i];
    p2_str[4 + data->p2_letters] = '\0';
    allstar_renderer_draw_text(renderer, p2_str, 14, 38, 3);

    /* Active Shooter */
    bool is_p1 = (data->current_turn == 1);
    const AllStarPlayerStats *p = allstar_roster_get_player(&game->roster, is_p1 ? game->selected_player_1 : game->selected_player_2);
    bool is_dark = (p && p->skin_tone == 0x90);
    allstar_renderer_draw_player(renderer, (int)data->spot_x, (int)data->spot_y, is_dark, true, false, 0.0f);

    /* Target Spot Marker */
    allstar_renderer_draw_rect_outline(renderer, (int)data->spot_x - 6, (int)data->spot_y - 2, 12, 4, 2);

    /* Bottom Prompt */
    char prompt_buf[32];
    snprintf(prompt_buf, sizeof(prompt_buf), "%dP: MOVE & PRESS A", data->current_turn);
    allstar_renderer_draw_text(renderer, prompt_buf, 8, 130, 3);
}

static void horse_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_horse(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_HORSE;
    scene->user_data = calloc(1, sizeof(SceneHorseData));
    scene->init = horse_init;
    scene->update = horse_update;
    scene->draw = horse_draw;
    scene->destroy = horse_destroy;
    return scene;
}
