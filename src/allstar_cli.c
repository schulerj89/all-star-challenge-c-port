#include "allstar_game.h"
#include "allstar_horse.h"
#include "allstar_rom.h"
#include "allstar_asset_pack.h"
#include "allstar_roster.h"
#include "allstar_physics.h"
#include "allstar_ai.h"
#include "allstar_rng.h"
#include "allstar_free_throw.h"
#include "allstar_accuracy.h"
#include "allstar_tournament.h"
#include "allstar_postgame.h"
#include "allstar_select.h"
#include "allstar_shot_result.h"
#include "allstar_court_state.h"
#include "allstar_game_clock.h"
#include "allstar_status_panel.h"
#include "allstar_menu.h"
#include "allstar_voice_state.h"
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
    printf("  --export-rom-sfx <pack> <05.wav> <0D.wav> <0C.wav> <0F.wav> <0E.wav> <09.wav> <04.wav>\n");
    printf("                                        Export decoded ROM cues\n");
    printf("  --export-free-throw-sfx <pack> <08.wav> <0A.wav>\n");
    printf("                                        Export Free Throw net/contact cues\n");
    printf("  --export-horse-sfx <pack> <07.wav> Export ROM Horse letter cue\n");
    printf("  --export-accuracy-sfx <pack> <02.wav> Export Accuracy result cue\n");
    printf("  --dump-screenshots <out_dir> [pack] Render all game scenes to BMP screenshots\n");
    printf("  --test-roster                      Verify roster data tables\n");
    printf("  --test-physics                     Run physics simulation unit tests\n");
    printf("  --test-mode-routing                Verify all ROM menu IDs route correctly\n");
    printf("  --test-settings                    Verify ROM settings values and persistence\n");
    printf("  --test-one-on-one-lifecycle        Verify One-on-One endings and returns\n");
    printf("  --test-one-on-one-shooting         Verify staged shooting and traveling\n");
    printf("  --test-one-on-one-presentation     Verify One-on-One movement/roster audio\n");
    printf("  --test-free-throw                 Verify ROM Free Throw lifecycle/physics\n");
    printf("  --test-horse                      Verify ROM H-O-R-S-E rules/scene\n");
    printf("  --test-accuracy                   Verify ROM Accuracy rules/scene\n");
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
        allstar_one_on_one_rom_shot_record_index(6) != 1 ||
        allstar_one_on_one_rom_shot_record_index(7) != 2 ||
        allstar_one_on_one_rom_shot_record_index(36) != 6 ||
        allstar_one_on_one_rom_shot_record_index(37) != 7 ||
        allstar_one_on_one_rom_shot_record_index(38) != 8 ||
        allstar_one_on_one_rom_shot_release_height(1) != 0x26 ||
        allstar_one_on_one_rom_shot_release_height(2) != 0x2f ||
        allstar_one_on_one_rom_shot_release_height(7) != 0x40 ||
        allstar_one_on_one_rom_shot_release_height(8) != 0x3e ||
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

    /* Live Mesen trace: a 37-frame release reads player +$03=$07,
       composes Z=$40 through $6A8C->$6C4D->$7F37, selects VZ=$01C8 in
       $7C58, and reaches $1CED's $54/$5C/$38 score cell. This exercises
       launch, all 64 $7BE8 integrations, and the actual contact dispatcher. */
    allstar_physics_shoot_ball_rom_7c58(
        &ball, 83.0f, 150.0f,
        (float)allstar_one_on_one_rom_shot_release_height(7),
        84.0f, 92.0f, 3,
        allstar_one_on_one_rom_shot_vertical_velocity(0, 3, 7),
        0, 1, 2);
    contacts = ALLSTAR_BALL_CONTACT_NONE;
    for (frame = 0; frame < 64; frame++) {
        allstar_physics_update_ball(&ball, ALLSTAR_PHYSICS_STEP_SECONDS);
        contacts = allstar_physics_apply_rom_court_contacts(&ball);
        if (contacts & ALLSTAR_BALL_CONTACT_SCORE) break;
    }
    if (!(contacts & ALLSTAR_BALL_CONTACT_SCORE) || !ball.made_basket ||
        ball.rom_step_state.x != 0x5400 ||
        ball.rom_step_state.y != 0x5c00 ||
        (ball.rom_step_state.z >> 8) != 0x38) {
        fprintf(stderr,
                "[Test] $6A8C/$7F37/$7C58 launched make did not reach $1CED score state\n");
        return 1;
    }

    allstar_physics_shoot_ball_rom_7c58(
        &miss, 83.0f, 150.0f,
        (float)allstar_one_on_one_rom_shot_release_height(2),
        84.0f, 92.0f, 3,
        allstar_one_on_one_rom_shot_vertical_velocity(0, 3, 2),
        0, 1, 2);
    contacts = ALLSTAR_BALL_CONTACT_NONE;
    for (frame = 0; frame < 90; frame++) {
        allstar_physics_update_ball(&miss, ALLSTAR_PHYSICS_STEP_SECONDS);
        contacts = allstar_physics_apply_rom_court_contacts(&miss);
        if (contacts & ALLSTAR_BALL_CONTACT_SCORE) break;
    }
    if ((contacts & ALLSTAR_BALL_CONTACT_SCORE) || miss.made_basket) {
        fprintf(stderr, "[Test] Early $7C58 release incorrectly scored\n");
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
        contact.rom_contact_cooldown_frames != 8 ||
        contact.recoverable) {
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
        bool uses_settings;
    } expected[ALLSTAR_MODE_COUNT] = {
        { ALLSTAR_MODE_ONE_ON_ONE, "One On One",        ALLSTAR_SCENE_ONE_ON_ONE,  true,  true  },
        { ALLSTAR_MODE_FREE_THROW, "Free Throws",       ALLSTAR_SCENE_FREE_THROW,  false, true  },
        { ALLSTAR_MODE_HORSE,      "Horse",             ALLSTAR_SCENE_HORSE,       true,  false },
        { ALLSTAR_MODE_ACCURACY,   "Accuracy Shootout", ALLSTAR_SCENE_THREE_POINT, false, true  },
        { ALLSTAR_MODE_TOURNAMENT, "Tournament",        ALLSTAR_SCENE_TOURNAMENT,  true,  true  }
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
        const bool uses_settings = allstar_game_mode_uses_settings(mode);
        const char *name = allstar_game_mode_name(mode);

        if (mode != expected[menu_index].mode ||
            scene_id != expected[menu_index].scene_id ||
            requires_opponent != expected[menu_index].requires_opponent ||
            uses_settings != expected[menu_index].uses_settings ||
            strcmp(name, expected[menu_index].name) != 0) {
            fprintf(stderr,
                    "[Test] Mode route %u mismatch: mode=%d name=%s scene=%d opponent=%d settings=%d\n",
                    menu_index, (int)mode, name, (int)scene_id,
                    requires_opponent ? 1 : 0, uses_settings ? 1 : 0);
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
        allstar_game_mode_scene((AllStarGameMode)ALLSTAR_MODE_COUNT) != ALLSTAR_SCENE_ONE_ON_ONE ||
        !allstar_game_mode_uses_settings((AllStarGameMode)ALLSTAR_MODE_COUNT)) {
        fprintf(stderr, "[Test] Invalid mode routing did not use the safe One-on-One fallback\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* Exercise the visible menu path: Down twice selects mode $02, whose
       $22EF branch bypasses settings and enters the two-player selector. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    allstar_input_update(&game.input, ALLSTAR_BTN_DOWN);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_DOWN);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    if (game.selected_mode != ALLSTAR_MODE_HORSE || !game.active_scene ||
        game.active_scene->id != ALLSTAR_SCENE_ROSTER_SELECT) {
        fprintf(stderr,
                "[Test] Mode $02 menu path did not bypass $22EF settings\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: All 5 ROM menu IDs route correctly; Horse bypasses $22EF settings\n");
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
    AllStarOneOnOneScorePresentation score_presentation;
    AllStarRomFoulPresentation foul_presentation;
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
    int score_frame;
    int possession_frame;
    AllStarSfxId score_sfx;
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
    AllStarBall score_ball;
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

    if (allstar_renderer_rom_player_palette_21fa(true, 0x90) != 0xe4 ||
        allstar_renderer_rom_player_palette_21fa(true, 0x91) != 0xd9 ||
        allstar_renderer_rom_player_palette_21fa(false, 0x90) != 0xe0 ||
        allstar_renderer_rom_player_palette_21fa(false, 0x91) != 0xd0) {
        fprintf(stderr, "[Test] $21FA roster OBJ palette table was incorrect\n");
        return 1;
    }
    if (!allstar_one_on_one_rom_shot_horizontal_flip_7138(83.0f) ||
        allstar_one_on_one_rom_shot_horizontal_flip_7138(84.0f) ||
        allstar_one_on_one_rom_shot_horizontal_flip_7138(156.0f)) {
        fprintf(stderr, "[Test] $7138 hoop-facing shot boundary was incorrect\n");
        return 1;
    }

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
    {
        AllStarRomHeldBallPresentation held_ball;
        allstar_renderer_rom_held_ball_7f37(
            80, 130, 0x13, 0x0d, false, &held_ball);
        if (!held_ball.visible || held_ball.ball_x != 0x4f ||
            held_ball.ball_y != 0x80 || held_ball.ball_z != 0x26 ||
            held_ball.behind_owner) {
            fprintf(stderr, "[Test] $7F37 unflipped held-ball placement was incorrect\n");
            return 1;
        }
        allstar_renderer_rom_held_ball_7f37(
            84, 130, 0x13, 0x0d, true, &held_ball);
        if (!held_ball.visible || held_ball.ball_x != 0x56 ||
            held_ball.ball_y != 0x80 || held_ball.ball_z != 0x26 ||
            !held_ball.behind_owner) {
            fprintf(stderr, "[Test] $7F37 flipped/rear held-ball placement was incorrect\n");
            return 1;
        }
        allstar_renderer_rom_held_ball_7f37(
            80, 130, 0x13, 0x0c, false, &held_ball);
        if (held_ball.visible) {
            fprintf(stderr, "[Test] $7F37 frame-contained ball was drawn twice\n");
            return 1;
        }
        /* Live $20F7->$6F2A trace: raw +$06=$4C, +$05=$70,
           +$15=$98, producing ball $5A/$96/$0C. */
        allstar_renderer_rom_dribble_ball_6f2a(
            84, 152, 0x13, 1, true, &held_ball);
        if (!held_ball.visible || held_ball.ball_x != 0x5a ||
            held_ball.ball_y != 0x96 || held_ball.ball_z != 0x0c) {
            fprintf(stderr, "[Test] $6F2A held-ball placement was incorrect\n");
            return 1;
        }
        allstar_renderer_rom_dribble_ball_6f2a(
            84, 152, 0x13, 6, true, &held_ball);
        if (held_ball.ball_z != 0x04) {
            fprintf(stderr, "[Test] $6FEA bounce table was incorrect\n");
            return 1;
        }
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
    animation_state.timer = 1;
    if (allstar_one_on_one_rom_select_movement_action_782e(
            &animation_state, ALLSTAR_BTN_DOWN, 0, ALLSTAR_BTN_UP,
            false, false, &animation_flip)) {
        fprintf(stderr, "[Test] $78DD repeated action incorrectly retriggered command $0D\n");
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
    allstar_rom_rng_init(&rng, 0xe018);
    if (allstar_rom_rng_high(&rng) != 0xe0 ||
        allstar_rom_rng_alternate(&rng) != 0x4a ||
        allstar_rom_rng_alternate_high(&rng) != 0x97 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x18 ||
        rng.alternate_seed != 0x51c5 ||
        allstar_rom_rng_end_frame_0714(&rng, 0, 0) != 0x03 ||
        rng.seed != 0xe103) {
        fprintf(stderr, "[Test] $FFFB..$FFFE paired RNG state was incorrect\n");
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
            &contact_ai, true, false, 0xbd, 0x20, 0x40, 80.0f, 112.0f) ||
        !allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, false, 0xbe, 0x20, 0x40, 80.0f, 112.0f) ||
        contact_ai.rom_contact_hold_frames != 10 ||
        contact_ai.rom_contact_saved_x != 80 ||
        contact_ai.rom_contact_saved_y != 112 ||
        allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, false, 0xff, 0x20, 0x40, 80.0f, 112.0f)) {
        fprintf(stderr, "[Test] $75CD defender threshold/hold response was incorrect\n");
        return 1;
    }
    allstar_ai_init(&contact_ai, NULL);
    contact_ai.rom_contact_offense_count = 13;
    if (!allstar_ai_rom_contact_response_75cd(
            &contact_ai, true, true, 0xbe, 0x20, 0x40, 80.0f, 112.0f) ||
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

    {
        AllStarRomPlayerController controller;
        AllStarRomPlayerControllerContext controller_context;
        uint32_t controller_events;
        memset(&controller, 0, sizeof(controller));
        memset(&controller_context, 0, sizeof(controller_context));
        controller_context.possession_active = true;
        controller_context.ball_x = 0x40;
        controller_context.player_center_x = 0x60;
        controller.action = 0x00;
        controller.held_input = 0x10;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if (controller_events != ALLSTAR_ROM_PLAYER_EVENT_NONE ||
            controller.input_direction != 1 ||
            controller.stored_direction != 1) {
            fprintf(stderr, "[Test] $702D did not latch normal direction input\n");
            return 1;
        }
        controller.held_input = 0x12;
        allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if (controller.direction_override != 1) {
            fprintf(stderr, "[Test] $702D held-ball B direction override diverged\n");
            return 1;
        }
        controller.held_input = 0;
        controller.blocked_contact = true;
        controller_context.transition_locked = true;
        allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if (controller.input_direction != 0 || controller.blocked_contact ||
            controller.direction_override != 0) {
            fprintf(stderr, "[Test] $702D transition clear path diverged\n");
            return 1;
        }

        memset(&controller, 0, sizeof(controller));
        controller_context.transition_locked = false;
        controller.without_ball = 1;
        controller.new_input = 1;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events &
                (ALLSTAR_ROM_PLAYER_EVENT_ACTION_RESET |
                 ALLSTAR_ROM_PLAYER_EVENT_DEFENSE_JUMP)) !=
                (ALLSTAR_ROM_PLAYER_EVENT_ACTION_RESET |
                 ALLSTAR_ROM_PLAYER_EVENT_DEFENSE_JUMP) ||
            controller.action != 0x05) {
            fprintf(stderr, "[Test] $702D/$70FD defensive jump branch diverged\n");
            return 1;
        }

        memset(&controller, 0, sizeof(controller));
        controller.without_ball = 1;
        controller.new_input = 2;
        controller.held_input = 2;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events & ALLSTAR_ROM_PLAYER_EVENT_STEAL) == 0) {
            fprintf(stderr, "[Test] $702D did not emit new-B steal input\n");
            return 1;
        }
        controller.steal_lock = 1;
        if ((allstar_one_on_one_rom_player_controller_702d(
                &controller, &controller_context) &
                ALLSTAR_ROM_PLAYER_EVENT_STEAL) != 0) {
            fprintf(stderr, "[Test] $702D ignored player +$17 steal lock\n");
            return 1;
        }

        memset(&controller, 0, sizeof(controller));
        controller.shot_variant = 1;
        controller.new_input = 1;
        controller.record_index = 7;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events & ALLSTAR_ROM_PLAYER_EVENT_SHOT_GATHER) == 0 ||
            controller.action != ALLSTAR_ROM_SHOT_ACTION_A ||
            controller.record_index != 0 || controller.shot_phase != 0 ||
            (controller.flags & 0x10u) != 0) {
            fprintf(stderr, "[Test] $702D/$0AA3 shot-gather state diverged\n");
            return 1;
        }
        controller.new_input = 0;
        controller.held_input = 2;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events & ALLSTAR_ROM_PLAYER_EVENT_SHOT_RELEASE) != 0 ||
            controller.shot_phase != 1 || controller.release_latch != 1) {
            fprintf(stderr, "[Test] $702D did not arm phase-one $C16A\n");
            return 1;
        }
        controller.held_input = 0;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events & ALLSTAR_ROM_PLAYER_EVENT_SHOT_RELEASE) == 0 ||
            controller.shot_phase != 2 || controller.release_latch != 0) {
            fprintf(stderr, "[Test] $702D $C16A expiry did not launch phase two\n");
            return 1;
        }

        memset(&controller, 0, sizeof(controller));
        controller.action = 0x03;
        controller.shot_variant = 2;
        controller_context.player_center_x = 0x40;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events &
                (ALLSTAR_ROM_PLAYER_EVENT_ACTION_DIRECT |
                 ALLSTAR_ROM_PLAYER_EVENT_BALL_PRESENTATION)) !=
                (ALLSTAR_ROM_PLAYER_EVENT_ACTION_DIRECT |
                 ALLSTAR_ROM_PLAYER_EVENT_BALL_PRESENTATION) ||
            controller.action != ALLSTAR_ROM_SHOT_ACTION_B ||
            (controller.flags & 0x10u) == 0) {
            fprintf(stderr, "[Test] $702D/$714D direct shot assignment diverged\n");
            return 1;
        }
        controller.action = 0x0c;
        controller_events = allstar_one_on_one_rom_player_controller_702d(
            &controller, &controller_context);
        if ((controller_events & ALLSTAR_ROM_PLAYER_EVENT_JUMP_RECOVERY) == 0 ||
            (controller_events & ALLSTAR_ROM_PLAYER_EVENT_ACTION_DIRECT) != 0) {
            fprintf(stderr, "[Test] $702D protected jump-recovery dispatch diverged\n");
            return 1;
        }
    }

    {
        AllStarAIController controller_ai;
        AllStarRomCpuControllerContext cpu_context;
        memset(&cpu_context, 0, sizeof(cpu_context));
        cpu_context.cpu_enabled = true;
        cpu_context.game_mode = 0;
        cpu_context.cpu_player = 2;
        cpu_context.possession_owner = 2;
        cpu_context.skill_level = 3;
        cpu_context.random_current = 0xff;
        cpu_context.random_target = 0x30;
        cpu_context.random_route = 0;
        cpu_context.random_position = 0x98;
        cpu_context.ball_x = 0x53;
        cpu_context.cpu_center_x = 0x54;
        cpu_context.cpu_ground_y = 0x98;
        cpu_context.cpu_roster_index = 0x02;
        cpu_context.cpu_shot_profile = 0;
        cpu_context.cpu_action = 0x13;
        allstar_ai_init(&controller_ai, NULL);
        allstar_ai_set_skill(&controller_ai, 3);
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_offense_active != 1 ||
            controller_ai.rom_initial_target_active != 1 ||
            controller_ai.rom_target_x != 0x1c ||
            controller_ai.rom_target_y != 0x8c ||
            controller_ai.rom_held_input == 0) {
            fprintf(stderr, "[Test] $7170/$72EA offense target state diverged\n");
            return 1;
        }
        cpu_context.cpu_center_x = controller_ai.rom_target_x;
        cpu_context.cpu_ground_y = controller_ai.rom_target_y;
        cpu_context.random_route = 0x20;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_initial_target_active != 0 ||
            controller_ai.rom_target_x != 0x14 ||
            controller_ai.rom_target_y != 0x88) {
            fprintf(stderr,
                "[Test] $7170/$732C route transition diverged "
                "(initial=%02X target=%02X,%02X stage=%02X input=%02X)\n",
                controller_ai.rom_initial_target_active,
                controller_ai.rom_target_x, controller_ai.rom_target_y,
                controller_ai.rom_offense_stage,
                controller_ai.rom_held_input);
            return 1;
        }
        cpu_context.cpu_center_x = controller_ai.rom_target_x;
        cpu_context.cpu_ground_y = controller_ai.rom_target_y;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_offense_stage != 2 ||
            controller_ai.rom_new_input != 1 ||
            controller_ai.rom_stored_shot_random != 0xff) {
            fprintf(stderr, "[Test] $7170/$74BB/$755D gather transition diverged\n");
            return 1;
        }
        cpu_context.cpu_action = ALLSTAR_ROM_SHOT_ACTION_A;
        cpu_context.cpu_record = 5;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_new_input != 1 ||
            !controller_ai.rom_shot_release) {
            fprintf(stderr, "[Test] $7170/$756C record-gated release diverged\n");
            return 1;
        }

        allstar_ai_init(&controller_ai, NULL);
        allstar_ai_set_skill(&controller_ai, 3);
        memset(&cpu_context, 0, sizeof(cpu_context));
        cpu_context.cpu_enabled = true;
        cpu_context.game_mode = 0;
        cpu_context.cpu_player = 2;
        cpu_context.possession_owner = 1;
        cpu_context.skill_level = 3;
        cpu_context.ball_contact = true;
        cpu_context.random_current = 0x45;
        cpu_context.ball_x = 0x70;
        cpu_context.ball_y = 0x78;
        cpu_context.cpu_center_x = 0x40;
        cpu_context.cpu_ground_y = 0x70;
        cpu_context.opponent_center_x = 0x70;
        cpu_context.opponent_ground_y = 0x78;
        cpu_context.opponent_stored_direction = 1;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if ((controller_ai.rom_new_input & 0x02u) == 0 ||
            !controller_ai.rom_steal_pressed) {
            fprintf(stderr, "[Test] $7170/$71B3 steal/chase input diverged\n");
            return 1;
        }
        cpu_context.ball_contact = false;
        cpu_context.initial_flight = true;
        cpu_context.shot_owner = 1;
        cpu_context.cpu_center_x = 0x54;
        cpu_context.cpu_ground_y = 0x60;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if ((controller_ai.rom_new_input & 0x01u) == 0) {
            fprintf(stderr, "[Test] $7170/$71EE contest input diverged\n");
            return 1;
        }
        cpu_context.initial_flight = false;
        cpu_context.movement_blocked = true;
        cpu_context.random_current = 0xff;
        cpu_context.cpu_center_x = 0x50;
        cpu_context.cpu_ground_y = 0x70;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_contact_hold_frames != 10 ||
            controller_ai.rom_contact_saved_x != 0x50 ||
            controller_ai.rom_contact_saved_y != 0x70) {
            fprintf(stderr, "[Test] $7170/$75CD contact hold diverged\n");
            return 1;
        }
        controller_ai.rom_new_input = 0xff;
        cpu_context.counted_wait_locked = true;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_new_input != 0 ||
            controller_ai.rom_held_input != 0) {
            fprintf(stderr, "[Test] $7170 entry gates did not clear CPU input\n");
            return 1;
        }

        allstar_ai_init(&controller_ai, NULL);
        allstar_ai_set_skill(&controller_ai, 3);
        memset(&cpu_context, 0, sizeof(cpu_context));
        cpu_context.cpu_enabled = true;
        cpu_context.game_mode = 2;
        cpu_context.cpu_player = 2;
        cpu_context.skill_level = 3;
        cpu_context.cpu_action = 0;
        cpu_context.cpu_center_x = 0x40;
        cpu_context.cpu_ground_y = 0x70;
        cpu_context.mode2_target_x = 0x50;
        cpu_context.mode2_target_y = 0x78;
        cpu_context.mode2_state = 2;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_held_input == 0) {
            fprintf(stderr, "[Test] $7170/$74A8 mode-2 target path diverged\n");
            return 1;
        }
        cpu_context.cpu_center_x = 0x50;
        cpu_context.cpu_ground_y = 0x78;
        controller_ai.rom_direction_hysteresis = 1;
        controller_ai.rom_direction_reload = 1;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_mode2_arrival != 2 ||
            controller_ai.rom_new_input != 1) {
            fprintf(stderr, "[Test] $7170/$74A8/$755D mode-2 gather diverged\n");
            return 1;
        }
        cpu_context.cpu_action = ALLSTAR_ROM_SHOT_ACTION_A;
        cpu_context.cpu_record = 5;
        cpu_context.cpu_shot_profile = 0;
        controller_ai.rom_stored_shot_random = 0xff;
        cpu_context.random_current = 0;
        allstar_ai_rom_controller_7170(&controller_ai, &cpu_context);
        if (controller_ai.rom_new_input != 1) {
            fprintf(stderr, "[Test] $7170/$756C mode-2 skill bypass diverged\n");
            return 1;
        }
    }
    if (allstar_one_on_one_rom_take_back_cleared_78e9(84.0f, 112.0f) ||
        !allstar_one_on_one_rom_take_back_cleared_78e9(58.0f, 112.0f) ||
        allstar_one_on_one_rom_take_back_cleared_78e9(59.0f, 128.0f) ||
        !allstar_one_on_one_rom_take_back_cleared_78e9(59.0f, 129.0f) ||
        allstar_one_on_one_rom_take_back_cleared_78e9(0.0f, 144.0f)) {
        fprintf(stderr, "[Test] $78E9/$796C take-back region was incorrect\n");
        return 1;
    }
    allstar_ai_rom_route_target_732c(
        0x02, 0x00, 0x98, &target_x, &target_y);
    if (target_x != 0x14 || target_y != 0x88) {
        fprintf(stderr,
                "[Test] $732C family-one route table was incorrect (%02X,%02X)\n",
                target_x, target_y);
        return 1;
    }
    allstar_ai_rom_route_target_732c(
        0x07, 0x00, 0x00, &target_x, &target_y);
    if (target_x != 0x54 || target_y != 0x5d) {
        fprintf(stderr, "[Test] $732C fixed-center route was incorrect\n");
        return 1;
    }
    allstar_ai_rom_route_target_732c(
        0x02, 0xa2, 0x13, &target_x, &target_y);
    if (target_x != 0x60 || target_y != 0x8c) {
        fprintf(stderr, "[Test] $732C family/bin thresholds were incorrect\n");
        return 1;
    }

    allstar_one_on_one_shot_reset(&attempt);
    allstar_one_on_one_shot_press(&attempt, 1);
    allstar_one_on_one_shot_tick(&attempt, 62.0f / 60.0f);
    if (attempt.rom_elapsed_frames != 62 ||
        allstar_one_on_one_shot_press(&attempt, 1) !=
            ALLSTAR_ONE_ON_ONE_SHOT_EVENT_NONE ||
        attempt.phase != ALLSTAR_ONE_ON_ONE_SHOT_GATHER) {
        fprintf(stderr,
                "[Test] $6A8C pointer-$0C terminal frame incorrectly launched\n");
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
    if (court_x != 12.0f || court_y != 96.0f) {
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
    court_x = 16.0f;
    court_y = 100.0f;
    if (!allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_LEFT | ALLSTAR_BTN_UP, 0,
            &court_x, &court_y, 140.0f, 140.0f, NULL) ||
        court_x != 12.0f || court_y != 96.0f ||
        allstar_one_on_one_rom_player_move_6b72(
            ALLSTAR_BTN_LEFT | ALLSTAR_BTN_UP, 0,
            &court_x, &court_y, 140.0f, 140.0f, NULL)) {
        fprintf(stderr,
                "[Test] $6BBA/$6BD4 did not preserve final edge step 16,100->12,96\n");
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

    allstar_one_on_one_foul_presentation_begin_05a3(
        &foul_presentation, ALLSTAR_ROM_CONTACT_CHARGING, 1);
    if (!foul_presentation.active || !foul_presentation.message_visible ||
        !foul_presentation.sprites_visible ||
        foul_presentation.bg_palette != 0xe4) {
        fprintf(stderr, "[Test] $05A3 charging popup did not begin correctly\n");
        return 1;
    }
    allstar_one_on_one_foul_presentation_begin_05a3(
        &foul_presentation, ALLSTAR_ROM_CONTACT_DIDNT_CLEAR, 1);
    if (!foul_presentation.active ||
        foul_presentation.violation != ALLSTAR_ROM_CONTACT_DIDNT_CLEAR) {
        fprintf(stderr, "[Test] $2C50/$067C take-back popup was rejected\n");
        return 1;
    }
    for (frame = 1; frame <= ALLSTAR_ROM_FOUL_COMPLETE_FRAME; frame++) {
        events = allstar_one_on_one_foul_presentation_tick_0c49(
            &foul_presentation, ALLSTAR_PHYSICS_STEP_SECONDS);
        if ((frame == ALLSTAR_ROM_FOUL_SPRITES_HIDE_FRAME &&
             (foul_presentation.sprites_visible ||
              foul_presentation.bg_palette != 0xf9)) ||
            (frame == ALLSTAR_ROM_FOUL_RESET_FRAME &&
             (!(events & ALLSTAR_ROM_FOUL_EVENT_RESET_POSSESSION) ||
              foul_presentation.message_visible ||
              foul_presentation.bg_palette != 0xff)) ||
            (frame == ALLSTAR_ROM_FOUL_SPRITES_RESTORE_FRAME &&
             (!foul_presentation.sprites_visible ||
              foul_presentation.bg_palette != 0xe4)) ||
            (frame == ALLSTAR_ROM_FOUL_COMPLETE_FRAME &&
             (!(events & ALLSTAR_ROM_FOUL_EVENT_COMPLETE) ||
              foul_presentation.active))) {
            fprintf(stderr,
                    "[Test] $0C49 foul presentation diverged at frame %d\n",
                    frame);
            return 1;
        }
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
    for (frame = 0; frame < 360 && game.one_on_one.p1_possession; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (game.one_on_one.p1_score != 0 || game.one_on_one.p1_possession) {
        AllStarOneOnOneDebugState shot_debug;
        allstar_scene_one_on_one_get_debug_state(
            game.active_scene, &shot_debug);
        fprintf(stderr,
                "[Test] Traced class-one shot did not resolve through ROM contacts "
                "(score=%u owner=%d ball=%d/%d xyz=%.0f,%.0f,%.0f "
                "actions=%02X/%02X cpu=%u/%u)\n",
                (unsigned)game.one_on_one.p1_score,
                game.one_on_one.p1_possession ? 1 : 2,
                shot_debug.ball_in_flight ? 1 : 0,
                shot_debug.ball_recoverable ? 1 : 0,
                shot_debug.ball_x, shot_debug.ball_y, shot_debug.ball_z,
                shot_debug.p1_action, shot_debug.p2_action,
                shot_debug.cpu_state, shot_debug.cpu_offense_stage);
        allstar_game_shutdown(&game);
        return 1;
    }
    if (allstar_one_on_one_rom_shot_jump_height_6c4d(0) != 0.0f ||
        allstar_one_on_one_rom_shot_jump_height_6c4d(7) != 9.0f ||
        allstar_one_on_one_rom_shot_jump_height_6c4d(37) != 26.0f ||
        allstar_one_on_one_rom_shot_jump_height_6c4d(38) != 24.0f ||
        allstar_one_on_one_rom_shot_jump_height_6c4d(62) != 0.0f) {
        fprintf(stderr, "[Test] $6A8C/$6C4D shot visual lift was incorrect\n");
        return 1;
    }

    allstar_one_on_one_score_presentation_begin_1e0e(
        &score_presentation, 1, 2);
    for (frame = 1; frame <= ALLSTAR_ROM_SCORE_INBOUND_FRAME; frame++) {
        uint32_t score_flags =
            allstar_one_on_one_score_presentation_tick_0c13(
                &score_presentation, ALLSTAR_PHYSICS_STEP_SECONDS);
        if ((frame == ALLSTAR_ROM_SCORE_COMMIT_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_COMMIT) != 0) ||
            (frame == ALLSTAR_ROM_SCORE_FADE_OUT_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_FADE_OUT) != 0) ||
            (frame == ALLSTAR_ROM_SCORE_POSSESSION_RESET_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_RESET_POSSESSION) != 0) ||
            (frame == ALLSTAR_ROM_SCORE_FADE_IN_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_FADE_IN) != 0) ||
            (frame == ALLSTAR_ROM_SCORE_INBOUND_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_INBOUND) != 0) ||
            (frame == ALLSTAR_ROM_SCORE_NET_FIRST_FRAME) !=
            ((score_flags & ALLSTAR_ROM_SCORE_EVENT_NET_SOUND) != 0)) {
            fprintf(stderr,
                    "[Test] $1E0E/$0C13 score event occurred on the wrong frame %d\n",
                    frame);
            return 1;
        }
        if ((frame == 191 && score_presentation.bg_palette != 0xe4) ||
            (frame == 192 && score_presentation.bg_palette != 0xf9) ||
            (frame == 203 && score_presentation.bg_palette != 0xfe) ||
            (frame == 214 && score_presentation.bg_palette != 0xff) ||
            (frame == 231 && score_presentation.bg_palette != 0xfe) ||
            (frame == 242 && score_presentation.bg_palette != 0xf9) ||
            (frame == 253 && score_presentation.bg_palette != 0xe4)) {
            fprintf(stderr, "[Test] $27C7/$27CC BGP stage was incorrect\n");
            return 1;
        }
    }
    if (allstar_one_on_one_score_net_frame_1ecc(19) !=
            ALLSTAR_ROM_NET_UNCHANGED ||
        allstar_one_on_one_score_net_frame_1ecc(20) !=
            ALLSTAR_ROM_NET_BEND ||
        allstar_one_on_one_score_net_frame_1ecc(35) !=
            ALLSTAR_ROM_NET_DEEP ||
        allstar_one_on_one_score_net_frame_1ecc(50) !=
            ALLSTAR_ROM_NET_BEND ||
        allstar_one_on_one_score_net_frame_1ecc(65) !=
            ALLSTAR_ROM_NET_REST) {
        fprintf(stderr, "[Test] $1ECC net frame sequence was incorrect\n");
        return 1;
    }
    if (score_presentation.active ||
        score_presentation.elapsed_frames != ALLSTAR_ROM_SCORE_INBOUND_FRAME ||
        score_presentation.shooter != 1 || score_presentation.points != 2) {
        fprintf(stderr, "[Test] Score presentation did not resume at frame 258\n");
        return 1;
    }
    allstar_physics_init_ball(&score_ball);
    score_ball.in_flight = true;
    score_ball.made_basket = true;
    score_ball.rom_step_state_valid = true;
    score_ball.rom_step_state.x = 0x5400;
    score_ball.rom_step_state.y = 0x5e00;
    score_ball.rom_step_state.z = 0x3820;
    score_ball.rom_step_state.vz = -0x0018;
    score_ball.rom_step_state.gravity_delay_frames = 35;
    score_ball.rom_hard_bounce_pending = true;
    for (frame = 0; frame < 76; frame++)
        allstar_physics_update_ball(
            &score_ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (score_ball.rom_step_state.z != 0 ||
        score_ball.rom_step_state.vz != 0x0153) {
        fprintf(stderr, "[Test] $1E0E delayed-gravity hard bounce was incorrect\n");
        return 1;
    }
    for (frame = 76; frame < 121; frame++)
        allstar_physics_update_ball(
            &score_ball, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (score_ball.rom_step_state.z != 0 ||
        score_ball.rom_step_state.vz != 0x0117) {
        fprintf(stderr, "[Test] $7BE8/$1E77 score-ball second bounce was incorrect\n");
        return 1;
    }

    allstar_one_on_one_score_presentation_begin_1e0e(
        &score_presentation, 1, 2);
    for (frame = 1;
         frame <= (int)ceilf(ALLSTAR_ROM_SCORE_INBOUND_FRAME /
                             ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE);
         frame++) {
        allstar_one_on_one_score_presentation_tick_0c13(
            &score_presentation,
            ALLSTAR_PHYSICS_STEP_SECONDS *
                ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE);
    }
    if (score_presentation.active || frame - 1 !=
            (int)ceilf(ALLSTAR_ROM_SCORE_INBOUND_FRAME /
                       ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE)) {
        fprintf(stderr,
                "[Test] Native accelerated score presentation duration was incorrect\n");
        return 1;
    }

    /* End-to-end native scene proof: for roster profile zero at the reset
       class-one location, pointer 5 couples Z=$3E with table VZ=$01D4 and
       enters $1CED's live score branch. The cartridge trace uses pointer 7
       at its class-three location, demonstrating that the make timing is a
       profile/distance/record state, not one universal release frame. */
    srand(1);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    /* Keep this shot-result proof at its original class-one geometry; the
       separate $20F7 test above owns the newly exact take-out placement. */
    if (!allstar_scene_one_on_one_set_test_positions(
            game.active_scene, 80.0f, 130.0f, 80.0f, 105.0f)) {
        fprintf(stderr, "[Test] Could not place class-one shot fixture\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_rom_rng_init(&game.one_on_one_rng, 0x0018);
    srand(1);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 25; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    score_frame = -1;
    possession_frame = -1;
    score_sfx = ALLSTAR_SFX_NONE;
    for (frame = 1; frame <= 500; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (score_frame < 0 && game.one_on_one.p1_score != 0) {
            score_frame = frame;
            score_sfx = game.audio.last_sfx;
        }
        if (score_frame >= 0 && !game.one_on_one.p1_possession) {
            possession_frame = frame;
            break;
        }
    }
    if (game.one_on_one.p1_score != 2 || game.one_on_one.p1_possession ||
        allstar_one_on_one_rom_shot_record_index(25) != 5 ||
        score_frame < 0 || possession_frame - score_frame !=
            (int)ceilf(
                ALLSTAR_ROM_SCORE_POSSESSION_RESET_FRAME /
                    ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE) -
            (int)ceilf(
                ALLSTAR_ROM_SCORE_COMMIT_FRAME /
                    ALLSTAR_NATIVE_SCORE_PRESENTATION_RATE) ||
        score_sfx != ALLSTAR_SFX_SCORE_CHIME) {
        fprintf(stderr,
                "[Test] Timed native make did not follow $1E0E->$1F23->$20F7 "
                "(score=%u possession=%d score_frame=%d reset_frame=%d sfx=%d)\n",
                (unsigned)game.one_on_one.p1_score,
                game.one_on_one.p1_possession ? 1 : 0,
                score_frame, possession_frame, (int)score_sfx);
        allstar_game_shutdown(&game);
        return 1;
    }
    printf("  Timed scene make: release frame 25 (ROM record $%02X), "
           "score cue at %d, reset at %d\n",
           allstar_one_on_one_rom_shot_record_index(25),
           score_frame, possession_frame);

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

/*
 * ROM mode-select menu and the sound-driver voice switch, from the
 * $038F..$0416 and $32B8..$3319 disassembly.
 */
int allstar_cli_test_menu_voice_rom(void) {
    AllStarMenuInputSeed seed;
    const AllStarVoiceField *fields;
    uint8_t working[ALLSTAR_VOICE_FIELDS];
    uint8_t slots[ALLSTAR_VOICE_FIELDS];
    uint8_t mode;
    uint8_t link;
    int count;
    int i;

    printf("[Test] Running ROM Menu and Voice Tests ($038F/$32B8)...\n");

    /* $0397: only a two-player game starts the menu music. */
    if (allstar_menu_plays_music(1u) || !allstar_menu_plays_music(2u)) {
        fprintf(stderr, "[Test] $0399 music gate diverged\n");
        return 1;
    }

    /* $03AE: new input cleared, held input primed to $FF on both pads. */
    allstar_menu_seed_input(&seed);
    if (seed.new_player_1 != 0x00u || seed.new_player_2 != 0x00u ||
        seed.held_player_1 != 0xFFu || seed.held_player_2 != 0xFFu) {
        fprintf(stderr, "[Test] $03B6 input seed diverged\n");
        return 1;
    }

    /* $03FD: A or Down steps forward and wraps past the last entry. */
    mode = 0u; link = 0xFFu;
    if (allstar_menu_step(0u, 0x01u, 1u, &mode, &link) != ALLSTAR_MENU_MOVED || mode != 1u) {
        fprintf(stderr, "[Test] A did not step forward\n");
        return 1;
    }
    if (allstar_menu_step(0u, 0x80u, 1u, &mode, &link) != ALLSTAR_MENU_MOVED || mode != 2u) {
        fprintf(stderr, "[Test] Down did not step forward\n");
        return 1;
    }
    mode = 4u;
    if (allstar_menu_step(0u, 0x01u, 1u, &mode, &link) != ALLSTAR_MENU_MOVED || mode != 0u) {
        fprintf(stderr, "[Test] $0404 did not wrap past the last entry\n");
        return 1;
    }
    /* $0407: B or Up steps back and wraps below zero. */
    if (allstar_menu_step(0u, 0x02u, 1u, &mode, &link) != ALLSTAR_MENU_MOVED || mode != 4u) {
        fprintf(stderr, "[Test] $040E did not wrap below zero, mode %u\n", mode);
        return 1;
    }
    if (allstar_menu_step(0u, 0x40u, 1u, &mode, &link) != ALLSTAR_MENU_MOVED || mode != 3u) {
        fprintf(stderr, "[Test] Up did not step back\n");
        return 1;
    }
    /* A button outside the $C3 mask does nothing. */
    if (allstar_menu_step(0u, 0x10u, 1u, &mode, &link) != ALLSTAR_MENU_IDLE || mode != 3u) {
        fprintf(stderr, "[Test] $03F5 accepted a button outside the mask\n");
        return 1;
    }

    /* $03D2: Start confirms. */
    mode = 0u;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 1u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED) {
        fprintf(stderr, "[Test] $03D6 Start did not confirm\n");
        return 1;
    }
    /* $03CD: $FFEC blocks the confirm, and the cursor does not move either. */
    if (allstar_menu_step(1u, ALLSTAR_MENU_CONFIRM_MASK, 1u, &mode, &link)
            != ALLSTAR_MENU_IDLE || mode != 0u) {
        fprintf(stderr, "[Test] $03D0 lock did not block the confirm\n");
        return 1;
    }
    /* $03C7: a two-player game cannot select the tournament. */
    mode = ALLSTAR_MENU_TOURNAMENT;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 2u, &mode, &link)
            != ALLSTAR_MENU_IDLE) {
        fprintf(stderr, "[Test] $03CB let two players pick the tournament\n");
        return 1;
    }
    /* One player can. */
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 1u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED) {
        fprintf(stderr, "[Test] $03C5 blocked a one-player tournament\n");
        return 1;
    }

    /* $03DD: modes $01 and $03 record the link flag, only with two players. */
    link = 0xFFu; mode = 0x01u;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 2u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED || link != 0x01u) {
        fprintf(stderr, "[Test] $03E7 Free Throw link flag diverged, got $%02X\n", link);
        return 1;
    }
    link = 0xFFu; mode = 0x03u;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 2u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED || link != 0x03u) {
        fprintf(stderr, "[Test] $03E7 Accuracy link flag diverged\n");
        return 1;
    }
    link = 0xFFu; mode = 0x00u;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 2u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED || link != 0xFFu) {
        fprintf(stderr, "[Test] $03E5 set the link flag for a non-link mode\n");
        return 1;
    }
    link = 0xFFu; mode = 0x01u;
    if (allstar_menu_step(0u, ALLSTAR_MENU_CONFIRM_MASK, 1u, &mode, &link)
            != ALLSTAR_MENU_CONFIRMED || link != 0xFFu) {
        fprintf(stderr, "[Test] $03DB set the link flag in a one-player game\n");
        return 1;
    }

    /* $32B8: six fields, with the third and fourth swapped. */
    fields = allstar_voice_fields(&count);
    if (count != ALLSTAR_VOICE_FIELDS) {
        fprintf(stderr, "[Test] $32B8 copies %d fields, expected %d\n", count, ALLSTAR_VOICE_FIELDS);
        return 1;
    }
    if (fields[0].scratch != 0xDE2Au || fields[0].table != 0xDDBFu ||
        fields[1].scratch != 0xDE2Bu || fields[1].table != 0xDDC7u ||
        fields[2].scratch != 0xDE2Du || fields[2].table != 0xDDCFu ||
        fields[3].scratch != 0xDE2Cu || fields[3].table != 0xDDD7u ||
        fields[4].scratch != 0xDE28u || fields[4].table != 0xDDDFu ||
        fields[5].scratch != 0xDE29u || fields[5].table != 0xDDE7u) {
        fprintf(stderr, "[Test] $32B8 field mapping diverged\n");
        return 1;
    }
    /* The swap is the point: $DE2D goes to the lower array, $DE2C to the higher. */
    if (!(fields[2].scratch > fields[3].scratch && fields[2].table < fields[3].table)) {
        fprintf(stderr, "[Test] $32C8/$32D0 lost the swapped pair\n");
        return 1;
    }
    /* The arrays are eight bytes apart, indexed by channel. */
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) {
        if (allstar_voice_slot(i, 0u) != fields[i].table ||
            allstar_voice_slot(i, 3u) != (uint16_t)(fields[i].table + 3u)) {
            fprintf(stderr, "[Test] $32BE channel indexing diverged on field %d\n", i);
            return 1;
        }
    }
    if (fields[1].table - fields[0].table != ALLSTAR_VOICE_TABLE_STRIDE) {
        fprintf(stderr, "[Test] $32B8 array stride diverged\n");
        return 1;
    }

    /* $32B8 then $32E9 must round-trip. */
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) working[i] = (uint8_t)(0x10u + i);
    allstar_voice_save(working, slots);
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) working[i] = 0u;
    allstar_voice_load(slots, working);
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) {
        if (working[i] != (uint8_t)(0x10u + i)) {
            fprintf(stderr, "[Test] $32E9 did not mirror $32B8 at field %d\n", i);
            return 1;
        }
    }

    printf("  two players cannot pick the tournament, and Free Throw or Accuracy sets $C18B\n");
    printf("  the voice switch swaps its third and fourth fields, $DE2D to $DDCF\n");
    printf("[Test] PASSED: $038F, $32B8, $32E9\n");
    return 0;
}

