#include "allstar_game.h"
#include "allstar_rom.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include "allstar_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static bool save_bmp_file(const char *filepath, const AllStarColor *pixels, int width, int height) {
    FILE *f = fopen(filepath, "wb");
    if (!f) return false;

    uint32_t row_size = ((width * 3 + 3) / 4) * 4;
    uint32_t image_size = row_size * height;
    uint32_t file_size = 54 + image_size;

    uint8_t header[54] = {
        'B', 'M',
        (uint8_t)(file_size & 0xFF), (uint8_t)((file_size >> 8) & 0xFF), (uint8_t)((file_size >> 16) & 0xFF), (uint8_t)((file_size >> 24) & 0xFF),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (uint8_t)(width & 0xFF), (uint8_t)((width >> 8) & 0xFF), (uint8_t)((width >> 16) & 0xFF), (uint8_t)((width >> 24) & 0xFF),
        (uint8_t)(height & 0xFF), (uint8_t)((height >> 8) & 0xFF), (uint8_t)((height >> 16) & 0xFF), (uint8_t)((height >> 24) & 0xFF),
        1, 0,
        24, 0,
        0, 0, 0, 0,
        (uint8_t)(image_size & 0xFF), (uint8_t)((image_size >> 8) & 0xFF), (uint8_t)((image_size >> 16) & 0xFF), (uint8_t)((image_size >> 24) & 0xFF),
        0x13, 0x0B, 0, 0,
        0x13, 0x0B, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0
    };

    fwrite(header, 1, 54, f);
    uint8_t *row_buf = (uint8_t*)calloc(1, row_size);

    for (int y = height - 1; y >= 0; y--) {
        for (int x = 0; x < width; x++) {
            AllStarColor c = pixels[y * width + x];
            row_buf[x * 3 + 0] = (uint8_t)(c & 0xFF);         /* B */
            row_buf[x * 3 + 1] = (uint8_t)((c >> 8) & 0xFF);  /* G */
            row_buf[x * 3 + 2] = (uint8_t)((c >> 16) & 0xFF); /* R */
        }
        fwrite(row_buf, 1, row_size, f);
    }

    free(row_buf);
    fclose(f);
    return true;
}

static void print_usage(const char *prog_name) {
    printf("NBA All-Star Challenge (Game Boy) - Native C Port CLI\n\n");
    printf("Usage: %s [options]\n\n", prog_name);
    printf("Options:\n");
    printf("  --play [assetpack]                 Launch game\n");
    printf("  --rom-test <rom.gb>                Validate Game Boy ROM header & checksum\n");
    printf("  --build-assetpack <rom> <out.pack> Build asset pack from ROM\n");
    printf("  --dump-screenshots <out_dir> [pack] Render all game scenes to BMP screenshots\n");
    printf("  --test-roster                      Verify roster data tables\n");
    printf("  --test-physics                     Run physics simulation unit tests\n");
    printf("  --test-mode-routing                Verify all ROM menu IDs route correctly\n");
    printf("  --test-settings                    Verify ROM settings values and persistence\n");
    printf("  --test-one-on-one-lifecycle        Verify One-on-One endings and returns\n");
    printf("  --test-one-on-one-shooting         Verify staged shooting and traveling\n");
    printf("  --test-tournament                  Verify seven-match bracket progression\n");
    printf("  --test-headless-frames             Run headless multi-scene frame tests\n");
    printf("  --test-all                         Execute all test suites\n");
    printf("  --help                             Show this help message\n");
}

int allstar_cli_rom_test(const char *rom_path) {
    printf("[Test] Validating ROM: %s\n", rom_path);
    AllStarRom rom;
    if (!allstar_rom_load_file(&rom, rom_path)) {
        fprintf(stderr, "[Test] FAILED: Could not load ROM\n");
        return 1;
    }

    printf("  Title:           %s\n", rom.header.title);
    printf("  Cart Type:       0x%02X\n", rom.header.cart_type);
    printf("  ROM Size:        %zu bytes\n", rom.size);
    printf("  Header Checksum: 0x%02X (Valid: %s)\n",
           rom.header.header_checksum, rom.header.is_valid_header ? "YES" : "NO");

    bool ok = allstar_rom_verify(&rom);
    allstar_rom_free(&rom);

    if (ok) {
        printf("[Test] PASSED: ROM is valid Game Boy image\n");
        return 0;
    } else {
        printf("[Test] FAILED: ROM verification failed\n");
        return 1;
    }
}

int allstar_cli_build_assetpack(const char *rom_path, const char *out_path) {
    printf("[AssetPack] Building asset pack from %s -> %s\n", rom_path, out_path);
    AllStarRom rom;
    if (!allstar_rom_load_file(&rom, rom_path)) {
        fprintf(stderr, "[AssetPack] FAILED: Could not load ROM\n");
        return 1;
    }

    AllStarAssetPack pack;
    if (!allstar_asset_pack_build_from_rom(&pack, &rom)) {
        fprintf(stderr, "[AssetPack] FAILED: Could not extract assets from ROM\n");
        allstar_rom_free(&rom);
        return 1;
    }

    allstar_rom_free(&rom);

    if (!allstar_asset_pack_save_file(&pack, out_path)) {
        fprintf(stderr, "[AssetPack] FAILED: Could not save asset pack\n");
        return 1;
    }

    printf("[AssetPack] SUCCESS: Built %s successfully\n", out_path);
    return 0;
}

int allstar_cli_test_roster(void) {
    printf("[Test] Running Roster Data Tests...\n");
    AllStarRoster roster;
    allstar_roster_init_default(&roster);

    if (roster.count != ALLSTAR_DEFAULT_ROSTER_COUNT) {
        fprintf(stderr, "[Test] Roster count mismatch: got %zu, expected %d\n", roster.count, ALLSTAR_DEFAULT_ROSTER_COUNT);
        return 1;
    }

    const AllStarPlayerStats *p0 = allstar_roster_get_player(&roster, 0);
    if (!p0 || strcmp(p0->name, "DANNY AINGE") != 0 || p0->number != 7) {
        fprintf(stderr, "[Test] Player 0 mismatch: %s #%d\n", p0 ? p0->name : "null", p0 ? p0->number : 0);
        return 1;
    }

    const AllStarPlayerStats *pj = allstar_roster_get_player(&roster, 13);
    if (!pj || strcmp(pj->name, "MICHAEL JORDAN") != 0 || pj->number != 23) {
        fprintf(stderr, "[Test] Jordan mismatch: %s #%d\n", pj ? pj->name : "null", pj ? pj->number : 0);
        return 1;
    }

    printf("[Test] PASSED: Roster data verification (%zu authentic NBA All-Star players)\n", roster.count);
    return 0;
}

