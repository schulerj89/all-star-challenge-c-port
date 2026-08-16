#include "allstar_scene.h"
#include "allstar_game.h"
#include "allstar_accuracy.h"
#include "allstar_physics.h"
#include "allstar_rng.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ACCURACY_MOVE_PERIOD 6
#define ACCURACY_MISS_FRAMES 64
#define ACCURACY_MAKE_FRAMES 80

typedef enum {
    ACCURACY_DEFINE = 0, ACCURACY_APPROACH, ACCURACY_CONTROL,
    ACCURACY_GATHER, ACCURACY_FLIGHT, ACCURACY_RESULT
} AccuracyPhase;

typedef struct {
    AllStarAccuracyState mode;
    AllStarRomRng rng;
    AllStarPlayerState player;
    AllStarBall ball;
    AllStarRomAnimationState animation;
    AllStarOneOnOneShotAttempt shot;
    AccuracyPhase phase;
    float fixed_accumulator, anim_time, shot_animation_clock;
    uint32_t frames_remaining;
    uint16_t phase_frames, shot_frames, made_frame, result_frames;
    uint8_t shot_action, input_direction, previous_direction;
    uint8_t marker_x, marker_y;
    bool horizontal_flip, shot_made;
    bool rim_audio, contact_audio, net_audio, score_audio, result_audio;
} AccuracyData;

static uint8_t direction(uint8_t held) {
    return held & (ALLSTAR_BTN_RIGHT | ALLSTAR_BTN_LEFT |
                   ALLSTAR_BTN_UP | ALLSTAR_BTN_DOWN);
}

static void reset_player(AccuracyData *d) {
    memset(&d->player, 0, sizeof(d->player));
    d->player.x = 0x50; d->player.y = 0x60; d->player.has_ball = true;
    d->previous_direction = ALLSTAR_BTN_UP;
    d->horizontal_flip = allstar_one_on_one_rom_shot_horizontal_flip_7138(
        d->player.x);
    allstar_one_on_one_rom_animation_init_6a8c(&d->animation, 0x0b);
    allstar_one_on_one_shot_reset(&d->shot);
    allstar_physics_init_ball(&d->ball);
}

static void next_position(AccuracyData *d) {
    allstar_accuracy_next_position_6ca2(
        &d->mode, allstar_rom_rng_current(&d->rng));
    reset_player(d);
    d->phase = ACCURACY_APPROACH; d->phase_frames = 0;
    d->shot_frames = d->made_frame = 0; d->shot_made = false;
    d->rim_audio = d->contact_audio = d->net_audio = d->score_audio = false;
}

static void accuracy_init(AllStarScene *scene, AllStarGame *game) {
    AccuracyData *d = (AccuracyData*)scene->user_data;
    memset(d, 0, sizeof(*d));
    allstar_accuracy_init_0e51_6c9b(
        &d->mode, game->settings.accuracy_computer_positions);
    allstar_rom_rng_init(&d->rng, 0xe018);
    d->frames_remaining = (uint32_t)(
        allstar_game_settings_time_seconds(&game->settings) * 60.0f);
    reset_player(d);
    if (d->mode.computer_positions) next_position(d);
    else {
        d->marker_x = 0x54; d->marker_y = 0x80; d->phase = ACCURACY_DEFINE;
    }
    /* Fixed $0E51 clears music state $DD73. */
    allstar_audio_stop_bgm(&game->audio);
}

static void begin_gather(AccuracyData *d) {
    uint8_t variant = allstar_one_on_one_rom_shot_variant(
        d->player.x, d->player.y);
    d->shot_action = variant == 1 ? ALLSTAR_ROM_SHOT_ACTION_A
                                  : ALLSTAR_ROM_SHOT_ACTION_B;
    d->horizontal_flip = allstar_one_on_one_rom_shot_horizontal_flip_7138(
        d->player.x);
    d->player.is_shooting = d->player.is_jumping = true;
    allstar_one_on_one_shot_reset(&d->shot);
    allstar_one_on_one_shot_press(&d->shot, 1);
    allstar_one_on_one_rom_animation_set_action_6a8c(
        &d->animation, d->shot_action);
    d->shot_animation_clock = ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS;
    d->phase = ACCURACY_GATHER; d->phase_frames = 0;
}