/*
 * ROM settings summary panel, from the $2578 dispatcher, its handlers at
 * $2585/$25ED/$2607/$2608, the writer at $2517 and the digit path at
 * $24E4/$2500/$250D.
 */
int allstar_cli_test_status_panel_rom(void) {
    static const uint16_t TABLE[ALLSTAR_STATUS_SLOTS] = {
        0x2585u, 0x25EDu, 0x2607u, 0x2608u, 0x2585u
    };
    /* $253D as the ROM stores it: three bit-7 terminated entries. */
    static const uint8_t LIST[8] = { 0x0Eu, 0x0Fu, 0x80u, 0x0Bu, 0x0Cu, 0x8Du, 0x00u, 0x86u };
    AllStarStatusOp ops[6];
    const uint16_t *table;
    uint16_t offset;
    uint8_t length;
    uint8_t tiles[ALLSTAR_STATUS_DIGIT_TILES];
    int count;
    int i;

    printf("[Test] Running ROM Status Panel Tests ($2578/$2517/$24E4)...\n");

    table = allstar_status_table(&count);
    if (count != ALLSTAR_STATUS_SLOTS) {
        fprintf(stderr, "[Test] $257B has %d slots, expected %d\n", count, ALLSTAR_STATUS_SLOTS);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != TABLE[i]) {
            fprintf(stderr, "[Test] $257B slot %d is $%04X, expected $%04X\n", i, table[i], TABLE[i]);
            return 1;
        }
    }
    /* The tournament shares One-on-One's handler; H-O-R-S-E has a bare ret. */
    if (table[4] != table[0] || table[2] != 0x2607u) {
        fprintf(stderr, "[Test] $257B mode aliasing diverged\n");
        return 1;
    }

    /* $2607: mode $02 draws nothing at all. */
    if (allstar_status_layout(0x02u, false, ops, 6) != 0) {
        fprintf(stderr, "[Test] $2607 drew something\n");
        return 1;
    }

    /* $2585: four fields, the first switching on whether $FF92 reads zero. */
    count = allstar_status_layout(0x00u, false, ops, 6);
    if (count != 4 ||
        ops[0].kind != ALLSTAR_STATUS_FIELD_DIGITS || ops[0].d != 0x0Fu || ops[0].e != 0x09u ||
        ops[1].source != 0x253Au || ops[1].index != ALLSTAR_STATUS_INDEX_MINUS_ONE ||
        ops[1].io != 0xFF97u || ops[1].e != 0x0Au ||
        ops[2].source != 0x253Du || ops[2].index != ALLSTAR_STATUS_INDEX_DIRECT ||
        ops[2].io != 0xFF96u || ops[2].e != 0x0Bu ||
        ops[3].source != 0x2551u || ops[3].index != ALLSTAR_STATUS_INDEX_BUCKET_3 ||
        ops[3].d != 0x0Fu || ops[3].e != 0x0Cu) {
        fprintf(stderr, "[Test] $2585 layout diverged\n");
        return 1;
    }
    count = allstar_status_layout(0x00u, true, ops, 6);
    if (ops[0].kind != ALLSTAR_STATUS_FIELD_FILLER || ops[0].source != ALLSTAR_STATUS_FILLER) {
        fprintf(stderr, "[Test] $258E filler path diverged\n");
        return 1;
    }
    /* Mode $04 goes through the same handler. */
    count = allstar_status_layout(0x04u, false, ops, 6);
    if (count != 4 || ops[3].d != 0x0Fu || ops[3].e != 0x0Cu) {
        fprintf(stderr, "[Test] $2585 tournament path diverged\n");
        return 1;
    }

    /* $25ED: Free Throw draws one bucketed field. */
    count = allstar_status_layout(0x01u, false, ops, 6);
    if (count != 1 || ops[0].source != 0x2543u ||
        ops[0].index != ALLSTAR_STATUS_INDEX_BUCKET_2 ||
        ops[0].io != 0xFF98u || ops[0].d != 0x09u || ops[0].e != 0x04u) {
        fprintf(stderr, "[Test] $25ED layout diverged\n");
        return 1;
    }

    /* $2608: two of its own, then the shared field at a different position. */
    count = allstar_status_layout(0x03u, false, ops, 6);
    if (count != 3 ||
        ops[0].source != 0x253Du || ops[0].io != 0xFF9Bu || ops[0].e != 0x03u ||
        ops[1].source != 0x253Du || ops[1].io != 0xFF9Au || ops[1].e != 0x04u ||
        ops[2].source != 0x2551u || ops[2].d != 0x0Du || ops[2].e != 0x05u) {
        fprintf(stderr, "[Test] $2608 layout diverged\n");
        return 1;
    }

    /* $2517: walk to entry N and measure it. */
    if (!allstar_status_entry(LIST, 8u, 0u, &offset, &length) || offset != 0u || length != 3u) {
        fprintf(stderr, "[Test] $2528 entry 0 gave offset %u length %u\n", offset, length);
        return 1;
    }
    if (!allstar_status_entry(LIST, 8u, 1u, &offset, &length) || offset != 3u || length != 3u) {
        fprintf(stderr, "[Test] $251D entry 1 gave offset %u length %u\n", offset, length);
        return 1;
    }
    if (!allstar_status_entry(LIST, 8u, 2u, &offset, &length) || offset != 6u || length != 2u) {
        fprintf(stderr, "[Test] $251D entry 2 gave offset %u length %u\n", offset, length);
        return 1;
    }
    if (allstar_status_entry(LIST, 8u, 3u, &offset, &length)) {
        fprintf(stderr, "[Test] $2517 walked past the end of the list\n");
        return 1;
    }

    /* $25C5 and $25EF thresholds. */
    if (allstar_status_bucket_3(0x02u) != 0u || allstar_status_bucket_3(0x05u) != 1u ||
        allstar_status_bucket_3(0x08u) != 2u || allstar_status_bucket_3(0x00u) != 3u ||
        allstar_status_bucket_3(0x03u) != 3u) {
        fprintf(stderr, "[Test] $25C5 bucket diverged\n");
        return 1;
    }
    if (allstar_status_bucket_2(0x05u) != 0u || allstar_status_bucket_2(0x10u) != 1u ||
        allstar_status_bucket_2(0x0Au) != 2u) {
        fprintf(stderr, "[Test] $25EF bucket diverged\n");
        return 1;
    }

    /* $250D maps digit N to tile N + 1, with no leading-zero handling. */
    allstar_status_digits(0x1234u, tiles);
    if (tiles[0] != 0x02u || tiles[1] != 0x03u || tiles[2] != 0x04u || tiles[3] != 0x05u) {
        fprintf(stderr, "[Test] $24E4 on 1234 gave $%02X $%02X $%02X $%02X\n",
                tiles[0], tiles[1], tiles[2], tiles[3]);
        return 1;
    }
    allstar_status_digits(0x0000u, tiles);
    if (tiles[0] != 0x01u || tiles[3] != 0x01u) {
        fprintf(stderr, "[Test] $250D zero digit diverged\n");
        return 1;
    }
    allstar_status_digits(0x0099u, tiles);
    if (tiles[2] != 0x0Au || tiles[3] != 0x0Au) {
        fprintf(stderr, "[Test] $250D nine digit diverged\n");
        return 1;
    }

    printf("  mode $02 draws nothing, mode $04 reuses One-on-One, mode $03 joins the shared tail\n");
    printf("  $250D is a plain digit-plus-one table, unlike the $C1 base the score fields use\n");
    printf("[Test] PASSED: $2517, $24E4, $2500, $2578, $2585, $25ED, $2607, $2608\n");
    return 0;
}

