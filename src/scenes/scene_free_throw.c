#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_physics.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int attempts;
    int attempt_limit;
    int makes;
    float gauge_x;
    float gauge_y;
    float gauge_dx;
    float gauge_dy;
    bool shooting;
    AllStarBall ball;
} SceneFreeThrowData;

static void free_throw_init(AllStarScene *scene, AllStarGame *game) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    data->attempts = 0;
    data->attempt_limit = game->settings.free_throw_attempts;
    data->makes = 0;
    data->gauge_x = 50.0f;
    data->gauge_y = 50.0f;
    data->gauge_dx = 80.0f;
    data->gauge_dy = 60.0f;
    data->shooting = false;
    allstar_physics_init_ball(&data->ball);
}

static void free_throw_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;

    data->gauge_x += data->gauge_dx * dt;
    if (data->gauge_x >= 100.0f || data->gauge_x <= 0.0f) data->gauge_dx = -data->gauge_dx;

    data->gauge_y += data->gauge_dy * dt;
    if (data->gauge_y >= 100.0f || data->gauge_y <= 0.0f) data->gauge_dy = -data->gauge_dy;

    if (data->attempts < data->attempt_limit && !data->ball.in_flight &&
        allstar_input_is_pressed(input, ALLSTAR_BTN_A)) {
        data->attempts++;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);

        bool accurate = (data->gauge_x >= 38.0f && data->gauge_x <= 62.0f &&
                         data->gauge_y >= 38.0f && data->gauge_y <= 62.0f);

        float target_offset = accurate ? 0.0f : ((data->gauge_x - 50.0f) * 0.4f);
        allstar_physics_shoot_ball(&data->ball, 80.0f, 96.0f, 80.0f + target_offset, 26.0f, 44.0f, 1, 1);
    }

    allstar_physics_update_ball(&data->ball, dt);
    if (allstar_physics_check_basket(&data->ball, 80.0f, 26.0f, 16.0f)) {
        data->ball.made_basket = true;
        data->makes++;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SWISH);
    }
}

static void free_throw_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    (void)game;
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Top HUD */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    char buf[32];
    snprintf(buf, sizeof(buf), "FT %02d  TRY %02d/%02d",
             data->makes, data->attempts, data->attempt_limit);
    allstar_renderer_draw_text(renderer, buf, 10, 4, 0);

    /* Free Throw Key / Paint */
    allstar_renderer_draw_rect_fill(renderer, 60, 20, 40, 75, 1);
    allstar_renderer_draw_rect_outline(renderer, 60, 20, 40, 75, 2);

    /* Free Throw Circle */
    for (int deg = 0; deg < 360; deg += 15) {
        float rad = (float)deg * 3.14159f / 180.0f;
        int cx = 80 + (int)(cosf(rad) * 18.0f);
        int cy = 95 + (int)(sinf(rad) * 10.0f);
        allstar_renderer_set_pixel(renderer, cx, cy, 2);
    }

    /* Backboard & Rim */
    allstar_renderer_draw_line(renderer, 70, 22, 90, 22, 3);
    allstar_renderer_draw_line(renderer, 76, 26, 84, 26, 3);
    allstar_renderer_draw_line(renderer, 78, 27, 82, 30, 2);

    /* Shooter at Line */
    const AllStarPlayerStats *p = allstar_roster_get_player(&game->roster, game->selected_player_1);
    bool is_dark = (p && p->skin_tone == 0x90);
    allstar_renderer_draw_player(renderer, 80, 96, is_dark, !data->ball.in_flight, data->ball.in_flight, 0.0f);

    /* Ball */
    if (data->ball.in_flight) {
        allstar_renderer_draw_ball(renderer, (int)data->ball.x, (int)data->ball.y, (int)data->ball.z);
    }

    /* Target Reticle Box (Aim Window) */
    allstar_renderer_draw_rect_fill(renderer, 10, 110, 140, 28, 1);
    allstar_renderer_draw_rect_outline(renderer, 10, 110, 140, 28, 3);
    allstar_renderer_draw_text(renderer, "AIM", 14, 114, 3);

    /* Target Box */
    allstar_renderer_draw_rect_fill(renderer, 50, 113, 60, 22, 0);
    allstar_renderer_draw_rect_outline(renderer, 50, 113, 60, 22, 3);

    /* Sweet Spot Center */
    allstar_renderer_draw_rect_fill(renderer, 74, 120, 12, 8, 2);

    /* Moving Crosshair */
    int ret_x = 50 + (int)(data->gauge_x * 0.58f);
    int ret_y = 113 + (int)(data->gauge_y * 0.20f);
    allstar_renderer_draw_line(renderer, ret_x - 3, ret_y, ret_x + 3, ret_y, 3);
    allstar_renderer_draw_line(renderer, ret_x, ret_y - 3, ret_x, ret_y + 3, 3);
}

static void free_throw_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_free_throw(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_FREE_THROW;
    scene->user_data = calloc(1, sizeof(SceneFreeThrowData));
    scene->init = free_throw_init;
    scene->update = free_throw_update;
    scene->draw = free_throw_draw;
    scene->destroy = free_throw_destroy;
    return scene;
}