static void launch(AccuracyData *d, AllStarGame *game) {
    uint8_t variant = allstar_one_on_one_rom_shot_variant(
        d->player.x, d->player.y);
    uint8_t distance = allstar_one_on_one_rom_shot_distance_class(
        d->player.x, d->player.y);
    uint8_t index = allstar_one_on_one_rom_shot_record_index(
        d->shot.rom_elapsed_frames);
    float x = d->player.x, y = d->player.y;
    float z = (float)allstar_one_on_one_rom_shot_release_height(index);
    AllStarOneOnOneReleaseOffset offset;
    if (allstar_one_on_one_rom_release_offset(
            d->shot_action, d->shot.rom_phase, variant,
            d->horizontal_flip, &offset)) {
        x += (float)offset.x_offset - ALLSTAR_ROM_PLAYER_X_TO_CENTER;
        y += (float)offset.ground_y_offset;
    }
    d->player.has_ball = d->player.is_jumping = false;
    allstar_physics_shoot_ball_rom_7c58(
        &d->ball, x, y, z, ALLSTAR_ONE_ON_ONE_HOOP_X,
        ALLSTAR_ONE_ON_ONE_HOOP_Y, distance,
        allstar_one_on_one_rom_shot_vertical_velocity(
            (uint8_t)game->selected_player_1, distance, index),
        d->shot.rom_phase, 1, 0);
    d->ball.rom_player_shot_phase_active = d->player.is_shooting;
    /* $0EE7->$0B20 increments attempts on release. */
    allstar_accuracy_bcd_increment_0b20(d->mode.attempts_bcd);
    d->phase = ACCURACY_FLIGHT; d->phase_frames = d->shot_frames = 0;
    allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOOT);
}

static void move_player(AccuracyData *d, uint8_t held) {
    bool blocked = false;
    d->input_direction = direction(held);
    if (d->input_direction && d->phase_frames % ACCURACY_MOVE_PERIOD == 0)
        allstar_one_on_one_rom_player_move_6b72(
            d->input_direction, 0x02, &d->player.x, &d->player.y,
            0.0f, 0.0f, &blocked);
    (void)blocked;
}

static void finish_attempt(AccuracyData *d) {
    d->player.is_shooting = false;
    if (d->shot_made) allstar_accuracy_bcd_increment_0b20(d->mode.makes_bcd);
    if (d->frames_remaining == 0) {
        d->phase = ACCURACY_RESULT; d->result_frames = 0;
    } else next_position(d);
}

static void tick_animation(AccuracyData *d, AllStarGame *game) {
    if (d->phase == ACCURACY_APPROACH || d->phase == ACCURACY_CONTROL) {
        if (allstar_one_on_one_rom_select_movement_action_782e(
                &d->animation, d->input_direction, 0, d->previous_direction,
                false, false, &d->horizontal_flip))
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SHOE_SQUEAK);
    }
    if (allstar_one_on_one_rom_animation_tick_6a8c(
            game->asset_pack, &d->animation)) {
        d->player.anim_frame = d->animation.display_frame;
        if (d->player.has_ball && d->animation.record_index == 6)
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_DRIBBLE);
    }
    if (d->shot_animation_clock > 0.0f) {
        uint16_t elapsed;
        d->shot_animation_clock -= ALLSTAR_PHYSICS_STEP_SECONDS;
        if (d->shot_animation_clock < 0.0f) d->shot_animation_clock = 0.0f;
        elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS -
             d->shot_animation_clock) * 60.0f);
        allstar_one_on_one_rom_shot_animation_frame(
            d->shot_action, d->shot.rom_phase, elapsed, &d->player.anim_frame);
    }
}