/*
 * ROM court clocks, from the bank 1 $79EE..$7A70 and $7A71..$7A8F
 * disassembly.
 */
int allstar_cli_test_game_clock_rom(void) {
    AllStarClockOwnership own;
    uint8_t counter;
    int i;

    printf("[Test] Running ROM Court Clock Tests ($79EE/$7A71)...\n");

    /* $79EE: either flag stops the clocks. */
    if (allstar_clock_suppressed(0u, 0u) ||
        !allstar_clock_suppressed(1u, 0u) ||
        !allstar_clock_suppressed(0u, 1u)) {
        fprintf(stderr, "[Test] $79F1 suppression diverged\n");
        return 1;
    }

    /* $79FC: Accuracy resets both clocks and runs neither. */
    allstar_clock_ownership(0x03u, 1u, ALLSTAR_CLOCK_ENABLE_GAME, &own);
    if (!own.resets_player_1 || !own.resets_player_2 ||
        own.enable != ALLSTAR_CLOCK_ENABLE_GAME) {
        fprintf(stderr, "[Test] $7A05 accuracy path gave enable $%02X\n", own.enable);
        return 1;
    }
    /* No possession does the same in any mode. */
    allstar_clock_ownership(0x00u, 0u, ALLSTAR_CLOCK_ENABLE_GAME, &own);
    if (!own.resets_player_1 || !own.resets_player_2 ||
        own.enable != ALLSTAR_CLOCK_ENABLE_GAME) {
        fprintf(stderr, "[Test] $7A03 loose-ball path diverged\n");
        return 1;
    }
    /* Player 1 holding: player 2's clock resets, player 1's runs. */
    allstar_clock_ownership(0x00u, 0x01u, ALLSTAR_CLOCK_ENABLE_GAME, &own);
    if (own.resets_player_1 || !own.resets_player_2 ||
        own.enable != (ALLSTAR_CLOCK_ENABLE_GAME | ALLSTAR_CLOCK_ENABLE_PLAYER_1)) {
        fprintf(stderr, "[Test] $7A15 player 1 possession gave enable $%02X\n", own.enable);
        return 1;
    }
    /* Player 2 holding is the mirror. */
    allstar_clock_ownership(0x00u, 0x02u, ALLSTAR_CLOCK_ENABLE_GAME, &own);
    if (!own.resets_player_1 || own.resets_player_2 ||
        own.enable != (ALLSTAR_CLOCK_ENABLE_GAME | ALLSTAR_CLOCK_ENABLE_PLAYER_2)) {
        fprintf(stderr, "[Test] $7A1E player 2 possession gave enable $%02X\n", own.enable);
        return 1;
    }
    /* $7A25 keeps bit 0 and replaces bits 1 and 2, whatever they were. */
    allstar_clock_ownership(0x00u, 0x01u, 0x07u, &own);
    if (own.enable != (ALLSTAR_CLOCK_ENABLE_GAME | ALLSTAR_CLOCK_ENABLE_PLAYER_1)) {
        fprintf(stderr, "[Test] $7A26 did not mask the old enable bits\n");
        return 1;
    }
    allstar_clock_ownership(0x00u, 0x01u, 0x00u, &own);
    if (own.enable != ALLSTAR_CLOCK_ENABLE_PLAYER_1) {
        fprintf(stderr, "[Test] $7A26 invented a game-clock bit\n");
        return 1;
    }

    /* $7A2A: twenty calls per tick. */
    counter = ALLSTAR_CLOCK_TICK_RELOAD;
    for (i = 0; i < ALLSTAR_CLOCK_TICK_RELOAD - 1; i++) {
        if (allstar_clock_tick(&counter)) {
            fprintf(stderr, "[Test] $7A2E ticked early at call %d\n", i);
            return 1;
        }
    }
    if (!allstar_clock_tick(&counter) || counter != ALLSTAR_CLOCK_TICK_RELOAD) {
        fprintf(stderr, "[Test] $7A2F reload diverged, counter %u\n", counter);
        return 1;
    }

    /* $7A71: BCD seconds step down. */
    if (allstar_clock_decrement(0x0024u) != 0x0023u ||
        allstar_clock_decrement(0x0020u) != 0x0019u ||
        allstar_clock_decrement(0x0010u) != 0x0009u ||
        allstar_clock_decrement(0x0001u) != 0x0000u) {
        fprintf(stderr, "[Test] $7A7E BCD seconds decrement diverged\n");
        return 1;
    }
    /* A minute borrow wraps the seconds to $59. */
    if (allstar_clock_decrement(0x0100u) != 0x0059u ||
        allstar_clock_decrement(0x1000u) != 0x0959u) {
        fprintf(stderr, "[Test] $7A83 minute borrow gave $%04X\n",
                allstar_clock_decrement(0x0100u));
        return 1;
    }
    /* A clock already at zero is left alone. */
    if (allstar_clock_decrement(0x0000u) != 0x0000u) {
        fprintf(stderr, "[Test] $7A78 decremented a stopped clock\n");
        return 1;
    }

    /* $7A4E: the warning fires below $12, but never at exactly 1. */
    if (allstar_clock_warns(0x00u, 0x0012u) ||
        !allstar_clock_warns(0x00u, 0x0011u) ||
        !allstar_clock_warns(0x00u, 0x0002u) ||
        allstar_clock_warns(0x00u, 0x0001u)) {
        fprintf(stderr, "[Test] $7A52 warning window diverged\n");
        return 1;
    }
    /* Any minutes left means no warning. */
    if (allstar_clock_warns(0x00u, 0x0105u)) {
        fprintf(stderr, "[Test] $7A4B warned with minutes remaining\n");
        return 1;
    }
    /* Modes $01 and $02 take the branch that skips the sound. */
    if (allstar_clock_warns(0x01u, 0x0005u) || allstar_clock_warns(0x02u, 0x0005u)) {
        fprintf(stderr, "[Test] $7A3C mode branch diverged\n");
        return 1;
    }

    printf("  possession resets the other player's clock to $24 and clears its enable bit\n");
    printf("  $7A71 steps BCD seconds down, wrapping to $59 with a minute borrow\n");
    printf("[Test] PASSED: $79EE, $7A71\n");
    return 0;
}

/*
 * ROM court-wide state, from the $1C1D..$1C60, $1F3E..$1F5E, $2BC6..$2C43 and
 * $2C72..$2CBC disassembly.
 */
int allstar_cli_test_court_state_rom(void) {
    static const uint16_t GROUPS[ALLSTAR_COURT_OAM_GROUPS] = { 0xC000u, 0xC010u, 0xC020u, 0xC030u };
    AllStarCourtPauseGates gates;
    AllStarCourtPause pause;
    AllStarCourtExpiry expiry;
    AllStarCourtViolation violation;
    const uint16_t *groups;
    uint8_t selector;
    uint8_t paused;
    int count;
    int i;

    printf("[Test] Running ROM Court State Tests ($1C3E/$1F3E/$2BC6/$2C95)...\n");

    groups = allstar_court_oam_groups(&count);
    if (count != ALLSTAR_COURT_OAM_GROUPS) {
        fprintf(stderr, "[Test] $1C1D walks %d groups, expected %d\n", count, ALLSTAR_COURT_OAM_GROUPS);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (groups[i] != GROUPS[i]) {
            fprintf(stderr, "[Test] $1C1D group %d is $%04X, expected $%04X\n", i, groups[i], GROUPS[i]);
            return 1;
        }
    }

    /* $1C3F: the boundary is inclusive at $58. */
    if (allstar_court_sprite_behind(0x57u, 0u) ||
        !allstar_court_sprite_behind(0x58u, 0u) ||
        !allstar_court_sprite_behind(0xFFu, 0u)) {
        fprintf(stderr, "[Test] $1C3F priority boundary diverged\n");
        return 1;
    }
    /* $1C37: a held ball forces every group behind regardless of Y. */
    if (!allstar_court_sprite_behind(0x00u, 1u)) {
        fprintf(stderr, "[Test] $1C37 held-ball override diverged\n");
        return 1;
    }

    /* $1F3E: either flag switches the cue, neither leaves it alone. */
    selector = 0u;
    if (allstar_court_cue_select(0u, 0u, &selector) || selector != 0u) {
        fprintf(stderr, "[Test] $1F46 fired with both flags clear\n");
        return 1;
    }
    if (!allstar_court_cue_select(1u, 0u, &selector) ||
        selector != ALLSTAR_COURT_CUE_SELECT_VALUE) {
        fprintf(stderr, "[Test] $1F49 first flag diverged\n");
        return 1;
    }
    selector = 0u;
    if (!allstar_court_cue_select(0u, 1u, &selector) ||
        selector != ALLSTAR_COURT_CUE_SELECT_VALUE) {
        fprintf(stderr, "[Test] $1F43 second flag diverged\n");
        return 1;
    }
    if (allstar_court_cue_id() != ALLSTAR_COURT_CUE_ID) {
        fprintf(stderr, "[Test] $1F5B cue id diverged\n");
        return 1;
    }

    /* $2BC6: Start with every gate clear toggles the pause. */
    gates.c185 = 0; gates.c16f = 0; gates.c12e = 0;
    gates.ffeb = 0; gates.c174 = 0; gates.ffec = 0;
    paused = 0u;
    allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 0u, 0u, 0u, 0u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_ENTERED || paused != 1u ||
        pause.sound != ALLSTAR_COURT_PAUSE_SOUND ||
        pause.message != ALLSTAR_COURT_PAUSE_MESSAGE) {
        fprintf(stderr, "[Test] $2C0C pause entry diverged\n");
        return 1;
    }
    allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 0u, 0u, 0u, 0u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_LEFT || paused != 0u || pause.sound != 0) {
        fprintf(stderr, "[Test] $2C22 unpause diverged\n");
        return 1;
    }
    /* Without Start nothing happens. */
    allstar_court_pause(0x01u, &gates, 0u, 0u, 0u, 0u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_IGNORED || paused != 0u) {
        fprintf(stderr, "[Test] $2BCA fired without Start\n");
        return 1;
    }
    /* Each of the six gates independently blocks it. */
    {
        uint8_t *fields[6];
        fields[0] = &gates.c185; fields[1] = &gates.c16f; fields[2] = &gates.c12e;
        fields[3] = &gates.ffeb; fields[4] = &gates.c174; fields[5] = &gates.ffec;
        for (i = 0; i < 6; i++) {
            *fields[i] = 1u;
            allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 0u, 0u, 0u, 0u, &paused, &pause);
            if (pause.result != ALLSTAR_COURT_PAUSE_IGNORED || paused != 0u) {
                fprintf(stderr, "[Test] $2BCB gate %d did not block the pause\n", i);
                return 1;
            }
            *fields[i] = 0u;
        }
    }
    /* $2BF9: in a link game the non-player-2 side posts a request instead. */
    allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 1u, 1u, 0u, 0u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_REQUESTED || !pause.posts_link_request ||
        paused != 0u) {
        fprintf(stderr, "[Test] $2BF9 link request diverged\n");
        return 1;
    }
    /* A request already pending is dropped. */
    allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 1u, 1u, 1u, 0u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_IGNORED || pause.posts_link_request) {
        fprintf(stderr, "[Test] $2BF8 did not drop a duplicate request\n");
        return 1;
    }
    /* Player 2 consumes the button and drives the toggle itself. */
    allstar_court_pause(ALLSTAR_COURT_PAUSE_BUTTON, &gates, 1u,
                        ALLSTAR_COURT_ROLE_PLAYER_2, 0u, 0x01u, &paused, &pause);
    if (pause.result != ALLSTAR_COURT_PAUSE_ENTERED || !pause.consumes_input ||
        !pause.toggles_objects || paused != 1u) {
        fprintf(stderr, "[Test] $2C00 player 2 pause path diverged\n");
        return 1;
    }
    paused = 0u;

    /* $2C72: only a zero counter posts the expiry message. */
    allstar_court_expiry(1u, 2u, &expiry);
    if (expiry.fires) {
        fprintf(stderr, "[Test] $2C74 fired on a live counter\n");
        return 1;
    }
    allstar_court_expiry(0u, 2u, &expiry);
    if (!expiry.fires || expiry.owner != 2u ||
        expiry.message != ALLSTAR_COURT_EXPIRY_MESSAGE) {
        fprintf(stderr, "[Test] $2C7D expiry message diverged\n");
        return 1;
    }

    /* $2C95: three accepted actions, one sub-state, and possession must match. */
    allstar_court_violation(0x03u, 0x0Cu, 1u, 1u, &violation);
    if (!violation.fires || violation.message != ALLSTAR_COURT_VIOLATION_MESSAGE ||
        !violation.unwinds_caller) {
        fprintf(stderr, "[Test] $2CB6 violation message diverged\n");
        return 1;
    }
    allstar_court_violation(0x0Au, 0x0Cu, 2u, 2u, &violation);
    if (!violation.fires) {
        fprintf(stderr, "[Test] $2C9D action $0A was rejected\n");
        return 1;
    }
    allstar_court_violation(0x12u, 0x0Cu, 2u, 2u, &violation);
    if (!violation.fires) {
        fprintf(stderr, "[Test] $2CA1 action $12 was rejected\n");
        return 1;
    }
    allstar_court_violation(0x04u, 0x0Cu, 1u, 1u, &violation);
    if (violation.fires) {
        fprintf(stderr, "[Test] $2CA3 accepted an action outside $03/$0A/$12\n");
        return 1;
    }
    allstar_court_violation(0x03u, 0x0Bu, 1u, 1u, &violation);
    if (violation.fires) {
        fprintf(stderr, "[Test] $2CA9 accepted the wrong sub-state\n");
        return 1;
    }
    allstar_court_violation(0x03u, 0x0Cu, 1u, 2u, &violation);
    if (violation.fires) {
        fprintf(stderr, "[Test] $2CB3 fired without possession\n");
        return 1;
    }

    printf("  sprites go behind at Y >= $58, and a held ball forces all four groups behind\n");
    printf("  pause has six independent gates and posts $CC to $C18E in a link game\n");
    printf("[Test] PASSED: $1C1D, $1C32, $1C3E, $1F3E, $1F5B, $2BC6, $2C72, $2C95\n");
    return 0;
}

/*
 * ROM rim and backboard outcomes, from the $1AF9 table, the $1B3F..$1BBC
 * handlers and the bounce routine at $1E74.
 */
int allstar_cli_test_shot_result_rom(void) {
    static const uint16_t TABLE[ALLSTAR_SHOT_RESULT_SLOTS] = {
        0x1BA7u, 0x1B3Fu, 0x1E74u, 0x1B93u, 0x1B53u, 0x1BBDu,
        0x1B59u, 0x1B99u, 0x1E74u, 0x1B45u, 0x1BADu
    };
    AllStarShotHandler handler;
    AllStarShotSettle settle;
    AllStarShotScore score;
    const uint16_t *table;
    uint8_t timer;
    uint8_t remaining;
    uint8_t bounces;
    uint8_t height;
    bool clears;
    bool sets_c128;
    int count;
    int i;

    printf("[Test] Running ROM Shot Result Tests ($1AF9/$1B3F/$1E74)...\n");

    table = allstar_shot_result_table(&count);
    if (count != ALLSTAR_SHOT_RESULT_SLOTS) {
        fprintf(stderr, "[Test] $1AF9 has %d slots, expected %d\n", count, ALLSTAR_SHOT_RESULT_SLOTS);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != TABLE[i]) {
            fprintf(stderr, "[Test] $1AF9 slot %d is $%04X, expected $%04X\n", i, table[i], TABLE[i]);
            return 1;
        }
    }

    /* $1B3F..$1BBC: mirrored velocity pairs, except the settle pair. */
    if (!allstar_shot_handler(0x1B3Fu, &handler) || handler.velocity != -50 ||
        handler.route != ALLSTAR_SHOT_ROUTE_BOUNCE) {
        fprintf(stderr, "[Test] $1B3F diverged\n");
        return 1;
    }
    if (!allstar_shot_handler(0x1B45u, &handler) || handler.velocity != 50) {
        fprintf(stderr, "[Test] $1B45 diverged\n");
        return 1;
    }
    if (!allstar_shot_handler(0x1B93u, &handler) || handler.velocity != 110 ||
        !allstar_shot_handler(0x1B99u, &handler) || handler.velocity != -110) {
        fprintf(stderr, "[Test] $1B93/$1B99 diverged\n");
        return 1;
    }
    /* $1BA7 and $1BAD carry the same magnitudes but skip $1E74 entirely. */
    if (!allstar_shot_handler(0x1BA7u, &handler) || handler.velocity != -110 ||
        handler.route != ALLSTAR_SHOT_ROUTE_CUE ||
        !allstar_shot_handler(0x1BADu, &handler) || handler.velocity != 110 ||
        handler.route != ALLSTAR_SHOT_ROUTE_CUE) {
        fprintf(stderr, "[Test] $1BA7/$1BAD route diverged\n");
        return 1;
    }
    /* The settle pair is not symmetric: +165 against -147. */
    if (!allstar_shot_handler(0x1B53u, &handler) || handler.velocity != 165 ||
        handler.route != ALLSTAR_SHOT_ROUTE_SETTLE ||
        !allstar_shot_handler(0x1B59u, &handler) || handler.velocity != -147 ||
        handler.route != ALLSTAR_SHOT_ROUTE_SETTLE) {
        fprintf(stderr, "[Test] $1B53/$1B59 settle pair diverged\n");
        return 1;
    }
    if (allstar_shot_handler(0x1BBDu, &handler)) {
        fprintf(stderr, "[Test] $1BBD is not one of these handlers\n");
        return 1;
    }

    /* $1B64: a rightward settle resets the vertical velocity and counts nothing. */
    bounces = 0u; height = 40u;
    allstar_shot_settle(165, &bounces, &height, &settle);
    if (settle.counts_bounce || settle.lowers_height || !settle.resets_vertical ||
        bounces != 0u || height != 40u) {
        fprintf(stderr, "[Test] $1B7E rightward settle diverged\n");
        return 1;
    }
    /* The first leftward bounce counts but still resets. */
    allstar_shot_settle(-147, &bounces, &height, &settle);
    if (!settle.counts_bounce || settle.lowers_height || !settle.resets_vertical ||
        bounces != 1u || height != 40u) {
        fprintf(stderr, "[Test] $1B68 first leftward bounce diverged\n");
        return 1;
    }
    /* The second drops the height by three and takes the other exit. */
    allstar_shot_settle(-147, &bounces, &height, &settle);
    if (!settle.lowers_height || settle.resets_vertical ||
        bounces != 2u || height != 37u) {
        fprintf(stderr, "[Test] $1B73 second bounce gave count %u height %u\n", bounces, height);
        return 1;
    }

    /* $1E7A: two's-complement reversal. */
    if (allstar_shot_reverse(1000) != -1000 || allstar_shot_reverse(-1000) != 1000 ||
        allstar_shot_reverse(0) != 0) {
        fprintf(stderr, "[Test] $1E80 reversal diverged\n");
        return 1;
    }

    /* $1E8F: Free Throw damps hard only while $C0AB is clear. */
    if (allstar_shot_damping(1u, 0u, 0u, &clears, &sets_c128) != ALLSTAR_SHOT_DAMP_FREETHROW ||
        clears || !sets_c128) {
        fprintf(stderr, "[Test] $1EA2 free-throw damping diverged\n");
        return 1;
    }
    if (allstar_shot_damping(1u, 1u, 0u, &clears, &sets_c128) != ALLSTAR_SHOT_DAMP_NORMAL ||
        !sets_c128) {
        fprintf(stderr, "[Test] $1EAA suppressed free-throw damping diverged\n");
        return 1;
    }
    /* Outside Free Throw a pending $FFD4 damps hardest and is consumed. */
    if (allstar_shot_damping(0u, 0u, 1u, &clears, &sets_c128) != ALLSTAR_SHOT_DAMP_HEAVY ||
        !clears || sets_c128) {
        fprintf(stderr, "[Test] $1EB8 heavy damping diverged\n");
        return 1;
    }
    if (allstar_shot_damping(0u, 1u, 1u, &clears, NULL) != ALLSTAR_SHOT_DAMP_NORMAL || clears) {
        fprintf(stderr, "[Test] $1EAD suppression must beat the $FFD4 request\n");
        return 1;
    }
    if (allstar_shot_damping(0u, 0u, 0u, &clears, NULL) != ALLSTAR_SHOT_DAMP_NORMAL) {
        fprintf(stderr, "[Test] $1EAA default damping diverged\n");
        return 1;
    }

    /* $1ECC: fifteen frames per rim step. */
    timer = 3u; remaining = 2u;
    if (allstar_shot_tick(&timer, &remaining) != ALLSTAR_SHOT_TICK_WAIT || timer != 2u ||
        allstar_shot_tick(&timer, &remaining) != ALLSTAR_SHOT_TICK_WAIT || timer != 1u) {
        fprintf(stderr, "[Test] $1ECF countdown diverged\n");
        return 1;
    }
    if (allstar_shot_tick(&timer, &remaining) != ALLSTAR_SHOT_TICK_ADVANCE ||
        timer != ALLSTAR_SHOT_RIM_RELOAD || remaining != 1u) {
        fprintf(stderr, "[Test] $1ED1 reload gave timer %u remaining %u\n", timer, remaining);
        return 1;
    }
    timer = 1u; remaining = 0u;
    if (allstar_shot_tick(&timer, &remaining) != ALLSTAR_SHOT_TICK_IDLE ||
        timer != ALLSTAR_SHOT_RIM_RELOAD || remaining != 0u) {
        fprintf(stderr, "[Test] $1ED7 idle path diverged\n");
        return 1;
    }

    /* $1EF4: the cue threshold is three, or two when $C12A is set. */
    allstar_shot_outcome(3u, 0u, 1u, 0u, &score);
    if (score.outcome != ALLSTAR_SHOT_RIM_CUE || score.sound != ALLSTAR_SHOT_SOUND_RIM) {
        fprintf(stderr, "[Test] $1F26 rim cue at three diverged\n");
        return 1;
    }
    allstar_shot_outcome(3u, 1u, 1u, 0u, &score);
    if (score.outcome != ALLSTAR_SHOT_NOTHING) {
        fprintf(stderr, "[Test] $1EFC moved the threshold the wrong way\n");
        return 1;
    }
    allstar_shot_outcome(2u, 1u, 1u, 0u, &score);
    if (score.outcome != ALLSTAR_SHOT_RIM_CUE) {
        fprintf(stderr, "[Test] $1EFC two-step cue diverged\n");
        return 1;
    }

    /* $1F06: the score lands only at zero, on the shooter's word. */
    allstar_shot_outcome(1u, 0u, 1u, 0u, &score);
    if (score.outcome != ALLSTAR_SHOT_NOTHING) {
        fprintf(stderr, "[Test] $1F04 scored before the counter ran out\n");
        return 1;
    }
    allstar_shot_outcome(0u, 0u, 1u, 0u, &score);
    if (score.outcome != ALLSTAR_SHOT_SCORE || score.sound != ALLSTAR_SHOT_SOUND_SCORE ||
        score.score_address != ALLSTAR_SHOT_SCORE_1 || score.points != 2u) {
        fprintf(stderr, "[Test] $1F12 player 1 two-point score diverged\n");
        return 1;
    }
    allstar_shot_outcome(0u, 0u, 2u, 1u, &score);
    if (score.score_address != ALLSTAR_SHOT_SCORE_2 || score.points != 3u) {
        fprintf(stderr, "[Test] $1F17 player 2 three-point score diverged\n");
        return 1;
    }

    printf("  rim velocities come in mirrored pairs; the settle pair is +165 against -147\n");
    printf("  $1E74 reverses the vertical word, damps by -57/-250/-300, then scores 2 or 3\n");
    printf("[Test] PASSED: $1AF9, $1B3F, $1B45, $1B53, $1B59, $1B93, $1B99, $1BA7, $1BAD, $1C12, $1E74\n");
    return 0;
}

/*
 * ROM record and prompt helpers, from the $2D85..$2EA7 and bank 1 $780A
 * disassembly.
 */
