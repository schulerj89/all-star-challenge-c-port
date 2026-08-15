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

#endif /* ALLSTAR_SCENE_H */