static void tick_fixed(AccuracyData *d, AllStarGame *game,
                       uint8_t held, uint8_t pressed) {
    d->phase_frames++;
    allstar_rom_rng_end_frame_0714(&d->rng, 0, 0);
    if (d->phase != ACCURACY_DEFINE && d->phase != ACCURACY_RESULT &&
        d->frames_remaining) d->frames_remaining--;
    if (d->phase == ACCURACY_DEFINE) {
        if (d->phase_frames % ACCURACY_MOVE_PERIOD == 0)
            allstar_accuracy_move_custom_cursor_6d57(
                held, &d->marker_x, &d->marker_y);
        if (pressed & ALLSTAR_BTN_A) {
            bool done = allstar_accuracy_record_custom_position_6d57(
                &d->mode, d->marker_x, d->marker_y);
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_MENU_SELECT);
            if (done) { d->mode.position_index = 0; next_position(d); }
        }
    } else if (d->phase == ACCURACY_APPROACH) {
        move_player(d, held);
        if (fabsf(d->player.x - d->mode.target_x) <= 4.0f &&
            fabsf(d->player.y - d->mode.target_y) <= 4.0f) {
            d->player.x = d->mode.target_x; d->player.y = d->mode.target_y;
            d->phase = ACCURACY_CONTROL; d->phase_frames = 0;
            d->input_direction = 0;
        }
    } else if (d->phase == ACCURACY_CONTROL) {
        d->input_direction = 0;
        if (pressed & ALLSTAR_BTN_A) begin_gather(d);
        else if (!d->frames_remaining) { d->phase = ACCURACY_RESULT; d->result_frames = 0; }
    } else if (d->phase == ACCURACY_GATHER) {
        uint32_t events;
        move_player(d, held); /* $702D remains active during gather. */
        events = allstar_one_on_one_shot_input(
            &d->shot, 1, (pressed & ALLSTAR_BTN_A) != 0,
            (held & ALLSTAR_BTN_B) != 0);
        if (!(events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE))
            events = allstar_one_on_one_shot_tick(
                &d->shot, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) launch(d, game);
    } else if (d->phase == ACCURACY_FLIGHT) {
        uint32_t contacts;
        d->shot_frames++;
        allstar_physics_update_ball(&d->ball, ALLSTAR_PHYSICS_STEP_SECONDS);
        contacts = allstar_physics_apply_rom_court_contacts(&d->ball);
        if ((contacts & ALLSTAR_BALL_CONTACT_RIM_BACKBOARD) && !d->rim_audio) {
            d->rim_audio = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_RIM_CLANK);
        }
        if ((contacts & ALLSTAR_BALL_CONTACT_BACK_COURT) && !d->contact_audio) {
            d->contact_audio = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_FREE_THROW_CONTACT);
        }
        if ((contacts & ALLSTAR_BALL_CONTACT_SCORE) && !d->shot_made) {
            d->shot_made = true; d->made_frame = d->shot_frames;
        }
        if (d->shot_made && !d->net_audio &&
            d->shot_frames == d->made_frame + 20) {
            d->net_audio = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_FREE_THROW_NET);
        }
        if (d->shot_made && !d->score_audio &&
            d->shot_frames == d->made_frame + 65) {
            d->score_audio = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_SCORE_CHIME);
        }
        if ((d->shot_made && d->shot_frames >= d->made_frame + ACCURACY_MAKE_FRAMES) ||
            (!d->shot_made && d->shot_frames >= ACCURACY_MISS_FRAMES &&
             (d->contact_audio || !d->ball.in_flight))) finish_attempt(d);
    } else if (d->phase == ACCURACY_RESULT) {
        d->result_frames++;
        if (!d->result_audio) {
            d->result_audio = true;
            allstar_audio_play_sfx(&game->audio, ALLSTAR_SFX_ACCURACY_RESULT);
        }
        if (d->result_frames >= 240) {
            allstar_game_change_scene(game, ALLSTAR_SCENE_INTRO);
            return;
        }
    }
    tick_animation(d, game);
    if (d->input_direction) d->previous_direction = d->input_direction;
}

static void accuracy_update(AllStarScene *scene, AllStarGame *game,
                            const AllStarInput *input, float dt) {
    AccuracyData *d = (AccuracyData*)scene->user_data;
    AllStarInput repeated = *input;
    d->fixed_accumulator += dt; d->anim_time += dt;
    while (d->fixed_accumulator + 0.000001f >= ALLSTAR_PHYSICS_STEP_SECONDS) {
        tick_fixed(d, game, repeated.buttons_held, repeated.buttons_pressed);
        if (game->active_scene != scene) return;
        d->fixed_accumulator -= ALLSTAR_PHYSICS_STEP_SECONDS;
        repeated.buttons_pressed = repeated.buttons_released = 0;
    }
}