int allstar_cli_test_select_records_rom(void) {
    AllStarSelectClearRun runs[4];
    AllStarSelectPrompt prompt;
    uint8_t slots[8];
    uint8_t digits[4];
    uint8_t seed;
    int count;
    int i;

    printf("[Test] Running ROM Record Helper Tests ($2D93/$2DBE/$2DD2/$780A)...\n");

    /* $2EA3 */
    for (i = 0; i < 8; i++) slots[i] = 0x11u;
    allstar_select_fill(slots, 4u, ALLSTAR_SELECT_EMPTY_SLOT);
    if (slots[0] != 0x80u || slots[3] != 0x80u || slots[4] != 0x11u) {
        fprintf(stderr, "[Test] $2EA3 fill diverged\n");
        return 1;
    }

    /* $2E73 and $2E8C clear opposite sides of every stage. */
    count = allstar_select_clear_runs(ALLSTAR_SELECT_PASS_1, runs, 4);
    if (count != ALLSTAR_SELECT_CLEAR_RUNS ||
        runs[0].address != 0xC0BFu || runs[0].count != 4u ||
        runs[1].address != 0xC0CBu || runs[1].count != 2u ||
        runs[2].address != 0xC0D1u || runs[2].count != 1u) {
        fprintf(stderr, "[Test] $2E73 clear runs diverged\n");
        return 1;
    }
    count = allstar_select_clear_runs(ALLSTAR_SELECT_PASS_2, runs, 4);
    if (runs[0].address != 0xC0C3u || runs[1].address != 0xC0CDu || runs[2].address != 0xC0D2u) {
        fprintf(stderr, "[Test] $2E8C clear runs diverged\n");
        return 1;
    }

    /* $2DD2: id 0 is the table head, later ids follow that many $FF bytes. */
    {
        static const uint8_t TABLE[12] = {
            0x41u, 0x42u, 0xFFu, 0x43u, 0xFFu, 0x44u, 0x45u, 0xFFu,
            0x46u, 0xFFu, 0x47u, 0xFFu
        };
        if (allstar_select_record_offset(TABLE, 12u, 0u) != 0u ||
            allstar_select_record_offset(TABLE, 12u, 1u) != 3u ||
            allstar_select_record_offset(TABLE, 12u, 2u) != 5u ||
            allstar_select_record_offset(TABLE, 12u, 3u) != 8u) {
            fprintf(stderr, "[Test] $2DDA record walk diverged\n");
            return 1;
        }
    }

    /* $2DBE: slot 1 lands one byte before the $C23C name buffer. */
    if (allstar_select_record_buffer(1u) != 0xC23Bu ||
        allstar_select_record_buffer(2u) != 0xC254u) {
        fprintf(stderr, "[Test] $2DC4 record buffer selection diverged\n");
        return 1;
    }

    /* $2D93: the seed becomes a roster index by counting sevens. */
    if (allstar_select_cpu_opponent(0u, 0xFFu, NULL) != 0u ||
        allstar_select_cpu_opponent(6u, 0xFFu, NULL) != 0u ||
        allstar_select_cpu_opponent(7u, 0xFFu, NULL) != 1u ||
        allstar_select_cpu_opponent(13u, 0xFFu, NULL) != 1u ||
        allstar_select_cpu_opponent(14u, 0xFFu, NULL) != 2u ||
        allstar_select_cpu_opponent(70u, 0xFFu, NULL) != 10u) {
        fprintf(stderr, "[Test] $2D98 seven-step walk diverged\n");
        return 1;
    }
    /* A collision with $FFAC nudges the seed by twenty and walks again. */
    seed = 0u;
    if (allstar_select_cpu_opponent(7u, 1u, &seed) != 3u || seed != 27u) {
        fprintf(stderr, "[Test] $2DAA collision retry gave index %u seed %u\n",
                allstar_select_cpu_opponent(7u, 1u, &seed), seed);
        return 1;
    }
    /* No collision leaves the seed untouched. */
    seed = 0u;
    if (allstar_select_cpu_opponent(7u, 5u, &seed) != 1u || seed != 7u) {
        fprintf(stderr, "[Test] $2DA8 non-collision disturbed the seed\n");
        return 1;
    }

    /* $2DEA: a one-player game never announces which player is picking. */
    allstar_select_prompt_shape(0x03u, 1u, 1u, &prompt);
    if (prompt.prompt_sound != 0x03u || prompt.announces_player ||
        prompt.player_sound != 0 || prompt.hold_frames != ALLSTAR_SELECT_PROMPT_HOLD) {
        fprintf(stderr, "[Test] $2DF2 one-player prompt diverged\n");
        return 1;
    }
    allstar_select_prompt_shape(0x05u, 2u, 1u, &prompt);
    if (!prompt.announces_player || prompt.player_sound != ALLSTAR_SELECT_PROMPT_P1) {
        fprintf(stderr, "[Test] $2DFB player 1 announcement diverged\n");
        return 1;
    }
    allstar_select_prompt_shape(0x06u, 2u, 2u, &prompt);
    if (prompt.player_sound != ALLSTAR_SELECT_PROMPT_P2) {
        fprintf(stderr, "[Test] $2DFF player 2 announcement diverged\n");
        return 1;
    }

    /* $780A: four nibbles, no leading-zero blanking, unlike $1726. */
    allstar_select_wide_digits(0x1234u, digits);
    if (digits[0] != 0xC2u || digits[1] != 0xC3u || digits[2] != 0xC4u || digits[3] != 0xC5u) {
        fprintf(stderr, "[Test] $780A on 1234 gave $%02X $%02X $%02X $%02X\n",
                digits[0], digits[1], digits[2], digits[3]);
        return 1;
    }
    allstar_select_wide_digits(0x0007u, digits);
    if (digits[0] != 0xC1u || digits[1] != 0xC1u || digits[2] != 0xC1u || digits[3] != 0xC8u) {
        fprintf(stderr, "[Test] $780A blanked a leading zero it should have printed\n");
        return 1;
    }

    printf("  $2E73/$2E8C mark every slot on one side $80 before its pass\n");
    printf("  $2D93 divides the $FFFB seed by seven and adds twenty on a collision\n");
    printf("[Test] PASSED: $2D85, $2D93, $2DBE, $2DD2, $2DEA, $2E70, $2E73, $2E8C, $2EA3, $2AB5, $780A\n");
    return 0;
}

/*
 * ROM player info card, from the $414B..$42A1 disassembly and the per-player
 * tables at $42A2 and $42BD.
 */
int allstar_cli_test_select_card_rom(void) {
    AllStarSelectCardOp ops[12];
    uint8_t tiles[ALLSTAR_SELECT_TILE_COUNT];
    int count;
    int i;

    printf("[Test] Running ROM Player Card Tests ($414B)...\n");

    count = allstar_select_card_layout(ops, 12);
    if (count != ALLSTAR_SELECT_CARD_OPS) {
        fprintf(stderr, "[Test] $414B emitted %d draw steps, expected %d\n",
                count, ALLSTAR_SELECT_CARD_OPS);
        return 1;
    }

    /* $415A/$417A/$4184: two rules with sixteen framed rows between them. */
    if (ops[0].source != ALLSTAR_SELECT_CARD_RULE || ops[0].e != 0x00u ||
        ops[1].source != ALLSTAR_SELECT_CARD_ROW || ops[1].e != 0x01u ||
        ops[1].rows != ALLSTAR_SELECT_CARD_ROWS ||
        ops[2].source != ALLSTAR_SELECT_CARD_RULE || ops[2].e != 0x11u) {
        fprintf(stderr, "[Test] $414B frame steps diverged\n");
        return 1;
    }

    /* $41C7 and $4217: a six-row block at (4,1) and a four-row block at (12,2). */
    if (ops[3].d != 0x04u || ops[3].e != 0x01u || ops[3].rows != 6u || ops[3].per_row != 4u ||
        ops[4].d != 0x0Cu || ops[4].e != 0x02u || ops[4].rows != 4u || ops[4].per_row != 4u) {
        fprintf(stderr, "[Test] $41C7/$4217 tile blocks diverged\n");
        return 1;
    }

    /* $4230..$4292: the name and the three labelled stats read fixed offsets. */
    if (ops[5].record_field != ALLSTAR_SELECT_FIELD_NAME || ops[5].d != 0x01u || ops[5].e != 0x08u ||
        ops[7].source != ALLSTAR_SELECT_LABEL_HEIGHT || ops[7].record_field != 0x0Au ||
        ops[7].d != 0x02u || ops[7].e != 0x0Bu ||
        ops[8].source != ALLSTAR_SELECT_LABEL_WEIGHT || ops[8].record_field != 0x0Eu || ops[8].e != 0x0Du ||
        ops[9].source != ALLSTAR_SELECT_LABEL_PPG    || ops[9].record_field != 0x12u || ops[9].e != 0x0Fu) {
        fprintf(stderr, "[Test] $4250/$426B/$4280 stat lines diverged\n");
        return 1;
    }

    /* $418D: the portrait table is two bytes per roster id. */
    if (allstar_select_portrait_slot(0u) != 0x2D4Fu ||
        allstar_select_portrait_slot(1u) != 0x2D51u ||
        allstar_select_portrait_slot(26u) != 0x2D83u) {
        fprintf(stderr, "[Test] $418D portrait table stride diverged\n");
        return 1;
    }

    /* $4199: with no hole the array is a plain 1..24. */
    allstar_select_punch_tiles(0xFFu, tiles);
    for (i = 0; i < (int)ALLSTAR_SELECT_TILE_COUNT; i++) {
        if (tiles[i] != (uint8_t)(i + 1)) {
            fprintf(stderr, "[Test] $4199 base fill diverged at %d\n", i);
            return 1;
        }
    }

    /* $41BE: a hole zeroes that slot and pulls everything after it down by one. */
    allstar_select_punch_tiles(20u, tiles);
    if (tiles[19] != 20u || tiles[20] != 0u || tiles[21] != 21u || tiles[23] != 23u) {
        fprintf(stderr, "[Test] $41BE hole at 20 gave %u/%u/%u/%u\n",
                tiles[19], tiles[20], tiles[21], tiles[23]);
        return 1;
    }
    allstar_select_punch_tiles(0u, tiles);
    if (tiles[0] != 0u || tiles[1] != 1u || tiles[23] != 23u) {
        fprintf(stderr, "[Test] $41BE hole at 0 diverged\n");
        return 1;
    }

    /* $41E0: the second block counts from 25 only when there was no hole. */
    if (allstar_select_tile_base(0xFFu) != 25u || allstar_select_tile_base(20u) != 24u) {
        fprintf(stderr, "[Test] $41E0 tile base diverged\n");
        return 1;
    }

    /* $41E7: player N's marker record starts after N delimiters. */
    {
        static const uint8_t STREAM[16] = {
            0xFFu, 0xFEu, 0xFFu, 0xFEu, 0xFFu, 0xFEu, 0x00u, 0x0Fu,
            0xFEu, 0xFFu, 0xFEu, 0xFFu, 0xFEu, 0xFFu, 0xFEu, 0x00u
        };
        if (allstar_select_mark_offset(STREAM, 16u, 0u) != 0u ||
            allstar_select_mark_offset(STREAM, 16u, 1u) != 2u ||
            allstar_select_mark_offset(STREAM, 16u, 3u) != 6u) {
            fprintf(stderr, "[Test] $41F0 delimiter walk diverged\n");
            return 1;
        }
    }

    /*
     * $41FF: a marked index writes zero and does not advance the running tile
     * number, so the numbering closes over the gap instead of skipping a value.
     */
    {
        static const uint8_t MARKS[2] = { 0x00u, 0x0Fu };
        allstar_select_build_tiles(MARKS, 2u, 25u, tiles);
        if (tiles[0] != 0u || tiles[1] != 25u || tiles[2] != 26u ||
            tiles[14] != 38u || tiles[15] != 0u || tiles[16] != 39u ||
            tiles[23] != 46u) {
            fprintf(stderr, "[Test] $41FF marked build gave %u/%u/%u ... %u/%u/%u\n",
                    tiles[0], tiles[1], tiles[2], tiles[14], tiles[15], tiles[16]);
            return 1;
        }
        allstar_select_build_tiles(NULL, 0u, 24u, tiles);
        if (tiles[0] != 24u || tiles[23] != 47u) {
            fprintf(stderr, "[Test] $420A unmarked build diverged\n");
            return 1;
        }
    }

    printf("  card frames rows 1..16, blocks at (4,1) and (12,2), stats at rows 11/13/15\n");
    printf("  HEIGHT/WEIGHT/PPG AVG read record offsets $0A/$0E/$12, the name reads $16\n");
    printf("[Test] PASSED: $414B\n");
    return 0;
}

/*
 * ROM bank 2 entrant selector, from the $4000..$40F3 and $40F4..$414A
 * disassembly plus the destination tables at $4358 and $4360.
 */
int allstar_cli_test_select_rom(void) {
    uint8_t index;
    uint8_t picked[8];

    printf("[Test] Running ROM Entrant Selector Tests ($4000/$4045/$40B1/$40F4)...\n");

    /*
     * $4358/$4360: the selector writes straight into the bracket slots the
     * $0F2E driver later reads, which is what ties the two halves together.
     */
    if (allstar_select_destination(4u, ALLSTAR_SELECT_PASS_1) != 0xC0BFu ||
        allstar_select_destination(4u, ALLSTAR_SELECT_PASS_2) != 0xC0C3u ||
        allstar_select_destination(2u, ALLSTAR_SELECT_PASS_1) != 0xC0CBu ||
        allstar_select_destination(2u, ALLSTAR_SELECT_PASS_2) != 0xC0CDu ||
        allstar_select_destination(1u, ALLSTAR_SELECT_PASS_1) != 0xC0D1u ||
        allstar_select_destination(1u, ALLSTAR_SELECT_PASS_2) != 0xC0D2u) {
        fprintf(stderr, "[Test] $4358/$4360 destination tables diverged\n");
        return 1;
    }
    if (allstar_select_destination(3u, ALLSTAR_SELECT_PASS_1) != 0x0000u) {
        fprintf(stderr, "[Test] $4358 slot 2 should be the unused $0000 entry\n");
        return 1;
    }

    /* $400F: only a one-player game in mode $01 or $03 skips the second pass. */
    if (!allstar_select_runs_second_pass(2u, 1u) ||
        !allstar_select_runs_second_pass(1u, 4u) ||
        !allstar_select_runs_second_pass(1u, 0u) ||
        allstar_select_runs_second_pass(1u, 1u) ||
        allstar_select_runs_second_pass(1u, 3u)) {
        fprintf(stderr, "[Test] $4012 second-pass gate diverged\n");
        return 1;
    }

    /* $406A: the tournament prompt encodes both stage and picker. */
    if (allstar_select_prompt(4u, 4u, 1u, 2u) != 0x03u ||
        allstar_select_prompt(4u, 4u, 2u, 2u) != 0x04u ||
        allstar_select_prompt(4u, 2u, 1u, 2u) != 0x05u ||
        allstar_select_prompt(4u, 2u, 2u, 2u) != 0x06u ||
        allstar_select_prompt(4u, 1u, 1u, 2u) != 0x07u ||
        allstar_select_prompt(4u, 1u, 2u, 2u) != 0x08u) {
        fprintf(stderr, "[Test] $4083 tournament prompt table diverged\n");
        return 1;
    }
    /* Outside the tournament only the one-player second picker differs. */
    if (allstar_select_prompt(0u, 4u, 1u, 1u) != 0x01u ||
        allstar_select_prompt(0u, 4u, 2u, 2u) != 0x01u ||
        allstar_select_prompt(0u, 4u, 2u, 1u) != 0x02u) {
        fprintf(stderr, "[Test] $4070 non-tournament prompt diverged\n");
        return 1;
    }

    /* $40C4: the duplicate scan spans both passes. */
    if (allstar_select_scan_length(4u) != 8u || allstar_select_scan_length(1u) != 2u) {
        fprintf(stderr, "[Test] $40C4 scan length diverged\n");
        return 1;
    }
    picked[0] = 9u; picked[1] = 3u; picked[2] = 14u; picked[3] = 0u;
    picked[4] = 22u; picked[5] = 7u; picked[6] = 1u; picked[7] = 5u;
    if (!allstar_select_is_duplicate(picked, 8u, 22u) ||
        !allstar_select_is_duplicate(picked, 8u, 9u) ||
        allstar_select_is_duplicate(picked, 8u, 26u)) {
        fprintf(stderr, "[Test] $40C9 duplicate scan diverged\n");
        return 1;
    }
    /* A shorter stage must not see the later pass's entries. */
    if (allstar_select_is_duplicate(picked, 2u, 22u)) {
        fprintf(stderr, "[Test] $40C4 scanned past this stage's entries\n");
        return 1;
    }

    /* $4100: player 2's pad is read only when she is picking in a two-player game. */
    if (allstar_select_buttons(2u, 1u, 0x11u, 0x22u) != 0x11u ||
        allstar_select_buttons(1u, 2u, 0x11u, 0x22u) != 0x11u ||
        allstar_select_buttons(2u, 2u, 0x11u, 0x22u) != 0x22u) {
        fprintf(stderr, "[Test] $4100 input source selection diverged\n");
        return 1;
    }

    /* $40F8: $FFEC stalls the loop entirely. */
    index = 3u;
    if (allstar_select_step(1u, ALLSTAR_SELECT_CONFIRM_MASK, 27u, &index) != ALLSTAR_SELECT_IDLE ||
        index != 3u) {
        fprintf(stderr, "[Test] $40F8 did not stall while $FFEC was set\n");
        return 1;
    }

    /* $410F: confirm is tested before movement, and uses held input. */
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_START, 27u, &index) != ALLSTAR_SELECT_CONFIRMED ||
        index != 3u) {
        fprintf(stderr, "[Test] $410F Start did not confirm\n");
        return 1;
    }
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_SELECT, 27u, &index) != ALLSTAR_SELECT_CONFIRMED) {
        fprintf(stderr, "[Test] $410F Select did not confirm\n");
        return 1;
    }
    /* Confirm wins even when a movement bit is held at the same time. */
    if (allstar_select_step(0u, (uint8_t)(ALLSTAR_SELECT_ROM_START | ALLSTAR_SELECT_ROM_RIGHT),
                            27u, &index) != ALLSTAR_SELECT_CONFIRMED || index != 3u) {
        fprintf(stderr, "[Test] $410F confirm lost to a movement bit\n");
        return 1;
    }

    /* $4119/$4127: A and Right step forward, B and Left step backward. */
    index = 0u;
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_RIGHT, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 1u) {
        fprintf(stderr, "[Test] Right did not step forward\n");
        return 1;
    }
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_A, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 2u) {
        fprintf(stderr, "[Test] A did not step forward\n");
        return 1;
    }
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_LEFT, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 1u) {
        fprintf(stderr, "[Test] Left did not step backward\n");
        return 1;
    }
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_B, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 0u) {
        fprintf(stderr, "[Test] B did not step backward\n");
        return 1;
    }

    /* $4131/$413C: both sentinels wrap. */
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_LEFT, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 26u) {
        fprintf(stderr, "[Test] $413C did not wrap to the last entry, index %u\n", index);
        return 1;
    }
    if (allstar_select_step(0u, ALLSTAR_SELECT_ROM_RIGHT, 27u, &index) != ALLSTAR_SELECT_MOVED ||
        index != 0u) {
        fprintf(stderr, "[Test] $4131 did not wrap to the first entry, index %u\n", index);
        return 1;
    }

    /* Nothing held moves nothing. */
    if (allstar_select_step(0u, 0x40u, 27u, &index) != ALLSTAR_SELECT_IDLE || index != 0u) {
        fprintf(stderr, "[Test] $4119 moved on a bit outside the $33 mask\n");
        return 1;
    }

    printf("  $4358/$4360 write the picks into $C0BF/$C0C3, $C0CB/$C0CD, $C0D1/$C0D2\n");
    printf("  cursor reads held input: Select or Start confirms, A/Right forward, B/Left back\n");
    printf("[Test] PASSED: $4000, $4034, $4045, $4053, $406A, $40B1, $40F4\n");
    return 0;
}

/*
 * ROM bracket chooser, from the $1483..$1553 and $1554..$15AA disassembly.
 */
int allstar_cli_test_postgame_chooser_rom(void) {
    AllStarPostgameChooser chooser;
    AllStarPostgameChooserCell cells[8];
    uint8_t selection;
    int count;
    int i;

    printf("[Test] Running ROM Bracket Chooser Tests ($1483/$14BD/$1554)...\n");

    /* $1483: $C184 picks the side, four names walk back from the list end. */
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R1_BY_PICKER, 1u, 1u, &chooser);
    if (chooser.list_end != 0xC0C2u || chooser.pair_only || chooser.count != 4u ||
        chooser.sound != 0x0Fu) {
        fprintf(stderr, "[Test] $1483 with $C184 == 1 diverged\n");
        return 1;
    }
    {
        static const uint16_t SLOTS[4] = { 0xC0C2u, 0xC0C1u, 0xC0C0u, 0xC0BFu };
        static const uint8_t ROWS[4] = { 10u, 8u, 6u, 4u };
        for (i = 0; i < 4; i++) {
            if (chooser.names[i].slot != SLOTS[i] || chooser.names[i].d != 0x03u ||
                chooser.names[i].e != ROWS[i]) {
                fprintf(stderr, "[Test] Chooser name %d read $%04X at (%u,%u)\n",
                        i, chooser.names[i].slot, chooser.names[i].d, chooser.names[i].e);
                return 1;
            }
        }
    }
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R1_BY_PICKER, 2u, 2u, &chooser);
    if (chooser.list_end != 0xC0C6u || chooser.sound != 0x12u) {
        fprintf(stderr, "[Test] $148E with $C184 != 1 diverged\n");
        return 1;
    }
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R1_BY_PICKER, 2u, 1u, &chooser);
    if (chooser.sound != 0x11u) {
        fprintf(stderr, "[Test] $14AA two-player sound diverged\n");
        return 1;
    }

    /* $14B6 and $14BD ignore $C184 and use a different one-player sound. */
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R1_RIGHT, 1u, 1u, &chooser);
    if (chooser.list_end != 0xC0C6u || chooser.sound != 0x10u || chooser.count != 4u) {
        fprintf(stderr, "[Test] $14B6 diverged\n");
        return 1;
    }
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R2_RIGHT, 1u, 1u, &chooser);
    if (chooser.list_end != 0xC0CEu || !chooser.pair_only || chooser.count != 2u ||
        chooser.sound != 0x10u) {
        fprintf(stderr, "[Test] $14BD diverged\n");
        return 1;
    }
    if (chooser.names[0].slot != 0xC0CEu || chooser.names[0].e != 6u ||
        chooser.names[1].slot != 0xC0CDu || chooser.names[1].e != 4u) {
        fprintf(stderr, "[Test] $14F7 pair rows diverged\n");
        return 1;
    }
    allstar_postgame_chooser(ALLSTAR_POSTGAME_CHOOSER_R2_BY_PICKER, 2u, 1u, &chooser);
    if (chooser.list_end != 0xC0CCu || !chooser.pair_only) {
        fprintf(stderr, "[Test] $1493 diverged\n");
        return 1;
    }

    /* $1521: player 2's input is only read in a two-player game she is picking in. */
    if (allstar_postgame_chooser_buttons(1u, 2u, 0x01u, 0x02u) != 0x01u ||
        allstar_postgame_chooser_buttons(2u, 1u, 0x01u, 0x02u) != 0x01u ||
        allstar_postgame_chooser_buttons(2u, 2u, 0x01u, 0x02u) != 0x02u) {
        fprintf(stderr, "[Test] $1524 input source selection diverged\n");
        return 1;
    }

    /* $1533 accepts mask $CB; Start confirms, anything else toggles. */
    selection = 0u;
    if (allstar_postgame_chooser_step(1u, ALLSTAR_POSTGAME_CHOOSER_CONFIRM, &selection)
            != ALLSTAR_POSTGAME_CHOOSER_IDLE || selection != 0u) {
        fprintf(stderr, "[Test] $151C did not stall while $FFEC was set\n");
        return 1;
    }
    if (allstar_postgame_chooser_step(0u, 0x04u, &selection) != ALLSTAR_POSTGAME_CHOOSER_IDLE ||
        selection != 0u) {
        fprintf(stderr, "[Test] $1533 accepted a button outside the $CB mask\n");
        return 1;
    }
    if (allstar_postgame_chooser_step(0u, 0x01u, &selection) != ALLSTAR_POSTGAME_CHOOSER_TOGGLED ||
        selection != 1u) {
        fprintf(stderr, "[Test] $153B did not toggle $C181\n");
        return 1;
    }
    if (allstar_postgame_chooser_step(0u, 0x40u, &selection) != ALLSTAR_POSTGAME_CHOOSER_TOGGLED ||
        selection != 0u) {
        fprintf(stderr, "[Test] $153E did not toggle $C181 back\n");
        return 1;
    }
    if (allstar_postgame_chooser_step(0u, ALLSTAR_POSTGAME_CHOOSER_CONFIRM, &selection)
            != ALLSTAR_POSTGAME_CHOOSER_CONFIRMED || selection != 0u) {
        fprintf(stderr, "[Test] $1537 Start did not confirm\n");
        return 1;
    }

    /* $1554: the two boxes swap rows and middle-row art with the selection. */
    count = allstar_postgame_chooser_layout(0u, cells, 8);
    if (count != ALLSTAR_POSTGAME_CHOOSER_CELLS ||
        cells[0].source != 0x15ABu || cells[0].e != 0x0Cu ||
        cells[1].source != 0x15B5u || cells[1].e != 0x0Du ||
        cells[2].source != 0x15C9u || cells[2].e != 0x0Eu ||
        cells[3].source != 0x15B0u || cells[3].e != 0x0Fu ||
        cells[4].source != 0x15C4u || cells[4].e != 0x10u ||
        cells[5].source != 0x15CEu || cells[5].e != 0x11u) {
        fprintf(stderr, "[Test] $1554 layout for selection 0 diverged\n");
        return 1;
    }
    count = allstar_postgame_chooser_layout(1u, cells, 8);
    if (cells[0].e != 0x0Fu || cells[1].source != 0x15BFu ||
        cells[3].e != 0x0Cu || cells[4].source != 0x15BAu) {
        fprintf(stderr, "[Test] $1554 layout for selection 1 diverged\n");
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (cells[i].d != ALLSTAR_POSTGAME_CHOOSER_COLUMN) {
            fprintf(stderr, "[Test] $1554 cell %d left column $0B\n", i);
            return 1;
        }
    }

    printf("  names list bottom-up in rows 10/8/6/4, or 6/4 for a pair\n");
    printf("  mask $CB toggles $C181, Start confirms and returns it\n");
    printf("[Test] PASSED: $1483, $1493, $14B6, $14BD, $1554\n");
    return 0;
}

/*
 * ROM VS screen and bracket display, from the $1343..$139A and $139B..$1463
 * disassembly.
 */
