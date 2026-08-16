#include "allstar_scene.h"
#include "allstar_free_throw.h"
#include "allstar_game.h"
#include "allstar_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    AllStarFreeThrowState mode;
    AllStarRomRng rng;
    float step_accumulator;
    float anim_time;
    uint32_t last_events;
} SceneFreeThrowData;

static void free_throw_init(AllStarScene *scene, AllStarGame *game) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    memset(data, 0, sizeof(*data));
    allstar_rom_rng_init(&data->rng, 0xe018);
    allstar_free_throw_init(&data->mode, game->settings.free_throw_attempts,
        allstar_rom_rng_current(&data->rng),
        (uint8_t)game->selected_player_1);
    /* Free Throw entry $0C8E clears $DD73, stopping the roster/title song. */
    allstar_audio_stop_bgm(&game->audio);
}

static void play_rom_events(AllStarGame *game, uint32_t events) {
    if (events & ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT)
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_FREE_THROW_CONTACT);
    if (events & ALLSTAR_FREE_THROW_EVENT_RIM)
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
    if (events & ALLSTAR_FREE_THROW_EVENT_NET)
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_FREE_THROW_NET);
    if (events & ALLSTAR_FREE_THROW_EVENT_SCORE)
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SCORE_CHIME);
}

static void free_throw_update(AllStarScene *scene, AllStarGame *game,
                              const AllStarInput *input, float dt) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    AllStarInput repeated_input;
    if (data->mode.phase == ALLSTAR_FREE_THROW_RESULT &&
        (allstar_input_is_pressed(input, ALLSTAR_BTN_A) ||
         allstar_input_is_pressed(input, ALLSTAR_BTN_START))) {
        allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
        allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);
        return;
    }

    data->step_accumulator += dt;
    data->anim_time += dt;
    data->last_events = 0;
    repeated_input = *input;
    while (data->step_accumulator + 0.000001f >=
           ALLSTAR_PHYSICS_STEP_SECONDS) {
        uint8_t current_rng = allstar_rom_rng_current(&data->rng);
        uint32_t events = allstar_free_throw_tick_100f(
            &data->mode, repeated_input.buttons_held,
            repeated_input.buttons_pressed, current_rng);
        data->last_events |= events;
        play_rom_events(game, events);
        allstar_rom_rng_end_frame_0714(&data->rng, 0, 0);
        data->step_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        repeated_input.buttons_pressed = 0;
        repeated_input.buttons_released = 0;
    }
}

static uint8_t free_throw_net_frame(const AllStarFreeThrowState *mode) {
    static const uint8_t frames[8] = { 0, 1, 2, 3, 2, 1, 0, 0 };
    return frames[mode->net_state <= 7 ? mode->net_state : 7];
}

static bool has_free_throw_art(const AllStarRenderer *renderer) {
    return renderer && renderer->asset_pack &&
        (renderer->asset_pack->header.feature_flags &
         ALLSTAR_ASSET_FEATURE_FREE_THROW_ART) != 0;
}

static uint8_t free_throw_char_tile(char value) {
    if (value == ' ') return 0xc0;
    if (value >= '0' && value <= '9')
        return (uint8_t)(0xc1 + value - '0');
    if (value >= 'A' && value <= 'Z')
        return (uint8_t)(0xcb + value - 'A');
    return 0xc0;
}

static void free_throw_map_text(uint8_t map[32 * 32], int column, int row,
                                const char *text) {
    while (text && *text && column < 32) {
        map[row * 32 + column++] = free_throw_char_tile(*text++);
    }
}

static void free_throw_map_number(uint8_t map[32 * 32], int right_column,
                                  int row, unsigned value, int width) {
    int i;
    for (i = width - 1; i >= 0; i--) {
        map[row * 32 + right_column - (width - 1 - i)] =
            (uint8_t)(0xc1 + value % 10);
        value /= 10;
    }
}

static void free_throw_map_last_name(uint8_t map[32 * 32], int row,
                                     const char *last_name) {
    size_t length = last_name ? strlen(last_name) : 0;
    int column;
    int i;
    if (length > 9) length = 9;
    for (i = 0; i < 9; i++) map[row * 32 + 5 + i] = 0xc0;
    column = 5 + (9 - (int)length) / 2;
    free_throw_map_text(map, column, row, last_name);
}

