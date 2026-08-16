#include "allstar_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AllStarGameMode mode;
    const char *name;
    AllStarSceneId scene_id;
    bool requires_opponent;
    bool uses_settings;
} AllStarModeRoute;

/* ROM menu selector $FF8F uses these IDs in the displayed 0..4 order. */
static const AllStarModeRoute ALLSTAR_MODE_ROUTES[ALLSTAR_MODE_COUNT] = {
    { ALLSTAR_MODE_ONE_ON_ONE, "One On One",        ALLSTAR_SCENE_ONE_ON_ONE,  true,  true  },
    { ALLSTAR_MODE_FREE_THROW, "Free Throws",       ALLSTAR_SCENE_FREE_THROW,  false, true  },
    { ALLSTAR_MODE_HORSE,      "Horse",             ALLSTAR_SCENE_HORSE,       true,  false },
    { ALLSTAR_MODE_ACCURACY,   "Accuracy Shootout", ALLSTAR_SCENE_THREE_POINT, false, true  },
    { ALLSTAR_MODE_TOURNAMENT, "Tournament",        ALLSTAR_SCENE_TOURNAMENT,  true,  true  }
};

static const AllStarModeRoute* allstar_game_mode_route(AllStarGameMode mode) {
    if ((int)mode < 0 || mode >= ALLSTAR_MODE_COUNT) {
        return &ALLSTAR_MODE_ROUTES[ALLSTAR_MODE_ONE_ON_ONE];
    }
    return &ALLSTAR_MODE_ROUTES[mode];
}

AllStarGameMode allstar_game_mode_from_menu_index(uint32_t menu_index) {
    if (menu_index >= ALLSTAR_MODE_COUNT) return ALLSTAR_MODE_ONE_ON_ONE;
    return ALLSTAR_MODE_ROUTES[menu_index].mode;
}

AllStarSceneId allstar_game_mode_scene(AllStarGameMode mode) {
    return allstar_game_mode_route(mode)->scene_id;
}

bool allstar_game_mode_requires_opponent(AllStarGameMode mode) {
    return allstar_game_mode_route(mode)->requires_opponent;
}

bool allstar_game_mode_uses_settings(AllStarGameMode mode) {
    return allstar_game_mode_route(mode)->uses_settings;
}

const char* allstar_game_mode_name(AllStarGameMode mode) {
    return allstar_game_mode_route(mode)->name;
}

void allstar_game_settings_init(AllStarGameSettings *settings) {
    if (!settings) return;
    settings->play_to = 0;
    settings->skill_level = 1;
    settings->winners_outs = false;
    settings->game_minutes = 2;
    settings->free_throw_attempts = 5;
    settings->accuracy_computer_positions = true;
}

static uint8_t allstar_cycle_setting(const uint8_t *values,
                                     size_t count,
                                     uint8_t current,
                                     int direction) {
    size_t index = 0;
    while (index < count && values[index] != current) index++;
    if (index == count) index = 0;
    if (direction < 0) index = (index + count - 1) % count;
    else index = (index + 1) % count;
    return values[index];
}

uint8_t allstar_game_settings_cycle_time(uint8_t current, int direction) {
    static const uint8_t ROM_TIME_VALUES[] = { 2, 5, 8, 12 };
    return allstar_cycle_setting(ROM_TIME_VALUES,
                                 sizeof(ROM_TIME_VALUES) / sizeof(ROM_TIME_VALUES[0]),
                                 current, direction);
}

uint8_t allstar_game_settings_cycle_throws(uint8_t current, int direction) {
    static const uint8_t ROM_THROW_VALUES[] = { 5, 10, 20 };
    return allstar_cycle_setting(ROM_THROW_VALUES,
                                 sizeof(ROM_THROW_VALUES) / sizeof(ROM_THROW_VALUES[0]),
                                 current, direction);
}

float allstar_game_settings_time_seconds(const AllStarGameSettings *settings) {
    return settings ? (float)settings->game_minutes * 60.0f : 120.0f;
}