int allstar_cli_test_postgame_bracket_rom(void) {
    AllStarPostgameDraw layout[8];
    AllStarPostgameDrawDetail detail;
    AllStarPostgameBracket bracket;
    int count;
    int i;

    printf("[Test] Running ROM VS/Bracket Tests ($1343/$139B/$1464)...\n");

    /* $134E: only the tournament gets the GAME line. */
    count = allstar_postgame_matchup_layout(ALLSTAR_POSTGAME_MODE_TOURNAMENT, layout, 8);
    if (count != ALLSTAR_POSTGAME_VS_LAYOUT_OPS ||
        layout[0].kind != ALLSTAR_POSTGAME_DRAW_TEXT_GAME   || layout[0].d != 0x06u || layout[0].e != 0x0Du ||
        layout[1].kind != ALLSTAR_POSTGAME_DRAW_MATCH_DIGIT || layout[1].d != 0x0Cu || layout[1].e != 0x0Du ||
        layout[2].kind != ALLSTAR_POSTGAME_DRAW_NAME_1_RAW  || layout[2].d != 0x05u || layout[2].e != 0x05u ||
        layout[3].kind != ALLSTAR_POSTGAME_DRAW_TEXT_VS     || layout[3].d != 0x08u || layout[3].e != 0x07u ||
        layout[4].kind != ALLSTAR_POSTGAME_DRAW_NAME_2_RAW  || layout[4].d != 0x05u || layout[4].e != 0x09u) {
        fprintf(stderr, "[Test] $1343 tournament layout diverged\n");
        return 1;
    }
    count = allstar_postgame_matchup_layout(0u, layout, 8);
    if (count != 3 ||
        layout[0].kind != ALLSTAR_POSTGAME_DRAW_NAME_1_RAW ||
        layout[1].kind != ALLSTAR_POSTGAME_DRAW_TEXT_VS ||
        layout[2].kind != ALLSTAR_POSTGAME_DRAW_NAME_2_RAW) {
        fprintf(stderr, "[Test] $1352 non-tournament layout kept the GAME line\n");
        return 1;
    }
    if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_TEXT_VS, &detail) ||
        detail.source != ALLSTAR_POSTGAME_TEXT_VS ||
        !allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_TEXT_GAME, &detail) ||
        detail.source != ALLSTAR_POSTGAME_TEXT_GAME) {
        fprintf(stderr, "[Test] $1354/$1374 string sources diverged\n");
        return 1;
    }

    /* $135D: the match counter shares the $C1 digit base with $1726. */
    if (allstar_postgame_match_digit(1u) != 0xC2u ||
        allstar_postgame_match_digit(7u) != 0xC8u) {
        fprintf(stderr, "[Test] $1360 match digit diverged\n");
        return 1;
    }

    /* $13A9: stage $01 returns before drawing anything. */
    allstar_postgame_bracket(1u, &bracket);
    if (bracket.draws || bracket.count != 0 || bracket.sound != 0 || bracket.hold_frames != 0) {
        fprintf(stderr, "[Test] $13AA did not return early for $C17F == $01\n");
        return 1;
    }

    /* Stage $04 lists all eight, interleaved left/right from row 1. */
    allstar_postgame_bracket(ALLSTAR_POSTGAME_BRACKET_FULL, &bracket);
    if (!bracket.draws || bracket.count != 8u || bracket.sound != 0x0Du ||
        bracket.hold_frames != ALLSTAR_POSTGAME_BRACKET_HOLD_FRAMES) {
        fprintf(stderr, "[Test] $13C4 full bracket header diverged\n");
        return 1;
    }
    {
        static const uint16_t EXPECTED[8] = {
            0xC0BFu, 0xC0C3u, 0xC0C0u, 0xC0C4u, 0xC0C1u, 0xC0C5u, 0xC0C2u, 0xC0C6u
        };
        for (i = 0; i < 8; i++) {
            if (bracket.entries[i].slot != EXPECTED[i] ||
                bracket.entries[i].d != 0x01u ||
                bracket.entries[i].e != (uint8_t)(1u + i * 2u)) {
                fprintf(stderr, "[Test] Full bracket row %d read $%04X at (%u,%u)\n",
                        i, bracket.entries[i].slot, bracket.entries[i].d, bracket.entries[i].e);
                return 1;
            }
        }
    }

    /* Any other stage lists the four semifinalists, starting at row 5. */
    allstar_postgame_bracket(2u, &bracket);
    if (!bracket.draws || bracket.count != 4u || bracket.sound != 0x0Eu) {
        fprintf(stderr, "[Test] $1426 semifinal bracket header diverged\n");
        return 1;
    }
    {
        static const uint16_t EXPECTED[4] = { 0xC0CBu, 0xC0CDu, 0xC0CCu, 0xC0CEu };
        for (i = 0; i < 4; i++) {
            if (bracket.entries[i].slot != EXPECTED[i] ||
                bracket.entries[i].e != (uint8_t)(5u + i * 2u)) {
                fprintf(stderr, "[Test] Semifinal row %d read $%04X at row %u\n",
                        i, bracket.entries[i].slot, bracket.entries[i].e);
                return 1;
            }
        }
    }
    allstar_postgame_bracket(3u, &bracket);
    if (bracket.count != 4u || bracket.sound != 0x00u) {
        fprintf(stderr, "[Test] $146B sound table diverged for stage 3\n");
        return 1;
    }

    printf("  bracket rows interleave left and right so each pair is adjacent\n");
    printf("  $C17F 2/3/4 -> sounds $0E/$00/$0D, hold $0384 frames\n");
    printf("[Test] PASSED: $1343, $139B, $1464\n");
    return 0;
}

/*
 * ROM mode-3 and mode-4 postgame screens, from the $1209..$12A5 and
 * $12A6..$1342 disassembly.
 */
int allstar_cli_test_postgame_modes_rom(void) {
    AllStarPostgameAccuracy accuracy;
    AllStarPostgameTournament tournament;
    AllStarPostgameDraw layout[8];
    AllStarPostgameDrawDetail detail;
    int count;

    printf("[Test] Running ROM Postgame Mode Tests ($1209/$12A6)...\n");

    /* $125E: $FF91 == $01 is a one-player game and skips the handshake. */
    allstar_postgame_accuracy_entry(1u, 1u, &accuracy);
    if (accuracy.two_player || accuracy.ready_flag != 0 || accuracy.poll_address != 0 ||
        accuracy.music != 0) {
        fprintf(stderr, "[Test] $125E one-player path diverged\n");
        return 1;
    }

    /* Two players: flag and poll swap on $C199, and $C270 keeps the $F0. */
    allstar_postgame_accuracy_entry(1u, 2u, &accuracy);
    if (!accuracy.two_player ||
        accuracy.ready_flag != ALLSTAR_POSTGAME_SCORE_1_HIGH ||
        accuracy.poll_address != ALLSTAR_POSTGAME_SCORE_2_HIGH ||
        accuracy.status_byte != ALLSTAR_POSTGAME_READY_FLAG ||
        accuracy.music != ALLSTAR_POSTGAME_MUSIC_ACCURACY) {
        fprintf(stderr, "[Test] $1264 player 1 handshake diverged\n");
        return 1;
    }
    allstar_postgame_accuracy_entry(ALLSTAR_POSTGAME_ROLE_PLAYER_2, 2u, &accuracy);
    if (accuracy.ready_flag != ALLSTAR_POSTGAME_SCORE_2_HIGH ||
        accuracy.poll_address != ALLSTAR_POSTGAME_SCORE_1_HIGH ||
        !accuracy.is_player_2) {
        fprintf(stderr, "[Test] $1272 player 2 handshake diverged\n");
        return 1;
    }

    /* $120E/$1229/$1234/$123A/$1243/$124C, in that order. */
    count = allstar_postgame_accuracy_layout(false, layout, 8);
    if (count != ALLSTAR_POSTGAME_ACC_LAYOUT_OPS ||
        layout[0].kind != ALLSTAR_POSTGAME_DRAW_NAME_1_RAW  || layout[0].d != 0x05u || layout[0].e != 0x03u ||
        layout[1].kind != ALLSTAR_POSTGAME_DRAW_TOTAL_HI    || layout[1].d != 0x0Fu || layout[1].e != 0x06u ||
        layout[2].kind != ALLSTAR_POSTGAME_DRAW_TOTAL_LO    || layout[2].d != 0x10u || layout[2].e != 0x06u ||
        layout[3].kind != ALLSTAR_POSTGAME_DRAW_WORD_C137   || layout[3].d != 0x11u || layout[3].e != 0x0Au ||
        layout[4].kind != ALLSTAR_POSTGAME_DRAW_WORD_C139   || layout[4].d != 0x11u || layout[4].e != 0x08u ||
        layout[5].kind != ALLSTAR_POSTGAME_DRAW_SCORE_1     || layout[5].d != 0x11u || layout[5].e != 0x0Cu) {
        fprintf(stderr, "[Test] $1209 draw layout diverged\n");
        return 1;
    }
    if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_WORD_C139, &detail) ||
        detail.source != ALLSTAR_POSTGAME_WORD_C139 || detail.routine != 0x1778u) {
        fprintf(stderr, "[Test] $1243 word source diverged\n");
        return 1;
    }

    /* $12A6: a decided quarterfinal draws the panel and returns to $0F2E. */
    allstar_postgame_tournament(1u, 1u, &tournament);
    if (tournament.path != ALLSTAR_POSTGAME_TR_RETURN || tournament.is_final ||
        !tournament.draws_score_panel || tournament.writes_tie_flag ||
        tournament.sound != 0) {
        fprintf(stderr, "[Test] $12B4 decided-match return diverged\n");
        return 1;
    }

    /* A tie in any earlier round replays the match rather than advancing. */
    allstar_postgame_tournament(3u, 0u, &tournament);
    if (tournament.path != ALLSTAR_POSTGAME_TR_REPLAY ||
        tournament.sound != ALLSTAR_POSTGAME_SOUND_REPLAY ||
        tournament.hold_frames != ALLSTAR_POSTGAME_REPLAY_FRAMES ||
        tournament.writes_tie_flag || tournament.tie_flag != 0) {
        fprintf(stderr, "[Test] $12B5 tie replay diverged\n");
        return 1;
    }

    /* Match $07 with a winner is the only path to the champion screen. */
    allstar_postgame_tournament(ALLSTAR_POSTGAME_FINAL_MATCH, 1u, &tournament);
    if (tournament.path != ALLSTAR_POSTGAME_TR_CHAMPION || !tournament.is_final ||
        tournament.draws_score_panel ||
        tournament.sound != ALLSTAR_POSTGAME_SOUND_CHAMPION ||
        tournament.champion_source != ALLSTAR_POSTGAME_ENTRANT_1 ||
        tournament.music != ALLSTAR_POSTGAME_MUSIC_CHAMPION ||
        !tournament.writes_tie_flag || tournament.tie_flag != 0) {
        fprintf(stderr, "[Test] $12FB champion path diverged for player 1\n");
        return 1;
    }
    allstar_postgame_tournament(ALLSTAR_POSTGAME_FINAL_MATCH, 2u, &tournament);
    if (tournament.champion_source != ALLSTAR_POSTGAME_ENTRANT_2) {
        fprintf(stderr, "[Test] $130D did not pick $FFC5 for a player 2 champion\n");
        return 1;
    }

    /* A tied final sets $C192, draws the panel, and replays. */
    allstar_postgame_tournament(ALLSTAR_POSTGAME_FINAL_MATCH, 0u, &tournament);
    if (tournament.path != ALLSTAR_POSTGAME_TR_REPLAY || tournament.tie_flag != 1u ||
        !tournament.draws_score_panel || !tournament.writes_tie_flag ||
        tournament.champion_source != 0) {
        fprintf(stderr, "[Test] $12C1 tied-final path diverged\n");
        return 1;
    }

    /* $1323: skip spaces from offset 1, step on, step past a '.', back up one. */
    {
        static const uint8_t DOTTED[12] = { 0x00u, 0x20u, 0x4Du, 0x2Eu, 0x20u, 0x4Au,
                                            0x4Fu, 0x52u, 0x44u, 0x41u, 0x4Eu, 0x00u };
        static const uint8_t PLAIN[8] = { 0x00u, 0x42u, 0x49u, 0x52u, 0x44u, 0x00u, 0x00u, 0x00u };
        if (allstar_postgame_record_surname(DOTTED, 12u) != 4u) {
            fprintf(stderr, "[Test] $132D dotted-initial walk landed on %u, expected 4\n",
                    allstar_postgame_record_surname(DOTTED, 12u));
            return 1;
        }
        if (allstar_postgame_record_surname(PLAIN, 8u) != 1u) {
            fprintf(stderr, "[Test] $1334 plain-name walk landed on %u, expected 1\n",
                    allstar_postgame_record_surname(PLAIN, 8u));
            return 1;
        }
    }

    printf("  a tie replays the match through $0B9A and re-enters $10A5 -- the bracket never advances\n");
    printf("  only $C0BE == $07 with a winner reaches $12FB, and that path skips $10FA\n");
    printf("[Test] PASSED: $1209, $12A6\n");
    return 0;
}

/*
 * ROM mode-1 and mode-2 postgame screens, from the $1121..$1208, $170D..$1725
 * and $1786..$17A9 disassembly.
 */
int allstar_cli_test_postgame_screens_rom(void) {
    AllStarPostgameFreeThrow entry;
    AllStarPostgameHorse horse;
    AllStarPostgameClear clear;
    AllStarPostgameDraw layout[4];
    AllStarPostgameDrawDetail detail;
    uint8_t scratch[24];
    uint8_t written;
    int count;

    printf("[Test] Running ROM Postgame Screen Tests ($1121/$11D5/$170D/$1786)...\n");

    /* $C199 == 0 is a solo game: no handshake, straight to the draw at $1195. */
    allstar_postgame_free_throw_entry(0u, 0x00u, 0x00u, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_DRAW || entry.ready_flag != 0 ||
        entry.poll_address != 0 || entry.announce_sound != 0 || entry.is_player_2) {
        fprintf(stderr, "[Test] $112C solo path diverged\n");
        return 1;
    }

    /* Player 1 waiting: flags $C134, polls $C136, announces with sound $19. */
    allstar_postgame_free_throw_entry(1u, 0x00u, 0x00u, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_WAIT ||
        entry.ready_flag != ALLSTAR_POSTGAME_SCORE_1_HIGH ||
        entry.poll_address != ALLSTAR_POSTGAME_SCORE_2_HIGH ||
        entry.announce_sound != ALLSTAR_POSTGAME_SOUND_READY_1 || entry.is_player_2) {
        fprintf(stderr, "[Test] $1132 player 1 wait path diverged\n");
        return 1;
    }

    /* Player 2 is the mirror image: flags $C136, polls $C134, sound $18. */
    allstar_postgame_free_throw_entry(ALLSTAR_POSTGAME_ROLE_PLAYER_2, 0x00u, 0x00u, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_WAIT ||
        entry.ready_flag != ALLSTAR_POSTGAME_SCORE_2_HIGH ||
        entry.poll_address != ALLSTAR_POSTGAME_SCORE_1_HIGH ||
        entry.announce_sound != ALLSTAR_POSTGAME_SOUND_READY_2 || !entry.is_player_2) {
        fprintf(stderr, "[Test] $1142 player 2 wait path diverged\n");
        return 1;
    }

    /* If the other side already flagged $F0, $1152 syncs and skips the wait. */
    allstar_postgame_free_throw_entry(1u, 0x00u, ALLSTAR_POSTGAME_READY_FLAG, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_SYNC ||
        entry.ready_flag != ALLSTAR_POSTGAME_SCORE_1_HIGH ||
        entry.poll_address != 0 || entry.announce_sound != 0) {
        fprintf(stderr, "[Test] $1152 sync path diverged for player 1\n");
        return 1;
    }
    allstar_postgame_free_throw_entry(ALLSTAR_POSTGAME_ROLE_PLAYER_2,
                                      ALLSTAR_POSTGAME_READY_FLAG, 0x00u, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_SYNC ||
        entry.ready_flag != ALLSTAR_POSTGAME_SCORE_2_HIGH) {
        fprintf(stderr, "[Test] $1152 sync path diverged for player 2\n");
        return 1;
    }
    /* Player 1's own $F0 must not be mistaken for the other side being ready. */
    allstar_postgame_free_throw_entry(1u, ALLSTAR_POSTGAME_READY_FLAG, 0x00u, &entry);
    if (entry.path != ALLSTAR_POSTGAME_FT_WAIT) {
        fprintf(stderr, "[Test] $1132 read the local flag instead of the remote one\n");
        return 1;
    }

    /* $11A2/$11B4/$11BF: name at $0504, attempts at $1107, score at $110A. */
    count = allstar_postgame_free_throw_layout(false, layout, 4);
    if (count != ALLSTAR_POSTGAME_FT_LAYOUT_OPS ||
        layout[0].kind != ALLSTAR_POSTGAME_DRAW_NAME_1_RAW || layout[0].d != 0x05u || layout[0].e != 0x04u ||
        layout[1].kind != ALLSTAR_POSTGAME_DRAW_ATTEMPTS   || layout[1].d != 0x11u || layout[1].e != 0x07u ||
        layout[2].kind != ALLSTAR_POSTGAME_DRAW_SCORE_1    || layout[2].d != 0x11u || layout[2].e != 0x0Au) {
        fprintf(stderr, "[Test] $1121 player 1 draw layout diverged\n");
        return 1;
    }
    count = allstar_postgame_free_throw_layout(true, layout, 4);
    if (layout[0].kind != ALLSTAR_POSTGAME_DRAW_NAME_2_RAW ||
        layout[2].kind != ALLSTAR_POSTGAME_DRAW_SCORE_2 ||
        layout[1].kind != ALLSTAR_POSTGAME_DRAW_ATTEMPTS) {
        fprintf(stderr, "[Test] $1121 player 2 draw layout diverged\n");
        return 1;
    }
    /* The attempt counter comes from $FF98, not from a score word. */
    if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_ATTEMPTS, &detail) ||
        detail.routine != 0x177Bu || detail.source != ALLSTAR_POSTGAME_ATTEMPTS) {
        fprintf(stderr, "[Test] $11B4 attempt source diverged\n");
        return 1;
    }

    /* $11D5: $FFAB picks the eliminated player, $C17D records the survivor. */
    allstar_postgame_horse(0u, &horse);
    if (horse.loser_name != ALLSTAR_POSTGAME_NAME_1 || horse.winner != 2u ||
        horse.message != ALLSTAR_POSTGAME_HORSE_MESSAGE ||
        horse.message_length != ALLSTAR_POSTGAME_HORSE_MESSAGE_LEN ||
        horse.d != 0x02u || horse.e != 0x07u) {
        fprintf(stderr, "[Test] $11D5 with $FFAB clear diverged\n");
        return 1;
    }
    allstar_postgame_horse(1u, &horse);
    if (horse.loser_name != ALLSTAR_POSTGAME_NAME_2 || horse.winner != 1u) {
        fprintf(stderr, "[Test] $11D5 with $FFAB set diverged\n");
        return 1;
    }

    /* $170D: bit 7 terminates, trailing spaces go, one space is appended. */
    {
        /* "JORDAN   " with the terminator bit on the last space. */
        static const uint8_t PADDED[9] = { 0x4Au, 0x4Fu, 0x52u, 0x44u, 0x41u, 0x4Eu, 0x20u, 0x20u, 0xA0u };
        static const uint8_t TIGHT[4] = { 0x42u, 0x49u, 0x52u, 0xC4u };  /* "BIRD" terminated */
        written = allstar_postgame_copy_name(PADDED, 9u, scratch, 24u);
        if (written != 7u || scratch[0] != 0x4Au || scratch[5] != 0x4Eu || scratch[6] != 0x20u) {
            fprintf(stderr, "[Test] $170D padded copy wrote %u bytes\n", written);
            return 1;
        }
        written = allstar_postgame_copy_name(TIGHT, 4u, scratch, 24u);
        if (written != 5u || scratch[3] != 0x44u || scratch[4] != 0x20u) {
            fprintf(stderr, "[Test] $170D did not clear bit 7 on the last character\n");
            return 1;
        }
    }

    /* $1786 clears sixteen tiles per row, six by six, from $9800. */
    allstar_postgame_clear_shape(&clear);
    if (clear.base != 0x9800u || clear.outer != 6u || clear.inner != 6u ||
        clear.per_row != 16u || clear.fill != 0u) {
        fprintf(stderr, "[Test] $1786 clear shape diverged\n");
        return 1;
    }

    printf("  $C199 0/1/3 -> draw/wait/wait, flags $C134 or $C136, sounds $19 and $18\n");
    printf("  H-O-R-S-E prints the eliminated name plus \"IS OUT\" at $0207\n");
    printf("[Test] PASSED: $1121, $11D5, $170D, $1786\n");
    return 0;
}

/*
 * ROM postgame spine parity, from the $10A5..$1120, $1638..$1670, $1726..$1750
 * and $1751..$1785 disassembly.  Both dispatch tables are decoded from the
 * bytes the ROM stores inline after each rst $08.
 */
