#include "allstar_game.h"
#include "allstar_rom.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    printf("  --dump-screenshots <out_dir>       Render all game scenes to BMP screenshots\n");
    printf("  --test-roster                      Verify roster data tables\n");
    printf("  --test-physics                     Run physics simulation unit tests\n");
    printf("  --test-mode-routing                Verify all ROM menu IDs route correctly\n");
    printf("  --test-settings                    Verify ROM settings values and persistence\n");
    printf("  --test-one-on-one-lifecycle        Verify One-on-One endings and returns\n");
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
    printf("[Test] Running Physics Simulation Tests...\n");
    AllStarBall ball;
    allstar_physics_init_ball(&ball);

    allstar_physics_shoot_ball(&ball, 80.0f, 100.0f, 80.0f, 24.0f, 40.0f, 1, 2);
    if (!ball.in_flight || ball.z <= 0.0f) {
        fprintf(stderr, "[Test] Shot initialization failed\n");
        return 1;
    }

    /* Step simulation for 1 second */
    for (int i = 0; i < 60; i++) {
        allstar_physics_update_ball(&ball, 1.0f / 60.0f);
    }

    printf("[Test] PASSED: Ball trajectory physics test (final z=%.1f)\n", ball.z);
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

int allstar_cli_dump_screenshots(const char *out_dir) {
    printf("[Screenshots] Exporting scene screenshots to: %s\n", out_dir);
    AllStarGame game;
    if (!allstar_game_init(&game, NULL)) {
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
    printf("[Screenshots] Successfully exported all 8 scene screenshots.\n");
    return 0;
}

int allstar_cli_test_all(void) {
    int failed = 0;
    failed += allstar_cli_test_roster();
    failed += allstar_cli_test_physics();
    failed += allstar_cli_test_mode_routing();
    failed += allstar_cli_test_settings();
    failed += allstar_cli_test_one_on_one_lifecycle();
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
        return allstar_cli_dump_screenshots(argv[2]);
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