static uint8_t font_tile(uint8_t c) {
    uint8_t m;
    if (c == 0x20) m = 0; else if (c == 0x27) m = 0x27;
    else if (c == 0x3f) m = 0x28; else if (c == 0x2e) m = 0x25;
    else if (c == 0x3a) m = 0x26; else if (c < 0x05) m = c + 0x36;
    else if (c < 0x3a) m = c - 0x2f; else m = c - 0x36;
    return (uint8_t)(m + 0xc0);
}

static void rom_text(AllStarRenderer *r, const char *text, int x, int y) {
    while (text && *text && x < 20) {
        uint8_t tile = font_tile((uint8_t)*text++);
        allstar_renderer_draw_tile(r, &r->asset_pack->free_throw_bg_tiles[tile],
                                   x++ * 8, y * 8, false, false);
    }
}

static void draw_marker(AllStarRenderer *r, const AccuracyData *d) {
    uint8_t x, y;
    int px, py, tx, ty;
    if (d->phase != ACCURACY_DEFINE && d->phase != ACCURACY_APPROACH) return;
    if (((d->phase_frames / 8u) & 1u) == 0) return;
    x = d->phase == ACCURACY_DEFINE ? d->marker_x : d->mode.target_x;
    y = d->phase == ACCURACY_DEFINE ? d->marker_y : d->mode.target_y;
    px = x - 8; py = y - 16;
    if (r->asset_pack && (r->asset_pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ONE_ON_ONE_ART)) {
        const AllStarTile *tile = &r->asset_pack->ball_source_tiles[41];
        for (ty = 0; ty < 8; ty++) for (tx = 0; tx < 8; tx++) {
            uint8_t shade = tile->pixels[ty * 8 + tx];
            if (shade) allstar_renderer_set_pixel(r, px + tx, py + ty, shade);
        }
    }
}

static void draw_hud(AllStarRenderer *r, AllStarGame *game,
                     const AccuracyData *d) {
    const AllStarPlayerStats *p = allstar_roster_get_player(
        &game->roster, game->selected_player_1);
    char line[20], name[10];
    uint32_t sec = d->frames_remaining / 60;
    if (r->asset_pack && (r->asset_pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_FREE_THROW_ART)) {
        size_t n = p ? strlen(p->last_name) : 0, off;
        if (n > 9) n = 9; memset(name, ' ', 9); name[9] = 0;
        off = (9 - n) / 2; if (n) memcpy(name + off, p->last_name, n);
        rom_text(r, name, 0, 5);
        snprintf(line, sizeof(line), "%02u %02u:%02u",
            allstar_accuracy_bcd_value(d->mode.makes_bcd),
            sec / 60, sec % 60);
        rom_text(r, line, 11, 5);
    }
}

static void accuracy_draw(AllStarScene *scene, AllStarGame *game,
                          AllStarRenderer *r) {
    AccuracyData *d = (AccuracyData*)scene->user_data;
    const AllStarPlayerStats *stats;
    uint8_t skin, net = 0; uint16_t after = 0; int32_t lift = 0;
    bool loose, behind;
    if (d->phase == ACCURACY_RESULT) {
        char line[32];
        allstar_renderer_clear(r, 0);
        allstar_renderer_draw_text(r, "TIME'S UP", 44, 40, 3);
        snprintf(line, sizeof(line), "MADE %u OF %u",
            allstar_accuracy_bcd_value(d->mode.makes_bcd),
            allstar_accuracy_bcd_value(d->mode.attempts_bcd));
        allstar_renderer_draw_text(r, line, 24, 72, 3); return;
    }
    allstar_renderer_clear(r, 0); allstar_renderer_draw_court(r);
    if (d->shot_made) {
        after = d->shot_frames >= d->made_frame ? d->shot_frames - d->made_frame : 0;
        net = (uint8_t)allstar_one_on_one_score_net_frame_1ecc(after);
    }
    loose = d->phase == ACCURACY_FLIGHT;
    behind = loose && d->shot_made && after < 35;
    if (behind) allstar_renderer_draw_ball_ex(r, (int)d->ball.x,
        (int)d->ball.y, (int)d->ball.z, d->anim_time);
    allstar_renderer_draw_net_overlay_1ecc(r, net);
    draw_hud(r, game, d); draw_marker(r, d);
    stats = allstar_roster_get_player(&game->roster, game->selected_player_1);
    skin = stats ? stats->skin_tone : 0x90;
    if (d->shot_animation_clock > 0.0f) {
        uint16_t elapsed = (uint16_t)lroundf(
            (ALLSTAR_ONE_ON_ONE_SHOT_ANIMATION_SECONDS - d->shot_animation_clock) * 60.0f);
        lift = (int32_t)allstar_one_on_one_rom_shot_jump_height_6c4d(elapsed);
    }
    allstar_renderer_draw_player_lifted_ex(r, (int)d->player.x,
        (int)d->player.y, lift, true, skin, d->player.has_ball,
        d->player.is_shooting, false,
        d->player.is_shooting ? d->shot_action : d->animation.action,
        d->player.anim_frame, d->animation.record_index, d->anim_time,
        d->horizontal_flip);
    if (loose && !behind) allstar_renderer_draw_ball_ex(r, (int)d->ball.x,
        (int)d->ball.y, (int)d->ball.z, d->anim_time);
}