int allstar_cli_test_physics(void) {
    AllStarBall ball;
    AllStarBall frame_stepped;
    AllStarBall chunk_stepped;
    AllStarBall miss;
    AllStarBall contact;
    AllStarBall rebound;
    AllStarRomBallStepState rom_step;
    uint32_t contacts;
    int frame;

    printf("[Test] Running Physics Simulation Tests...\n");

    memset(&rom_step, 0, sizeof(rom_step));
    rom_step.vx = 1;
    rom_step.vy = -1;
    rom_step.vz = 0x0100;
    rom_step.x = 0x5000;
    rom_step.y = 0x7000;
    rom_step.z = 0x0080;
    allstar_physics_rom_step_7be8(&rom_step);
    if (rom_step.vx != -1 || rom_step.vy != 1 ||
        rom_step.vz != 0x00f1 || rom_step.x != 0x4fff ||
        rom_step.y != 0x7001 || rom_step.z != 0x0171) {
        fprintf(stderr, "[Test] $7BE8 8.8 gravity/friction/integration order was incorrect\n");
        return 1;
    }
    rom_step.gravity_delay_frames = 1;
    allstar_physics_rom_step_7be8(&rom_step);
    if (rom_step.gravity_delay_frames != 0 || rom_step.vz != 0x00f1) {
        fprintf(stderr, "[Test] $7BE8 gravity delay did not preserve vertical velocity\n");
        return 1;
    }
    allstar_physics_init_ball(&ball);

    if (allstar_one_on_one_rom_shot_distance_class(61.0f, 115.0f) != 0 ||
        allstar_one_on_one_rom_shot_distance_class(60.0f, 115.0f) != 1 ||
        allstar_one_on_one_rom_shot_distance_class(45.0f, 131.0f) != 1 ||
        allstar_one_on_one_rom_shot_distance_class(44.0f, 131.0f) != 2 ||
        allstar_one_on_one_rom_shot_distance_class(29.0f, 147.0f) != 2 ||
        allstar_one_on_one_rom_shot_distance_class(28.0f, 147.0f) != 3 ||
        allstar_one_on_one_rom_shot_distance_class(12.0f, 148.0f) != 4 ||
        allstar_one_on_one_rom_shot_distance_class(13.0f, 148.0f) != 3 ||
        allstar_one_on_one_rom_shot_profile(2) != 0 ||
        allstar_one_on_one_rom_shot_profile(25) != 1 ||
        allstar_one_on_one_rom_shot_profile(0) != 2 ||
        allstar_one_on_one_rom_shot_vertical_velocity(0, 3, 2) != 0x0198 ||
        allstar_one_on_one_rom_shot_vertical_velocity(2, 0, 7) != 0x00f0 ||
        allstar_one_on_one_rom_shot_vertical_velocity(25, 4, 7) != 0x01b4) {
        fprintf(stderr, "[Test] $07B4/$7EC4/$2F40/$7C58 launch selectors were incorrect\n");
        return 1;
    }

    allstar_physics_shoot_ball_rom_7c58(
        &ball, 83.0f, 150.0f, 48.0f, 84.0f, 92.0f,
        3, 0x0198, 0, 1, 2);
    if (ball.rom_step_state.vx != 4 || ball.rom_step_state.vy != -232 ||
        ball.rom_step_state.vz != 0x0198) {
        fprintf(stderr, "[Test] $7C58 64-frame launch vector was incorrect\n");
        return 1;
    }
    for (frame = 0; frame < 64; frame++) {
        allstar_physics_update_ball(&ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (ball.rom_step_state.x != 0x5400 ||
        ball.rom_step_state.y != 0x5c00) {
        fprintf(stderr, "[Test] $7EA9 class-three vector did not reach $54/$5C at frame 64\n");
        return 1;
    }

    allstar_physics_shoot_ball_rom_7c58(
        &ball, 83.0f, 150.0f, 43.0f, 84.0f, 92.0f,
        3, 0x0198, 2, 1, 2);
    if (ball.rom_step_state.vx != 0 || ball.rom_step_state.vy != 0 ||
        ball.rom_step_state.vz != -0x0100) {
        fprintf(stderr, "[Test] $7F0A phase-two launch vector was incorrect\n");
        return 1;
    }
    allstar_physics_update_ball(&ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (ball.rom_step_state.vz != (int16_t)0xfef1) {
        fprintf(stderr, "[Test] $7F0A first integrated VZ did not match trace $FEF1\n");
        return 1;
    }

    allstar_physics_shoot_ball(&ball, 80.0f, 130.0f, 80.0f, 82.0f,
                               ALLSTAR_HOOP_HEIGHT, 1, 2);
    if (!ball.in_flight || ball.z != ALLSTAR_BALL_RELEASE_HEIGHT ||
        ball.vz <= 0.0f) {
        fprintf(stderr, "[Test] Shot initialization failed\n");
        return 1;
    }

    for (frame = 1; frame < ALLSTAR_SHOT_FLIGHT_FRAMES; frame++) {
        allstar_physics_update_ball(&ball, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (allstar_physics_check_basket(&ball, 80.0f, 82.0f,
                                         ALLSTAR_HOOP_HEIGHT)) {
            fprintf(stderr, "[Test] Shot scored before descending through the rim\n");
            return 1;
        }
    }
    allstar_physics_update_ball(&ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (fabsf(ball.x - 80.0f) > 0.001f ||
        fabsf(ball.y - 82.0f) > 0.001f ||
        fabsf(ball.z - (ALLSTAR_HOOP_HEIGHT - 1.0f / 16.0f)) > 0.001f ||
        ball.vz >= 0.0f ||
        !allstar_physics_check_basket(&ball, 80.0f, 82.0f,
                                      ALLSTAR_HOOP_HEIGHT) ||
        allstar_physics_check_basket(&ball, 80.0f, 82.0f,
                                     ALLSTAR_HOOP_HEIGHT)) {
        fprintf(stderr, "[Test] Clean shot did not cross the rim once on descent "
                        "(x=%.4f y=%.4f z=%.4f vx=%.4f vy=%.4f vz=%.4f cross=%.4f,%.4f)\n",
                ball.x, ball.y, ball.z, ball.vx, ball.vy, ball.vz,
                ball.target_crossing_x, ball.target_crossing_y);
        return 1;
    }

    allstar_physics_init_ball(&frame_stepped);
    allstar_physics_shoot_ball(&frame_stepped, 32.0f, 128.0f, 80.0f, 82.0f,
                               ALLSTAR_HOOP_HEIGHT, 1, 3);
    chunk_stepped = frame_stepped;
    for (frame = 0; frame < ALLSTAR_SHOT_FLIGHT_FRAMES; frame++) {
        allstar_physics_update_ball(&frame_stepped,
                                    ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    allstar_physics_update_ball(&chunk_stepped,
                                ALLSTAR_SHOT_FLIGHT_FRAMES *
                                ALLSTAR_PHYSICS_STEP_SECONDS);
    if (fabsf(frame_stepped.x - chunk_stepped.x) > 0.001f ||
        fabsf(frame_stepped.y - chunk_stepped.y) > 0.001f ||
        fabsf(frame_stepped.z - chunk_stepped.z) > 0.001f ||
        fabsf(frame_stepped.vz - chunk_stepped.vz) > 0.001f ||
        !allstar_physics_check_basket(&frame_stepped, 80.0f, 82.0f,
                                      ALLSTAR_HOOP_HEIGHT) ||
        !allstar_physics_check_basket(&chunk_stepped, 80.0f, 82.0f,
                                      ALLSTAR_HOOP_HEIGHT)) {
        fprintf(stderr, "[Test] Fixed-step trajectory changed with dt chunking\n");
        return 1;
    }

    allstar_physics_init_ball(&miss);
    allstar_physics_shoot_ball(&miss, 80.0f, 130.0f, 90.0f, 82.0f,
                               ALLSTAR_HOOP_HEIGHT, 1, 2);
    for (frame = 0; frame < ALLSTAR_SHOT_FLIGHT_FRAMES; frame++) {
        allstar_physics_update_ball(&miss, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (allstar_physics_check_basket(&miss, 80.0f, 82.0f,
                                     ALLSTAR_HOOP_HEIGHT)) {
        fprintf(stderr, "[Test] Offset shot incorrectly passed through the rim\n");
        return 1;
    }

    allstar_physics_init_ball(&rebound);
    allstar_physics_shoot_ball(&rebound, 80.0f, 130.0f, 90.0f, 92.0f,
                               ALLSTAR_HOOP_HEIGHT, 1, 2);
    for (frame = 0; frame < 120 && !rebound.recoverable; frame++) {
        allstar_physics_update_ball(&rebound,
                                    ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (!rebound.recoverable || !rebound.in_flight || rebound.z != 0.0f) {
        fprintf(stderr, "[Test] First ground contact did not enable live rebound recovery\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.x = 9.0f;
    contact.y = 100.0f;
    contact.vx = 123.0f;
    contact.vy = -45.0f;
    contacts = allstar_physics_apply_rom_court_contacts(&contact);
    if (!(contacts & ALLSTAR_BALL_CONTACT_DEAD_BOUNDARY) ||
        contact.vx != 0.0f || contact.vy != 0.0f) {
        fprintf(stderr, "[Test] $1F4D dead-boundary stop did not zero planar velocity\n");
        return 1;
    }

    contact.x = 160.0f;
    contact.y = 100.0f;
    contact.vx = -30.0f;
    contact.vy = 30.0f;
    contact.rom_step_state_valid = false;
    if (!(allstar_physics_apply_rom_court_contacts(&contact) &
          ALLSTAR_BALL_CONTACT_DEAD_BOUNDARY) ||
        contact.vx != 0.0f || contact.vy != 0.0f) {
        fprintf(stderr, "[Test] $1CED x>=$A0 boundary was incorrect\n");
        return 1;
    }

    contact.x = 80.0f;
    contact.y = 151.0f;
    contact.vx = 30.0f;
    contact.vy = 30.0f;
    contact.rom_step_state_valid = false;
    if (!(allstar_physics_apply_rom_court_contacts(&contact) &
          ALLSTAR_BALL_CONTACT_DEAD_BOUNDARY) ||
        contact.vx != 0.0f || contact.vy != 0.0f) {
        fprintf(stderr, "[Test] $1CED y>=$97 boundary was incorrect\n");
        return 1;
    }

    contact.x = 10.0f;
    contact.y = 150.0f;
    contact.vx = 60.0f;
    contact.vy = 60.0f;
    contact.rom_step_state_valid = false;
    if (allstar_physics_apply_rom_court_contacts(&contact) !=
            ALLSTAR_BALL_CONTACT_NONE) {
        fprintf(stderr, "[Test] $1CED accepted-boundary values collided\n");
        return 1;
    }

    contact.x = 80.0f;
    contact.y = 91.0f;
    contact.vx = -60.0f;
    contact.vy = -60.0f;
    contact.rom_step_state_valid = false;
    contacts = allstar_physics_apply_rom_court_contacts(&contact);
    if (!(contacts & ALLSTAR_BALL_CONTACT_BACK_COURT) ||
        contact.y != ALLSTAR_ROM_BACK_COURT_RETURN_Y ||
        contact.vx >= 0.0f || contact.vy <= 0.0f) {
        fprintf(stderr, "[Test] $1CED back-court return response was incorrect\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.rom_step_state_valid = true;
    contact.rom_step_state.x = 0x5400;
    contact.rom_step_state.y = 0x5d00;
    contact.rom_step_state.z = 0x3700;
    contact.rom_step_state.vx = 12;
    contact.rom_step_state.vy = -20;
    contact.rom_step_state.vz = -80;
    contacts = allstar_physics_apply_rom_court_contacts(&contact);
    if (!(contacts & ALLSTAR_BALL_CONTACT_SCORE) || !contact.made_basket ||
        contact.rom_step_state.vx != 0 || contact.rom_step_state.vy != 0 ||
        contact.rom_step_state.vz != 0) {
        fprintf(stderr, "[Test] $1CED exact $54/$5D/$37 score branch was incorrect\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.rom_step_state_valid = true;
    contact.rom_step_state.x = 0x5300;
    contact.rom_step_state.y = 0x5e00;
    contact.rom_step_state.z = 0x3700;
    contact.rom_step_state.vz = -80;
    contacts = allstar_physics_apply_rom_court_contacts(&contact);
    if (!(contacts & ALLSTAR_BALL_CONTACT_RIM_BACKBOARD) ||
        contact.rom_step_state.vx != 0x0046 ||
        contact.rom_step_state.vz != -1 ||
        contact.rom_contact_cooldown_frames != 8) {
        fprintf(stderr, "[Test] $1CED left-rim impulse was incorrect\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.rom_step_state_valid = true;
    contact.rom_step_state.x = 0x5400;
    contact.rom_step_state.y = 0x5f00;
    contact.rom_step_state.z = 0x3900;
    contact.rom_step_state.vz = -100;
    contacts = allstar_physics_apply_rom_court_contacts(&contact);
    if (!(contacts & ALLSTAR_BALL_CONTACT_RIM_BACKBOARD) ||
        contact.rom_step_state.vz != 43 ||
        contact.rom_contact_cooldown_frames != 8) {
        fprintf(stderr, "[Test] $1CED back-rim bounce was incorrect\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.rom_step_state_valid = true;
    contact.rom_step_state.x = 0x5000;
    contact.rom_step_state.y = 0x7000;
    contact.rom_step_state.z = 0x0010;
    contact.rom_step_state.vz = -0x0100;
    allstar_physics_update_ball(&contact, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (!contact.recoverable || contact.rom_step_state.z != 0 ||
        contact.rom_step_state.vz != 214) {
        fprintf(stderr, "[Test] $1E5B/$1E77 raw ground bounce was incorrect\n");
        return 1;
    }

    allstar_physics_init_ball(&contact);
    contact.in_flight = true;
    contact.rom_step_state_valid = true;
    contact.rom_hard_bounce_pending = true;
    contact.rom_step_state.x = 0x5000;
    contact.rom_step_state.y = 0x7000;
    contact.rom_step_state.z = 0x0010;
    contact.rom_step_state.vz = -0x0200;
    allstar_physics_update_ball(&contact, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (contact.rom_hard_bounce_pending ||
        contact.rom_step_state.vz != 227) {
        fprintf(stderr, "[Test] $FFD4 one-shot $012C bounce loss was incorrect\n");
        return 1;
    }

    printf("[Test] PASSED: flight, rim crossing, miss, and ROM court contacts\n");
    return 0;
}

int allstar_cli_test_mode_routing(void) {
    static const struct {
        AllStarGameMode mode;
        const char *name;
        AllStarSceneId scene_id;
        bool requires_opponent;
    } expected[ALLSTAR_MODE_COUNT] = {
        { ALLSTAR_MODE_ONE_ON_ONE, "One On One",        ALLSTAR_SCENE_ONE_ON_ONE,  true  },
        { ALLSTAR_MODE_FREE_THROW, "Free Throws",       ALLSTAR_SCENE_FREE_THROW,  false },
        { ALLSTAR_MODE_HORSE,      "Horse",             ALLSTAR_SCENE_HORSE,       false },
        { ALLSTAR_MODE_ACCURACY,   "Accuracy Shootout", ALLSTAR_SCENE_THREE_POINT, true  },
        { ALLSTAR_MODE_TOURNAMENT, "Tournament",        ALLSTAR_SCENE_TOURNAMENT,  true  }
    };

    printf("[Test] Running ROM Menu Mode Routing Parity Tests...\n");

    AllStarGame game;
    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing game for mode routing\n");
        return 1;
    }

    for (uint32_t menu_index = 0; menu_index < ALLSTAR_MODE_COUNT; menu_index++) {
        AllStarGameMode mode = allstar_game_mode_from_menu_index(menu_index);
        const AllStarSceneId scene_id = allstar_game_mode_scene(mode);
        const bool requires_opponent = allstar_game_mode_requires_opponent(mode);
        const char *name = allstar_game_mode_name(mode);

        if (mode != expected[menu_index].mode ||
            scene_id != expected[menu_index].scene_id ||
            requires_opponent != expected[menu_index].requires_opponent ||
            strcmp(name, expected[menu_index].name) != 0) {
            fprintf(stderr,
                    "[Test] Mode route %u mismatch: mode=%d name=%s scene=%d opponent=%d\n",
                    menu_index, (int)mode, name, (int)scene_id, requires_opponent ? 1 : 0);
            allstar_game_shutdown(&game);
            return 1;
        }

        allstar_game_change_scene(&game, scene_id);
        if (!game.active_scene || game.active_scene->id != scene_id) {
            fprintf(stderr, "[Test] Mode route %u did not create scene %d\n",
                    menu_index, (int)scene_id);
            allstar_game_shutdown(&game);
            return 1;
        }
    }

    if (allstar_game_mode_from_menu_index(ALLSTAR_MODE_COUNT) != ALLSTAR_MODE_ONE_ON_ONE ||
        allstar_game_mode_scene((AllStarGameMode)ALLSTAR_MODE_COUNT) != ALLSTAR_SCENE_ONE_ON_ONE) {
        fprintf(stderr, "[Test] Invalid mode routing did not use the safe One-on-One fallback\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: All 5 ROM menu IDs route to the intended native scenes\n");
    return 0;
}

int allstar_cli_test_one_on_one_lifecycle(void) {
    AllStarOneOnOneMatch match;
    uint32_t events;
    AllStarGame game;
    uint32_t tournament_p1;
    uint32_t tournament_p2;

    printf("[Test] Running One-on-One Lifecycle Parity Tests...\n");

    if (allstar_one_on_one_compare_scores(0, 0) != 0 ||
        allstar_one_on_one_compare_scores(0x0100, 0x00ff) != 1 ||
        allstar_one_on_one_compare_scores(0x00ff, 0x0100) != 2 ||
        allstar_one_on_one_compare_scores(0xffff, 0xffff) != 0) {
        fprintf(stderr, "[Test] ROM $28E1 unsigned 16-bit score comparison diverged\n");
        return 1;
    }

    if (!allstar_one_on_one_result_can_dismiss(ALLSTAR_BTN_A) ||
        !allstar_one_on_one_result_can_dismiss(ALLSTAR_BTN_B) ||
        allstar_one_on_one_result_can_dismiss(ALLSTAR_BTN_START) ||
        !allstar_one_on_one_overtime_can_dismiss(ALLSTAR_BTN_A) ||
        allstar_one_on_one_overtime_can_dismiss(ALLSTAR_BTN_B) ||
        allstar_one_on_one_overtime_can_dismiss(ALLSTAR_BTN_START)) {
        fprintf(stderr, "[Test] Result/overtime dismissal masks diverged from $10FA/$1638\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 120.0f, 24.0f, 0, false);
    events = allstar_one_on_one_match_tick(&match, 24.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_SHOT_CLOCK) ||
        match.p1_possession || match.shot_clock != 24.0f) {
        fprintf(stderr, "[Test] Shot-clock turnover did not switch possession\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 1.0f, 24.0f, 0, false);
    allstar_one_on_one_match_add_score(&match, 1, 2);
    allstar_one_on_one_match_add_score(&match, 2, 2);
    events = allstar_one_on_one_match_tick(&match, 1.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) ||
        match.phase != ALLSTAR_ONE_ON_ONE_RESULT || match.winner != 0) {
        fprintf(stderr, "[Test] Tied regulation did not enter the result phase\n");
        return 1;
    }
    events = allstar_one_on_one_match_dismiss_result(&match);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME_NOTICE) ||
        match.phase != ALLSTAR_ONE_ON_ONE_OVERTIME || match.period != 1 ||
        match.p1_score != 2 || match.p2_score != 2) {
        fprintf(stderr, "[Test] Tied result did not enter the four-second overtime notice\n");
        return 1;
    }
    events = allstar_one_on_one_match_tick(&match,
                                            ALLSTAR_ONE_ON_ONE_OVERTIME_SECONDS);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_OVERTIME) ||
        match.phase != ALLSTAR_ONE_ON_ONE_PLAYING || match.period != 2 ||
        match.p1_score != 2 || match.p2_score != 2) {
        fprintf(stderr, "[Test] Overtime notice did not preserve scores into period 2\n");
        return 1;
    }
    allstar_one_on_one_match_add_score(&match, 1, 2);
    events = allstar_one_on_one_match_tick(&match, 1.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) || match.winner != 1) {
        fprintf(stderr, "[Test] Overtime winner was not detected\n");
        return 1;
    }
    events = allstar_one_on_one_match_dismiss_result(&match);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE) ||
        match.phase != ALLSTAR_ONE_ON_ONE_COMPLETE) {
        fprintf(stderr, "[Test] Winning result did not complete the match\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 1.0f, 24.0f, 0, false);
    allstar_one_on_one_match_add_score(&match, 2, 2);
    allstar_one_on_one_match_tick(&match, 1.0f);
    events = allstar_one_on_one_match_tick(&match, 15.0f);
    if (events != ALLSTAR_ONE_ON_ONE_EVENT_NONE ||
        match.phase != ALLSTAR_ONE_ON_ONE_RESULT) {
        fprintf(stderr, "[Test] Result hold ended before 960 frames\n");
        return 1;
    }
    events = allstar_one_on_one_match_tick(&match, 1.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_COMPLETE) || match.winner != 2) {
        fprintf(stderr, "[Test] Result hold did not auto-complete after 960 frames\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 120.0f, 24.0f, 3, false);
    if (allstar_one_on_one_match_add_score(&match, 1, 2) != ALLSTAR_ONE_ON_ONE_EVENT_NONE ||
        !(allstar_one_on_one_match_add_score(&match, 1, 1) & ALLSTAR_ONE_ON_ONE_EVENT_RESULT) ||
        match.end_reason != ALLSTAR_ONE_ON_ONE_END_SCORE || match.winner != 1) {
        fprintf(stderr, "[Test] Configured play-to ending did not trigger at the target score\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 120.0f, 24.0f, 0, false);
    if (allstar_one_on_one_next_possession_after_score(&match, 1) != 2 ||
        allstar_one_on_one_next_possession_after_score(&match, 2) != 1) {
        fprintf(stderr, "[Test] Losers-outs possession did not pass the ball after a score\n");
        return 1;
    }
    allstar_one_on_one_match_init(&match, 120.0f, 24.0f, 0, true);
    if (allstar_one_on_one_next_possession_after_score(&match, 1) != 1 ||
        allstar_one_on_one_next_possession_after_score(&match, 2) != 2) {
        fprintf(stderr, "[Test] Winners-outs possession did not retain the ball after a score\n");
        return 1;
    }
    match.shot_clock = 7.0f;
    allstar_one_on_one_match_take_possession(&match, 2, false);
    if (match.p1_possession || match.shot_clock != 7.0f) {
        fprintf(stderr, "[Test] Same-play possession update incorrectly reset the shot clock\n");
        return 1;
    }
    allstar_one_on_one_match_take_possession(&match, 1, true);
    if (!match.p1_possession || match.shot_clock != 24.0f) {
        fprintf(stderr, "[Test] New possession did not reset the shot clock\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing game for lifecycle routing\n");
        return 1;
    }
    game.selected_mode = ALLSTAR_MODE_TOURNAMENT;
    allstar_tournament_reset(&game.tournament);
    if (!allstar_tournament_get_current_match(&game.tournament, &tournament_p1, &tournament_p2)) {
        fprintf(stderr, "[Test] Tournament did not expose its opening match\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    game.selected_player_1 = tournament_p1;
    game.selected_player_2 = tournament_p2;
    game.tournament.match_in_progress = true;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    game.one_on_one.p1_score = 2;
    game.one_on_one.game_clock = 0.01f;
    game.input.buttons_pressed = 0;
    allstar_game_tick(&game, 0.02f);
    game.input.buttons_pressed = ALLSTAR_BTN_A;
    allstar_game_tick(&game, 0.0f);
    if (!game.active_scene || game.active_scene->id != ALLSTAR_SCENE_TOURNAMENT ||
        game.tournament.current_match != 1 ||
        game.tournament.semifinalists[0] != tournament_p1 ||
        game.tournament.match_in_progress) {
        fprintf(stderr, "[Test] Tournament match winner did not return to the advancing bracket\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.selected_mode = ALLSTAR_MODE_ONE_ON_ONE;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    game.one_on_one.p1_score = 1;
    game.one_on_one.game_clock = 0.01f;
    game.input.buttons_pressed = 0;
    allstar_game_tick(&game, 0.02f);
    game.input.buttons_pressed = ALLSTAR_BTN_B;
    allstar_game_tick(&game, 0.0f);
    if (!game.active_scene || game.active_scene->id != ALLSTAR_SCENE_INTRO) {
        fprintf(stderr, "[Test] Ordinary One-on-One result did not return to the title flow\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: clock, score, possession, overtime, result, exit, and tournament return\n");
    return 0;
}

int allstar_cli_test_one_on_one_shooting(void) {
    AllStarOneOnOneShotAttempt attempt;
    AllStarOneOnOneRecoveryState recovery;
    AllStarOneOnOneMatch match;
    AllStarGame game;
    uint32_t events;
    AllStarOneOnOneReleaseOffset release;
    uint8_t animation_frame;
    uint8_t target_x;
    uint8_t target_y;
    float court_x;
    float court_y;
    int frame;
    AllStarRomRng rng;
    AllStarAssetPack animation_pack;
    AllStarRomAnimationState animation_state;
    bool animation_flip;
    bool contact_latch;
    AllStarRomPlayerContactState p1_contact;
    AllStarRomPlayerContactState p2_contact;
    AllStarRomContactEvent contact_event;
    int contact_offender;
    AllStarAIController contact_ai;
    AllStarRomBallPresentation ball_presentation;
    uint16_t composed_tiles[ALLSTAR_PLAYER_FRAME_TILE_COUNT];
    static const uint8_t movement_cases[4][3] = {
        {ALLSTAR_BTN_RIGHT, 0x10, 0x11},
        {ALLSTAR_BTN_LEFT,  0x10, 0x11},
        {ALLSTAR_BTN_UP,    0x08, 0x09},
        {ALLSTAR_BTN_DOWN,  0x01, 0x02}
    };
    static const uint8_t idle_cases[4][3] = {
        {ALLSTAR_BTN_RIGHT, 0x13, 0x15},
        {ALLSTAR_BTN_LEFT,  0x13, 0x15},
        {ALLSTAR_BTN_UP,    0x0b, 0x0d},
        {ALLSTAR_BTN_DOWN,  0x04, 0x06}
    };

    printf("[Test] Running One-on-One Shooting Tests...\n");

    allstar_asset_pack_init_default(&animation_pack);
    if (animation_pack.header.animation_action_count != 24 ||
        animation_pack.animation_actions[0x00].rom_pointer != 0x6787 ||
        animation_pack.animation_actions[0x0a].rom_pointer != 0x6837 ||
        animation_pack.animation_actions[0x17].rom_pointer != 0x6940 ||
        animation_pack.animation_actions[0x0a].record_count != 13) {
        fprintf(stderr, "[Test] $6C60 action pointer map was incorrect\n");
        return 1;
    }
    {
        int tile;
        for (tile = 0; tile < ALLSTAR_PLAYER_FRAME_TILE_COUNT; tile++) {
            animation_pack.player_frames[0].tile_indices[tile] = (uint8_t)tile;
            animation_pack.player_frames[37].tile_indices[tile] = (uint8_t)tile;
            animation_pack.player_frames[59].tile_indices[tile] = (uint8_t)tile;
        }
    }
    if (!allstar_renderer_rom_player_tiles_2945(
            &animation_pack, 0x00, 0, false, composed_tiles) ||
        composed_tiles[0] != 0 || composed_tiles[1] != 3 ||
        composed_tiles[2] != 1 || composed_tiles[3] != 4 ||
        composed_tiles[4] != 2 || composed_tiles[5] != 5 ||
        !allstar_renderer_rom_player_tiles_2945(
            &animation_pack, 0x00, 0, true, composed_tiles) ||
        composed_tiles[0] != 2 || composed_tiles[1] != 5 ||
        composed_tiles[4] != 0 || composed_tiles[5] != 3 ||
        !allstar_renderer_rom_player_tiles_2945(
            &animation_pack, 0x08, 21, false, composed_tiles) ||
        composed_tiles[0] != 147 ||
        !allstar_renderer_rom_player_tiles_2945(
            &animation_pack, 0x10, 21, false, composed_tiles) ||
        composed_tiles[0] != 358) {
        fprintf(stderr, "[Test] $2945/$2A2B player OAM traversal was incorrect\n");
        return 1;
    }
    allstar_renderer_rom_ball_presentation_6945(
        0x43, 0x70, 7, false, &ball_presentation);
    if (ball_presentation.phase != 3 ||
        ball_presentation.adjusted_phase != 3 ||
        ball_presentation.oam_x != 0x40 ||
        ball_presentation.ball_oam_y != 0x69 ||
        ball_presentation.shadow_tier != 2 ||
        ball_presentation.ball_pair_index != 3 ||
        ball_presentation.shadow_pair_index != 27) {
        fprintf(stderr, "[Test] $6945 ground ball phase/shadow selection was incorrect\n");
        return 1;
    }
    allstar_renderer_rom_ball_presentation_6945(
        0x43, 0x70, 0x1f, true, &ball_presentation);
    if (ball_presentation.adjusted_phase != 7 ||
        ball_presentation.oam_x != 0x3c ||
        ball_presentation.shadow_tier != 0 ||
        ball_presentation.ball_pair_index != 7 ||
        ball_presentation.shadow_pair_index != 15) {
        fprintf(stderr, "[Test] $6945 rear/high ball phase selection was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x10);
    if (!allstar_one_on_one_rom_animation_tick_6a8c(
            &animation_pack, &animation_state) ||
        animation_state.display_frame != 0 ||
        animation_state.record_index != 1 || animation_state.timer != 6 ||
        !animation_state.new_frame) {
        fprintf(stderr, "[Test] $6A8C first dribble record was incorrect\n");
        return 1;
    }
    for (frame = 0; frame < 6; frame++) {
        if (!allstar_one_on_one_rom_animation_tick_6a8c(
                &animation_pack, &animation_state)) {
            fprintf(stderr, "[Test] $6A8C rejected a valid dribble record\n");
            return 1;
        }
    }
    if (animation_state.display_frame != 0 ||
        animation_state.record_index != 2 || animation_state.timer != 6 ||
        !animation_state.new_frame) {
        fprintf(stderr, "[Test] $6A8C six-frame cadence was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_set_action_6a8c(
        &animation_state, 0x07);
    if (!allstar_one_on_one_rom_animation_tick_6a8c(
            &animation_pack, &animation_state) ||
        animation_state.display_frame != 0x0f || animation_state.timer != 15) {
        fprintf(stderr, "[Test] $6A8C steal setup record was incorrect\n");
        return 1;
    }
    for (frame = 0; frame < 15; frame++)
        allstar_one_on_one_rom_animation_tick_6a8c(
            &animation_pack, &animation_state);
    if (animation_state.action != 0x06 ||
        animation_state.record_index != 0 || animation_state.timer != 1) {
        fprintf(stderr, "[Test] $6A8C steal transition was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x13);
    if (!allstar_one_on_one_rom_animation_tick_6a8c(
            &animation_pack, &animation_state) ||
        animation_state.display_frame != 0x0c ||
        animation_state.record_index != 1 || animation_state.timer != 6) {
        fprintf(stderr, "[Test] $6A8C held-ball idle record was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x0d);
    if (!allstar_one_on_one_rom_animation_tick_6a8c(
            &animation_pack, &animation_state) ||
        animation_state.display_frame != 0x11 ||
        animation_state.record_index != 1 || animation_state.timer != 6) {
        fprintf(stderr, "[Test] $6A8C loose-player idle record was incorrect\n");
        return 1;
    }

    for (frame = 0; frame < 4; frame++) {
        animation_flip = false;
        allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
        if (!allstar_one_on_one_rom_select_movement_action_782e(
                &animation_state, movement_cases[frame][0], 0, 0,
                false, false, &animation_flip) ||
            animation_state.action != movement_cases[frame][1] ||
            animation_flip != (frame == 0)) {
            fprintf(stderr, "[Test] $782E held-ball movement selector was incorrect\n");
            return 1;
        }
        allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
        if (!allstar_one_on_one_rom_select_movement_action_782e(
                &animation_state, movement_cases[frame][0], 0, 0,
                true, false, &animation_flip) ||
            animation_state.action != movement_cases[frame][2]) {
            fprintf(stderr, "[Test] $782E no-ball movement selector was incorrect\n");
            return 1;
        }
        allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
        if (!allstar_one_on_one_rom_select_movement_action_782e(
                &animation_state, 0, 0, idle_cases[frame][0],
                false, false, &animation_flip) ||
            animation_state.action != idle_cases[frame][1]) {
            fprintf(stderr, "[Test] $782E held-ball idle selector was incorrect\n");
            return 1;
        }
        allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
        if (!allstar_one_on_one_rom_select_movement_action_782e(
                &animation_state, 0, 0, idle_cases[frame][0],
                true, false, &animation_flip) ||
            animation_state.action != idle_cases[frame][2]) {
            fprintf(stderr, "[Test] $782E no-ball idle selector was incorrect\n");
            return 1;
        }
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
    if (!allstar_one_on_one_rom_select_movement_action_782e(
            &animation_state, ALLSTAR_BTN_RIGHT, ALLSTAR_BTN_DOWN,
            ALLSTAR_BTN_UP, false, false, &animation_flip) ||
        animation_state.action != 0x01) {
        fprintf(stderr, "[Test] $782E +$14 direction override was incorrect\n");
        return 1;
    }
    animation_state.timer = 2;
    if (allstar_one_on_one_rom_select_movement_action_782e(
            &animation_state, ALLSTAR_BTN_RIGHT, 0, 0,
            false, false, &animation_flip)) {
        fprintf(stderr, "[Test] $782E record-boundary gate was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x14);
    if (allstar_one_on_one_rom_select_movement_action_782e(
            &animation_state, ALLSTAR_BTN_RIGHT, 0, 0,
            false, false, &animation_flip)) {
        fprintf(stderr, "[Test] $782E protected-action gate was incorrect\n");
        return 1;
    }
    allstar_one_on_one_rom_animation_init_6a8c(&animation_state, 0x00);
    if (allstar_one_on_one_rom_select_movement_action_782e(
            &animation_state, ALLSTAR_BTN_RIGHT, 0, 0,
            false, true, &animation_flip)) {
        fprintf(stderr, "[Test] $782E reaction gate was incorrect\n");
        return 1;
    }

    /* Ghidra $0714/$072F plus the Mesen $FFFB trace: the low-byte stream
       holds for two frames and then follows 18,03,46,A1,D4,9F... */
    allstar_rom_rng_init(&rng, 0x0c18);
    if (allstar_rom_rng_current(&rng) != 0x18 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x18 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x03 ||
        rng.seed != 0x6d03 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x03 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x46 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x46 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0xa1 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0xa1 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0xd4 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0xd4 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x9f ||
        allstar_rom_rng_step_072f(0x00ff, 0x01, 0x02) != 0x0925 ||
        allstar_rom_bcd_byte(59) != 0x59 ||
        allstar_rom_bcd_byte(7) != 0x07) {
        fprintf(stderr, "[Test] $0714/$072F shared frame RNG stream was incorrect\n");
        return 1;
    }

    if (allstar_one_on_one_rom_point_value(18.0f, 92.0f) != 3 ||
        allstar_one_on_one_rom_point_value(19.0f, 92.0f) != 2 ||
        allstar_one_on_one_rom_point_value(145.0f, 92.0f) != 2 ||
        allstar_one_on_one_rom_point_value(146.0f, 92.0f) != 3 ||
        allstar_one_on_one_rom_point_value(47.0f, 144.0f) != 2 ||
        allstar_one_on_one_rom_point_value(118.0f, 144.0f) != 3 ||
        allstar_one_on_one_rom_point_value(84.0f, 145.0f) != 3) {
        fprintf(stderr, "[Test] $798B/$FFD6 two/three-point region was incorrect\n");
        return 1;
    }

    if (allstar_ai_rom_direction_74bb(83.0f, 95.0f, 80, 92) != 0 ||
        allstar_ai_rom_direction_74bb(84.0f, 96.0f, 80, 92) != 0x60 ||
        allstar_ai_rom_direction_74bb(75.0f, 87.0f, 80, 92) != 0x90) {
        fprintf(stderr, "[Test] $74BB four-pixel AI target dead zone was incorrect\n");
        return 1;
    }
    allstar_ai_rom_offense_target_72ea(0x53, 0x2f, &target_x, &target_y);
    if (target_x != 0x10 || target_y != 0x68) return 1;
    allstar_ai_rom_offense_target_72ea(0x53, 0x30, &target_x, &target_y);
    if (target_x != 0x1c || target_y != 0x8c) return 1;
    allstar_ai_rom_offense_target_72ea(0x54, 0x70, &target_x, &target_y);
    if (target_x != 0x90 || target_y != 0x7c) return 1;
    allstar_ai_rom_offense_target_72ea(0x54, 0xb0, &target_x, &target_y);
    if (target_x != 0x78 || target_y != 0x98) {
        fprintf(stderr, "[Test] $72EA side/random AI target tables were incorrect\n");
        return 1;
    }

    if (!allstar_ai_rom_should_shoot_756c(0, 0, 4, 1, 0xaf, 0) ||
        allstar_ai_rom_should_shoot_756c(0, 0, 5, 1, 0xaf, 0xff) ||
        allstar_ai_rom_should_shoot_756c(0, 1, 5, 1, 0xb0, 0x19) ||
        !allstar_ai_rom_should_shoot_756c(0, 1, 5, 1, 0xb0, 0x1a) ||
        !allstar_ai_rom_should_shoot_756c(2, 4, 7, 3, 0x3f, 0) ||
        allstar_ai_rom_should_shoot_756c(2, 4, 8, 3, 0x3f, 0xff)) {
        fprintf(stderr, "[Test] $756C profile/skill CPU shot decision was incorrect\n");
        return 1;
    }

    if (!allstar_ai_rom_should_contest_71ee(71.0f, 105.0f, true) ||
        allstar_ai_rom_should_contest_71ee(70.0f, 105.0f, true) ||
        allstar_ai_rom_should_contest_71ee(98.0f, 105.0f, true) ||
        allstar_ai_rom_should_contest_71ee(71.0f, 106.0f, true) ||
        allstar_ai_rom_should_contest_71ee(84.0f, 95.0f, false)) {
        fprintf(stderr, "[Test] $71EE/$07B4 CPU contest gate was incorrect\n");
        return 1;
    }

    if (!allstar_ai_rom_should_steal_71b3(1, 0x03, true) ||
        allstar_ai_rom_should_steal_71b3(1, 0x04, true) ||
        !allstar_ai_rom_should_steal_71b3(2, 0x18, true) ||
        allstar_ai_rom_should_steal_71b3(2, 0x19, true) ||
        !allstar_ai_rom_should_steal_71b3(3, 0x45, true) ||
        allstar_ai_rom_should_steal_71b3(3, 0x46, true) ||
        allstar_ai_rom_should_steal_71b3(3, 0x00, false)) {
        fprintf(stderr, "[Test] $71B3/$762C CPU steal thresholds were incorrect\n");
        return 1;
    }

    allstar_ai_init(&contact_ai, NULL);
    allstar_ai_set_skill(&contact_ai, 1);
    if (allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, false, 0xbd, 0x40, 80.0f, 112.0f) ||
        !allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, false, 0xbe, 0x40, 80.0f, 112.0f) ||
        contact_ai.rom_contact_hold_frames != 10 ||
        contact_ai.rom_contact_saved_x != 80 ||
        contact_ai.rom_contact_saved_y != 112 ||
        allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, false, 0xff, 0x40, 80.0f, 112.0f)) {
        fprintf(stderr, "[Test] $75CD defender threshold/hold response was incorrect\n");
        return 1;
    }
    allstar_ai_init(&contact_ai, NULL);
    contact_ai.rom_contact_offense_count = 13;
    if (!allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, true, 0xbe, 0x40, 80.0f, 112.0f) ||
        contact_ai.rom_contact_offense_count != 14 ||
        !contact_ai.rom_force_shot) {
        fprintf(stderr, "[Test] $75CD fourteenth-contact shot response was incorrect\n");
        return 1;
    }

    if (!allstar_one_on_one_rom_action_eligible_0a78(0x00) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x03) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x0a) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x12) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x05) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x0c) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x14) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x0e) ||
        allstar_one_on_one_rom_action_eligible_0a78(0x16) ||
        allstar_one_on_one_rom_defense_jump_action_70fd(0x00) != 0x05 ||
        allstar_one_on_one_rom_defense_jump_action_70fd(0x08) != 0x0c ||
        allstar_one_on_one_rom_defense_jump_action_70fd(0x10) != 0x14 ||
        allstar_one_on_one_rom_steal_action_2b14(0x00) != 0x07 ||
        allstar_one_on_one_rom_steal_action_2b14(0x08) != 0x0f ||
        allstar_one_on_one_rom_steal_action_2b14(0x10) != 0x17) {
        fprintf(stderr, "[Test] $0A78/$2B14 steal action gates were incorrect\n");
        return 1;
    }

    if (!allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x00, 80.0f, 120.0f, 91.0f, 125.0f, 0x01, 0x02) ||
        !allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x00, 80.0f, 120.0f, 91.0f, 125.0f, 0x04, 0x08) ||
        allstar_one_on_one_rom_steal_contact_2b14(
            false, 0x00, 80.0f, 120.0f, 91.0f, 125.0f, 0x01, 0x02) ||
        allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x0a, 80.0f, 120.0f, 91.0f, 125.0f, 0x01, 0x02) ||
        allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x00, 80.0f, 120.0f, 92.0f, 125.0f, 0x01, 0x02) ||
        allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x00, 80.0f, 120.0f, 91.0f, 126.0f, 0x01, 0x02) ||
        allstar_one_on_one_rom_steal_contact_2b14(
            true, 0x00, 80.0f, 120.0f, 91.0f, 125.0f, 0x01, 0x01)) {
        fprintf(stderr, "[Test] $2B14 steal collision/facing transfer was incorrect\n");
        return 1;
    }

    if (allstar_one_on_one_rom_jump_height_6c4d(0) != 0.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(6) != 9.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(18) != 21.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(30) != 26.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(36) != 26.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(66) != 0.0f ||
        allstar_one_on_one_rom_jump_height_6c4d(72) != 0.0f) {
        fprintf(stderr, "[Test] $6C4D defensive jump-height records were incorrect\n");
        return 1;
    }

    if (!allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 59.0f) ||
        !allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 66.0f) ||
        allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 58.0f) ||
        allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 67.0f) ||
        allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 80.0f, 130.0f, 26.0f,
            92.0f, 135.0f, 60.0f) ||
        allstar_one_on_one_rom_jump_recovery_2b6c(
            true, false, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 60.0f) ||
        allstar_one_on_one_rom_jump_recovery_2b6c(
            false, true, 80.0f, 130.0f, 26.0f,
            91.0f, 135.0f, 60.0f) ||
        !allstar_one_on_one_rom_jump_recovery_2b6c(
            false, false, 84.0f, 94.0f, 0.0f,
            84.0f, 92.0f, 39.0f)) {
        fprintf(stderr,
                "[Test] $2B6C/$2B88 jump-recovery/flight lock was incorrect\n");
        return 1;
    }

    allstar_one_on_one_shot_reset(&attempt);

    events = allstar_one_on_one_shot_press(&attempt, 1);
    if (!(events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_GATHER) ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER ||
        attempt.shooter != 1 ||
        attempt.gather_clock != ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS) {
        fprintf(stderr, "[Test] First A press did not begin the shooting gather\n");
        return 1;
    }

    if (allstar_one_on_one_shot_press(&attempt, 2) !=
            ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE ||
        allstar_one_on_one_shot_tick(
            &attempt, ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS * 0.5f) !=
            ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE) {
        fprintf(stderr, "[Test] Another player or an early tick disturbed the gather\n");
        return 1;
    }

    events = allstar_one_on_one_shot_press(&attempt, 1);
    if (!(events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_RELEASED ||
        attempt.rom_phase != 0 || attempt.release_latch_frames != 0) {
        fprintf(stderr, "[Test] $702D new-A did not release at phase zero\n");
        return 1;
    }

    allstar_one_on_one_shot_reset(&attempt);
    if (allstar_one_on_one_shot_input(&attempt, 1, false, true) !=
            ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_IDLE) {
        fprintf(stderr, "[Test] $702D accepted B before the A gather\n");
        return 1;
    }
    allstar_one_on_one_shot_input(&attempt, 1, true, false);
    if (allstar_one_on_one_shot_input(&attempt, 1, false, true) !=
            ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER ||
        attempt.rom_phase != 1 || attempt.release_latch_frames != 1) {
        fprintf(stderr, "[Test] $702D held-B did not arm $C16A\n");
        return 1;
    }
    events = allstar_one_on_one_shot_tick(&attempt, 1.0f / 60.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_RELEASE) ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_RELEASED ||
        attempt.rom_phase != 2 || attempt.release_latch_frames != 0) {
        fprintf(stderr, "[Test] $702D $C16A release did not advance to phase two\n");
        return 1;
    }

    allstar_one_on_one_shot_reset(&attempt);
    allstar_one_on_one_shot_press(&attempt, 1);
    events = allstar_one_on_one_shot_tick(
        &attempt, (ALLSTAR_ONE_ON_ONE_SHOT_GATHER_FRAMES - 1) / 60.0f);
    if (events != ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER) {
        fprintf(stderr, "[Test] Traveling was called before the 67-frame landing\n");
        return 1;
    }
    events = allstar_one_on_one_shot_tick(&attempt, 1.0f / 60.0f);
    if (!(events & ALLSTAR_ONE_ON_ONE_SHOT_EVENT_TRAVELING) ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_IDLE || attempt.shooter != 0) {
        fprintf(stderr, "[Test] Landing without release did not call traveling\n");
        return 1;
    }

    allstar_one_on_one_match_init(&match, 120.0f, 24.0f, 0, false);
    match.shot_clock = 8.0f;
    if (allstar_one_on_one_match_call_traveling(&match, 2) !=
            ALLSTAR_ONE_ON_ONE_EVENT_NONE) {
        fprintf(stderr, "[Test] Traveling was accepted for the non-possessor\n");
        return 1;
    }
    events = allstar_one_on_one_match_call_traveling(&match, 1);
    if (!(events & ALLSTAR_ONE_ON_ONE_EVENT_TRAVELING) ||
        match.p1_possession || match.shot_clock != 24.0f) {
        fprintf(stderr, "[Test] Traveling did not award a reset possession\n");
        return 1;
    }

    if (!allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 0, false, &release) ||
        release.x_offset != 7 || release.ground_y_offset != -2 ||
        release.height_offset != -2 ||
        !allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_B, 0, 0, true, &release) ||
        release.x_offset != 10 || release.height_offset != -2 ||
        !allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_B, 2, 0, false, &release) ||
        release.x_offset != 20 || release.ground_y_offset != -4 ||
        release.height_offset != -2 ||
        !allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_A, 2, 1, false, &release) ||
        release.x_offset != 8 || release.height_offset != 1 ||
        !allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_B, 2, 2, false, &release) ||
        release.x_offset != -5 || release.height_offset != -2 ||
        allstar_one_on_one_rom_release_offset(
            ALLSTAR_ROM_SHOT_ACTION_A, 3, 0, false, &release)) {
        fprintf(stderr, "[Test] $7F37 release-offset table mapping was incorrect\n");
        return 1;
    }
    if (allstar_one_on_one_rom_release_height(90, 130, 0, -2) != 40 ||
        allstar_one_on_one_rom_release_height(90, 130, 2, -2) != 38 ||
        allstar_one_on_one_rom_release_height(90, 130, 2, 1) != 35) {
        fprintf(stderr, "[Test] $7F37 release-height coordinate calculation was incorrect\n");
        return 1;
    }

    if (!allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 0, &animation_frame) ||
        animation_frame != 0x08 ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 5, &animation_frame) ||
        animation_frame != 0x08 ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 6, &animation_frame) ||
        animation_frame != 0x09 ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 36, &animation_frame) ||
        animation_frame != 0x0a ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 61, &animation_frame) ||
        animation_frame != 0x0c ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_B, 0, 66, &animation_frame) ||
        animation_frame != 0x0b ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 1, 0, &animation_frame) ||
        animation_frame != 0x12 ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 2, 0, &animation_frame) ||
        animation_frame != 0x13 ||
        !allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 3, 0, &animation_frame) ||
        animation_frame != 0x14 ||
        allstar_one_on_one_rom_shot_animation_frame(
            ALLSTAR_ROM_SHOT_ACTION_A, 0, 67, &animation_frame)) {
        fprintf(stderr, "[Test] $6A8C shot animation record sequence was incorrect\n");
        return 1;
    }

    if (allstar_one_on_one_rom_shot_variant(16.0f, 96.0f) != 0 ||
        allstar_one_on_one_rom_shot_variant(76.0f, 96.0f) != 0 ||
        allstar_one_on_one_rom_shot_variant(77.0f, 96.0f) != 1 ||
        allstar_one_on_one_rom_shot_variant(88.0f, 96.0f) != 1 ||
        allstar_one_on_one_rom_shot_variant(89.0f, 96.0f) != 2 ||
        allstar_one_on_one_rom_shot_variant(64.0f, 112.0f) != 0 ||
        allstar_one_on_one_rom_shot_variant(65.0f, 112.0f) != 1 ||
        allstar_one_on_one_rom_shot_variant(101.0f, 112.0f) != 2 ||
        allstar_one_on_one_rom_shot_variant(80.0f, 129.0f) != 1) {
        fprintf(stderr, "[Test] $791D/$794B shot-position class was incorrect\n");
        return 1;
    }

    if (!allstar_one_on_one_player_can_pick_up_ball(
            80.0f, 100.0f, 91.0f, 107.0f) ||
        allstar_one_on_one_player_can_pick_up_ball(
            80.0f, 100.0f, 92.0f, 107.0f) ||
        allstar_one_on_one_player_can_pick_up_ball(
            80.0f, 100.0f, 91.0f, 108.0f)) {
        fprintf(stderr, "[Test] $077D loose-ball collision limits were incorrect\n");
        return 1;
    }

    court_x = 0.0f;
    court_y = 0.0f;
    allstar_one_on_one_rom_clamp_player_court(&court_x, &court_y);
    if (court_x != 16.0f || court_y != 98.0f) {
        fprintf(stderr, "[Test] ROM player minimum court bounds were incorrect\n");
        return 1;
    }
    court_x = 200.0f;
    court_y = 200.0f;
    allstar_one_on_one_rom_clamp_player_court(&court_x, &court_y);
    if (court_x != 156.0f || court_y != 152.0f) {
        fprintf(stderr, "[Test] ROM player maximum court bounds were incorrect\n");
        return 1;
    }
    court_x = 80.0f;
    court_y = 120.0f;
    allstar_one_on_one_rom_clamp_player_court(&court_x, &court_y);
    if (court_x != 80.0f || court_y != 120.0f) {
        fprintf(stderr, "[Test] In-bounds ROM player coordinate was disturbed\n");
        return 1;
    }

    if (allstar_one_on_one_rom_player_x_side_6ec0(100, 110, 0) != 3 ||
        allstar_one_on_one_rom_player_x_side_6ec0(100, 111, 0) != 0 ||
        allstar_one_on_one_rom_player_x_side_6ec0(100, 89, 0) != 4 ||
        allstar_one_on_one_rom_player_x_side_6ec0(100, 88, 0) != 0 ||
        allstar_one_on_one_rom_player_y_side_6eea(100, 105, 0) != 2 ||
        allstar_one_on_one_rom_player_y_side_6eea(100, 106, 0) != 0 ||
        allstar_one_on_one_rom_player_y_side_6eea(100, 94, 0) != 1 ||
        allstar_one_on_one_rom_player_y_side_6eea(100, 93, 0) != 0) {
        fprintf(stderr, "[Test] $6EC0/$6EEA contact-side windows were incorrect\n");
        return 1;
    }
    if (!allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_RIGHT, 0, 64, 112, 72, 112) ||
        allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_RIGHT, 0, 64, 112, 60, 112) ||
        allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_RIGHT, 0, 64, 112, 72, 118) ||
        !allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_LEFT, 0, 64, 112, 56, 112) ||
        !allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_UP, 0, 64, 112, 64, 104) ||
        !allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_DOWN, 0, 64, 112, 64, 120) ||
        allstar_one_on_one_rom_player_pair_blocks_6e3c(
            ALLSTAR_BTN_RIGHT, 2, 64, 112, 72, 112) ||
        allstar_one_on_one_rom_player_pair_blocks_6e3c(
            0, 0, 64, 112, 72, 112)) {
        fprintf(stderr, "[Test] $6E3C directional player-pair gate was incorrect\n");
        return 1;
    }
    court_x = 80.0f;
    court_y = 120.0f;
    contact_latch = true;
    if (!allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_RIGHT, 0, &court_x, &court_y,
            140.0f, 140.0f, &contact_latch) ||
        court_x != 84.0f || court_y != 120.0f || contact_latch) {
        fprintf(stderr, "[Test] $6B72/$6BAD four-pixel movement was incorrect\n");
        return 1;
    }
    court_x = 72.0f;
    court_y = 112.0f;
    contact_latch = false;
    if (allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_RIGHT, 0, &court_x, &court_y,
            80.0f, 112.0f, &contact_latch) ||
        court_x != 72.0f || court_y != 112.0f || !contact_latch) {
        fprintf(stderr, "[Test] $6BAD did not return before contact displacement\n");
        return 1;
    }
    court_x = 72.0f;
    court_y = 112.0f;
    if (!allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_RIGHT, 0, &court_x, &court_y,
            68.0f, 112.0f, NULL) ||
        court_x != 76.0f || court_y != 112.0f) {
        fprintf(stderr, "[Test] $6BAD did not permit movement away from contact\n");
        return 1;
    }
    court_x = 80.0f;
    court_y = 120.0f;
    if (!allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_UP | ALLSTAR_BTN_LEFT, 0,
            &court_x, &court_y, 140.0f, 140.0f, NULL) ||
        court_x != 76.0f || court_y != 116.0f) {
        fprintf(stderr, "[Test] $6B72 direction order/displacement was incorrect\n");
        return 1;
    }
    court_x = 156.0f;
    court_y = 152.0f;
    if (allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_RIGHT | ALLSTAR_BTN_DOWN, 0,
            &court_x, &court_y, 20.0f, 100.0f, NULL) ||
        court_x != 156.0f || court_y != 152.0f) {
        fprintf(stderr, "[Test] $6BAD/$6BC7 court return gates were incorrect\n");
        return 1;
    }

    memset(&p1_contact, 0, sizeof(p1_contact));
    memset(&p2_contact, 0, sizeof(p2_contact));
    p1_contact.blocked_contact = true;
    p1_contact.violation_counter = 1;
    contact_event = allstar_one_on_one_rom_contact_tick_2c50(
        true, 1, &p1_contact, &p2_contact, 0x01, 0x09,
        72.0f, 96.0f, 84.0f, 96.0f, &contact_offender);
    if (contact_event != ALLSTAR_ROM_CONTACT_CHARGING ||
        contact_offender != 1) {
        fprintf(stderr, "[Test] $2CCA/$0AC5 charging classification was incorrect\n");
        return 1;
    }
    memset(&p1_contact, 0, sizeof(p1_contact));
    memset(&p2_contact, 0, sizeof(p2_contact));
    p2_contact.blocked_contact = true;
    p2_contact.violation_counter = 1;
    contact_event = allstar_one_on_one_rom_contact_tick_2c50(
        true, 1, &p1_contact, &p2_contact, 0x01, 0x09,
        72.0f, 96.0f, 84.0f, 96.0f, &contact_offender);
    if (contact_event != ALLSTAR_ROM_CONTACT_BLOCKING ||
        contact_offender != 2) {
        fprintf(stderr, "[Test] $2CCA/$0AC5 blocking classification was incorrect\n");
        return 1;
    }
    p1_contact.blocked_contact = true;
    p1_contact.violation_counter = 1;
    p2_contact.blocked_contact = true;
    p2_contact.violation_counter = 1;
    contact_event = allstar_one_on_one_rom_contact_tick_2c50(
        true, 1, &p1_contact, &p2_contact, 0x01, 0x09,
        72.0f, 96.0f, 84.0f, 96.0f, &contact_offender);
    if (contact_event != ALLSTAR_ROM_CONTACT_CHARGING ||
        contact_offender != 1) {
        fprintf(stderr, "[Test] $2C50 player-one violation priority was incorrect\n");
        return 1;
    }
    memset(&p1_contact, 0, sizeof(p1_contact));
    memset(&p2_contact, 0, sizeof(p2_contact));
    p1_contact.blocked_contact = true;
    p1_contact.violation_counter = 1;
    contact_event = allstar_one_on_one_rom_contact_tick_2c50(
        true, 1, &p1_contact, &p2_contact,
        ALLSTAR_ROM_SHOT_ACTION_A, 0x09,
        72.0f, 96.0f, 84.0f, 96.0f, &contact_offender);
    if (contact_event != ALLSTAR_ROM_CONTACT_NONE ||
        p1_contact.blocked_contact ||
        p1_contact.violation_counter != ALLSTAR_ROM_CONTACT_VIOLATION_FRAMES) {
        fprintf(stderr, "[Test] $2CCA protected-action contact clear was incorrect\n");
        return 1;
    }
    p1_contact.blocked_contact = true;
    p1_contact.violation_counter = 1;
    contact_event = allstar_one_on_one_rom_contact_tick_2c50(
        true, 1, &p1_contact, &p2_contact, 0x01, 0x09,
        72.0f, 96.0f, 88.0f, 96.0f, &contact_offender);
    if (contact_event != ALLSTAR_ROM_CONTACT_NONE ||
        p1_contact.blocked_contact) {
        fprintf(stderr, "[Test] $0AC5 rejected-alignment reentry was incorrect\n");
        return 1;
    }

    memset(&recovery, 0, sizeof(recovery));
    recovery.cooldown_frames = 1;
    if (allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 23.0f, false, false, false,
            true, true, true, true) != 1 ||
        recovery.cooldown_frames != ALLSTAR_ROM_RECOVERY_COOLDOWN_FRAMES) {
        fprintf(stderr, "[Test] $2AE2 cooldown order or P1 recovery priority was incorrect\n");
        return 1;
    }
    if (allstar_one_on_one_rom_recovery_dispatch(
            &recovery, true, false, 0.0f, false, false, false,
            true, true, true, true) != 0 ||
        recovery.cooldown_frames != ALLSTAR_ROM_RECOVERY_COOLDOWN_FRAMES - 1) {
        fprintf(stderr, "[Test] $2AE2 cooldown did not tick before possession exit\n");
        return 1;
    }
    recovery.cooldown_frames = 0;
    if (allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 24.0f, false, false, false,
            true, true, true, true) != 0 ||
        allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, true, 0.0f, false, false, false,
            true, true, true, true) != 0 ||
        allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 0.0f, true, false, false,
            true, true, true, true) != 0 ||
        allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 0.0f, false, true, false,
            true, true, true, true) != 0 ||
        allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 0.0f, false, false, true,
            true, true, true, true) != 0 ||
        allstar_one_on_one_rom_recovery_dispatch(
            &recovery, false, false, 0.0f, false, false, false,
            false, true, true, true) != 2) {
        fprintf(stderr, "[Test] $2AE2/$2B07/$2B88 recovery gates were incorrect\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing game for shooting integration\n");
        return 1;
    }
    game.selected_mode = ALLSTAR_MODE_ONE_ON_ONE;
    game.selected_player_1 = 2;
    game.roster.players[2].shooting_3pt = 100;
    game.roster.players[2].shooting_2pt = 100;
    srand(1);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 120 && game.one_on_one.p1_possession; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (game.one_on_one.p1_score != 0 || game.one_on_one.p1_possession) {
        fprintf(stderr, "[Test] Traced class-one shot did not resolve through ROM contacts\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    game.roster.players[2].shooting_3pt = 0;
    game.roster.players[2].shooting_2pt = 0;
    srand(1);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 240 && game.one_on_one.p1_possession; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (game.one_on_one.p1_score != 0 || game.one_on_one.p1_possession ||
        game.one_on_one.shot_clock != game.one_on_one.shot_clock_seconds) {
        fprintf(stderr, "[Test] Miss did not return to court and resolve by recovery\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_ONE_ON_ONE_SHOT_GATHER_SECONDS);
    if (game.one_on_one.p1_possession ||
        game.one_on_one.shot_clock != game.one_on_one.shot_clock_seconds) {
        fprintf(stderr, "[Test] Scene did not apply the unreleased-shot turnover\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_shutdown(&game);

    printf("[Test] PASSED: shooting, steals, contest jumps, ROM no-goaltend behavior, and recovery\n");
    return 0;
}

int allstar_cli_test_tournament(void) {
    AllStarTournamentState tournament;
    AllStarGame game;
    uint32_t player_1;
    uint32_t player_2;
    uint32_t quarterfinal_winners[4];
    uint32_t semifinal_winners[2];
    int match;

    printf("[Test] Running Tournament Bracket Parity Tests...\n");
    allstar_tournament_reset(&tournament);

    for (match = 0; match < 4; match++) {
        if (tournament.round != 0 || tournament.current_match != match ||
            !allstar_tournament_get_current_match(&tournament, &player_1, &player_2)) {
            fprintf(stderr, "[Test] Quarterfinal %d was not exposed in bracket order\n", match + 1);
            return 1;
        }
        tournament.match_in_progress = true;
        if (allstar_tournament_record_winner(&tournament, UINT32_MAX)) {
            fprintf(stderr, "[Test] Tournament accepted a winner outside the active match\n");
            return 1;
        }
        quarterfinal_winners[match] = (match & 1) ? player_2 : player_1;
        if (!allstar_tournament_record_winner(&tournament, quarterfinal_winners[match]) ||
            tournament.match_in_progress) {
            fprintf(stderr, "[Test] Quarterfinal %d winner was not recorded\n", match + 1);
            return 1;
        }
    }

    if (tournament.round != 1 || tournament.current_match != 0 ||
        memcmp(tournament.semifinalists, quarterfinal_winners,
               sizeof(quarterfinal_winners)) != 0) {
        fprintf(stderr, "[Test] Four quarterfinal winners did not form the semifinal round\n");
        return 1;
    }

    for (match = 0; match < 2; match++) {
        if (!allstar_tournament_get_current_match(&tournament, &player_1, &player_2) ||
            player_1 != quarterfinal_winners[match * 2] ||
            player_2 != quarterfinal_winners[match * 2 + 1]) {
            fprintf(stderr, "[Test] Semifinal %d pairing did not use adjacent bracket winners\n", match + 1);
            return 1;
        }
        semifinal_winners[match] = player_2;
        tournament.match_in_progress = true;
        if (!allstar_tournament_record_winner(&tournament, semifinal_winners[match])) {
            fprintf(stderr, "[Test] Semifinal %d winner was not recorded\n", match + 1);
            return 1;
        }
    }

    if (tournament.round != 2 || tournament.current_match != 0 ||
        memcmp(tournament.finalists, semifinal_winners,
               sizeof(semifinal_winners)) != 0 ||
        !allstar_tournament_get_current_match(&tournament, &player_1, &player_2) ||
        player_1 != semifinal_winners[0] || player_2 != semifinal_winners[1]) {
        fprintf(stderr, "[Test] Two semifinal winners did not form the final\n");
        return 1;
    }

    tournament.match_in_progress = true;
    if (!allstar_tournament_record_winner(&tournament, player_1) ||
        !tournament.complete || tournament.champion != player_1 ||
        tournament.match_in_progress ||
        allstar_tournament_get_current_match(&tournament, NULL, NULL) ||
        allstar_tournament_record_winner(&tournament, player_2)) {
        fprintf(stderr, "[Test] Final winner did not close and lock the bracket\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing game for champion exit flow\n");
        return 1;
    }
    game.selected_mode = ALLSTAR_MODE_TOURNAMENT;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_TOURNAMENT);
    game.tournament = tournament;
    game.input.buttons_pressed = ALLSTAR_BTN_A;
    allstar_game_tick(&game, 0.0f);
    if (!game.active_scene || game.active_scene->id != ALLSTAR_SCENE_INTRO ||
        game.tournament.active) {
        fprintf(stderr, "[Test] Champion dismissal did not return to the title flow\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_shutdown(&game);

    printf("[Test] PASSED: 4 quarterfinals, 2 semifinals, final, champion, and exit\n");
    return 0;
}

int allstar_cli_test_settings(void) {
    AllStarGameSettings settings;
    AllStarGame game;
    AllStarAIController ai;
    AllStarColor reference_frame[ALLSTAR_GB_WIDTH * ALLSTAR_GB_HEIGHT];

    printf("[Test] Running Settings Persistence Tests...\n");
    allstar_game_settings_init(&settings);
    if (settings.play_to != 0 || settings.skill_level != 1 ||
        settings.winners_outs || settings.game_minutes != 2 ||
        settings.free_throw_attempts != 5 ||
        !settings.accuracy_computer_positions) {
        fprintf(stderr, "[Test] Settings defaults diverged from ROM $20D0\n");
        return 1;
    }
    if (allstar_game_settings_cycle_time(2, 1) != 5 ||
        allstar_game_settings_cycle_time(5, 1) != 8 ||
        allstar_game_settings_cycle_time(8, 1) != 12 ||
        allstar_game_settings_cycle_time(12, 1) != 2 ||
        allstar_game_settings_cycle_time(2, -1) != 12 ||
        allstar_game_settings_cycle_throws(5, 1) != 10 ||
        allstar_game_settings_cycle_throws(10, 1) != 20 ||
        allstar_game_settings_cycle_throws(20, 1) != 5) {
        fprintf(stderr, "[Test] Settings cycles diverged from ROM $22EF\n");
        return 1;
    }

    allstar_ai_init(&ai, NULL);
    allstar_ai_set_skill(&ai, 1);
    if (ai.decision_interval != 8.0f / 60.0f) return 1;
    allstar_ai_set_skill(&ai, 2);
    if (ai.decision_interval != 4.0f / 60.0f) return 1;
    allstar_ai_set_skill(&ai, 3);
    if (ai.decision_interval != 1.0f / 60.0f) {
        fprintf(stderr, "[Test] Skill delays diverged from ROM $1FFA\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing game for settings persistence\n");
        return 1;
    }
    game.selected_mode = ALLSTAR_MODE_ONE_ON_ONE;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    game.input.buttons_pressed = ALLSTAR_BTN_RIGHT;
    allstar_game_tick(&game, 0.0f);
    game.input.buttons_pressed = 0;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    if (game.settings.play_to != 1) {
        fprintf(stderr, "[Test] Settings were discarded after leaving and revisiting the scene\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.selected_mode = ALLSTAR_MODE_FREE_THROW;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    game.input.buttons_pressed = ALLSTAR_BTN_RIGHT;
    allstar_game_tick(&game, 0.0f);
    game.input.buttons_pressed = 0;
    if (game.settings.free_throw_attempts != 10) {
        fprintf(stderr, "[Test] Free Throw attempt setting did not use the ROM cycle\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.selected_mode = ALLSTAR_MODE_ACCURACY;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    game.input.buttons_pressed = ALLSTAR_BTN_RIGHT;
    allstar_game_tick(&game, 0.0f);
    game.input.buttons_pressed = 0;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    if (game.settings.accuracy_computer_positions) {
        fprintf(stderr, "[Test] Complementary Accuracy position setting did not persist\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.settings.accuracy_computer_positions = true;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    allstar_game_tick(&game, 0.0f);
    memcpy(reference_frame, game.renderer->pixels, sizeof(reference_frame));
    game.settings.accuracy_computer_positions = false;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    allstar_game_tick(&game, 0.0f);
    if (memcmp(reference_frame, game.renderer->pixels, sizeof(reference_frame)) == 0) {
        fprintf(stderr, "[Test] Accuracy position source did not affect the native scene\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.settings.free_throw_attempts = 5;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    allstar_game_tick(&game, 0.0f);
    memcpy(reference_frame, game.renderer->pixels, sizeof(reference_frame));
    game.settings.free_throw_attempts = 10;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    allstar_game_tick(&game, 0.0f);
    if (memcmp(reference_frame, game.renderer->pixels, sizeof(reference_frame)) == 0) {
        fprintf(stderr, "[Test] Free Throw attempt limit did not affect the native scene\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    game.settings.play_to = 21;
    game.settings.skill_level = 3;
    game.settings.winners_outs = true;
    game.settings.game_minutes = 5;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    if (game.one_on_one.play_to != 21 || !game.one_on_one.winners_outs ||
        game.one_on_one.period_seconds != 300.0f ||
        game.one_on_one.game_clock != 300.0f) {
        fprintf(stderr, "[Test] Persistent settings were not consumed by One-on-One\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: ROM defaults, cycles, scene persistence, and gameplay consumption\n");
    return 0;
}

int allstar_cli_test_headless_frames(void) {
    printf("[Test] Running Headless Multi-Scene Frame Verification...\n");
    AllStarGame game;
    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing headless game\n");
        return 1;
    }

    /* Tick 60 frames in Intro */
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to Menu Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to 1-on-1 Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to 3-Point Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to Free Throw Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to HORSE Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_HORSE);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    /* Switch to Tournament Scene and tick 60 frames */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_TOURNAMENT);
    for (int i = 0; i < 60; i++) {
        allstar_game_tick(&game, 1.0f / 60.0f);
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: Headless multi-scene 420 frames rendered without error\n");
    return 0;
}

int allstar_cli_dump_screenshots(const char *out_dir,
                                 const char *asset_pack_path) {
    printf("[Screenshots] Exporting scene screenshots to: %s\n", out_dir);
    AllStarGame game;
    if (!allstar_game_init(&game, asset_pack_path)) {
        fprintf(stderr, "[Screenshots] Failed initializing game\n");
        return 1;
    }

    char path[512];

    /* 0. Copyright */
    for (int i = 0; i < 20; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\00_copyright.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 1. Title Screen */
    for (int i = 0; i < 120; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\01_intro.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 2. Menu */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\02_menu.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 2b. Settings */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\02b_settings.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 3. Roster Select */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ROSTER_SELECT);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\03_roster.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4. One on One */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\04_one_on_one.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4a. One-on-One gather: one native A edge begins the held-ball jump. */
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04a_one_on_one_gather.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4b. One-on-One shot flight: release A, then a second A edge. */
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 6; i++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path), "%s\\04b_one_on_one_flight.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4c. One-on-One defensive jump pose while the ball remains live. */
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 12; i++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path), "%s\\04c_one_on_one_defense_jump.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 5. Three Point */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\05_three_point.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 6. Free Throw */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\06_free_throw.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7. HORSE */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_HORSE);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\07_horse.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 8. Tournament */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_TOURNAMENT);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\08_tournament.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    allstar_game_shutdown(&game);
    printf("[Screenshots] Exported all scenes plus One-on-One gameplay frames.\n");
    return 0;
}

int allstar_cli_test_all(void) {
    int failed = 0;
    failed += allstar_cli_test_roster();
    failed += allstar_cli_test_physics();
    failed += allstar_cli_test_mode_routing();
    failed += allstar_cli_test_settings();
    failed += allstar_cli_test_one_on_one_lifecycle();
    failed += allstar_cli_test_one_on_one_shooting();
    failed += allstar_cli_test_tournament();
    failed += allstar_cli_test_headless_frames();

    if (failed == 0) {
        printf("\n========================================\n");
        printf("ALL TESTS PASSED SUCCESSFULLY!\n");
        printf("========================================\n");
        return 0;
    } else {
        fprintf(stderr, "\n%d TEST SUITE(S) FAILED!\n", failed);
        return 1;
    }
}

int allstar_cli_main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    } else if (strcmp(cmd, "--rom-test") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --rom-test requires <rom.gb> path\n");
            return 1;
        }
        return allstar_cli_rom_test(argv[2]);
    } else if (strcmp(cmd, "--build-assetpack") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: --build-assetpack requires <rom.gb> and <out.assetpack>\n");
            return 1;
        }
        return allstar_cli_build_assetpack(argv[2], argv[3]);
    } else if (strcmp(cmd, "--dump-screenshots") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: --dump-screenshots requires <out_dir> path\n");
            return 1;
        }
        return allstar_cli_dump_screenshots(argv[2], argc >= 4 ? argv[3] : NULL);
    } else if (strcmp(cmd, "--test-roster") == 0) {
        return allstar_cli_test_roster();
    } else if (strcmp(cmd, "--test-physics") == 0) {
        return allstar_cli_test_physics();
    } else if (strcmp(cmd, "--test-mode-routing") == 0) {
        return allstar_cli_test_mode_routing();
    } else if (strcmp(cmd, "--test-settings") == 0) {
        return allstar_cli_test_settings();
    } else if (strcmp(cmd, "--test-one-on-one-lifecycle") == 0) {
        return allstar_cli_test_one_on_one_lifecycle();
    } else if (strcmp(cmd, "--test-one-on-one-shooting") == 0) {
        return allstar_cli_test_one_on_one_shooting();
    } else if (strcmp(cmd, "--test-tournament") == 0) {
        return allstar_cli_test_tournament();
    } else if (strcmp(cmd, "--test-headless-frames") == 0) {
        return allstar_cli_test_headless_frames();
    } else if (strcmp(cmd, "--test-all") == 0) {
        return allstar_cli_test_all();
    } else if (strcmp(cmd, "--play") == 0) {
        const char *pack_path = (argc >= 3) ? argv[2] : NULL;
        AllStarGame game;
        if (!allstar_game_init(&game, pack_path)) {
            fprintf(stderr, "Failed to start game\n");
            return 1;
        }
        printf("[Game] Initialized game in mode: %u\n", game.selected_mode);
        allstar_game_shutdown(&game);
        return 0;
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd);
        print_usage(argv[0]);
        return 1;
    }
}
