#ifndef ALLSTAR_GAME_H
#define ALLSTAR_GAME_H

#include "allstar_types.h"
#include "allstar_controls.h"
#include "allstar_renderer.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_audio.h"
#include "allstar_scene.h"

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
    uint32_t selected_mode;
    float frame_time;
} AllStarGame;

bool allstar_game_init(AllStarGame *game, const char *asset_pack_path);
void allstar_game_shutdown(AllStarGame *game);
void allstar_game_change_scene(AllStarGame *game, AllStarSceneId scene_id);
void allstar_game_tick(AllStarGame *game, float dt);

#endif /* ALLSTAR_GAME_H */
