#ifndef ALLSTAR_GAME_H
#define ALLSTAR_GAME_H

#include "allstar_types.h"
#include "allstar_controls.h"
#include "allstar_renderer.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_audio.h"
#include "allstar_scene.h"
#include "allstar_one_on_one.h"
#include "allstar_rng.h"

typedef enum {
    ALLSTAR_MODE_ONE_ON_ONE = 0,
    ALLSTAR_MODE_FREE_THROW,
    ALLSTAR_MODE_HORSE,
    ALLSTAR_MODE_ACCURACY,
    ALLSTAR_MODE_TOURNAMENT,
    ALLSTAR_MODE_COUNT
} AllStarGameMode;

typedef struct {
    uint8_t play_to;
    uint8_t skill_level;
    bool winners_outs;
    uint8_t game_minutes;
    uint8_t free_throw_attempts;
    bool accuracy_computer_positions;
} AllStarGameSettings;

typedef struct {
    uint32_t seeds[8];
    uint32_t semifinalists[4];
    uint32_t finalists[2];
    uint32_t champion;
    int round;
    int current_match;
    bool active;
    bool match_in_progress;
    bool complete;
} AllStarTournamentState;

typedef struct AllStarGame {
    AllStarRenderer *renderer;
    AllStarAssetPack *asset_pack;
    AllStarRoster roster;
    AllStarAudioEngine audio;
    AllStarInput input;
    AllStarScene *active_scene;
    AllStarSceneId requested_scene_id;
    bool is_running;
    uint32_t selected_player_1;
    uint32_t selected_player_2;
    AllStarGameMode selected_mode;
    AllStarGameSettings settings;
    AllStarOneOnOneMatch one_on_one;
    AllStarRomRng one_on_one_rng;
    AllStarTournamentState tournament;
    float one_on_one_shot_clock_seconds;
    int last_match_winner;
    float frame_time;
} AllStarGame;

bool allstar_game_init(AllStarGame *game, const char *asset_pack_path);
void allstar_game_shutdown(AllStarGame *game);
void allstar_game_change_scene(AllStarGame *game, AllStarSceneId scene_id);
void allstar_game_tick(AllStarGame *game, float dt);
AllStarGameMode allstar_game_mode_from_menu_index(uint32_t menu_index);
AllStarSceneId allstar_game_mode_scene(AllStarGameMode mode);
bool allstar_game_mode_requires_opponent(AllStarGameMode mode);
bool allstar_game_mode_uses_settings(AllStarGameMode mode);
const char* allstar_game_mode_name(AllStarGameMode mode);
void allstar_game_settings_init(AllStarGameSettings *settings);
uint8_t allstar_game_settings_cycle_time(uint8_t current, int direction);
uint8_t allstar_game_settings_cycle_throws(uint8_t current, int direction);
float allstar_game_settings_time_seconds(const AllStarGameSettings *settings);
void allstar_tournament_reset(AllStarTournamentState *tournament);
bool allstar_tournament_get_current_match(const AllStarTournamentState *tournament,
                                          uint32_t *player_1,
                                          uint32_t *player_2);
bool allstar_tournament_record_winner(AllStarTournamentState *tournament,
                                      uint32_t winner);

#endif /* ALLSTAR_GAME_H */