static void draw_free_throw_map(AllStarRenderer *renderer,
                                const uint8_t map[32 * 32]) {
    const AllStarAssetPack *pack = renderer->asset_pack;
    int row;
    int column;
    for (row = 0; row < 18; row++) {
        for (column = 0; column < 20; column++) {
            uint8_t tile = map[row * 32 + column];
            allstar_renderer_draw_tile(renderer,
                &pack->free_throw_bg_tiles[tile],
                column * 8, row * 8, false, false);
        }
    }
}

static void draw_free_throw_ball_1884(AllStarRenderer *renderer,
                                      const AllStarFreeThrowState *mode) {
    const AllStarAssetPack *pack = renderer->asset_pack;
    uint8_t raw_y = mode->oam_y;
    uint8_t raw_x = mode->oam_x;
    uint8_t band = mode->oam_band;
    int row;
    int column;
    for (row = 0; row < 4; row++) {
        /* $1E0E seeds $C12B=$2D and $1C1D turns that into OBJ priority for
           all four rows. The software renderer sees the post-update C12B
           immediately, so include it here rather than showing one stale
           foreground frame before the next OAM composition pass. */
        bool behind_bg = mode->priority_timer != 0 ||
            (mode->oam_priority_rows & (1u << row)) != 0;
        for (column = 0; column < 4; column++) {
            uint8_t tile_index =
                pack->free_throw_ball_maps[band][row * 4 + column];
            const AllStarTile *tile = &pack->free_throw_obj_tiles[tile_index];
            int screen_x = (int)raw_x + column * 8 - 8;
            int screen_y = (int)raw_y + row * 8 - 16;
            int ty;
            int tx;
            for (ty = 0; ty < 8; ty++) {
                int y = screen_y + ty;
                if (y < 0 || (uint32_t)y >= renderer->height) continue;
                for (tx = 0; tx < 8; tx++) {
                    int x = screen_x + tx;
                    uint8_t shade;
                    if (x < 0 || (uint32_t)x >= renderer->width) continue;
                    shade = tile->pixels[ty * 8 + tx];
                    if (shade == 0) continue;
                    if (behind_bg && renderer->pixels[y * renderer->width + x] !=
                            renderer->current_palette.shades[0]) continue;
                    allstar_renderer_set_pixel(renderer, x, y, shade);
                }
            }
        }
    }
}

static void draw_free_throw_reticle_1a25(AllStarRenderer *renderer,
                                         const AllStarFreeThrowState *mode) {
    const AllStarTile *tile = &renderer->asset_pack->free_throw_reticle_tile;
    int screen_x = (int)mode->previous_aim_x - 8;
    int screen_y = (int)mode->previous_aim_y - 16;
    int y;
    int x;
    /* $2243 builds OAM entry C098 as {Y, X, $7F, $00}; $1A25 updates its
       raw OAM Y/X after the aim integrator every frame. */
    if (mode->previous_aim_x == 0 || mode->previous_aim_y == 0) return;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 8; x++) {
            uint8_t shade = tile->pixels[y * 8 + x];
            if (shade != 0)
                allstar_renderer_set_pixel(renderer,
                    screen_x + x, screen_y + y, shade);
        }
    }
}

static void draw_result(AllStarRenderer *renderer, AllStarGame *game,
                        const AllStarFreeThrowState *mode) {
    const AllStarPlayerStats *player = allstar_roster_get_player(
        &game->roster, game->selected_player_1);
    uint8_t map[32 * 32];
    char score[32];
    int x;
    if (has_free_throw_art(renderer)) {
        memset(map, 0, sizeof(map));
        free_throw_map_last_name(map, 4, player ? player->last_name : "");
        free_throw_map_text(map, 1, 7, "SHOTS ATTEMPTED");
        free_throw_map_number(map, 19, 7, mode->attempts_taken,
                              mode->attempts_taken >= 10 ? 2 : 1);
        free_throw_map_text(map, 1, 10, "SHOTS MADE");
        free_throw_map_number(map, 19, 10, mode->makes,
                              mode->makes >= 10 ? 2 : 1);
        draw_free_throw_map(renderer, map);
        return;
    }
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_rect_fill(renderer, 8, 16, 144, 104, 1);
    allstar_renderer_draw_rect_outline(renderer, 8, 16, 144, 104, 3);
    allstar_renderer_draw_text(renderer, "FREE THROWS", 36, 28, 3);
    if (player) {
        x = (160 - (int)strlen(player->name) * 8) / 2;
        if (x < 12) x = 12;
        allstar_renderer_draw_text(renderer, player->name, x, 52, 3);
    }
    snprintf(score, sizeof(score), "SCORE  %u / %u",
             (unsigned)mode->makes, (unsigned)mode->attempts_limit);
    allstar_renderer_draw_text(renderer, score, 28, 76, 3);
    allstar_renderer_draw_text(renderer, "A/START: CONTINUE", 12, 104, 2);
}