void allstar_tournament_reset(AllStarTournamentState *tournament) {
    static const uint32_t DEFAULT_SEEDS[8] = { 13, 2, 1, 11, 15, 18, 25, 8 };
    if (!tournament) return;
    memset(tournament, 0, sizeof(*tournament));
    memcpy(tournament->seeds, DEFAULT_SEEDS, sizeof(DEFAULT_SEEDS));
    tournament->active = true;
}

bool allstar_tournament_get_current_match(const AllStarTournamentState *tournament,
                                          uint32_t *player_1,
                                          uint32_t *player_2) {
    const uint32_t *round_players;
    int match_count;
    if (!tournament || !tournament->active || tournament->complete) return false;

    if (tournament->round == 0) {
        round_players = tournament->seeds;
        match_count = 4;
    } else if (tournament->round == 1) {
        round_players = tournament->semifinalists;
        match_count = 2;
    } else if (tournament->round == 2) {
        round_players = tournament->finalists;
        match_count = 1;
    } else {
        return false;
    }

    if (tournament->current_match < 0 || tournament->current_match >= match_count) return false;
    if (player_1) *player_1 = round_players[tournament->current_match * 2];
    if (player_2) *player_2 = round_players[tournament->current_match * 2 + 1];
    return true;
}

bool allstar_tournament_record_winner(AllStarTournamentState *tournament,
                                      uint32_t winner) {
    uint32_t player_1;
    uint32_t player_2;
    if (!tournament || !tournament->active || tournament->complete ||
        !tournament->match_in_progress) {
        return false;
    }
    if (!allstar_tournament_get_current_match(tournament, &player_1, &player_2) ||
        (winner != player_1 && winner != player_2)) {
        return false;
    }

    tournament->match_in_progress = false;
    if (tournament->round == 0) {
        tournament->semifinalists[tournament->current_match] = winner;
        tournament->current_match++;
        if (tournament->current_match == 4) {
            tournament->round = 1;
            tournament->current_match = 0;
        }
    } else if (tournament->round == 1) {
        tournament->finalists[tournament->current_match] = winner;
        tournament->current_match++;
        if (tournament->current_match == 2) {
            tournament->round = 2;
            tournament->current_match = 0;
        }
    } else if (tournament->round == 2) {
        tournament->champion = winner;
        tournament->complete = true;
    } else {
        return false;
    }

    return true;
}

bool allstar_game_init(AllStarGame *game, const char *asset_pack_path) {
    if (!game) return false;
    memset(game, 0, sizeof(AllStarGame));

    /* Initialize Renderer */
    game->renderer = (AllStarRenderer*)malloc(sizeof(AllStarRenderer));
    if (!game->renderer ||
        !allstar_renderer_init(game->renderer, ALLSTAR_GB_WIDTH,
                               ALLSTAR_GB_HEIGHT)) {
        fprintf(stderr, "[Game] Failed to initialize renderer\n");
        free(game->renderer);
        game->renderer = NULL;
        return false;
    }

    /* Initialize Asset Pack */
    game->asset_pack = (AllStarAssetPack*)malloc(sizeof(AllStarAssetPack));
    if (!game->asset_pack) {
        allstar_renderer_free(game->renderer);
        free(game->renderer);
        game->renderer = NULL;
        return false;
    }
    if (asset_pack_path) {
        if (!allstar_asset_pack_load_file(game->asset_pack,
                                          asset_pack_path)) {
            fprintf(stderr, "[Game] Refusing invalid asset pack: %s\n",
                    asset_pack_path);
            free(game->asset_pack);
            game->asset_pack = NULL;
            allstar_renderer_free(game->renderer);
            free(game->renderer);
            game->renderer = NULL;
            return false;
        }
    } else {
        allstar_asset_pack_init_default(game->asset_pack);
    }
    allstar_renderer_set_asset_pack(game->renderer, game->asset_pack);

    /* Initialize Roster */
    allstar_roster_load_from_asset_pack(&game->roster, game->asset_pack);

    /* Initialize Audio & Controls */
    allstar_audio_init(&game->audio);
    if ((game->asset_pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_GAMEPLAY_AUDIO) != 0 &&
        !allstar_audio_bind_rom_sfx(&game->audio, game->asset_pack)) {
        fprintf(stderr, "[Game] Invalid decoded ROM audio or title music\n");
        return false;
    }
    allstar_input_init(&game->input);

    game->is_running = true;
    game->selected_player_1 = 0;
    game->selected_player_2 = 1;
    game->selected_mode = ALLSTAR_MODE_ONE_ON_ONE;
    allstar_game_settings_init(&game->settings);
    game->one_on_one_shot_clock_seconds = 24.0f;

    /* Start in Intro Scene */
    allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);

    return true;
}

