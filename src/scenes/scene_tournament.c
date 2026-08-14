#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    uint32_t seeds[8];
    int round; /* 0 = Quarterfinals, 1 = Semifinals, 2 = Finals */
    int current_match;
    bool bracket_active;
} SceneTournamentData;

static void tournament_init(AllStarScene *scene, AllStarGame *game) {
    (void)game;
    SceneTournamentData *data = (SceneTournamentData*)scene->user_data;
    /* Default seeds */
    data->seeds[0] = 13; /* Jordan (#1 seed) */
    data->seeds[1] = 2;  /* Bird */
    data->seeds[2] = 1;  /* Barkley */
    data->seeds[3] = 11; /* Ewing */
    data->seeds[4] = 15; /* Karl Malone */
    data->seeds[5] = 18; /* Olajuwon */
    data->seeds[6] = 25; /* Wilkins */
    data->seeds[7] = 8;  /* Drexler */
    data->round = 0;
    data->current_match = 0;
    data->bracket_active = true;
}

static void tournament_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    (void)dt;
    SceneTournamentData *data = (SceneTournamentData*)scene->user_data;

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) || allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        /* Set tournament match contestants */
        game->selected_player_1 = data->seeds[data->current_match * 2];
        game->selected_player_2 = data->seeds[data->current_match * 2 + 1];
        allstar_game_change_scene(game, ALLSTAR_SCENE_ONE_ON_ONE);
    }
}

static void tournament_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    SceneTournamentData *data = (SceneTournamentData*)scene->user_data;
    allstar_renderer_clear(renderer, 0);

    /* Top HUD */
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    allstar_renderer_draw_text(renderer, "TOURNAMENT BRACKET", 8, 4, 0);

    /* Bracket Tree Lines */
    for (int i = 0; i < 4; i++) {
        int y = 24 + i * 26;
        /* Match Box */
        allstar_renderer_draw_rect_fill(renderer, 6, y, 70, 22, 1);
        allstar_renderer_draw_rect_outline(renderer, 6, y, 70, 22, (i == data->current_match) ? 3 : 2);

        const AllStarPlayerStats *p1 = allstar_roster_get_player(&game->roster, data->seeds[i * 2]);
        const AllStarPlayerStats *p2 = allstar_roster_get_player(&game->roster, data->seeds[i * 2 + 1]);

        if (p1) allstar_renderer_draw_text(renderer, p1->last_name, 10, y + 3, 3);
        if (p2) allstar_renderer_draw_text(renderer, p2->last_name, 10, y + 12, 3);

        /* Branch Lines */
        allstar_renderer_draw_line(renderer, 76, y + 11, 86, y + 11, 2);
    }

    /* Semifinals Connecting Lines */
    allstar_renderer_draw_line(renderer, 86, 35, 86, 61, 2);
    allstar_renderer_draw_line(renderer, 86, 48, 106, 48, 2);

    allstar_renderer_draw_line(renderer, 86, 87, 86, 113, 2);
    allstar_renderer_draw_line(renderer, 86, 100, 106, 100, 2);

    /* Finals Box */
    allstar_renderer_draw_rect_fill(renderer, 106, 62, 48, 24, 1);
    allstar_renderer_draw_rect_outline(renderer, 106, 62, 48, 24, 2);
    allstar_renderer_draw_text(renderer, "FINALS", 110, 70, 3);

    /* Bottom Prompt */
    allstar_renderer_draw_text(renderer, "PRESS A TO PLAY", 20, 130, 3);
}

static void tournament_destroy(AllStarScene *scene) {
    if (scene) {
        if (scene->user_data) free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_tournament(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_TOURNAMENT;
    scene->user_data = calloc(1, sizeof(SceneTournamentData));
    scene->init = tournament_init;
    scene->update = tournament_update;
    scene->draw = tournament_draw;
    scene->destroy = tournament_destroy;
    return scene;
}