static void draw_free_throw_court(AllStarRenderer *renderer) {
    allstar_renderer_clear(renderer, 0);
    allstar_renderer_draw_rect_fill(renderer, 0, 16, 160, 128, 1);
    allstar_renderer_draw_rect_outline(renderer, 4, 18, 152, 122, 2);
    allstar_renderer_draw_rect_outline(renderer, 48, 18, 64, 96, 2);
    allstar_renderer_draw_line(renderer, 52, 20, 88, 20, 3);
    allstar_renderer_draw_rect_outline(renderer, 60, 20, 20, 14, 3);
    allstar_renderer_draw_line(renderer, 65, 30, 77, 30, 3);
    allstar_renderer_draw_line(renderer, 67, 31, 75, 38, 2);
    allstar_renderer_draw_line(renderer, 75, 31, 67, 38, 2);
    allstar_renderer_draw_line(renderer, 48, 112, 112, 112, 3);
}

static void free_throw_draw(AllStarScene *scene, AllStarGame *game,
                            AllStarRenderer *renderer) {
    SceneFreeThrowData *data = (SceneFreeThrowData*)scene->user_data;
    const AllStarFreeThrowState *mode = &data->mode;
    const AllStarPlayerStats *player;
    uint8_t skin;
    char hud[32];
    int ball_x, ball_y;
    bool ball_behind_net;

    if (mode->phase == ALLSTAR_FREE_THROW_RESULT) {
        draw_result(renderer, game, mode);
        return;
    }

    player = allstar_roster_get_player(&game->roster,
                                       game->selected_player_1);
    if (has_free_throw_art(renderer)) {
        uint8_t map[32 * 32];
        uint8_t pose = 0;
        uint8_t net = free_throw_net_frame(mode);
        unsigned shot_number = mode->attempts_taken;
        int row;
        memcpy(map, renderer->asset_pack->free_throw_tilemap, sizeof(map));
        if (mode->phase == ALLSTAR_FREE_THROW_PRESENTATION)
            pose = mode->presentation_frame < 10 ? 1 : 2;
        for (row = 0; row < 8; row++)
            memcpy(map + 0x145 + row * 32,
                   renderer->asset_pack->free_throw_pose_maps[pose] + row * 9,
                   9);
        for (row = 0; row < 5; row++)
            memcpy(map + 0x0a8 + row * 32,
                   renderer->asset_pack->free_throw_net_maps[net] + row * 3,
                   3);
        free_throw_map_last_name(map, 0, player ? player->last_name : "");
        free_throw_map_text(map, 0, 1, "SHOT");
        free_throw_map_text(map, 15, 1, "SCORE");
        if (mode->phase == ALLSTAR_FREE_THROW_AIMING) shot_number++;
        free_throw_map_number(map, 2, 3, shot_number, 2);
        free_throw_map_number(map, 18, 3, mode->makes, 3);
        draw_free_throw_map(renderer, map);
        /* OAM $C098 follows the 4x4 ball entries, so draw it first and let
           the lower-index ball objects win any overlap. */
        draw_free_throw_reticle_1a25(renderer, mode);
        draw_free_throw_ball_1884(renderer, mode);
        return;
    }

    draw_free_throw_court(renderer);
    snprintf(hud, sizeof(hud), "FT %02u  LEFT %02u/%02u",
             (unsigned)mode->makes, (unsigned)mode->attempts_remaining,
             (unsigned)mode->attempts_limit);
    allstar_renderer_draw_rect_fill(renderer, 0, 0, 160, 16, 3);
    allstar_renderer_draw_text(renderer, hud, 6, 4, 0);

    if (mode->phase == ALLSTAR_FREE_THROW_AIMING) {
        int aim_x = (int)(mode->aim_x >> 8);
        int aim_y = (int)(mode->aim_y >> 8);
        allstar_renderer_draw_line(renderer, aim_x - 4, aim_y,
                                   aim_x + 4, aim_y, 3);
        allstar_renderer_draw_line(renderer, aim_x, aim_y - 4,
                                   aim_x, aim_y + 4, 3);
        allstar_renderer_draw_rect_outline(renderer, aim_x - 6, aim_y - 6,
                                            13, 13, 2);
    }

    skin = player ? player->skin_tone : 0x90;
    allstar_renderer_draw_player_ex(
        renderer, 96, 112, true, skin, false,
        mode->phase == ALLSTAR_FREE_THROW_PRESENTATION, false,
        mode->phase == ALLSTAR_FREE_THROW_AIMING ? 0x0b : 0x12,
        (uint8_t)((mode->presentation_frame / 6) & 3),
        data->anim_time, true);

    allstar_free_throw_ball_screen_1884(mode, &ball_x, &ball_y);
    ball_behind_net = mode->made_current && mode->net_state < 3;
    if (ball_behind_net)
        allstar_renderer_draw_ball_ex(renderer, ball_x, ball_y, 0,
                                      data->anim_time);
    allstar_renderer_draw_net_overlay_1ecc(
        renderer, free_throw_net_frame(mode));
    if (!ball_behind_net)
        allstar_renderer_draw_ball_ex(renderer, ball_x, ball_y, 0,
                                      data->anim_time);

    if (mode->phase == ALLSTAR_FREE_THROW_AIMING)
        allstar_renderer_draw_text(renderer, "D-PAD AIM   A SHOOT", 4, 132, 3);
}