int allstar_cli_test_postgame_rom(void) {
    static const uint16_t SCREEN_TABLE[4] = { 0x10E1u, 0x1343u, 0x139Bu, 0x146Fu };
    static const uint16_t MODE_TABLE[5]   = { 0x10EEu, 0x1121u, 0x11D5u, 0x1209u, 0x12A6u };
    static const AllStarPostgameDrawKind LAYOUT_KIND[6] = {
        ALLSTAR_POSTGAME_DRAW_PANEL, ALLSTAR_POSTGAME_DRAW_NAME_1, ALLSTAR_POSTGAME_DRAW_SCORE_1,
        ALLSTAR_POSTGAME_DRAW_PANEL, ALLSTAR_POSTGAME_DRAW_NAME_2, ALLSTAR_POSTGAME_DRAW_SCORE_2
    };
    static const uint8_t LAYOUT_D[6] = { 0x03u, 0x04u, 0x0Du, 0x03u, 0x04u, 0x0Du };
    static const uint8_t LAYOUT_E[6] = { 0x05u, 0x06u, 0x06u, 0x0Bu, 0x0Cu, 0x0Cu };

    AllStarPostgameEnter enter;
    AllStarPostgameDraw layout[8];
    const uint16_t *table;
    uint16_t sources[ALLSTAR_POSTGAME_PANEL_ROWS];
    uint8_t rows[ALLSTAR_POSTGAME_PANEL_ROWS];
    uint8_t digits[3];
    uint8_t bank;
    uint16_t frames;
    int count;
    int i;

    printf("[Test] Running ROM Postgame Dispatch Tests ($10A5/$10D9/$10E4/$1726)...\n");

    /* $10D9 and $10E4 are literal pointer tables stored after the rst $08. */
    table = allstar_postgame_screen_table(&count);
    if (count != 4) {
        fprintf(stderr, "[Test] $10D9 table has %d entries, expected 4\n", count);
        return 1;
    }
    for (i = 0; i < 4; i++) {
        if (table[i] != SCREEN_TABLE[i]) {
            fprintf(stderr, "[Test] $10D9 slot %d is $%04X, expected $%04X\n", i, table[i], SCREEN_TABLE[i]);
            return 1;
        }
    }
    table = allstar_postgame_mode_table(&count);
    if (count != 5) {
        fprintf(stderr, "[Test] $10E4 table has %d entries, expected 5\n", count);
        return 1;
    }
    for (i = 0; i < 5; i++) {
        if (table[i] != MODE_TABLE[i]) {
            fprintf(stderr, "[Test] $10E4 slot %d is $%04X, expected $%04X\n", i, table[i], MODE_TABLE[i]);
            return 1;
        }
    }

    /* $10A5 is the stub the tournament driver uses; mode 4 must reach $12A6. */
    allstar_postgame_enter(ALLSTAR_POSTGAME_ENTRY_RESULT, 4u, &enter);
    if (enter.screen != 0u || !enter.sets_result_flag || enter.bank != 1u ||
        enter.route != ALLSTAR_POSTGAME_ROUTE_BY_MODE || enter.handler != 0x12A6u) {
        fprintf(stderr, "[Test] $10A5 with mode 4 resolved to $%04X, expected $12A6\n", enter.handler);
        return 1;
    }
    if (!enter.loads_tiles) {
        fprintf(stderr, "[Test] $10BC skipped the tile load for a mode other than $01\n");
        return 1;
    }

    /* Mode $01 is the one mode that skips the $0444/$047E/$050F block. */
    allstar_postgame_enter(ALLSTAR_POSTGAME_ENTRY_RESULT, 1u, &enter);
    if (enter.loads_tiles || enter.handler != 0x1121u) {
        fprintf(stderr, "[Test] $10BC-$10C0 mode $01 shortcut diverged\n");
        return 1;
    }

    /* Every mode routes through $10E1 for screen 0. */
    for (i = 0; i < 5; i++) {
        allstar_postgame_enter(ALLSTAR_POSTGAME_ENTRY_RESULT, (uint8_t)i, &enter);
        if (enter.handler != MODE_TABLE[i]) {
            fprintf(stderr, "[Test] $10E1 mode %d resolved to $%04X, expected $%04X\n",
                    i, enter.handler, MODE_TABLE[i]);
            return 1;
        }
    }

    /* $10B1 is what $28D9 calls after the bank 2 selector: screen 2 -> $139B. */
    allstar_postgame_enter(ALLSTAR_POSTGAME_ENTRY_SELECT, 4u, &enter);
    if (enter.screen != 2u || enter.sets_result_flag ||
        enter.route != ALLSTAR_POSTGAME_ROUTE_139B || enter.handler != 0x139Bu) {
        fprintf(stderr, "[Test] $10B1 resolved to $%04X, expected $139B\n", enter.handler);
        return 1;
    }

    /* $10EE: only a tie continues into $12C9. */
    if (allstar_postgame_result_route(1u) != ALLSTAR_POSTGAME_RESULT_DRAW_ONLY ||
        allstar_postgame_result_route(2u) != ALLSTAR_POSTGAME_RESULT_DRAW_ONLY ||
        allstar_postgame_result_route(0u) != ALLSTAR_POSTGAME_RESULT_DRAW_THEN_12C9) {
        fprintf(stderr, "[Test] $10EE result routing diverged\n");
        return 1;
    }

    /* $10FA loads six DE pairs, in order. */
    count = allstar_postgame_final_score_layout(layout, 8);
    if (count != ALLSTAR_POSTGAME_LAYOUT_OPS) {
        fprintf(stderr, "[Test] $10FA emitted %d draw calls, expected 6\n", count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (layout[i].kind != LAYOUT_KIND[i] || layout[i].d != LAYOUT_D[i] || layout[i].e != LAYOUT_E[i]) {
            fprintf(stderr, "[Test] $10FA call %d used DE=$%02X%02X, expected $%02X%02X\n",
                    i, layout[i].d, layout[i].e, LAYOUT_D[i], LAYOUT_E[i]);
            return 1;
        }
    }

    /* $1657 stacks three rows from $166F/$167D/$168B at E, E+1, E+2. */
    allstar_postgame_panel_rows(0x03u, 0x05u, sources, rows);
    if (sources[0] != 0x166Fu || sources[1] != 0x167Du || sources[2] != 0x168Bu ||
        rows[0] != 0x05u || rows[1] != 0x06u || rows[2] != 0x07u) {
        fprintf(stderr, "[Test] $1657 row stacking diverged\n");
        return 1;
    }

    /* $1751/$1756/$175B/$1760/$1770/$1775 read the buffers the ROM names. */
    {
        AllStarPostgameDrawDetail detail;
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_NAME_1, &detail) ||
            detail.routine != 0x175Bu || detail.source != ALLSTAR_POSTGAME_NAME_1 ||
            !detail.skip_spaces || detail.is_score) {
            fprintf(stderr, "[Test] $175B draw detail diverged\n");
            return 1;
        }
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_NAME_1_RAW, &detail) ||
            detail.routine != 0x1751u || detail.skip_spaces) {
            fprintf(stderr, "[Test] $1751 must not skip leading spaces\n");
            return 1;
        }
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_NAME_2_RAW, &detail) ||
            detail.routine != 0x1756u || detail.source != ALLSTAR_POSTGAME_NAME_2) {
            fprintf(stderr, "[Test] $1756 draw detail diverged\n");
            return 1;
        }
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_NAME_2, &detail) ||
            detail.routine != 0x1760u || !detail.skip_spaces) {
            fprintf(stderr, "[Test] $1760 draw detail diverged\n");
            return 1;
        }
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_SCORE_1, &detail) ||
            detail.routine != 0x1770u || detail.source != ALLSTAR_POSTGAME_SCORE_1 || !detail.is_score) {
            fprintf(stderr, "[Test] $1770 draw detail diverged\n");
            return 1;
        }
        if (!allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_SCORE_2, &detail) ||
            detail.routine != 0x1775u || detail.source != ALLSTAR_POSTGAME_SCORE_2) {
            fprintf(stderr, "[Test] $1775 draw detail diverged\n");
            return 1;
        }
        if (allstar_postgame_draw_detail(ALLSTAR_POSTGAME_DRAW_PANEL, &detail)) {
            fprintf(stderr, "[Test] $1657 is a panel, not a name or score writer\n");
            return 1;
        }
    }

    /* $177B writes exactly the three $1726 tiles. */
    {
        uint8_t tiles[3];
        if (allstar_postgame_score_tiles(0x0042u, tiles) != ALLSTAR_POSTGAME_SCORE_TILES ||
            tiles[0] != 0x00u || tiles[1] != 0xC5u || tiles[2] != 0xC3u) {
            fprintf(stderr, "[Test] $177B tile write diverged\n");
            return 1;
        }
    }

    /* $1769 stops at the first non-space. */
    {
        static const uint8_t PADDED[8] = { 0x20u, 0x20u, 0x20u, 0x4Au, 0x4Fu, 0x52u, 0x44u, 0x20u };
        static const uint8_t EMPTY[3] = { 0x20u, 0x20u, 0x20u };
        if (allstar_postgame_skip_spaces(PADDED, 8u) != 3u ||
            allstar_postgame_skip_spaces(PADDED + 3, 5u) != 0u ||
            allstar_postgame_skip_spaces(EMPTY, 3u) != 3u) {
            fprintf(stderr, "[Test] $1769 space skip diverged\n");
            return 1;
        }
    }

    /*
     * $1726: BCD hundreds in H's low nibble, tens and units in L.  Leading zeros
     * blank, but a zero tens digit prints once the hundreds digit has printed.
     */
    allstar_postgame_score_digits(0x0021u, digits);
    if (digits[0] != 0x00u || digits[1] != 0xC3u || digits[2] != 0xC2u) {
        fprintf(stderr, "[Test] $1726 on 21 gave $%02X $%02X $%02X\n", digits[0], digits[1], digits[2]);
        return 1;
    }
    allstar_postgame_score_digits(0x0007u, digits);
    if (digits[0] != 0x00u || digits[1] != 0x00u || digits[2] != 0xC8u) {
        fprintf(stderr, "[Test] $1726 on 7 gave $%02X $%02X $%02X\n", digits[0], digits[1], digits[2]);
        return 1;
    }
    allstar_postgame_score_digits(0x0100u, digits);
    if (digits[0] != 0xC2u || digits[1] != 0xC1u || digits[2] != 0xC1u) {
        fprintf(stderr, "[Test] $1726 on 100 gave $%02X $%02X $%02X\n", digits[0], digits[1], digits[2]);
        return 1;
    }
    allstar_postgame_score_digits(0x0000u, digits);
    if (digits[0] != 0x00u || digits[1] != 0x00u || digits[2] != 0xC1u) {
        fprintf(stderr, "[Test] $1726 on 0 gave $%02X $%02X $%02X\n", digits[0], digits[1], digits[2]);
        return 1;
    }
    allstar_postgame_score_digits(0x0999u, digits);
    if (digits[0] != 0xCAu || digits[1] != 0xCAu || digits[2] != 0xCAu) {
        fprintf(stderr, "[Test] $1726 on 999 gave $%02X $%02X $%02X\n", digits[0], digits[1], digits[2]);
        return 1;
    }

    /* $146F pages in bank 2 and dispatches on $C181 through $147B. */
    if (allstar_postgame_route_146f(0u, &bank) != 0x1483u || bank != 2u ||
        allstar_postgame_route_146f(1u, &bank) != 0x14B6u ||
        allstar_postgame_route_146f(2u, &bank) != 0x1493u ||
        allstar_postgame_route_146f(3u, &bank) != 0x14BDu) {
        fprintf(stderr, "[Test] $146F/$147B dispatch diverged\n");
        return 1;
    }

    /* $1638: Start exits only while $FFEC is clear, otherwise BC has to expire. */
    frames = 3u;
    if (allstar_postgame_hold_step(&frames, 0u, ALLSTAR_POSTGAME_START_MASK) != ALLSTAR_POSTGAME_HOLD_INPUT ||
        frames != 3u) {
        fprintf(stderr, "[Test] $1645 did not exit the hold on new Start\n");
        return 1;
    }
    frames = 3u;
    if (allstar_postgame_hold_step(&frames, 1u, ALLSTAR_POSTGAME_START_MASK) != ALLSTAR_POSTGAME_HOLD_WAITING ||
        frames != 2u) {
        fprintf(stderr, "[Test] $1640 let Start through while $FFEC was set\n");
        return 1;
    }
    if (allstar_postgame_hold_step(&frames, 1u, 0u) != ALLSTAR_POSTGAME_HOLD_WAITING ||
        allstar_postgame_hold_step(&frames, 1u, 0u) != ALLSTAR_POSTGAME_HOLD_TIMEOUT) {
        fprintf(stderr, "[Test] $164B frame countdown diverged\n");
        return 1;
    }

    printf("  $FF8D 0..3 -> $10E1/$1343/$139B/$146F; mode 0..4 -> $10EE/$1121/$11D5/$1209/$12A6\n");
    printf("  tournament postgame lands on $12A6; $1726 blanks leading zeros, never the units\n");
    printf("[Test] PASSED: $10A5, $10B1, $10E1, $10EE, $10FA, $146F, $1638, $1657, $1726, $1751-$177B\n");
    return 0;
}

/*
 * ROM $0F2E driver parity.  The expected call order, the entrant address read
 * for each of the seven matches, the $C0BE values, and the bracket slots that
 * $284D/$286E fill are all taken from the $0F2E..$0FBA, $2835..$28E0 and
 * $28E1..$290A disassembly.
 */