static void accuracy_destroy(AllStarScene *scene) {
    if (scene) { free(scene->user_data); free(scene); }
}

AllStarScene* allstar_scene_create_three_point(void) {
    AllStarScene *s = (AllStarScene*)calloc(1, sizeof(AllStarScene));
    if (!s) return NULL; s->id = ALLSTAR_SCENE_THREE_POINT;
    s->user_data = calloc(1, sizeof(AccuracyData));
    if (!s->user_data) { free(s); return NULL; }
    s->init = accuracy_init; s->update = accuracy_update;
    s->draw = accuracy_draw; s->destroy = accuracy_destroy; return s;
}

bool allstar_scene_accuracy_get_debug_state(
    const AllStarScene *scene, AllStarAccuracyDebugState *out) {
    const AccuracyData *d;
    if (!scene || scene->id != ALLSTAR_SCENE_THREE_POINT || !out) return false;
    d = (const AccuracyData*)scene->user_data; memset(out, 0, sizeof(*out));
    out->phase = d->phase; out->group = d->mode.group;
    out->position_index = d->mode.position_index;
    out->target_x = d->mode.target_x; out->target_y = d->mode.target_y;
    out->player_x = (uint8_t)d->player.x; out->player_y = (uint8_t)d->player.y;
    out->custom_count = d->mode.custom_count;
    out->attempts = (uint8_t)allstar_accuracy_bcd_value(d->mode.attempts_bcd);
    out->makes = (uint8_t)allstar_accuracy_bcd_value(d->mode.makes_bcd);
    out->shot_frames = d->shot_frames; out->frames_remaining = d->frames_remaining;
    out->marker_visible = d->phase == ACCURACY_DEFINE || d->phase == ACCURACY_APPROACH;
    out->ball_in_flight = d->ball.in_flight; out->shot_made = d->shot_made;
    return true;
}

bool allstar_scene_accuracy_snap_to_target(AllStarScene *scene) {
    AccuracyData *d;
    if (!scene || scene->id != ALLSTAR_SCENE_THREE_POINT) return false;
    d = (AccuracyData*)scene->user_data; d->player.x = d->mode.target_x;
    d->player.y = d->mode.target_y; d->phase = ACCURACY_CONTROL;
    d->phase_frames = 0; return true;
}

bool allstar_scene_accuracy_force_test_score_frame(
    AllStarScene *scene, uint16_t after) {
    AccuracyData *d;
    if (!scene || scene->id != ALLSTAR_SCENE_THREE_POINT) return false;
    d = (AccuracyData*)scene->user_data; d->phase = ACCURACY_FLIGHT;
    d->shot_made = true; d->made_frame = 1; d->shot_frames = 1 + after;
    d->ball.in_flight = true; d->ball.x = ALLSTAR_ONE_ON_ONE_HOOP_X;
    d->ball.y = ALLSTAR_ONE_ON_ONE_HOOP_Y; d->ball.z = 8; return true;
}

bool allstar_scene_accuracy_force_test_result(AllStarScene *scene) {
    AccuracyData *d;
    if (!scene || scene->id != ALLSTAR_SCENE_THREE_POINT) return false;
    d = (AccuracyData*)scene->user_data; d->phase = ACCURACY_RESULT;
    d->frames_remaining = d->result_frames = 0; return true;
}
