#ifndef ALLSTAR_SCENE_H
#define ALLSTAR_SCENE_H

#include "allstar_types.h"
#include "allstar_controls.h"
#include "allstar_renderer.h"

struct AllStarGame;

typedef enum {
    ALLSTAR_SCENE_INTRO = 0,
    ALLSTAR_SCENE_MENU,
    ALLSTAR_SCENE_SETTINGS,
    ALLSTAR_SCENE_ROSTER_SELECT,
    ALLSTAR_SCENE_ONE_ON_ONE,
    ALLSTAR_SCENE_THREE_POINT,
    ALLSTAR_SCENE_FREE_THROW,
    ALLSTAR_SCENE_HORSE,
    ALLSTAR_SCENE_TOURNAMENT,
    ALLSTAR_SCENE_COUNT
} AllStarSceneId;

typedef struct AllStarScene {
    AllStarSceneId id;
    void *user_data;
    void (*init)(struct AllStarScene *scene, struct AllStarGame *game);
    void (*update)(struct AllStarScene *scene, struct AllStarGame *game, const AllStarInput *input, float dt);
    void (*draw)(struct AllStarScene *scene, struct AllStarGame *game, AllStarRenderer *renderer);
    void (*destroy)(struct AllStarScene *scene);
} AllStarScene;

AllStarScene* allstar_scene_create_intro(void);
AllStarScene* allstar_scene_create_menu(void);
AllStarScene* allstar_scene_create_settings(void);
AllStarScene* allstar_scene_create_roster_select(void);
AllStarScene* allstar_scene_create_one_on_one(void);
AllStarScene* allstar_scene_create_three_point(void);
AllStarScene* allstar_scene_create_free_throw(void);
AllStarScene* allstar_scene_create_horse(void);
AllStarScene* allstar_scene_create_tournament(void);

/* Deterministic headless-test placement; production gameplay never calls it. */
bool allstar_scene_one_on_one_set_test_positions(AllStarScene *scene,
                                                 float p1_x, float p1_y,
                                                 float p2_x, float p2_y);

typedef struct {
    float p1_x;
    float p1_y;
    float p2_x;
    float p2_y;
    float ball_x;
    float ball_y;
    float ball_z;
    uint8_t p1_action;
    uint8_t p2_action;
    uint8_t p1_record;
    uint8_t p2_record;
    uint8_t cpu_state;
    uint8_t cpu_offense_stage;
    uint8_t cpu_target_x;
    uint8_t cpu_target_y;
    uint32_t rim_audio_events;
    uint32_t steal_transfer_events;
    uint32_t foul_events;
    uint16_t foul_elapsed_frames;
    uint8_t foul_violation;
    bool p1_has_ball;
    bool p2_has_ball;
    bool ball_in_flight;
    bool ball_recoverable;
    bool p1_defense_jump_active;
    bool p2_defense_jump_active;
    bool score_presentation_active;
    bool foul_presentation_active;
    bool foul_message_visible;
} AllStarOneOnOneDebugState;

/* Deterministic scene integration probes used by the CLI regression suite. */
bool allstar_scene_one_on_one_set_test_possession(
    AllStarScene *scene, struct AllStarGame *game, int player);
bool allstar_scene_one_on_one_get_debug_state(
    const AllStarScene *scene, AllStarOneOnOneDebugState *state);
bool allstar_scene_one_on_one_set_test_ball_rom(
    AllStarScene *scene, uint16_t x, uint16_t y, uint16_t z,
    int16_t vx, int16_t vy, int16_t vz, int shooter);
bool allstar_scene_one_on_one_set_test_player_state(
    AllStarScene *scene, int player, uint8_t action, uint8_t record,
    uint8_t previous_direction, bool horizontal_flip);
bool allstar_scene_one_on_one_try_test_steal(
    AllStarScene *scene, struct AllStarGame *game, int defender);
bool allstar_scene_one_on_one_begin_test_foul(
    AllStarScene *scene, struct AllStarGame *game,
    uint8_t violation, int offender);

#endif /* ALLSTAR_SCENE_H */