void allstar_game_shutdown(AllStarGame *game) {
    if (!game) return;

    allstar_audio_shutdown(&game->audio);

    if (game->active_scene) {
        if (game->active_scene->destroy) game->active_scene->destroy(game->active_scene);
        game->active_scene = NULL;
    }

    if (game->renderer) {
        allstar_renderer_free(game->renderer);
        free(game->renderer);
        game->renderer = NULL;
    }

    if (game->asset_pack) {
        free(game->asset_pack);
        game->asset_pack = NULL;
    }

    game->is_running = false;
}

void allstar_game_change_scene(AllStarGame *game, AllStarSceneId scene_id) {
    if (!game) return;

    if (game->active_scene) {
        if (game->active_scene->destroy) game->active_scene->destroy(game->active_scene);
        game->active_scene = NULL;
    }

    switch (scene_id) {
        case ALLSTAR_SCENE_INTRO:
            game->active_scene = allstar_scene_create_intro();
            break;
        case ALLSTAR_SCENE_MENU:
            game->active_scene = allstar_scene_create_menu();
            break;
        case ALLSTAR_SCENE_SETTINGS:
            game->active_scene = allstar_scene_create_settings();
            break;
        case ALLSTAR_SCENE_ROSTER_SELECT:
            game->active_scene = allstar_scene_create_roster_select();
            break;
        case ALLSTAR_SCENE_ONE_ON_ONE:
            game->active_scene = allstar_scene_create_one_on_one();
            break;
        case ALLSTAR_SCENE_THREE_POINT:
            game->active_scene = allstar_scene_create_three_point();
            break;
        case ALLSTAR_SCENE_FREE_THROW:
            game->active_scene = allstar_scene_create_free_throw();
            break;
        case ALLSTAR_SCENE_HORSE:
            game->active_scene = allstar_scene_create_horse();
            break;
        case ALLSTAR_SCENE_TOURNAMENT:
            game->active_scene = allstar_scene_create_tournament();
            break;
        default:
            game->active_scene = allstar_scene_create_intro();
            break;
    }

    if (game->active_scene && game->active_scene->init) {
        game->active_scene->init(game->active_scene, game);
    }
}

void allstar_game_tick(AllStarGame *game, float dt) {
    bool tick_one_on_one_rng;
    if (!game || !game->is_running) return;

    tick_one_on_one_rng = game->active_scene &&
        game->active_scene->id == ALLSTAR_SCENE_ONE_ON_ONE &&
        game->one_on_one.phase == ALLSTAR_ONE_ON_ONE_PLAYING;

    if (game->active_scene && game->active_scene->update) {
        game->active_scene->update(game->active_scene, game, &game->input, dt);
    }

    /* The cartridge calls $0714 at $0B68 after the per-frame gameplay work,
       including paths that return early from the mode controller. */
    if (tick_one_on_one_rng) {
        allstar_rom_rng_end_frame_0714(
            &game->one_on_one_rng,
            allstar_rom_bcd_byte((uint8_t)game->one_on_one.p1_score),
            allstar_rom_bcd_byte(
                (uint8_t)((int)game->one_on_one.game_clock % 60)));
    }

    allstar_audio_update(&game->audio, dt);

    if (game->active_scene && game->active_scene->draw && game->renderer) {
        game->active_scene->draw(game->active_scene, game, game->renderer);
    }

    if (game->renderer) {
        allstar_renderer_present(game->renderer);
    }
}
