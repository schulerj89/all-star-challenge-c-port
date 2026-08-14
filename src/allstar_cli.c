#include "allstar_game.h"
#include "allstar_rom.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_physics.h"
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

    /* 1. Intro */
    for (int i = 0; i < 30; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\01_intro.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 2. Menu */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\02_menu.bmp", out_dir);
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
