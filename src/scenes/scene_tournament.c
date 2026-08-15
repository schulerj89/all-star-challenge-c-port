#include "allstar_scene.h"
#include "allstar_game.h"
#include <stdlib.h>

typedef struct {
    float timer;
} SceneTournamentData;

static void tournament_init(AllStarScene *scene, AllStarGame *game) {
    SceneTournamentData *data = (SceneTournamentData*)scene->user_data;
    data->timer = 0.0f;
    if (!game->tournament.active) allstar_tournament_reset(&game->tournament);
}

static void tournament_update(AllStarScene *scene, AllStarGame *game, const AllStarInput *input, float dt) {
    SceneTournamentData *data = (SceneTournamentData*)scene->user_data;
    data->timer += dt;

    if (game->tournament.complete) {
        if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) ||
            allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
            game->tournament.active = false;
            allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);
        }
        return;
    }

    if (allstar_input_is_pressed(input, ALLSTAR_BTN_A) ||
        allstar_input_is_pressed(input, ALLSTAR_BTN_START)) {
        uint32_t player_1;
        uint32_t player_2;
        if (!allstar_tournament_get_current_match(&game->tournament, &player_1, &player_2)) return;
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        game->selected_player_1 = player_1;
        game->selected_player_2 = player_2;
        game->tournament.match_in_progress = true;
        allstar_game_change_scene(game, ALLSTAR_SCENE_ONE_ON_ONE);
    }
}

static void tournament_draw(AllStarScene *scene, AllStarGame *game, AllStarRenderer *renderer) {
    AllStarTournamentState *tournament = &game->tournament;
    (void)scene;
    allstar_renderer_clear(renderer, 0);

    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    allstar_renderer_draw_text(renderer, "TOURNAMENT BRACKET", 8, 4, 0);

    for (int i = 0; i < 4; i++) {
        int y = 24 + i * 26;
        const AllStarPlayerStats *p1;
        const AllStarPlayerStats *p2;
        allstar_renderer_draw_rect_fill(renderer, 6, y, 70, 22, 1);
        allstar_renderer_draw_rect_outline(renderer, 6, y, 70, 22,
            (tournament->round == 0 && i == tournament->current_match) ? 3 : 2);

        p1 = allstar_roster_get_player(&game->roster, tournament->seeds[i * 2]);
        p2 = allstar_roster_get_player(&game->roster, tournament->seeds[i * 2 + 1]);
        if (p1) allstar_renderer_draw_text(renderer, p1->last_name, 10, y + 3, 3);
        if (p2) allstar_renderer_draw_text(renderer, p2->last_name, 10, y + 12, 3);
        allstar_renderer_draw_line(renderer, 76, y + 11, 86, y + 11, 2);
    }

    allstar_renderer_draw_line(renderer, 86, 35, 86, 61, 2);
    allstar_renderer_draw_line(renderer, 86, 48, 106, 48, 2);
    allstar_renderer_draw_line(renderer, 86, 87, 86, 113, 2);
    allstar_renderer_draw_line(renderer, 86, 100, 106, 100, 2);
    allstar_renderer_draw_rect_fill(renderer, 106, 62, 48, 24, 1);
    allstar_renderer_draw_rect_outline(renderer, 106, 62, 48, 24, 2);
    allstar_renderer_draw_text(renderer, "FINALS", 110, 70, 3);

    if (tournament->complete) {
        const AllStarPlayerStats *champion = allstar_roster_get_player(&game->roster, tournament->champion);
        allstar_renderer_draw_text(renderer, "CHAMPION", 88, 96, 3);
        if (champion) allstar_renderer_draw_text(renderer, champion->last_name, 88, 108, 3);
        allstar_renderer_draw_text(renderer, "PRESS A", 92, 126, 3);
    } else {
        static const char *ROUND_NAMES[3] = { "QUARTER", "SEMIFINAL", "FINAL" };
        int round = tournament->round;
        if (round < 0 || round > 2) round = 0;
        allstar_renderer_draw_text(renderer, ROUND_NAMES[round], 88, 108, 3);
        allstar_renderer_draw_text(renderer, "PRESS A", 92, 126, 3);
    }
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