bool allstar_scene_free_throw_get_debug_state(
        const AllStarScene *scene, AllStarFreeThrowDebugState *debug) {
    const SceneFreeThrowData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_FREE_THROW || !scene->user_data ||
        !debug) return false;
    data = (const SceneFreeThrowData*)scene->user_data;
    memset(debug, 0, sizeof(*debug));
    debug->phase = (uint8_t)data->mode.phase;
    debug->attempts_taken = data->mode.attempts_taken;
    debug->attempts_remaining = data->mode.attempts_remaining;
    debug->makes = data->mode.makes;
    debug->aim_x = (uint8_t)(data->mode.aim_x >> 8);
    debug->aim_y = (uint8_t)(data->mode.aim_y >> 8);
    debug->ball_x = (uint8_t)(data->mode.ball.x >> 8);
    debug->ball_y = (uint8_t)(data->mode.ball.y >> 8);
    debug->ball_z = (uint8_t)(data->mode.ball.z >> 8);
    debug->presentation_frame = data->mode.presentation_frame;
    debug->made_current = data->mode.made_current;
    debug->last_events = data->last_events;
    return true;
}

bool allstar_scene_free_throw_set_test_aim(AllStarScene *scene,
                                           uint8_t x, uint8_t y) {
    SceneFreeThrowData *data;
    if (!scene || scene->id != ALLSTAR_SCENE_FREE_THROW || !scene->user_data)
        return false;
    data = (SceneFreeThrowData*)scene->user_data;
    allstar_free_throw_set_test_aim(&data->mode, x, y);
    return true;
}

static void free_throw_destroy(AllStarScene *scene) {
    if (scene) {
        free(scene->user_data);
        free(scene);
    }
}

AllStarScene* allstar_scene_create_free_throw(void) {
    AllStarScene *scene = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!scene) return NULL;
    scene->id = ALLSTAR_SCENE_FREE_THROW;
    scene->user_data = calloc(1, sizeof(SceneFreeThrowData));
    if (!scene->user_data) { free(scene); return NULL; }
    scene->init = free_throw_init;
    scene->update = free_throw_update;
    scene->draw = free_throw_draw;
    scene->destroy = free_throw_destroy;
    return scene;
}