int allstar_cli_test_tournament_rom(void) {
    static const AllStarTournamentStep EXPECTED[] = {
        ALLSTAR_TR_STEP_RESET, ALLSTAR_TR_STEP_PICK_FIELD, ALLSTAR_TR_STEP_BANK_WINS,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R1,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R1,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R1,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R1,
        ALLSTAR_TR_STEP_SEED_SEMIS, ALLSTAR_TR_STEP_BANK_WINS,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R2,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH, ALLSTAR_TR_STEP_POSTGAME, ALLSTAR_TR_STEP_ADVANCE_R2,
        ALLSTAR_TR_STEP_SEED_FINAL, ALLSTAR_TR_STEP_BANK_WINS,
        ALLSTAR_TR_STEP_LOAD_PLAYERS, ALLSTAR_TR_STEP_PLAY_MATCH,
        ALLSTAR_TR_STEP_DONE
    };
    /* Entrants the bank 2 selector will place, and which side wins each match. */
    static const uint8_t R1_LEFT[4]  = { 10, 11, 12, 13 };
    static const uint8_t R1_RIGHT[4] = { 20, 21, 22, 23 };
    static const uint8_t R2_LEFT[2]  = { 10, 12 };
    static const uint8_t R2_RIGHT[2] = { 21, 23 };
    static const uint8_t WINNING_SIDE[7] = { 1, 2, 1, 2, 1, 2, 1 };
    static const uint8_t EXPECTED_LEFT[7]  = { 10, 11, 12, 13, 10, 12, 10 };
    static const uint8_t EXPECTED_RIGHT[7] = { 20, 21, 22, 23, 21, 23, 23 };
    static const uint8_t EXPECTED_R1_WINNERS[4] = { 10, 21, 12, 23 };
    static const uint8_t EXPECTED_R2_WINNERS[2] = { 10, 23 };

    AllStarTournamentRom rom;
    size_t index;
    int match = 0;
    int i;

    printf("[Test] Running ROM Tournament Driver Tests ($0F2E/$22DE/$0FBB/$284D/$286E)...\n");

    allstar_tournament_rom_begin(&rom);

    /* $22DE must clear the counter and both win-counter pairs, and nothing else. */
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_MATCH_COUNT, 0x77u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_STAGE_WINS_LEFT, 0x77u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT, 0x77u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_TOTAL_WINS_LEFT, 0x77u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_TOTAL_WINS_RIGHT, 0x77u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_R1_WINNERS, 0x77u);

    for (index = 0; index < sizeof(EXPECTED) / sizeof(EXPECTED[0]); index++) {
        AllStarTournamentStep step = allstar_tournament_rom_step(&rom);
        if (step != EXPECTED[index]) {
            fprintf(stderr,
                    "[Test] $0F2E call %u was %s, expected %s\n",
                    (unsigned)index,
                    allstar_tournament_rom_step_name(step),
                    allstar_tournament_rom_step_name(EXPECTED[index]));
            return 1;
        }

        switch (step) {
        case ALLSTAR_TR_STEP_RESET:
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_MATCH_COUNT) != 0 ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_LEFT) != 0 ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT) != 0 ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_LEFT) != 0 ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_RIGHT) != 0) {
                fprintf(stderr, "[Test] $22DE did not clear $C0BE/$C0D4-$C0D7\n");
                return 1;
            }
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_R1_WINNERS) != 0x77u) {
                fprintf(stderr, "[Test] $22DE cleared bracket slots it must not touch\n");
                return 1;
            }
            if ((rom.lcdc & 0x83u) != 0x80u) {
                fprintf(stderr, "[Test] $0F2E left $FF40 at $%02X, expected bit 7 set and bits 0-1 clear\n", rom.lcdc);
                return 1;
            }
            break;

        case ALLSTAR_TR_STEP_PICK_FIELD:
            /* $2890 -> $0B35 builds ids 0..26 between $FF sentinels. */
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_SELECT_LIST) != 0xFFu ||
                allstar_tournament_rom_peek(&rom, 0xC0F4u) != 0xFFu ||
                allstar_tournament_rom_peek(&rom, 0xC0D9u) != 0u ||
                allstar_tournament_rom_peek(&rom, 0xC0F3u) != 26u) {
                fprintf(stderr, "[Test] $0B35 candidate list diverged\n");
                return 1;
            }
            if (!rom.select.pending || rom.select.count != 4u || rom.select_flag != 1u) {
                fprintf(stderr, "[Test] $2890 did not request a four-pair selection\n");
                return 1;
            }
            for (i = 0; i < 4; i++) {
                allstar_tournament_rom_poke(&rom, (uint16_t)(ALLSTAR_TR_R1_LEFT + i), R1_LEFT[i]);
                allstar_tournament_rom_poke(&rom, (uint16_t)(ALLSTAR_TR_R1_RIGHT + i), R1_RIGHT[i]);
            }
            break;

        case ALLSTAR_TR_STEP_SEED_SEMIS:
            for (i = 0; i < 4; i++) {
                if (allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_R1_WINNERS + i)) != EXPECTED_R1_WINNERS[i]) {
                    fprintf(stderr,
                            "[Test] $284D put %u in $%04X, expected %u\n",
                            allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_R1_WINNERS + i)),
                            (unsigned)(ALLSTAR_TR_R1_WINNERS + i), EXPECTED_R1_WINNERS[i]);
                    return 1;
                }
                if (allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 1u + i)) != EXPECTED_R1_WINNERS[i]) {
                    fprintf(stderr, "[Test] $2897 did not copy the round 1 winners into the list\n");
                    return 1;
                }
            }
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_SELECT_LIST) != 0xFFu ||
                allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 5u)) != 0xFFu ||
                rom.select.count != 2u || rom.select.destination != ALLSTAR_TR_R2_LEFT) {
                fprintf(stderr, "[Test] $2897 selection request diverged\n");
                return 1;
            }
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_LEFT) != 2u ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT) != 2u) {
                fprintf(stderr, "[Test] Round 1 stage win counters diverged\n");
                return 1;
            }
            for (i = 0; i < 2; i++) {
                allstar_tournament_rom_poke(&rom, (uint16_t)(ALLSTAR_TR_R2_LEFT + i), R2_LEFT[i]);
                allstar_tournament_rom_poke(&rom, (uint16_t)(ALLSTAR_TR_R2_RIGHT + i), R2_RIGHT[i]);
            }
            break;

        case ALLSTAR_TR_STEP_SEED_FINAL:
            for (i = 0; i < 2; i++) {
                if (allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_R2_WINNERS + i)) != EXPECTED_R2_WINNERS[i]) {
                    fprintf(stderr,
                            "[Test] $286E put %u in $%04X, expected %u\n",
                            allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_R2_WINNERS + i)),
                            (unsigned)(ALLSTAR_TR_R2_WINNERS + i), EXPECTED_R2_WINNERS[i]);
                    return 1;
                }
            }
            if (rom.select.count != 1u || rom.select.destination != ALLSTAR_TR_FINAL_LEFT ||
                allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 3u)) != 0xFFu) {
                fprintf(stderr, "[Test] $28B0 selection request diverged\n");
                return 1;
            }
            allstar_tournament_rom_poke(&rom, ALLSTAR_TR_FINAL_LEFT, EXPECTED_R2_WINNERS[0]);
            allstar_tournament_rom_poke(&rom, ALLSTAR_TR_FINAL_RIGHT, EXPECTED_R2_WINNERS[1]);
            break;

        case ALLSTAR_TR_STEP_BANK_WINS:
            if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_LEFT) != 0 ||
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT) != 0) {
                fprintf(stderr, "[Test] $2835 did not clear $C0D4/$C0D5\n");
                return 1;
            }
            break;

        case ALLSTAR_TR_STEP_LOAD_PLAYERS:
            if (rom.current_left != EXPECTED_LEFT[match] ||
                rom.current_right != EXPECTED_RIGHT[match]) {
                fprintf(stderr,
                        "[Test] Match %d loaded ($FFAC=%u,$FFC5=%u), expected (%u,%u)\n",
                        match + 1, rom.current_left, rom.current_right,
                        EXPECTED_LEFT[match], EXPECTED_RIGHT[match]);
                return 1;
            }
            if (rom.loaded_slot[0] != rom.current_left ||
                rom.loaded_slot[1] != rom.current_right ||
                rom.loaded_bank != 1u) {
                fprintf(stderr, "[Test] $0FBB slot load or bank restore diverged on match %d\n", match + 1);
                return 1;
            }
            if ((rom.lcdc & 0x03u) != 0) {
                fprintf(stderr, "[Test] $0F4A did not clear $FF40 bits 0-1 before match %d\n", match + 1);
                return 1;
            }
            break;

        case ALLSTAR_TR_STEP_PLAY_MATCH:
            if (allstar_tournament_rom_match_number(&rom) != (uint8_t)(match + 1)) {
                fprintf(stderr,
                        "[Test] $C0BE was %u entering match %d, expected %d\n",
                        allstar_tournament_rom_match_number(&rom), match + 1, match + 1);
                return 1;
            }
            /* $0B80 leaves the two score words behind for $28E1 to compare. */
            rom.score_left = (WINNING_SIDE[match] == 1u) ? 21u : 15u;
            rom.score_right = (WINNING_SIDE[match] == 1u) ? 15u : 21u;
            match++;
            break;

        default:
            break;
        }
    }

    if (match != 7) {
        fprintf(stderr, "[Test] $0F2E ran %d matches, expected 7\n", match);
        return 1;
    }
    if (rom.music_command != 0) {
        fprintf(stderr, "[Test] $0F40 did not clear the $DD73 music command\n");
        return 1;
    }
    /* The final runs no advance routine, so the round 2 slots must be untouched. */
    if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_R2_WINNERS) != EXPECTED_R2_WINNERS[0] ||
        allstar_tournament_rom_peek(&rom, (uint16_t)(ALLSTAR_TR_R2_WINNERS + 1u)) != EXPECTED_R2_WINNERS[1] ||
        allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_LEFT) != 0 ||
        allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT) != 0) {
        fprintf(stderr, "[Test] The final mutated bracket state the ROM leaves alone\n");
        return 1;
    }
    if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_LEFT) != 3u ||
        allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_RIGHT) != 3u) {
        fprintf(stderr, "[Test] $2835 banked totals diverged (%u/%u), expected 3/3\n",
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_LEFT),
                allstar_tournament_rom_peek(&rom, ALLSTAR_TR_TOTAL_WINS_RIGHT));
        return 1;
    }
    if (allstar_tournament_rom_step(&rom) != ALLSTAR_TR_STEP_DONE) {
        fprintf(stderr, "[Test] $0F2E did not stay returned after the final\n");
        return 1;
    }

    /* $2850/$2871: a tied match records nothing at all. */
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_MATCH_COUNT, 0x01u);
    allstar_tournament_rom_poke(&rom, ALLSTAR_TR_R1_WINNERS, 0x5Au);
    rom.score_left = 17u;
    rom.score_right = 17u;
    allstar_tournament_rom_advance_round_1(&rom);
    if (allstar_tournament_rom_peek(&rom, ALLSTAR_TR_R1_WINNERS) != 0x5Au ||
        allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_LEFT) != 0 ||
        allstar_tournament_rom_peek(&rom, ALLSTAR_TR_STAGE_WINS_RIGHT) != 0) {
        fprintf(stderr, "[Test] A tied $28E1 result still advanced the bracket\n");
        return 1;
    }

    printf("  7 matches, $C0BE 1..7, round breaks at $04 and $06, final skips $10A5\n");
    printf("  bracket: $C0C7-$C0CA = 10/21/12/23, $C0CF/$C0D0 = 10/23, totals 3/3\n");
    printf("[Test] PASSED: $0F2E order, $22DE, $0FBB, $0B35, $2835, $284D, $286E, $2890, $2897, $28B0, $28E1\n");
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
    AllStarAccuracyDebugState accuracy_computer;
    AllStarAccuracyDebugState accuracy_custom;

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
    allstar_scene_accuracy_get_debug_state(
        game.active_scene, &accuracy_computer);
    game.settings.accuracy_computer_positions = false;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    allstar_game_tick(&game, 0.0f);
    allstar_scene_accuracy_get_debug_state(game.active_scene, &accuracy_custom);
    if (accuracy_computer.phase == accuracy_custom.phase ||
        !accuracy_computer.marker_visible || !accuracy_custom.marker_visible) {
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

int allstar_cli_export_rom_sfx(const char *pack_path,
                               const char *score_path,
                               const char *squeak_path,
                               const char *dribble_path,
                               const char *navigation_path,
                               const char *confirm_path,
                               const char *rim_path,
                               const char *foul_path) {
    AllStarAssetPack pack;
    const AllStarRomSfxProgram *squeak;
    const AllStarRomSfxProgram *score;
    const AllStarRomSfxProgram *dribble;
    const AllStarRomSfxProgram *navigation;
    const AllStarRomSfxProgram *confirm;
    const AllStarRomSfxProgram *rim;
    const AllStarRomSfxProgram *foul;
    if (!allstar_asset_pack_load_file(&pack, pack_path)) return 1;
    if (pack.header.rom_sfx_program_count != ALLSTAR_ROM_SFX_PROGRAM_COUNT)
        return 1;
    squeak = &pack.rom_sfx_programs[0];
    score = &pack.rom_sfx_programs[1];
    dribble = &pack.rom_sfx_programs[2];
    navigation = &pack.rom_sfx_programs[3];
    confirm = &pack.rom_sfx_programs[4];
    rim = &pack.rom_sfx_programs[5];
    foul = &pack.rom_sfx_programs[6];
    if (!allstar_audio_export_rom_sfx_wav(&pack, 0x05, score_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x0d, squeak_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x0c, dribble_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x0f, navigation_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x0e, confirm_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x09, rim_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x04, foul_path)) {
        fprintf(stderr, "[ROM SFX] Failed to export decoded WAV proof\n");
        return 1;
    }
    printf("[ROM SFX] command $05 -> program $%02X, priority %u, "
           "streams $%04X/$%04X, %u frames, source FNV-1a %08X\n",
           score->program_id, score->priority_frames,
           score->stream_pointer_1, score->stream_pointer_2,
           score->frame_count, score->source_checksum);
    printf("[ROM SFX] command $0D -> program $%02X, priority %u, "
           "stream $%04X, %u frames, source FNV-1a %08X\n",
           squeak->program_id, squeak->priority_frames,
           squeak->stream_pointer_1, squeak->frame_count,
           squeak->source_checksum);
    printf("[ROM SFX] command $0C -> program $%02X, priority %u, "
           "stream $%04X, %u frames, source FNV-1a %08X\n",
           dribble->program_id, dribble->priority_frames,
           dribble->stream_pointer_1, dribble->frame_count,
           dribble->source_checksum);
    printf("[ROM SFX] command $0F -> program $%02X, priority %u, "
           "stream $%04X, %u frames, source FNV-1a %08X\n",
           navigation->program_id, navigation->priority_frames,
           navigation->stream_pointer_1, navigation->frame_count,
           navigation->source_checksum);
    printf("[ROM SFX] command $0E -> program $%02X, priority %u, "
           "stream $%04X, %u frames, source FNV-1a %08X\n",
           confirm->program_id, confirm->priority_frames,
           confirm->stream_pointer_1, confirm->frame_count,
           confirm->source_checksum);
    printf("[ROM SFX] command $09 -> program $%02X, priority %u, "
           "stream $%04X, %u frames, NR41/42/43/44=%02X/%02X/%02X/%02X, "
           "source FNV-1a %08X\n",
           rim->program_id, rim->priority_frames,
           rim->stream_pointer_1, rim->frame_count,
           rim->noise_length, rim->noise_envelope,
           rim->frames[0].noise_polynomial, rim->noise_control,
           rim->source_checksum);
    printf("[ROM SFX] command $04 -> program $%02X, priority %u, "
           "streams $%04X/$%04X, %u frames, first Hz $%04X/$%04X, "
           "source FNV-1a %08X\n",
           foul->program_id, foul->priority_frames,
           foul->stream_pointer_1, foul->stream_pointer_2,
           foul->frame_count, foul->frames[0].square1_frequency,
           foul->frames[0].square2_frequency, foul->source_checksum);
    printf("[ROM SFX] Exported %s, %s, %s, %s, %s, %s, and %s\n",
           score_path, squeak_path, dribble_path,
           navigation_path, confirm_path, rim_path, foul_path);
    return 0;
}

int allstar_cli_export_free_throw_sfx(const char *pack_path,
                                      const char *net_path,
                                      const char *contact_path) {
    AllStarAssetPack pack;
    const AllStarRomSfxProgram *net;
    const AllStarRomSfxProgram *contact;
    if (!allstar_asset_pack_load_file(&pack, pack_path) ||
        pack.header.rom_sfx_program_count != ALLSTAR_ROM_SFX_PROGRAM_COUNT)
        return 1;
    net = &pack.rom_sfx_programs[7];
    contact = &pack.rom_sfx_programs[8];
    if (!allstar_audio_export_rom_sfx_wav(&pack, 0x08, net_path) ||
        !allstar_audio_export_rom_sfx_wav(&pack, 0x0a, contact_path)) {
        fprintf(stderr, "[Free Throw SFX] Failed to export decoded WAV proof\n");
        return 1;
    }
    printf("[Free Throw SFX] $08 -> program $%02X priority %u stream $%04X, "
           "%u frames, NR41/42/43/44=%02X/%02X/%02X/%02X\n",
           net->program_id, net->priority_frames, net->stream_pointer_1,
           net->frame_count, net->noise_length, net->noise_envelope,
           net->frames[0].noise_polynomial, net->noise_control);
    printf("[Free Throw SFX] $0A -> program $%02X priority %u stream $%04X, "
           "%u frames, NR10/11/12=%02X/%02X/%02X\n",
           contact->program_id, contact->priority_frames,
           contact->stream_pointer_1, contact->frame_count,
           contact->square1_sweep, contact->square1_duty_length,
           contact->square1_envelope);
    printf("[Free Throw SFX] source FNV-1a %08X; exported %s and %s\n",
           net->source_checksum, net_path, contact_path);
    return 0;
}

int allstar_cli_export_horse_sfx(const char *pack_path,
                                 const char *letter_path) {
    AllStarAssetPack pack;
    const AllStarRomSfxProgram *letter;
    if (!allstar_asset_pack_load_file(&pack, pack_path) ||
        pack.header.rom_sfx_program_count != ALLSTAR_ROM_SFX_PROGRAM_COUNT)
        return 1;
    letter = &pack.rom_sfx_programs[9];
    if (!allstar_audio_export_rom_sfx_wav(&pack, 0x07, letter_path)) {
        fprintf(stderr, "[Horse SFX] Failed to export command $07 WAV proof\n");
        return 1;
    }
    printf("[Horse SFX] $07 -> program $%02X priority %u stream $%04X, "
           "%u frames, NR10/11/12=%02X/%02X/%02X, "
           "notes $%04X->$%04X, source FNV-1a %08X\n",
           letter->program_id, letter->priority_frames,
           letter->stream_pointer_1, letter->frame_count,
           letter->square1_sweep, letter->square1_duty_length,
           letter->square1_envelope,
           letter->frames[0].square1_frequency,
           letter->frames[6].square1_frequency,
           letter->source_checksum);
    printf("[Horse SFX] Exported %s\n", letter_path);
    return 0;
}

int allstar_cli_export_accuracy_sfx(const char *pack_path,
                                    const char *result_path) {
    AllStarAssetPack pack;
    const AllStarRomSfxProgram *result;
    if (!allstar_asset_pack_load_file(&pack, pack_path) ||
        pack.header.rom_sfx_program_count != ALLSTAR_ROM_SFX_PROGRAM_COUNT)
        return 1;
    result = &pack.rom_sfx_programs[10];
    if (!allstar_audio_export_rom_sfx_wav(&pack, 0x02, result_path)) {
        fprintf(stderr, "[Accuracy SFX] Failed to export command $02 WAV proof\n");
        return 1;
    }
    printf("[Accuracy SFX] $02 -> program $%02X priority %u "
           "streams $%04X/$%04X, %u frames, first NR13/23 $%04X/$%04X, "
           "source FNV-1a %08X\n",
           result->program_id, result->priority_frames,
           result->stream_pointer_1, result->stream_pointer_2,
           result->frame_count, result->frames[0].square1_frequency,
           result->frames[0].square2_frequency, result->source_checksum);
    printf("[Accuracy SFX] Exported %s\n", result_path);
    return 0;
}

int allstar_cli_test_accuracy(void) {
    AllStarAccuracyState mode;
    AllStarAccuracyHud hud;
    AllStarAccuracyDebugState debug;
    AllStarGame game;
    uint8_t x = 0x54, y = 0x80;
    int i;
    printf("[Test] Running ROM Accuracy Shootout Tests...\n");

    allstar_accuracy_init_0e51_6c9b(&mode, true);
    allstar_accuracy_next_position_6ca2(&mode, 0x00);
    if (mode.group != 0 || mode.position_index != 1 ||
        mode.target_x != 0x0c || mode.target_y != 0x94) {
        fprintf(stderr, "[Test] $6C9B/$6CA2 first group/position mismatch\n");
        return 1;
    }
    for (i = 1; i < 10; i++)
        allstar_accuracy_next_position_6ca2(&mode, 0x00);
    allstar_accuracy_next_position_6ca2(&mode, 0xba);
    if (mode.group != 4 || mode.position_index != 1 ||
        mode.target_x != 0xa0 || mode.target_y != 0x60) {
        fprintf(stderr, "[Test] $6CAB ten-position reselection mismatch\n");
        return 1;
    }
    allstar_accuracy_move_custom_cursor_6d57(
        ALLSTAR_BTN_LEFT | ALLSTAR_BTN_UP, &x, &y);
    if (x != 0x50 || y != 0x7c) {
        fprintf(stderr, "[Test] $6D57 four-pixel cursor movement mismatch\n");
        return 1;
    }
    allstar_accuracy_init_0e51_6c9b(&mode, false);
    for (i = 0; i < 10; i++)
        allstar_accuracy_record_custom_position_6d57(
            &mode, (uint8_t)(0x20 + i * 4), (uint8_t)(0x64 + i * 4));
    mode.position_index = 0;
    allstar_accuracy_next_position_6ca2(&mode, 0);
    if (mode.target_x != 0x20 || mode.target_y != 0x64 ||
        mode.custom_count != 10) {
        fprintf(stderr, "[Test] $6D57 custom table playback mismatch\n");
        return 1;
    }
    for (i = 0; i < 101; i++)
        allstar_accuracy_bcd_increment_0b20(mode.attempts_bcd);
    if (mode.attempts_bcd[0] != 0x01 || mode.attempts_bcd[1] != 0x01 ||
        allstar_accuracy_bcd_value(mode.attempts_bcd) != 101) {
        fprintf(stderr, "[Test] $0B20 packed-BCD increment mismatch\n");
        return 1;
    }
    allstar_accuracy_hud_76a7(119u * 60u, 2, &hud);
    if (strcmp(hud.left_timer, "01:59") != 0 ||
        strcmp(hud.right_timer, "00:00") != 0 ||
        strcmp(hud.left_points, "002") != 0 ||
        strcmp(hud.right_points, "000") != 0 ||
        ALLSTAR_ACCURACY_TIMER_LEFT_X != 1 ||
        ALLSTAR_ACCURACY_TIMER_RIGHT_X != 14 ||
        ALLSTAR_ACCURACY_TIMER_Y != 1 ||
        ALLSTAR_ACCURACY_POINTS_LEFT_X != 2 ||
        ALLSTAR_ACCURACY_POINTS_RIGHT_X != 15 ||
        ALLSTAR_ACCURACY_POINTS_Y != 3) {
        fprintf(stderr, "[Test] $76A7 Accuracy scoreboard layout mismatch\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) return 1;
    game.selected_mode = ALLSTAR_MODE_ACCURACY;
    game.settings.accuracy_computer_positions = true;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    if (!allstar_scene_accuracy_get_debug_state(game.active_scene, &debug) ||
        !debug.marker_visible || debug.attempts != 0 || debug.makes != 0 ||
        allstar_game_mode_requires_opponent(ALLSTAR_MODE_ACCURACY)) {
        fprintf(stderr, "[Test] $4000/$4034/$7AFD one-player approach mismatch\n");
        allstar_game_shutdown(&game); return 1;
    }
    allstar_scene_accuracy_snap_to_target(game.active_scene);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_accuracy_get_debug_state(game.active_scene, &debug);
    if (debug.attempts != 1 || !debug.ball_in_flight) {
        fprintf(stderr, "[Test] $0EE7 attempt/release path did not launch\n");
        allstar_game_shutdown(&game); return 1;
    }
    allstar_scene_accuracy_force_test_score_frame(game.active_scene, 20);
    allstar_game_tick(&game, 0.0f);
    allstar_scene_accuracy_force_test_result(game.active_scene);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (game.audio.last_sfx != ALLSTAR_SFX_ACCURACY_RESULT) {
        fprintf(stderr, "[Test] $0FDE command-$02 result dispatch mismatch\n");
        allstar_game_shutdown(&game); return 1;
    }
    for (i = 1; i < 240; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (!game.active_scene || game.active_scene->id != ALLSTAR_SCENE_INTRO) {
        fprintf(stderr, "[Test] $0FDE 240-frame result hold did not exit\n");
        allstar_game_shutdown(&game); return 1;
    }
    allstar_game_shutdown(&game);
    printf("[Test] PASSED: one-player route, 50 spots, marker, shot, BCD, $76A7 HUD, result cue\n");
    return 0;
}

int allstar_cli_test_horse(void) {
    AllStarHorseState mode;
    AllStarHorseDebugState debug;
    AllStarGame game;
    uint8_t x;
    uint8_t y;
    uint32_t events;
    int frame;
    printf("[Test] Running ROM H-O-R-S-E Rule/Scene Tests...\n");
    allstar_horse_init_0cdf(&mode);
    if (mode.current_player != 1 || mode.caller != 1 ||
        mode.letters_remaining[0] != 5 || mode.letters_remaining[1] != 5 ||
        strcmp(allstar_horse_letters_7bc0(5), "") != 0 ||
        strcmp(allstar_horse_letters_7bc0(0), "HORSE") != 0) {
        fprintf(stderr, "[Test] $0CDF/$22B9/$7BC0 initial state mismatch\n");
        return 1;
    }
    allstar_horse_cpu_spot_6cab(0x00, 0, &x, &y);
    if (x != 0x0c || y != 0x94) {
        fprintf(stderr, "[Test] $6CAB group-0 spot mismatch\n");
        return 1;
    }
    allstar_horse_cpu_spot_6cab(0xba, 9, &x, &y);
    if (x != 0x8c || y != 0x78) {
        fprintf(stderr, "[Test] $6CAB group-4 spot mismatch\n");
        return 1;
    }
    events = allstar_horse_resolve_shot_0d57(&mode, true, 117.0f, 136.0f);
    if ((events & ALLSTAR_HORSE_EVENT_CALLED_MAKE) == 0 ||
        mode.current_player != 2 || mode.caller != 1 ||
        mode.saved_x != 0x74 || mode.saved_y != 0x88) {
        fprintf(stderr, "[Test] $0D57 caller make/save mismatch\n");
        return 1;
    }
    events = allstar_horse_resolve_shot_0d57(&mode, false, 116.0f, 136.0f);
    if ((events & ALLSTAR_HORSE_EVENT_LETTER) == 0 ||
        mode.letters_remaining[1] != 4 || mode.current_player != 1 ||
        strcmp(allstar_horse_letters_7bc0(4), "H") != 0) {
        fprintf(stderr, "[Test] $0E26 matcher letter mismatch\n");
        return 1;
    }
    events = allstar_horse_resolve_shot_0d57(&mode, false, 88.0f, 152.0f);
    if ((events & ALLSTAR_HORSE_EVENT_CALLER_CHANGED) == 0 ||
        mode.caller != 2 || mode.current_player != 2 ||
        mode.saved_y != 0x94) {
        fprintf(stderr, "[Test] $0D57 caller miss/edge alignment mismatch\n");
        return 1;
    }
    mode.current_player = 1;
    mode.caller = 2;
    mode.called_shot_made = true;
    mode.letters_remaining[0] = 1;
    events = allstar_horse_resolve_shot_0d57(&mode, false, 88.0f, 96.0f);
    if ((events & ALLSTAR_HORSE_EVENT_COMPLETE) == 0 || !mode.complete ||
        mode.winner != 2 || mode.letters_remaining[0] != 0) {
        fprintf(stderr, "[Test] $0E26 HORSE completion/winner mismatch\n");
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) return 1;
    game.selected_mode = ALLSTAR_MODE_HORSE;
    game.selected_player_1 = 0;
    game.selected_player_2 = 1;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_HORSE);
    if (!allstar_scene_horse_get_debug_state(game.active_scene, &debug) ||
        debug.current_player != 1 || debug.caller != 1 ||
        debug.p1_letters_remaining != 5 || debug.p2_letters_remaining != 5) {
        fprintf(stderr, "[Test] Horse scene did not initialize mode-2 state\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    if (!allstar_scene_horse_force_test_result(game.active_scene, true)) {
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    for (frame = 0; frame < 192; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (!allstar_scene_horse_get_debug_state(game.active_scene, &debug) ||
        debug.current_player != 2 || debug.caller != 1 ||
        !debug.marker_visible) {
        fprintf(stderr, "[Test] $7AFD matching turn/X marker did not begin\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_scene_horse_force_test_result(game.active_scene, false);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (!allstar_scene_horse_get_debug_state(game.active_scene, &debug) ||
        debug.p2_letters_remaining != 4 ||
        (debug.last_events & ALLSTAR_HORSE_EVENT_LETTER) == 0 ||
        game.audio.last_sfx != ALLSTAR_SFX_HORSE_LETTER) {
        fprintf(stderr, "[Test] Horse matcher miss did not award H/command $07\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_shutdown(&game);
    printf("[Test] PASSED: $0CDF/$0D57/$0E26/$0E36/$6CAB/$7AFD/$7BC0\n");
    return 0;
}

int allstar_cli_test_one_on_one_presentation(void) {
    AllStarGame game;
    AllStarRomInboundPlacement inbound;
    AllStarOneOnOneDebugState debug;
    bool dribble_heard = false;
    bool cpu_reached_route = false;
    bool cpu_released = false;
    bool defender_recovered = false;
    float defender_start_x;
    float rebound_catch_x;
    int frame;
    printf("[Test] Running One-on-One Presentation/Audio Integration Tests...\n");
    allstar_one_on_one_rom_inbound_placement_20f7(1, &inbound);
    if (inbound.p1_center_x != 84.0f || inbound.p1_ground_y != 152.0f ||
        inbound.p2_center_x != 84.0f || inbound.p2_ground_y != 136.0f) {
        fprintf(stderr, "[Test] $20F7 P1 take-out placement mismatch\n");
        return 1;
    }
    allstar_one_on_one_rom_inbound_placement_20f7(2, &inbound);
    if (inbound.p1_center_x != 84.0f || inbound.p1_ground_y != 136.0f ||
        inbound.p2_center_x != 84.0f || inbound.p2_ground_y != 152.0f) {
        fprintf(stderr, "[Test] $20F7 P2 take-out placement mismatch\n");
        return 1;
    }
    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Failed initializing presentation/audio game\n");
        return 1;
    }

    game.selected_mode = ALLSTAR_MODE_ONE_ON_ONE;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (game.audio.last_sfx != ALLSTAR_SFX_SHOE_SQUEAK) {
        fprintf(stderr, "[Test] $78DD command-$0D shoe squeak was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    for (frame = 0; frame < 72; frame++) {
        game.audio.last_sfx = ALLSTAR_SFX_NONE;
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (game.audio.last_sfx == ALLSTAR_SFX_DRIBBLE) {
            dribble_heard = true;
            break;
        }
    }
    if (!dribble_heard) {
        fprintf(stderr, "[Test] $6FE5 command-$0C dribble cadence was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* $702D held-B arms $C16A; the next update reaches phase two and
       $6A8C:$6B34 forces extracted display frame $13 for the dunk/drop. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 104.0f, 132.0f, 136.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_B);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (debug.shot_phase != 2 || debug.p1_display_frame != 0x13 ||
        !debug.ball_in_flight || debug.p1_has_ball) {
        fprintf(stderr,
                "[Test] $702D/$C16A/$6B34 dunk pose did not survive "
                "$6A8C (phase=%u display=$%02X flight=%d)\n",
                debug.shot_phase, debug.p1_display_frame,
                debug.ball_in_flight ? 1 : 0);
        allstar_game_shutdown(&game);
        return 1;
    }

    /* $7C58 latches an uncleared changed possession in $C178; the next
       $2C50 update must show $067C and restart opposite the offender. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 112.0f, 132.0f, 136.0f);
    allstar_scene_one_on_one_set_test_take_back_required(
        game.active_scene, true);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (!debug.foul_presentation_active ||
        debug.foul_violation != ALLSTAR_ROM_CONTACT_DIDNT_CLEAR ||
        game.audio.last_sfx != ALLSTAR_SFX_WHISTLE) {
        fprintf(stderr,
                "[Test] $7C58->$C178->$2C50 take-back violation was absent\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    for (frame = 0; frame < ALLSTAR_ROM_FOUL_RESET_FRAME; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (!debug.p2_has_ball || debug.p1_has_ball) {
        fprintf(stderr,
                "[Test] DIDN'T CLEAR BALL did not turn possession over\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* Scene-level bank-1 $72BF->$72EA->$732C->$755D->$756C proof. */
    if (!allstar_scene_one_on_one_set_test_possession(
            game.active_scene, &game, 2)) {
        fprintf(stderr, "[Test] Could not seed CPU possession\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 1200; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (!allstar_scene_one_on_one_get_debug_state(
                game.active_scene, &debug)) break;
        if (debug.cpu_offense_stage >= 2) cpu_reached_route = true;
        if (debug.ball_in_flight && !debug.p2_has_ball) {
            cpu_released = true;
            break;
        }
    }
    if (!cpu_reached_route || !cpu_released) {
        fprintf(stderr,
                "[Test] CPU stalled in $72EA/$732C/$756C path "
                "(frame=%d state=%u stage=%u target=%u,%u p2=%.0f,%.0f "
                "action=$%02X record=%u)\n",
                frame, debug.cpu_state, debug.cpu_offense_stage,
                debug.cpu_target_x, debug.cpu_target_y,
                debug.p2_x, debug.p2_y,
                debug.p2_action, debug.p2_record);
        allstar_game_shutdown(&game);
        return 1;
    }
    printf("  CPU route released at scene frame %d after reaching stage 2\n",
           frame);

    /* $70FD leaves the direction sampled at the jump edge in player +$07.
       Protected action $05 bypasses $702D's normal direction refresh, while
       $6BF9->$6B72 continues moving on each jump-record boundary. $2B88 must
       also preserve that direction when the airborne defender rebounds. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(
        game.active_scene, &game, 2);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 20.0f, 128.0f, 132.0f, 128.0f);
    allstar_input_update(
        &game.input, ALLSTAR_BTN_A | ALLSTAR_BTN_RIGHT);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 18; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (!debug.p1_defense_jump_active || debug.p1_x <= 20.0f) {
        fprintf(stderr,
                "[Test] $70FD/$6BF9 block jump discarded latched movement "
                "(action=$%02X record=%u active=%d x=20->%.0f)\n",
                debug.p1_action, debug.p1_record,
                debug.p1_defense_jump_active ? 1 : 0, debug.p1_x);
        allstar_game_shutdown(&game);
        return 1;
    }
    if (!allstar_scene_one_on_one_take_test_live_possession(
            game.active_scene, &game, 1)) {
        fprintf(stderr, "[Test] Could not seed $2B88 airborne rebound\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    rebound_catch_x = debug.p1_x;
    for (frame = 0; frame < 12; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (!debug.p1_has_ball || !debug.p1_defense_jump_active ||
        debug.p1_x <= rebound_catch_x) {
        fprintf(stderr,
                "[Test] $2B88 rebound erased $6BF9/$6B72 jump movement "
                "(action=$%02X active=%d owner=%d x=%.0f->%.0f)\n",
                debug.p1_action,
                debug.p1_defense_jump_active ? 1 : 0,
                debug.p1_has_ball ? 1 : 0,
                rebound_catch_x, debug.p1_x);
        allstar_game_shutdown(&game);
        return 1;
    }
    for (frame = 0; frame < ALLSTAR_ROM_DEFENSE_JUMP_FRAMES; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    defender_start_x = debug.p1_x;
    /* The exact $7170 defender is holding the right-hand contact lane here;
       move away from that legal block to prove the landed player controller
       has re-entered normal movement. */
    allstar_input_update(&game.input, ALLSTAR_BTN_LEFT);
    for (frame = 0; frame < 24; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    defender_recovered = !debug.p1_defense_jump_active &&
        debug.p1_action != 0x05 && debug.p1_action != 0x0c &&
        debug.p1_action != 0x14 && debug.p1_x < defender_start_x;
    if (!defender_recovered) {
        fprintf(stderr,
                "[Test] Defender froze after $70FD/$6A8C block jump "
                "(action=$%02X record=%u active=%d x=%.0f->%.0f)\n",
                debug.p1_action, debug.p1_record,
                debug.p1_defense_jump_active ? 1 : 0,
                defender_start_x, debug.p1_x);
        allstar_game_shutdown(&game);
        return 1;
    }
    printf("  Defender recovered $05->$06 and moved %.0f->%.0f\n",
           defender_start_x, debug.p1_x);

    /* Fixed $2B14->$2B88 uses the live $6F2A ball point and each player's
       stored +$10 direction. A transfer changes owner in place and never
       enters the score/foul presentation machinery. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(
        game.active_scene, &game, 1);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 152.0f, 90.0f, 152.0f);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 1, 0x13, 0, ALLSTAR_BTN_LEFT, true);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 2, 0x0d, 0, ALLSTAR_BTN_RIGHT, false);
    if (!allstar_scene_one_on_one_try_test_steal(
            game.active_scene, &game, 2) ||
        !allstar_scene_one_on_one_get_debug_state(
            game.active_scene, &debug) ||
        !debug.p2_has_ball || debug.p1_has_ball ||
        debug.p1_x != 84.0f || debug.p2_x != 90.0f ||
        debug.score_presentation_active || debug.foul_presentation_active ||
        debug.steal_transfer_events != 1) {
        fprintf(stderr,
                "[Test] $2B14/$2B88 CPU steal did not continue live in place\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(
        game.active_scene, &game, 1);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 152.0f, 90.0f, 152.0f);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 1, 0x13, 0, ALLSTAR_BTN_LEFT, true);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 2, 0x0d, 0, ALLSTAR_BTN_LEFT, false);
    if (allstar_scene_one_on_one_try_test_steal(
            game.active_scene, &game, 2)) {
        fprintf(stderr,
                "[Test] $2B14 allowed same-direction left-movement steal\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* $05A3->$0C49: command $04, 120-frame message, fade/restart, resume. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    if (!allstar_scene_one_on_one_begin_test_foul(
            game.active_scene, &game, ALLSTAR_ROM_CONTACT_CHARGING, 1) ||
        game.audio.last_sfx != ALLSTAR_SFX_WHISTLE) {
        fprintf(stderr, "[Test] $05A3 command-$04 foul cue was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < ALLSTAR_ROM_FOUL_RESET_FRAME; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (!debug.foul_presentation_active || debug.foul_message_visible ||
        !debug.p2_has_ball || debug.p1_has_ball || debug.foul_events != 1) {
        fprintf(stderr,
                "[Test] $0C49 charging restart did not award opposite player\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    for (; frame < ALLSTAR_ROM_FOUL_COMPLETE_FRAME; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (debug.foul_presentation_active) {
        fprintf(stderr, "[Test] $0C49 foul presentation did not resume play\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* Exact $1F5F rim cell: the scene emits command $09 but preserves the
       $FFF8-equivalent first-flight lock until $1E5B/$1E77 ground bounce. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    if (!allstar_scene_one_on_one_set_test_ball_rom(
            game.active_scene, 0x5300, 0x5e00, 0x370f,
            0, 0, 0, 1)) {
        fprintf(stderr, "[Test] Could not seed exact rim-audio fixture\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (debug.rim_audio_events != 1 || debug.ball_recoverable) {
        fprintf(stderr,
                "[Test] $1F5F rim contact did not dispatch command-$09 "
                "or incorrectly changed first flight "
                "(events=%u last_sfx=%d recoverable=%d)\n",
                (unsigned)debug.rim_audio_events,
                (int)game.audio.last_sfx,
                debug.ball_recoverable ? 1 : 0);
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ROSTER_SELECT);
    allstar_game_tick(&game, 0.8f);
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT);
    allstar_game_tick(&game, 0.0f);
    if (game.audio.last_sfx != ALLSTAR_SFX_MENU_MOVE) {
        fprintf(stderr, "[Test] bank-2 command-$0F roster navigation was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    if (game.audio.last_sfx != ALLSTAR_SFX_MENU_SELECT) {
        fprintf(stderr, "[Test] bank-2 command-$0E roster confirm was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.0f);
    allstar_game_tick(&game, 0.8f);
    game.audio.last_sfx = ALLSTAR_SFX_NONE;
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    if (game.audio.last_sfx != ALLSTAR_SFX_MENU_SELECT) {
        fprintf(stderr, "[Test] second command-$0E roster confirm was not dispatched\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < 34; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        if (game.active_scene->id != ALLSTAR_SCENE_ROSTER_SELECT) {
            fprintf(stderr, "[Test] command-$0E matchup carry ended before 35 frames\n");
            allstar_game_shutdown(&game);
            return 1;
        }
    }
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (game.active_scene->id != ALLSTAR_SCENE_ONE_ON_ONE ||
        game.audio.last_sfx != ALLSTAR_SFX_MENU_SELECT) {
        fprintf(stderr, "[Test] command-$0E did not carry into the first gameplay update\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    allstar_game_shutdown(&game);
    printf("[Test] PASSED: CPU release, defender recovery, $09 rim, "
           "$0C/$0D gameplay, and $0F/$0E roster cues\n");
    return 0;
}

int allstar_cli_test_free_throw(void) {
    AllStarFreeThrowState state;
    AllStarFreeThrowDebugState debug;
    AllStarGame game;
    uint32_t events;
    int16_t make_vz = 0;
    int frame;
    int make_frame = -1, net_frame = -1, score_frame = -1;
    int attempt;
    int profile_index;
    static const int expected_clean_cells[3] = { 21, 15, 6 };

    printf("[Test] ROM Free Throw mode ($0C8E/$100F)...\n");
    allstar_free_throw_init(&state, 5, 0x00, 0);
    if (state.aim_x != 0x5b00 || state.aim_y != 0x3800 ||
        state.aim_vx != 0x0045 || state.aim_vy != 0x0040) {
        fprintf(stderr, "[Test] FAILED: $18E7 RNG row 0 mismatch\n");
        return 1;
    }
    allstar_free_throw_tick_100f(&state, 0, 0, 0);
    if (state.oam_y != 0x56 || state.oam_x != 0x4f ||
        state.oam_band != 2 || state.oam_priority_rows != 0x0e) {
        fprintf(stderr, "[Test] FAILED: $1C1D/$1884 initial OAM state mismatch\n");
        return 1;
    }
    allstar_free_throw_aim_init_18e7(&state, 0x40);
    if (state.aim_x != 0x4700 || state.aim_y != 0x3400 ||
        state.aim_vx != 0x0050 || state.aim_vy != (int16_t)0xffb0) {
        fprintf(stderr, "[Test] FAILED: $18E7 RNG row 1 mismatch\n");
        return 1;
    }
    allstar_free_throw_aim_init_18e7(&state, 0x90);
    if (state.aim_x != 0x5600 || state.aim_y != 0x4300) {
        fprintf(stderr, "[Test] FAILED: $18E7 RNG row 2 mismatch\n");
        return 1;
    }
    allstar_free_throw_aim_init_18e7(&state, 0xe0);
    if (state.aim_x != 0x4900 || state.aim_y != 0x3300) {
        fprintf(stderr, "[Test] FAILED: $18E7 RNG row 3 mismatch\n");
        return 1;
    }

    state.aim_x = 0x5000;
    state.aim_y = 0x3f00;
    state.aim_vx = 0;
    state.aim_vy = 0;
    state.aim_timer = 2;
    allstar_free_throw_aim_input_1942(
        &state, ALLSTAR_BTN_RIGHT | ALLSTAR_BTN_DOWN);
    allstar_free_throw_aim_step_1986(&state);
    if (state.aim_vx != 5 || state.aim_vy != 5 ||
        state.aim_x != 0x5005 || state.aim_y != 0x3f05) {
        fprintf(stderr, "[Test] FAILED: $1942/$1986 aim integration mismatch\n");
        return 1;
    }

    allstar_free_throw_init(&state, 5, 0x00, 0);
    allstar_free_throw_set_test_aim(&state, 0x52, 0x3c);
    if (!allstar_free_throw_launch_1caa_7c58(&state, 0xff) ||
        state.ball.vx != -36 || state.ball.vy != -160 ||
        state.ball.vz != 484 || state.attempts_remaining != 4) {
        fprintf(stderr,
            "[Test] FAILED: $1CAA/$7C58 center vector got (%d,%d,%d)\n",
            state.ball.vx, state.ball.vy, state.ball.vz);
        return 1;
    }
    allstar_free_throw_init(&state, 5, 0, 0);
    allstar_free_throw_set_test_aim(&state, 0x48, 0x36);
    if (!allstar_free_throw_launch_1caa_7c58(&state, 0x00) ||
        state.ball.vx != -36 || state.ball.vy != -160 ||
        state.ball.vz != 484) {
        fprintf(stderr, "[Test] FAILED: $1CAA 19/256 assist did not snap\n");
        return 1;
    }
    allstar_free_throw_init(&state, 5, 0, 0);
    allstar_free_throw_set_test_aim(&state, 0x52, 0x3c);
    allstar_free_throw_launch_1caa_7c58(&state, 0xff);
    for (frame = 1; frame <= ALLSTAR_FREE_THROW_PRESENTATION_FRAMES; frame++) {
        events = allstar_free_throw_tick_100f(&state, 0, 0, 0xff);
        if ((events & ALLSTAR_FREE_THROW_EVENT_MAKE) && make_frame < 0) {
            make_frame = frame;
            make_vz = state.ball.vz;
            if (state.priority_timer != 0x2d) {
                fprintf(stderr, "[Test] FAILED: $1E49 priority timer mismatch\n");
                return 1;
            }
        } else if (make_frame > 0 && frame == make_frame + 1 &&
                   (state.oam_priority_rows != 0x0f ||
                    state.priority_timer != 0x2c ||
                    state.ball.vz != make_vz)) {
            fprintf(stderr,
                "[Test] FAILED: $1C1D/$7BE8 made-ball priority/gravity hold mismatch\n");
            return 1;
        }
        if ((events & ALLSTAR_FREE_THROW_EVENT_NET) && net_frame < 0)
            net_frame = frame;
        if ((events & ALLSTAR_FREE_THROW_EVENT_SCORE) && score_frame < 0)
            score_frame = frame;
    }
    printf("  traced center shot: make=%d net=$08@%d score=$05@%d next=%u\n",
           make_frame, net_frame, score_frame,
           (unsigned)state.attempts_remaining);
    if (state.phase != ALLSTAR_FREE_THROW_AIMING ||
        state.attempts_remaining != 4 || state.makes != 1 ||
        make_frame < 0 || net_frame - make_frame != 27 ||
        score_frame - make_frame != 71) {
        fprintf(stderr,
            "[Test] FAILED: $17E2/$1A31/$1C61 shot lifecycle mismatch\n");
        return 1;
    }

    /* $1A7E->$1AA6 checks all three X columns from $1AAD against the
       rating-selected $1AB0/$1AB8/$1ABE Y list before rim ricochets. */
    for (profile_index = 0; profile_index < 3; profile_index++) {
        int clean_cells = 0;
        int target_y;
        int target_x;
        for (target_y = 0x36; target_y <= 0x43; target_y++) {
            for (target_x = 0x48; target_x <= 0x57; target_x++) {
                if (allstar_free_throw_clean_make_window_1a7e(
                        (uint8_t)target_x, (uint8_t)target_y,
                        (uint8_t)profile_index)) clean_cells++;
            }
        }
        printf("  profile %d clean make cells: %d / 224\n",
               profile_index, clean_cells);
        if (clean_cells != expected_clean_cells[profile_index]) {
            fprintf(stderr,
                "[Test] FAILED: $1AAD profile %d expected %d clean cells, got %d\n",
                profile_index, expected_clean_cells[profile_index],
                clean_cells);
            return 1;
        }
    }
    allstar_free_throw_init(&state, 5, 0, 0x00);
    allstar_free_throw_set_test_aim(&state, 0x50, 0x3b);
    allstar_free_throw_launch_1caa_7c58(&state, 0xff);
    make_frame = -1;
    for (frame = 1; frame <= 100; frame++) {
        events = allstar_free_throw_tick_100f(&state, 0, 0, 0xff);
        if (events & ALLSTAR_FREE_THROW_EVENT_MAKE) {
            make_frame = frame;
            break;
        }
    }
    if (make_frame < 0 || !state.made_current || state.priority_timer != 0x2d) {
        fprintf(stderr,
            "[Test] FAILED: $1A7E profile-2 clean target did not make\n");
        return 1;
    }
    printf("  profile 2 clean target $50/$3B made at release +%d\n",
           make_frame);

    allstar_free_throw_init(&state, 5, 0, 0);
    allstar_free_throw_set_test_aim(&state, 0x39, 0x28);
    if (!allstar_free_throw_launch_1caa_7c58(&state, 0xff)) return 1;
    for (frame = 0; frame < ALLSTAR_FREE_THROW_PRESENTATION_FRAMES; frame++)
        allstar_free_throw_tick_100f(&state, 0, 0, 0xff);
    if (state.makes != 0 || state.phase != ALLSTAR_FREE_THROW_AIMING) {
        fprintf(stderr, "[Test] FAILED: off-target Free Throw did not miss\n");
        return 1;
    }

    allstar_free_throw_init(&state, 5, 0, 0);

    for (attempt = 0; attempt < 5; ++attempt) {
        allstar_free_throw_set_test_aim(&state, 0x52, 0x3c);
        if (!allstar_free_throw_launch_1caa_7c58(&state, 0xff)) return 1;
        for (frame = 0; frame < ALLSTAR_FREE_THROW_PRESENTATION_FRAMES; frame++)
            allstar_free_throw_tick_100f(&state, 0, 0, 0xff);
    }
    if (state.phase != ALLSTAR_FREE_THROW_RESULT ||
        state.attempts_taken != 5 || state.attempts_remaining != 0 ||
        state.makes != 5) {
        fprintf(stderr,
            "[Test] FAILED: $0C8E five-attempt result got phase=%u tries=%u left=%u makes=%u\n",
            (unsigned)state.phase, (unsigned)state.attempts_taken,
            (unsigned)state.attempts_remaining, (unsigned)state.makes);
        return 1;
    }

    if (!allstar_game_init(&game, NULL)) return 1;
    /* Bank 2 $4000:$4014-$401D sends mode $01 directly through the
       single-player $4034 selector, and $0C8E clears music command $DD73. */
    game.selected_mode = ALLSTAR_MODE_FREE_THROW;
    allstar_audio_play_bgm(&game.audio, ALLSTAR_BGM_TITLE);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ROSTER_SELECT);
    allstar_game_tick(&game, 0.8f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    if (!game.active_scene ||
        game.active_scene->id != ALLSTAR_SCENE_FREE_THROW ||
        game.audio.current_bgm != ALLSTAR_BGM_NONE ||
        game.audio.last_sfx != ALLSTAR_SFX_MENU_SELECT) {
        fprintf(stderr,
            "[Test] FAILED: mode-$01 selector did not bypass VS and clear $DD73\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, 0);
    game.settings.free_throw_attempts = 5;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    for (attempt = 0; attempt < 5; attempt++) {
        allstar_scene_free_throw_set_test_aim(
            game.active_scene, 0x52, 0x3c);
        allstar_input_update(&game.input, ALLSTAR_BTN_A);
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        allstar_input_update(&game.input, 0);
        for (frame = 0; frame < ALLSTAR_FREE_THROW_PRESENTATION_FRAMES;
             frame++)
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    if (!allstar_scene_free_throw_get_debug_state(game.active_scene, &debug) ||
        debug.phase != ALLSTAR_FREE_THROW_RESULT || debug.makes != 5 ||
        debug.attempts_remaining != 0) {
        fprintf(stderr,
            "[Test] FAILED: native Free Throw scene did not reach 5/5 results\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    if (!game.active_scene || game.active_scene->id != ALLSTAR_SCENE_INTRO) {
        fprintf(stderr,
            "[Test] FAILED: Free Throw result did not return to intro\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_shutdown(&game);
    printf("[Test] PASSED: Free Throw single selector, reticle state, music stop, "
           "aim, launch, rim/net score, five attempts, result\n");
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

    /* 3a/3b. Mode-$01 takes bank-2 $4018->$4034: one player card, then
       immediate Free Throw entry with no opponent or VS presentation. */
    game.selected_mode = ALLSTAR_MODE_FREE_THROW;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ROSTER_SELECT);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, 0.8f);
    snprintf(path, sizeof(path), "%s\\03a_free_throw_roster.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\03b_free_throw_no_vs.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4d. Held-ball movement exposes the final $6F2A dribble placement. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 48.0f, 144.0f, 120.0f, 112.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT);
    for (int i = 0; i < 8; i++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04d_one_on_one_dribble.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4e. Let $782E cross the next record boundary into the right-facing
       held-ball idle family, making the shared $2945/$6F2A side bit clear. */
    for (int i = 0; i < 12; i++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path), "%s\\04e_one_on_one_idle_dribble.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4a. One-on-One gather: one native A edge begins the held-ball jump. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04a_one_on_one_gather.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* The ROM preserves direction in +$07 while $714D holds action $0A/$12;
       capture the later $6A8C/$6B72 record after moving through the gather. */
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT);
    for (int i = 0; i < 24; i++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path),
             "%s\\04aa_one_on_one_moving_gather.bmp", out_dir);
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

    /* 4e-4i. Complete traced make: profile-zero player, record-$05 release,
       $1F23 score cue, $27C7 fade, and $27CC inbound restoration. */
    game.selected_player_1 = 2;
    srand(1);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 25; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04e_one_on_one_shot_lift.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, 0.0f);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 20; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04f_one_on_one_make_flight.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    for (int i = 20; i < 98; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04g_one_on_one_score_cue.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    for (int i = 98; i < 160; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04h_one_on_one_fade.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    for (int i = 160; i < 196; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\04i_one_on_one_inbound.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4j-4l. Fixed $1ECC net frames use the extracted $793F tile stream. */
    allstar_renderer_clear(game.renderer, 0);
    allstar_renderer_draw_court_net_1ecc(
        game.renderer, ALLSTAR_ROM_NET_BEND);
    allstar_renderer_draw_text(game.renderer, "NET BEND +20", 8, 128, 3);
    snprintf(path, sizeof(path), "%s\\04j_one_on_one_net_bend.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    allstar_renderer_clear(game.renderer, 0);
    allstar_renderer_draw_court_net_1ecc(
        game.renderer, ALLSTAR_ROM_NET_DEEP);
    allstar_renderer_draw_text(game.renderer, "NET DEEP +35", 8, 128, 3);
    snprintf(path, sizeof(path), "%s\\04k_one_on_one_net_deep.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    allstar_renderer_clear(game.renderer, 0);
    allstar_renderer_draw_court_net_1ecc(
        game.renderer, ALLSTAR_ROM_NET_REST);
    allstar_renderer_draw_text(game.renderer, "NET REST +65", 8, 128, 3);
    snprintf(path, sizeof(path), "%s\\04l_one_on_one_net_rest.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4m-4n. Scene-level proof of the corrected $74BB dead zone and the
       complete $72EA->$732C->$755D->$756C CPU route/release. */
    {
        AllStarOneOnOneDebugState debug;
        allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
        allstar_scene_one_on_one_set_test_possession(
            game.active_scene, &game, 2);
        allstar_scene_one_on_one_set_test_positions(
            game.active_scene, 140.0f, 128.0f, 84.0f, 152.0f);
        allstar_input_update(&game.input, 0);
        for (int i = 0; i < 900; i++) {
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
            allstar_scene_one_on_one_get_debug_state(
                game.active_scene, &debug);
            if (debug.cpu_offense_stage >= 2) break;
        }
        snprintf(path, sizeof(path),
                 "%s\\04m_one_on_one_cpu_route_grounded.bmp", out_dir);
        save_bmp_file(path, game.renderer->pixels,
                      ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
        for (int i = 0; i < 900; i++) {
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
            allstar_scene_one_on_one_get_debug_state(
                game.active_scene, &debug);
            if (debug.ball_in_flight && !debug.p2_has_ball) break;
        }
        snprintf(path, sizeof(path),
                 "%s\\04n_one_on_one_cpu_release.bmp", out_dir);
        save_bmp_file(path, game.renderer->pixels,
                      ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    }

    /* 4o-4p. $70FD jump completes through action $05->$06, then movement
       resumes; both frames also expose the corrected player-foot baseline. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(
        game.active_scene, &game, 2);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 20.0f, 128.0f, 132.0f, 128.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 30; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04o_one_on_one_defender_block.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    for (int i = 30; i < ALLSTAR_ROM_DEFENSE_JUMP_FRAMES + 18; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT);
    for (int i = 0; i < 24; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04p_one_on_one_defender_recovered.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4q. Exact $53/$5E/$37 miss cell after $1F5F installs VX=$0046,
       cooldown 8, preserves the recovery lock, and dispatches command $09. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_ball_rom(
        game.active_scene, 0x5300, 0x5e00, 0x370f, 0, 0, 0, 1);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04q_one_on_one_rim_bounce.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4r. $2DD2 roster records feed $21FA's exact P1/P2 OBJ palettes;
       gameplay body tiles remain the shared $2945 animation families. */
    game.selected_player_1 = 0; /* Ainge: roster skin byte $91. */
    game.selected_player_2 = 1; /* Barkley: roster skin byte $90. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 48.0f, 144.0f, 116.0f, 136.0f);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path),
             "%s\\04r_one_on_one_roster_palettes.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4s-4t. $7138 forces either sideline gather to face the center hoop. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 20.0f, 144.0f, 132.0f, 112.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04s_one_on_one_left_shot_faces_hoop.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 140.0f, 144.0f, 28.0f, 112.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04t_one_on_one_right_shot_faces_hoop.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4u. $2B14->$2B88 transfers possession in place, with no score fade. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(
        game.active_scene, &game, 1);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 152.0f, 90.0f, 152.0f);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 1, 0x13, 0, ALLSTAR_BTN_LEFT, true);
    allstar_scene_one_on_one_set_test_player_state(
        game.active_scene, 2, 0x0d, 0, ALLSTAR_BTN_RIGHT, false);
    allstar_scene_one_on_one_try_test_steal(game.active_scene, &game, 2);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path),
             "%s\\04u_one_on_one_cpu_steal_live.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4v-4w. $05A3 uses explicit CHARGING/BLOCKING popups before $0C49. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_begin_test_foul(
        game.active_scene, &game, ALLSTAR_ROM_CONTACT_CHARGING, 1);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path),
             "%s\\04v_one_on_one_charging_foul.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_begin_test_foul(
        game.active_scene, &game, ALLSTAR_ROM_CONTACT_BLOCKING, 2);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path),
             "%s\\04w_one_on_one_blocking_foul.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4x. $702D A-then-B reaches phase two; $6A8C:$6B34 uses extracted
       display frame $13 and $7F0A drops the ball vertically for the dunk. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 84.0f, 104.0f, 132.0f, 136.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_B);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    /* The cartridge keeps the prior display for the release update; the
       next $6A8C record load applies phase-two display $13. */
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04x_one_on_one_dunk_phase2.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4xa. At score +20, $C12B is still nonzero and $6945 gives the ball
       OBJ priority bit 7. Capture it behind the first bent-net BG frame. */
    {
        AllStarOneOnOneDebugState score_debug;
        game.selected_player_1 = 2;
        srand(1);
        allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
        allstar_input_update(&game.input, ALLSTAR_BTN_A);
        allstar_game_tick(&game, 0.0f);
        allstar_input_update(&game.input, 0);
        for (int i = 0; i < 25; i++)
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        allstar_input_update(&game.input, ALLSTAR_BTN_A);
        allstar_game_tick(&game, 0.0f);
        allstar_input_update(&game.input, 0);
        for (int i = 0; i < 100; i++) {
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
            allstar_scene_one_on_one_get_debug_state(
                game.active_scene, &score_debug);
            if (score_debug.score_presentation_active) break;
        }
        for (int i = 0; i < 7; i++)
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        snprintf(path, sizeof(path),
                 "%s\\04xa_one_on_one_score_ball_behind_net.bmp", out_dir);
        save_bmp_file(path, game.renderer->pixels,
                      ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    }

    /* 4y. Both $7FC7 and $7FCB held tables are {+7,-2}/{+10,-2}; this
       captures the corrected action-$12 left-side shot ball attachment. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 20.0f, 144.0f, 132.0f, 112.0f);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path),
             "%s\\04y_one_on_one_left_shot_ball.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 4z. $7C58->$C178->$2C50 selects text pointer $067C. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_begin_test_foul(
        game.active_scene, &game, ALLSTAR_ROM_CONTACT_DIDNT_CLEAR, 1);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path),
             "%s\\04z_one_on_one_didnt_clear_ball.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 5a. Menu path to ROM selector $FF8F=$03 (Accuracy Shootout). */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_MENU);
    for (int i = 0; i < 3; i++) {
        allstar_input_update(&game.input, ALLSTAR_BTN_DOWN);
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        allstar_input_update(&game.input, 0);
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path), "%s\\05a_accuracy_menu.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 5b. $22EF mode-3 settings: complementary position source + timer. */
    game.selected_mode = ALLSTAR_MODE_ACCURACY;
    game.settings.accuracy_computer_positions = true;
    allstar_game_change_scene(&game, ALLSTAR_SCENE_SETTINGS);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\05b_accuracy_settings.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 5c. Bank 2 $4000->$4034 selects P1 only: no opponent and no VS. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ROSTER_SELECT);
    for (int i = 0; i < 50; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\05c_accuracy_player_select.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 5d-5g. $0E51->$6C9B->$6CA2->$7AFD gameplay, shared shot/net,
       and $0FDE TIME'S UP result. */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_THREE_POINT);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\05d_accuracy_target_marker.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_accuracy_snap_to_target(game.active_scene);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 12; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 18; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\05e_accuracy_shot_flight.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_accuracy_force_test_score_frame(game.active_scene, 20);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\05f_accuracy_net.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_accuracy_force_test_result(game.active_scene);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\05g_accuracy_result.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 6. Free Throw */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_FREE_THROW);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\06_free_throw.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* The exact $22A9 OBJ tile follows $1A25's raw OAM coordinates. Two
       deterministic aim states make the displacement visible in proof. */
    allstar_scene_free_throw_set_test_aim(game.active_scene, 0x40, 0x30);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\06e_free_throw_reticle_left.bmp",
             out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_free_throw_set_test_aim(game.active_scene, 0x60, 0x50);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\06f_free_throw_reticle_right.bmp",
             out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 6a-6d. Fixed $1942/$1986 aim, $1CAA/$7C58 flight,
       $1A31->$1C05 make, $1C61 net, and $0C8E five-attempt results. */
    allstar_scene_free_throw_set_test_aim(game.active_scene, 0x52, 0x3c);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (int i = 0; i < 60; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\06a_free_throw_flight.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    for (int i = 60; i < 118; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\06b_free_throw_make.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    for (int i = 118; i < 145; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\06c_free_throw_net.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    for (int i = 145; i < ALLSTAR_FREE_THROW_PRESENTATION_FRAMES; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    for (int attempt = 1; attempt < 5; attempt++) {
        allstar_scene_free_throw_set_test_aim(game.active_scene, 0x52, 0x3c);
        allstar_input_update(&game.input, ALLSTAR_BTN_A);
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        allstar_input_update(&game.input, 0);
        for (int i = 0; i < ALLSTAR_FREE_THROW_PRESENTATION_FRAMES; i++)
            allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    }
    snprintf(path, sizeof(path), "%s\\06d_free_throw_result.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7. HORSE */
    allstar_game_change_scene(&game, ALLSTAR_SCENE_HORSE);
    for (int i = 0; i < 10; i++) allstar_game_tick(&game, 1.0f / 60.0f);
    snprintf(path, sizeof(path), "%s\\07_horse.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels, ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7a. Shared $702D->$714D->$7C58 gather and released ball. */
    allstar_input_update(&game.input, ALLSTAR_BTN_RIGHT | ALLSTAR_BTN_DOWN);
    for (int i = 0; i < 36; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    for (int i = 0; i < 18; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\07a_horse_shot_flight.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7b. $0E36 called spot followed by $7AFD/$7B7A's exact tile-$76 X. */
    allstar_scene_horse_force_test_result(game.active_scene, true);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    for (int i = 0; i < 200; i++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\07b_horse_match_x.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7c. Matcher miss reaches $0E26 then $7BA8/$7BC0 and command $07. */
    allstar_scene_horse_force_test_result(game.active_scene, false);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    snprintf(path, sizeof(path), "%s\\07c_horse_letter_h.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

    /* 7d-7g. $1E0E->$1ECC exact +20/+35/+50/+65 net sequence. */
    allstar_scene_horse_force_test_score_frame(game.active_scene, 20);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\07d_horse_net_bend.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_horse_force_test_score_frame(game.active_scene, 35);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\07e_horse_net_deep.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_horse_force_test_score_frame(game.active_scene, 50);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\07f_horse_net_return.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);
    allstar_scene_horse_force_test_score_frame(game.active_scene, 65);
    allstar_game_tick(&game, 0.0f);
    snprintf(path, sizeof(path), "%s\\07g_horse_net_rest.bmp", out_dir);
    save_bmp_file(path, game.renderer->pixels,
                  ALLSTAR_GB_WIDTH, ALLSTAR_GB_HEIGHT);

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
    failed += allstar_cli_test_one_on_one_presentation();
    failed += allstar_cli_test_free_throw();
    failed += allstar_cli_test_horse();
    failed += allstar_cli_test_accuracy();
    failed += allstar_cli_test_postgame_rom();
    failed += allstar_cli_test_postgame_screens_rom();
    failed += allstar_cli_test_postgame_modes_rom();
    failed += allstar_cli_test_postgame_bracket_rom();
    failed += allstar_cli_test_postgame_chooser_rom();
    failed += allstar_cli_test_select_rom();
    failed += allstar_cli_test_select_card_rom();
    failed += allstar_cli_test_select_records_rom();
    failed += allstar_cli_test_shot_result_rom();
    failed += allstar_cli_test_court_state_rom();
    failed += allstar_cli_test_game_clock_rom();
    failed += allstar_cli_test_status_panel_rom();
    failed += allstar_cli_test_menu_voice_rom();
    failed += allstar_cli_test_tournament_rom();
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
    } else if (strcmp(cmd, "--export-rom-sfx") == 0) {
        if (argc < 10) {
            fprintf(stderr, "Error: --export-rom-sfx requires <pack>, "
                    "<05.wav>, <0D.wav>, <0C.wav>, <0F.wav>, <0E.wav>, "
                    "<09.wav>, and <04.wav>\n");
            return 1;
        }
        return allstar_cli_export_rom_sfx(
            argv[2], argv[3], argv[4], argv[5], argv[6], argv[7], argv[8],
            argv[9]);
    } else if (strcmp(cmd, "--export-free-throw-sfx") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: --export-free-throw-sfx requires "
                    "<pack>, <08.wav>, and <0A.wav>\n");
            return 1;
        }
        return allstar_cli_export_free_throw_sfx(
            argv[2], argv[3], argv[4]);
    } else if (strcmp(cmd, "--export-horse-sfx") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: --export-horse-sfx requires "
                    "<pack> and <07.wav>\n");
            return 1;
        }
        return allstar_cli_export_horse_sfx(argv[2], argv[3]);
    } else if (strcmp(cmd, "--export-accuracy-sfx") == 0) {
        if (argc < 4) {
            fprintf(stderr, "Error: --export-accuracy-sfx requires "
                    "<pack> and <02.wav>\n");
            return 1;
        }
        return allstar_cli_export_accuracy_sfx(argv[2], argv[3]);
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
    } else if (strcmp(cmd, "--test-one-on-one-presentation") == 0) {
        return allstar_cli_test_one_on_one_presentation();
    } else if (strcmp(cmd, "--test-free-throw") == 0) {
        return allstar_cli_test_free_throw();
    } else if (strcmp(cmd, "--test-horse") == 0) {
        return allstar_cli_test_horse();
    } else if (strcmp(cmd, "--test-accuracy") == 0) {
        return allstar_cli_test_accuracy();
    } else if (strcmp(cmd, "--test-tournament") == 0) {
        return allstar_cli_test_tournament();
    } else if (strcmp(cmd, "--test-tournament-rom") == 0) {
        return allstar_cli_test_tournament_rom();
    } else if (strcmp(cmd, "--test-postgame-rom") == 0) {
        return allstar_cli_test_postgame_rom();
    } else if (strcmp(cmd, "--test-postgame-screens") == 0) {
        return allstar_cli_test_postgame_screens_rom();
    } else if (strcmp(cmd, "--test-postgame-modes") == 0) {
        return allstar_cli_test_postgame_modes_rom();
    } else if (strcmp(cmd, "--test-postgame-bracket") == 0) {
        return allstar_cli_test_postgame_bracket_rom();
    } else if (strcmp(cmd, "--test-postgame-chooser") == 0) {
        return allstar_cli_test_postgame_chooser_rom();
    } else if (strcmp(cmd, "--test-select-rom") == 0) {
        return allstar_cli_test_select_rom();
    } else if (strcmp(cmd, "--test-select-card") == 0) {
        return allstar_cli_test_select_card_rom();
    } else if (strcmp(cmd, "--test-select-records") == 0) {
        return allstar_cli_test_select_records_rom();
    } else if (strcmp(cmd, "--test-shot-result") == 0) {
        return allstar_cli_test_shot_result_rom();
    } else if (strcmp(cmd, "--test-court-state") == 0) {
        return allstar_cli_test_court_state_rom();
    } else if (strcmp(cmd, "--test-game-clock") == 0) {
        return allstar_cli_test_game_clock_rom();
    } else if (strcmp(cmd, "--test-status-panel") == 0) {
        return allstar_cli_test_status_panel_rom();
    } else if (strcmp(cmd, "--test-menu-voice") == 0) {
        return allstar_cli_test_menu_voice_rom();
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
