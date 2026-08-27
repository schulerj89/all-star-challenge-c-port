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
#include "allstar_settings_screen.h"
#include "allstar_system.h"
#include "allstar_link.h"
#include "allstar_cpu_target.h"
#include "allstar_apu_program.h"
#include "allstar_boot.h"
#include "allstar_handshake.h"
#include "allstar_session.h"
#include "allstar_pad.h"
#include "allstar_frame.h"
#include "allstar_caption.h"
#include "allstar_kernel.h"
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
    printf("  --export-title-music <pack> <wav>  Render the ROM title song to a WAV\n");
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
    printf("  --test-frame                       Verify the ROM $2729 frame spine\n");
    printf("  --test-cpu-head                    Verify the ROM $73C9 steering head\n");
    printf("  --test-captions                    Verify the ROM $07E3 caption script\n");
    printf("  --test-kernel                      Verify the ROM vector table and helpers\n");
    printf("  --test-sfx-envelope                Verify the ROM NR12 cue envelopes\n");
    printf("  --test-rom-art                     Verify the ROM screen and portrait art\n");
    printf("  --test-title-music                 Verify the ROM $35B6 title-music routing\n");
    printf("  --test-defense-jump                Verify the ROM $6C27 defensive jump lift\n");
    printf("  --test-pad                         Verify the ROM $2639 joypad poll\n");
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
 * ROM screen and portrait art, from $04B1/$04EF, $0271 and bank 2 $418D.
 *
 * These used to be 624 KB of rasterised bitmaps committed as C headers, which
 * the repo's own data-boundary rule forbids.  They are now decoded from the
 * cartridge into the asset pack like every other ROM asset.
 *
 * The migration was verified by composing each screen in a scratch decoder and
 * diffing it against the bitmap it replaced: all five screens and 27 of 27
 * team logos were pixel-identical, and 20 of 27 portraits.  The seven that
 * differed were wrong in the committed header, which is why the roster
 * screenshots changed by exactly the 182 pixels player 0 was off by.
 */
int allstar_cli_test_rom_art(void) {
    AllStarAssetPack *pack;
    const AllStarRomScreen *title;
    const AllStarRomScreen *menu;
    const AllStarRomScreen *credits;
    const AllStarRomPlayerArt *art;
    int i;
    int blanks;

    printf("[Test] Running ROM Screen/Portrait Tests ($04B1/$2D4F)...\n");

    pack = (AllStarAssetPack *)calloc(1, sizeof(*pack));
    if (!pack) {
        fprintf(stderr, "[Test] Could not allocate an asset pack\n");
        return 1;
    }
    if (!allstar_asset_pack_load_file(pack, "build/allstar.assetpack") ||
        (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ROM_SCREENS) == 0) {
        /* The ROM is never committed, so a pack may legitimately be absent. */
        free(pack);
        printf("  (no build/allstar.assetpack -- skipped)\n");
        printf("[Test] PASSED: $04B1 (skipped)\n");
        return 0;
    }

    /* $04EF's eight records, with slot 5 genuinely empty. */
    if (pack->header.rom_screen_count != ALLSTAR_ROM_SCREEN_COUNT) {
        fprintf(stderr, "[Test] the pack has %u screens\n",
                (unsigned)pack->header.rom_screen_count);
        free(pack);
        return 1;
    }
    if (pack->rom_screens[5].present) {
        fprintf(stderr, "[Test] $04EF slot 5 is supposed to be unused\n");
        free(pack);
        return 1;
    }

    title = &pack->rom_screens[0];
    menu = &pack->rom_screens[2];
    credits = &pack->rom_screens[ALLSTAR_ROM_SCREEN_CREDITS];
    if (title->tile_pointer != 0x406Du || title->tile_count != 231u ||
        title->tilemap_pointer != 0x4CE6u) {
        fprintf(stderr, "[Test] the title screen record diverged\n");
        free(pack);
        return 1;
    }
    if (menu->tile_pointer != 0x4E3Bu || menu->tile_count != 128u) {
        fprintf(stderr, "[Test] the menu screen record diverged\n");
        free(pack);
        return 1;
    }
    /* $0271 is the only screen whose tiles come from bank 1. */
    if (credits->tile_bank != 1u || credits->tile_pointer != 0x640Fu ||
        credits->tilemap_bank != 3u || credits->tilemap_pointer != 0x4000u) {
        fprintf(stderr, "[Test] the $0271 copyright pair diverged\n");
        free(pack);
        return 1;
    }

    /* $22FC reaches the settings screens as mode + 3.  Two pairs share tiles
       and differ only in the map, which is what the old two-background
       approximation could not represent. */
    if (pack->rom_screens[3].tile_pointer !=
            pack->rom_screens[7].tile_pointer ||
        pack->rom_screens[4].tile_pointer !=
            pack->rom_screens[6].tile_pointer) {
        fprintf(stderr, "[Test] the settings screens stopped sharing tiles\n");
        free(pack);
        return 1;
    }
    if (pack->rom_screens[3].tilemap_pointer ==
            pack->rom_screens[7].tilemap_pointer ||
        pack->rom_screens[4].tilemap_pointer ==
            pack->rom_screens[6].tilemap_pointer) {
        fprintf(stderr, "[Test] the settings variants share a map\n");
        free(pack);
        return 1;
    }

    /* Every present screen's map has to stay inside its own tile stream. */
    for (i = 0; i < ALLSTAR_ROM_SCREEN_COUNT; i++) {
        const AllStarRomScreen *s = &pack->rom_screens[i];
        int ty;
        if (!s->present) continue;
        if (s->tile_count == 0) {
            fprintf(stderr, "[Test] screen %d has no tiles\n", i);
            free(pack);
            return 1;
        }
        for (ty = 0; ty < 18; ty++) {
            int tx;
            for (tx = 0; tx < 20; tx++) {
                uint8_t idx =
                    s->tilemap[ty * ALLSTAR_ROM_SCREEN_MAP_STRIDE + tx];
                if (idx >= s->tile_count) {
                    fprintf(stderr,
                            "[Test] screen %d cell %d,%d indexes tile %u of "
                            "%u\n", i, tx, ty, idx, s->tile_count);
                    free(pack);
                    return 1;
                }
            }
        }
    }

    /* $2D4F: one stream per roster entry. */
    if (pack->header.rom_player_art_count != ALLSTAR_ROM_PLAYER_ART_COUNT) {
        fprintf(stderr, "[Test] the pack has %u player art entries\n",
                (unsigned)pack->header.rom_player_art_count);
        free(pack);
        return 1;
    }
    art = &pack->rom_player_art[0];
    if (art->stream_pointer != 0x5E15u || art->tile_count != 40u) {
        fprintf(stderr,
                "[Test] player 0's stream is $%04X with %u tiles\n",
                art->stream_pointer, art->tile_count);
        free(pack);
        return 1;
    }
    /*
     * $4199 fills the portrait map with 1..24, but $41B0 then patches it for
     * any player whose $42A2 byte is not $FF: the named cell is blanked and
     * every cell after it decrements.  Exactly seven of the 27 take that path,
     * and missing it leaves those seven with the wrong portrait.
     */
    {
        int patched = 0;
        int p;
        for (p = 0; p < ALLSTAR_ROM_PLAYER_ART_COUNT; p++) {
            const AllStarRomPlayerArt *a = &pack->rom_player_art[p];
            uint8_t flag = a->portrait_patch;
            int cell;
            if (flag == 0xFFu) {
                /* Untouched: a plain 1..24. */
                for (cell = 0; cell < ALLSTAR_ROM_PORTRAIT_CELLS; cell++) {
                    if (a->portrait_cells[cell] != (uint8_t)(cell + 1)) {
                        fprintf(stderr,
                                "[Test] player %d cell %d is %u, expected the "
                                "unpatched %d\n", p, cell,
                                a->portrait_cells[cell], cell + 1);
                        free(pack);
                        return 1;
                    }
                }
                continue;
            }
            patched++;
            if (flag >= ALLSTAR_ROM_PORTRAIT_CELLS ||
                a->portrait_cells[flag] != 0u) {
                fprintf(stderr,
                        "[Test] player %d did not blank cell $%02X\n", p, flag);
                free(pack);
                return 1;
            }
            /* Before the gap the map is untouched; after it, shifted down. */
            for (cell = 0; cell < ALLSTAR_ROM_PORTRAIT_CELLS; cell++) {
                uint8_t want;
                if (cell == (int)flag) continue;
                want = cell < (int)flag ? (uint8_t)(cell + 1)
                                        : (uint8_t)cell;
                if (a->portrait_cells[cell] != want) {
                    fprintf(stderr,
                            "[Test] player %d cell %d is %u, expected %u\n",
                            p, cell, a->portrait_cells[cell], want);
                    free(pack);
                    return 1;
                }
            }
        }
        if (patched != 7) {
            fprintf(stderr,
                    "[Test] $41B0 patched %d portraits, expected 7\n",
                    patched);
            free(pack);
            return 1;
        }
    }

    /*
     * $41E7's logo map is the interesting one: entries named by the $42BD
     * list are blank and everything else counts up from $18 or $19.  Every
     * player has at least one blank cell, and the non-blank cells must be
     * strictly increasing.
     */
    blanks = 0;
    for (i = 0; i < ALLSTAR_ROM_PLAYER_ART_COUNT; i++) {
        const AllStarRomPlayerArt *a = &pack->rom_player_art[i];
        int previous = -1;
        int first = -1;
        int cell;
        if (a->logo_base != 0x18u && a->logo_base != 0x19u) {
            fprintf(stderr, "[Test] player %d has logo base $%02X\n", i,
                    a->logo_base);
            free(pack);
            return 1;
        }
        for (cell = 0; cell < ALLSTAR_ROM_PORTRAIT_CELLS; cell++) {
            int value = a->logo_cells[cell];
            if (value == 0) { blanks++; continue; }
            if (first < 0) first = value;
            if (value <= previous) {
                fprintf(stderr,
                        "[Test] player %d logo cell %d went backwards\n",
                        i, cell);
                free(pack);
                return 1;
            }
            previous = value;
        }
        /* The counter starts at the base, so the first sounding cell is it. */
        if (first != (int)a->logo_base) {
            fprintf(stderr,
                    "[Test] player %d starts at %d, not its base $%02X\n",
                    i, first, a->logo_base);
            free(pack);
            return 1;
        }
    }
    /* $42BD blanks 42 cells in total; twelve players have none at all, so a
       per-player "must have a blank" rule would be wrong. */
    if (blanks != 42) {
        fprintf(stderr, "[Test] $42BD blanked %d cells, expected 42\n",
                blanks);
        free(pack);
        return 1;
    }

    /*
     * A digest over every decoded screen and every player's art.  It is
     * anchored to an extraction that was verified image by image against the
     * bitmaps it replaced -- five screens, 27 logos and 27 portraits, all
     * pixel-identical.  A change here means the extractor is reading something
     * different out of the cartridge, which is exactly the failure nothing
     * caught when the $41B0 portrait patch was first missed.
     */
    {
        uint32_t digest = allstar_asset_pack_rom_art_digest(pack);
        if (digest != 0x6208BA77u) {
            fprintf(stderr,
                    "[Test] the art digest is %08X, expected 6208BA77 -- the "
                    "extractor changed what it reads from the ROM\n",
                    (unsigned)digest);
            free(pack);
            return 1;
        }
    }
    printf("  $04EF's eight screens plus $0271's, slot 5 empty, every map "
           "inside its own tiles\n");
    printf("  the settings variants share tiles and differ by map, as "
           "$22FC's mode + 3 requires\n");
    printf("  $2D4F gives 27 streams; $4199 lays out 1..24 and $41E7 blanks "
           "cells from $42BD\n");
    free(pack);
    printf("[Test] PASSED: $04B1, $04EF, $0271, $2D4F, $4199, $41E7\n");
    return 0;
}

/*
 * ROM sound-effect envelopes, from $2AB5 -> $2F88 -> $2FB0 and the NR12 byte
 * each program carries.
 *
 * The cue the roster selector plays when it moves between players is command
 * $0F: bank 2 $411D calls $2AB5, which is `ld a,$0F / jp $2F88`.  $2F88 maps it
 * through $2FB0 to program $07 with a $19-frame priority window, and the
 * program's descriptor supplies NR12 = $F1 -- level 15, decreasing, pace one.
 *
 * Captured from the cartridge (tools/emulator/trace_navigation_sfx.lua) the cue
 * is a SINGLE channel-1 trigger:
 *
 *     NR10=$08  NR11=$88  NR12=$F1  NR13=$B1  NR14=$BF
 *
 * NR14 bit 6 is clear, so no length counter runs; the note is silenced purely
 * by that envelope, fifteen steps of 1/64s -- about 234 ms.  Rendering it at a
 * flat level instead turns a short blip into a 402 ms sustained tone.
 */
int allstar_cli_test_sfx_envelope_rom(void) {
    AllStarAssetPack *pack;
    const AllStarRomSfxProgram *navigation = NULL;
    size_t i;
    int step;

    printf("[Test] Running ROM SFX Envelope Tests ($2AB5/$0F)...\n");

    /* NR12 = $F1: fifteen decreasing steps, one 1/64s apart. */
    if (allstar_audio_rom_envelope_level(0xF1u, 0.0) != 15u) {
        fprintf(stderr, "[Test] $F1 did not start at level 15\n");
        return 1;
    }
    for (step = 1; step <= 15; step++) {
        /* Just past the step boundary, the level has dropped once more. */
        double t = (double)step / 64.0 + 0.0005;
        uint8_t want = (uint8_t)(15 - step);
        if (allstar_audio_rom_envelope_level(0xF1u, t) != want) {
            fprintf(stderr,
                    "[Test] $F1 at %.4fs is level %u, expected %u\n", t,
                    allstar_audio_rom_envelope_level(0xF1u, t), want);
            return 1;
        }
    }
    /* Silent from 15/64s onward, and it must not wrap back up. */
    if (allstar_audio_rom_envelope_level(0xF1u, 15.0 / 64.0) != 0u ||
        allstar_audio_rom_envelope_level(0xF1u, 0.402) != 0u ||
        allstar_audio_rom_envelope_level(0xF1u, 5.0) != 0u) {
        fprintf(stderr, "[Test] $F1 did not stay silent after decaying\n");
        return 1;
    }
    /* A pace of zero never moves, whatever the direction bit says. */
    if (allstar_audio_rom_envelope_level(0xF0u, 1.0) != 15u ||
        allstar_audio_rom_envelope_level(0x80u, 1.0) != 8u) {
        fprintf(stderr, "[Test] a pace of zero stepped anyway\n");
        return 1;
    }
    /* Bit 3 set counts upwards and clamps at 15. */
    if (allstar_audio_rom_envelope_level(0x09u, 0.0) != 0u ||
        allstar_audio_rom_envelope_level(0x09u, 0.0161) != 1u ||
        allstar_audio_rom_envelope_level(0x09u, 5.0) != 15u) {
        fprintf(stderr, "[Test] the rising envelope diverged\n");
        return 1;
    }

    pack = (AllStarAssetPack *)calloc(1, sizeof(*pack));
    if (!pack) {
        fprintf(stderr, "[Test] Could not allocate an asset pack\n");
        return 1;
    }
    if (!allstar_asset_pack_load_file(pack, "build/allstar.assetpack") ||
        pack->header.rom_sfx_program_count != ALLSTAR_ROM_SFX_PROGRAM_COUNT) {
        /* The ROM is never committed, so a pack may legitimately be absent. */
        free(pack);
        printf("  the NR12 envelope curve is verified\n");
        printf("  (no build/allstar.assetpack -- skipped the cue itself)\n");
        printf("[Test] PASSED: the DMG envelope\n");
        return 0;
    }
    for (i = 0; i < pack->header.rom_sfx_program_count; i++) {
        if (pack->rom_sfx_programs[i].command == 0x0Fu) {
            navigation = &pack->rom_sfx_programs[i];
            break;
        }
    }
    if (!navigation) {
        fprintf(stderr, "[Test] the pack has no command $0F program\n");
        free(pack);
        return 1;
    }

    /* $2FB0 maps command $0F to program $07 with a $19-frame window. */
    if (navigation->program_id != 0x07u ||
        navigation->priority_frames != 0x19u ||
        navigation->stream_pointer_1 != 0x3EBCu) {
        fprintf(stderr,
                "[Test] $2FB0 mapping for $0F diverged (program $%02X "
                "window $%02X)\n", navigation->program_id,
                navigation->priority_frames);
        free(pack);
        return 1;
    }
    /* The exact register program the cartridge writes on the trigger. */
    if (navigation->square1_sweep != 0x08u ||
        navigation->square1_duty_length != 0x88u ||
        navigation->square1_envelope != 0xF1u ||
        navigation->frames[0].square1_frequency != 0x07B1u) {
        fprintf(stderr,
                "[Test] the $0F trigger diverged (NR10=$%02X NR11=$%02X "
                "NR12=$%02X freq=$%04X)\n", navigation->square1_sweep,
                navigation->square1_duty_length, navigation->square1_envelope,
                navigation->frames[0].square1_frequency);
        free(pack);
        return 1;
    }
    /* NR10's pace field is zero, so the sweep is inert -- the pitch holds. */
    if ((navigation->square1_sweep & 0x70u) != 0) {
        fprintf(stderr, "[Test] $08 was read as an active sweep\n");
        free(pack);
        return 1;
    }

    /*
     * The program runs 24 frames but the envelope silences it after fifteen
     * 1/64s steps, so most of the tail is quiet.  That gap is the whole
     * difference between a blip and a sustained tone.
     */
    {
        const double frame_seconds = 70224.0 / 4194304.0;
        double total = navigation->frame_count * frame_seconds;
        double silent_at = 15.0 / 64.0;
        if (navigation->frame_count != 24u) {
            fprintf(stderr, "[Test] the $0F program is %u frames\n",
                    navigation->frame_count);
            free(pack);
            return 1;
        }
        if (silent_at >= total) {
            fprintf(stderr,
                    "[Test] the envelope outlasts the program (%.3fs of "
                    "%.3fs)\n", silent_at, total);
            free(pack);
            return 1;
        }
        if (allstar_audio_rom_envelope_level(
                navigation->square1_envelope, total) != 0u) {
            fprintf(stderr,
                    "[Test] the cue is still sounding at its last frame\n");
            free(pack);
            return 1;
        }
        printf("  command $0F decays to silence at %.0f ms of a %.0f ms "
               "program\n", silent_at * 1000.0, total * 1000.0);

        /*
         * And the renderer has to actually apply it.  Rendered through the
         * path the game plays, the cue must be loud at the attack and silent
         * once the envelope has run out -- which the flat-volume renderer
         * this replaced could not satisfy.
         */
        {
            int head = 0;
            int tail = 0;
            if (!allstar_audio_rom_sfx_peak(pack, 0x0Fu, 0.0, 0.02, &head) ||
                !allstar_audio_rom_sfx_peak(pack, 0x0Fu, silent_at + 0.01,
                                            total, &tail)) {
                fprintf(stderr, "[Test] could not render command $0F\n");
                free(pack);
                return 1;
            }
            if (head < 2000) {
                fprintf(stderr,
                        "[Test] the cue opens at peak %d, far too quiet\n",
                        head);
                free(pack);
                return 1;
            }
            if (tail != 0) {
                fprintf(stderr,
                        "[Test] the cue still sounds at peak %d after its "
                        "envelope ran out -- the renderer is holding it "
                        "flat\n", tail);
                free(pack);
                return 1;
            }
            printf("  rendered: peak %d at the attack, silent from %.0f ms\n",
                   head, (silent_at + 0.01) * 1000.0);
        }
    }

    /* Every square cue in the pack carries a real envelope pace. */
    for (i = 0; i < pack->header.rom_sfx_program_count; i++) {
        const AllStarRomSfxProgram *p = &pack->rom_sfx_programs[i];
        if (p->square1_envelope == 0u) continue;
        if ((p->square1_envelope & 7u) == 0u) {
            fprintf(stderr,
                    "[Test] command $%02X has a pace-zero NR12 ($%02X)\n",
                    p->command, p->square1_envelope);
            free(pack);
            return 1;
        }
    }

    free(pack);
    printf("  a pace of zero holds, bit 3 rises and clamps at 15\n");
    printf("[Test] PASSED: $2AB5, $2F88, $2FB0 and the NR12 envelope\n");
    return 0;
}

/*
 * ROM kernel and the last of the small routines: the $0000..$005F vectors,
 * $045E, $0466, $05FA, $0773, $0A91, $18D0, $1982, $20BA, $27EA, $2AA5,
 * $2D0C, $2F79, $3119, $329B, $7015, $7712, $7914 and $7AEA.
 *
 * Individually these are a few bytes each.  Together they are the dispatch
 * mechanism every other ported routine's comments point at, the OAM DMA copy,
 * the interrupt mask and the frame waits.
 */
int allstar_cli_test_kernel_rom(void) {
    AllStarKernelInterrupts interrupts;
    AllStarKernelWait wait;
    AllStarKernelCountdown countdown;
    AllStarKernelOamFill fill;
    AllStarKernelSequence sequence;
    AllStarKernelStatusCopy status;
    AllStarKernelOamEntry entries[4];
    AllStarApuReset apu;
    AllStarHorsePreShot pre_shot;
    AllStarHudWrite hud[2];
    const AllStarVector *vectors;
    const uint16_t *cleared;
    const uint8_t *dma;
    static const uint8_t TILES[3] = {0x11u, 0x22u, 0x33u};
    uint8_t oam[4];
    uint16_t slots[2];
    uint8_t order[4];
    int count = 0;
    int i;

    printf("[Test] Running ROM Kernel Tests ($0000-$005F and friends)...\n");

    /* The vector table, and the three entries that actually go somewhere. */
    vectors = allstar_kernel_vectors(&count);
    if (!vectors || count != ALLSTAR_VECTOR_COUNT) {
        fprintf(stderr, "[Test] the vector table is the wrong size\n");
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (vectors[i].address == 0x0040u &&
            (vectors[i].target != 0x2729u ||
             vectors[i].kind != ALLSTAR_VECTOR_INTERRUPT)) {
            fprintf(stderr, "[Test] vblank does not reach $2729\n");
            return 1;
        }
        if (vectors[i].address == 0x0058u && vectors[i].target != 0x0061u) {
            fprintf(stderr, "[Test] the serial vector does not reach $0061\n");
            return 1;
        }
        if (vectors[i].address == 0x0030u && vectors[i].target != 0x0B4Fu) {
            fprintf(stderr, "[Test] rst $30 does not reach $0B4F\n");
            return 1;
        }
        if (i > 0 && vectors[i].address <= vectors[i - 1].address) {
            fprintf(stderr, "[Test] the vector table is out of order\n");
            return 1;
        }
    }

    /* $0010 doubles the index; the table is words. */
    if (allstar_kernel_dispatch_0010(0x738Du, 0u) != 0x738Du ||
        allstar_kernel_dispatch_0010(0x738Du, 1u) != 0x738Fu ||
        allstar_kernel_dispatch_0010(0x6C60u, 5u) != 0x6C6Au) {
        fprintf(stderr, "[Test] $0010 did not double the index\n");
        return 1;
    }

    /* $0466 installs the canonical DMA routine in HRAM. */
    dma = allstar_kernel_dma_routine_0466(&count);
    if (!dma || count != ALLSTAR_KERNEL_DMA_BYTES || dma[0] != 0x3Eu ||
        dma[1] != ALLSTAR_KERNEL_OAM_PAGE || dma[3] != 0x46u ||
        dma[5] != 0x28u || dma[count - 1] != 0xC9u) {
        fprintf(stderr, "[Test] the $0474 DMA payload diverged\n");
        return 1;
    }

    /* $045E clears the pending flags before enabling anything. */
    allstar_kernel_set_interrupts_045e(0x1Fu, &interrupts);
    if (interrupts.interrupt_flags != 0u ||
        interrupts.interrupt_enable != 0x1Fu) {
        fprintf(stderr, "[Test] $0460 did not clear $FF0F first\n");
        return 1;
    }

    /* $0A91: a link game keeps the seed both cartridges agreed on. */
    if (allstar_kernel_clears_rng_0a91(1u) ||
        !allstar_kernel_clears_rng_0a91(2u)) {
        fprintf(stderr, "[Test] $0A94 cleared the RNG in the wrong game\n");
        return 1;
    }
    cleared = allstar_kernel_rng_cleared_0a91(&count);
    if (!cleared || count != ALLSTAR_KERNEL_RNG_CLEARED ||
        cleared[0] != ALLSTAR_FRAME_COUNTER) {
        fprintf(stderr,
                "[Test] $0A96 does not clear the counter $276D increments\n");
        return 1;
    }

    /* $0773 and bank 1 $7914 name the same two slots. */
    if (allstar_kernel_entity_slot_0773(0x02u) != ALLSTAR_KERNEL_SLOT_TWO ||
        allstar_kernel_entity_slot_0773(0x01u) != ALLSTAR_KERNEL_SLOT_ONE ||
        allstar_kernel_entity_slot_0773(0x00u) != ALLSTAR_KERNEL_SLOT_ONE) {
        fprintf(stderr, "[Test] $0778 picked the wrong entity slot\n");
        return 1;
    }
    if (allstar_entity_slots_7914(slots, 2) != 2 ||
        slots[0] != ALLSTAR_KERNEL_SLOT_TWO ||
        slots[1] != ALLSTAR_KERNEL_SLOT_ONE) {
        fprintf(stderr, "[Test] $7914 does not cover both $0773 slots\n");
        return 1;
    }

    /* $2D0C parks its count in the byte $276D decrements. */
    allstar_kernel_wait_2d0c(ALLSTAR_HORSE_HANDOFF_WAIT, &wait);
    if (wait.frames != 4u || !wait.clears_ffeb ||
        ALLSTAR_KERNEL_WAIT_COUNTER != ALLSTAR_FRAME_DELAY_COUNTER) {
        fprintf(stderr, "[Test] $2D0D does not use $276D's counter\n");
        return 1;
    }

    /* $2F79: only the frame it reaches zero clears $C193. */
    allstar_kernel_countdown_2f79(0u, &countdown);
    if (countdown.counter != 0u || countdown.clears_state) {
        fprintf(stderr, "[Test] $2F7D acted on an expired countdown\n");
        return 1;
    }
    allstar_kernel_countdown_2f79(2u, &countdown);
    if (countdown.counter != 1u || countdown.clears_state) {
        fprintf(stderr, "[Test] $2F82 cleared $C193 early\n");
        return 1;
    }
    allstar_kernel_countdown_2f79(1u, &countdown);
    if (countdown.counter != 0u || !countdown.clears_state) {
        fprintf(stderr, "[Test] $2F84 did not clear $C193 on expiry\n");
        return 1;
    }

    /* $20BA reloads with $10 exactly on the frame it blanks. */
    allstar_kernel_oam_fill_20ba(2u, &fill);
    if (fill.blanks || fill.counter != 1u) {
        fprintf(stderr, "[Test] $20BF blanked early\n");
        return 1;
    }
    allstar_kernel_oam_fill_20ba(1u, &fill);
    if (!fill.blanks || fill.blank_bytes != ALLSTAR_KERNEL_OAM_STRIDE ||
        fill.counter != ALLSTAR_KERNEL_OAM_STRIDE) {
        fprintf(stderr, "[Test] $20CB reload diverged\n");
        return 1;
    }

    /* $27EA post-increments the step, so the dispatch uses the old value. */
    allstar_kernel_sequence_27ea(3u, 4u, &sequence);
    if (!sequence.waits || sequence.counter != 2u || sequence.next_step != 4u) {
        fprintf(stderr, "[Test] $27EE did not keep waiting\n");
        return 1;
    }
    allstar_kernel_sequence_27ea(1u, 4u, &sequence);
    if (sequence.waits || sequence.counter != ALLSTAR_KERNEL_SEQUENCE_RELOAD ||
        sequence.step != 4u || sequence.next_step != 5u) {
        fprintf(stderr,
                "[Test] $27FD sequence step diverged (step=%u next=%u)\n",
                sequence.step, sequence.next_step);
        return 1;
    }

    /* $05FA copies the bottom map row, 32 groups of 3. */
    allstar_kernel_status_copy_05fa(&status);
    if (!status.waits_vblank || status.source != 0xC271u ||
        status.destination != 0x98E0u || status.groups != 0x20u ||
        status.total_bytes != 96u) {
        fprintf(stderr, "[Test] $05FE status copy diverged\n");
        return 1;
    }

    /* $18D0 steps X by eight and only writes attributes while $FF8D is clear. */
    if (allstar_kernel_oam_row_18d0(0x40u, 0x20u, TILES, 3, 0u, entries, 4)
            != 3 ||
        entries[0].x != 0x20u || entries[1].x != 0x28u ||
        entries[2].x != 0x30u || entries[2].tile != 0x33u ||
        entries[0].y != 0x40u || !entries[0].wrote_attributes) {
        fprintf(stderr, "[Test] $18E0 OAM row diverged\n");
        return 1;
    }
    if (allstar_kernel_oam_row_18d0(0x40u, 0x20u, TILES, 3, 1u, entries, 4)
            != 3 || entries[0].wrote_attributes) {
        fprintf(stderr, "[Test] $18DB wrote attributes with $FF8D set\n");
        return 1;
    }

    /* $2AA5 builds one OAM entry with attribute $01 then a zero. */
    allstar_kernel_oam_seed_2aa5(0x50u, 0x60u, oam);
    if (oam[0] != 0x50u || oam[1] != 0x60u ||
        oam[2] != ALLSTAR_KERNEL_OAM_ATTRIBUTE || oam[3] != 0u) {
        fprintf(stderr, "[Test] $2AAE OAM seed diverged\n");
        return 1;
    }

    /* $1982/$1984 are the two directions of a one-step trampoline. */
    if (allstar_kernel_step_1982(true) != 1 ||
        allstar_kernel_step_1982(false) != -1) {
        fprintf(stderr, "[Test] $1982 trampoline diverged\n");
        return 1;
    }

    /*
     * $3119.  NR50 at $77 is maximum and symmetric, which is what lets the
     * title song's NR51 routing be the only thing placing its voices.
     */
    allstar_apu_reset_3119(&apu);
    if (apu.nr51 != 0x00u || apu.nr50 != 0x77u || apu.nr52 != 0x8Fu ||
        apu.wave_cache != 0xFFu) {
        fprintf(stderr, "[Test] $3119 APU reset diverged\n");
        return 1;
    }
    /* $329B starts a song over the same voices $3264 later ticks. */
    if (allstar_voice_start_order_329b(order, 4) != 4 || order[0] != 3u ||
        order[3] != 0u) {
        fprintf(stderr, "[Test] $329B did not walk voices 3..0\n");
        return 1;
    }

    /* $7015: the single-shooter modes freeze the other player's animation. */
    if (!allstar_animation_advances_7015(0x00u, 2u, 1u) ||
        !allstar_animation_advances_7015(0x04u, 2u, 1u)) {
        fprintf(stderr, "[Test] $7027 gated a mode it should not\n");
        return 1;
    }
    if (allstar_animation_advances_7015(0x02u, 2u, 1u) ||
        !allstar_animation_advances_7015(0x02u, 1u, 1u) ||
        allstar_animation_advances_7015(0x03u, 1u, 2u)) {
        fprintf(stderr, "[Test] $7025 shooter gate diverged\n");
        return 1;
    }

    /* $7AEA has two gates and clears what $0D2B clears. */
    allstar_horse_pre_shot_7aea(2u, 2u, &pre_shot);
    if (pre_shot.runs) {
        fprintf(stderr, "[Test] $7AED ran in a two-player game\n");
        return 1;
    }
    allstar_horse_pre_shot_7aea(1u, 1u, &pre_shot);
    if (pre_shot.runs) {
        fprintf(stderr, "[Test] $7AF1 ran for shooter one\n");
        return 1;
    }
    allstar_horse_pre_shot_7aea(1u, 2u, &pre_shot);
    if (!pre_shot.runs || !pre_shot.calls_6cab ||
        pre_shot.cleared[0] != 0xC0FDu || pre_shot.cleared[1] != 0xC145u) {
        fprintf(stderr, "[Test] $7AF2 pre-shot reset diverged\n");
        return 1;
    }

    /* $7712's two writers differ in how they read their source. */
    if (allstar_hud_writes_7712(hud, 2) != 2 ||
        hud[0].destination != ALLSTAR_HUD_LEFT_DEST ||
        !hud[0].source_is_word ||
        hud[1].destination != ALLSTAR_HUD_RIGHT_DEST ||
        hud[1].source_is_word ||
        hud[0].digits_at == hud[1].digits_at) {
        fprintf(stderr, "[Test] $7712 HUD pair diverged\n");
        return 1;
    }

    printf("  the vector table routes vblank to $2729 and serial to $0061\n");
    printf("  $0A91 clears the RNG in one-player games only, so a link "
           "session keeps its seed\n");
    printf("  $3119 leaves NR50 at $77, symmetric, which is why NR51 alone "
           "places the title song\n");
    printf("[Test] PASSED: the $0000-$005F vectors and seventeen helpers\n");
    return 0;
}

/*
 * ROM captions and three small consumers: $07E3, $0D2B, $6E1B, $3264, $327F.
 *
 * $07E3 is the game's whole caption system -- twenty-five layouts covering
 * every prompt in the cartridge -- and the other four are the small routines
 * that were sitting unported alongside it.  $0D2B parks a shooter in $FFDA and
 * $6E1B is what reads it back, so those two are asserted as a pair.
 */
int allstar_cli_test_caption_rom(void) {
    AllStarCaptionDraw draws[ALLSTAR_ROM_CAPTION_RECORDS];
    AllStarHorseHandoff handoff;
    AllStarShooterSeed seed;
    AllStarAssetPack *pack;
    uint8_t order[8];
    int count;
    int i;

    printf("[Test] Running ROM Caption Tests ($07E3/$0D2B/$6E1B/$3264)...\n");

    /* $07E8/$07FD: bit 7 marks the last tile, and is not part of it. */
    if (allstar_caption_tile(0xD4u) != 0x54u ||
        allstar_caption_tile(0x53u) != 0x53u) {
        fprintf(stderr, "[Test] $07FD terminator handling diverged\n");
        return 1;
    }
    if (!allstar_caption_clears_first_07de(0x07DEu) ||
        allstar_caption_clears_first_07de(0x07E3u)) {
        fprintf(stderr, "[Test] $07DF clear-first entry diverged\n");
        return 1;
    }

    /* $3264 and $327F walk their halves downwards and never overlap. */
    count = allstar_voice_bank_order(ALLSTAR_VOICE_BANK_MUSIC, order, 8);
    if (count != 4 || order[0] != 3 || order[1] != 2 || order[2] != 1 ||
        order[3] != 0) {
        fprintf(stderr, "[Test] $3264 did not walk channels 3..0\n");
        return 1;
    }
    count = allstar_voice_bank_order(ALLSTAR_VOICE_BANK_SFX, order, 8);
    if (count != 4 || order[0] != 7 || order[1] != 6 || order[2] != 5 ||
        order[3] != 4) {
        fprintf(stderr, "[Test] $327F did not walk channels 7..4\n");
        return 1;
    }
    if (allstar_voice_active_slot(0) != ALLSTAR_VOICE_ACTIVE_BASE ||
        allstar_voice_active_slot(7) != ALLSTAR_VOICE_ACTIVE_BASE + 7u) {
        fprintf(stderr, "[Test] $3266 indexed $DD7F wrongly\n");
        return 1;
    }
    if (allstar_voice_channel_runs(0) || !allstar_voice_channel_runs(1)) {
        fprintf(stderr, "[Test] $326B skipped the wrong channels\n");
        return 1;
    }

    /* $0D2B parks the shooter where $6E1B will read it. */
    allstar_horse_handoff_0d2b(2u, &handoff);
    if (handoff.shooter != 2u || handoff.wait_frames != 4u ||
        !handoff.enables_objects || handoff.set_flags[0] != 1u ||
        handoff.set_flags[1] != 1u || handoff.set_flags[2] != 1u ||
        handoff.cleared[0] != 0u || handoff.cleared[1] != 0u) {
        fprintf(stderr, "[Test] $0D2B handoff diverged\n");
        return 1;
    }
    /* $0D41 -> $FFDA -> $6E1D: the byte survives the trip unchanged. */
    allstar_shooter_seed_6e1b(handoff.shooter, &seed);
    if (seed.possession != 2u || seed.slot_x != ALLSTAR_SHOOTER_SLOT_TWO) {
        fprintf(stderr,
                "[Test] $6E1B put shooter 2 in slot $%04X\n", seed.slot_x);
        return 1;
    }
    allstar_horse_handoff_0d2b(1u, &handoff);
    allstar_shooter_seed_6e1b(handoff.shooter, &seed);
    if (seed.possession != 1u || seed.slot_x != ALLSTAR_SHOOTER_SLOT_ONE ||
        seed.slot_y != ALLSTAR_SHOOTER_SLOT_ONE + 1u ||
        seed.slot_z != ALLSTAR_SHOOTER_SLOT_ONE + 2u) {
        fprintf(stderr, "[Test] $6E20 slot-one path diverged\n");
        return 1;
    }
    /* $6E1F is `dec a` then `jr nz`, so only an exact 1 takes slot one. */
    allstar_shooter_seed_6e1b(0u, &seed);
    if (seed.slot_x != ALLSTAR_SHOOTER_SLOT_TWO) {
        fprintf(stderr, "[Test] $6E20 treated zero as shooter one\n");
        return 1;
    }
    /* Both slots take the same fixed spot. */
    if (seed.x != ALLSTAR_SHOOTER_SEED_X || seed.y != ALLSTAR_SHOOTER_SEED_Y ||
        seed.z != ALLSTAR_SHOOTER_SEED_Z) {
        fprintf(stderr, "[Test] $6E22 seed coordinates diverged\n");
        return 1;
    }

    pack = (AllStarAssetPack *)calloc(1, sizeof(*pack));
    if (!pack) {
        fprintf(stderr, "[Test] Could not allocate an asset pack\n");
        return 1;
    }
    if (!allstar_asset_pack_load_file(pack, "build/allstar.assetpack") ||
        (pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ROM_CAPTIONS) == 0) {
        /* The ROM is never committed, so a pack may legitimately be absent. */
        free(pack);
        printf("  $07E3 index handling verified\n");
        printf("  (no build/allstar.assetpack -- skipped the script)\n");
        printf("[Test] PASSED: $0D2B, $6E1B, $3264, $327F\n");
        return 0;
    }

    /* $07E7: a one-based index, because $0802 is itself a marker. */
    if (allstar_caption_layout_07e3(pack, 0u, draws,
                                    ALLSTAR_ROM_CAPTION_RECORDS) != 0) {
        fprintf(stderr, "[Test] $07E4 gave layout zero records\n");
        free(pack);
        return 1;
    }

    /* Layout 1 is the plain "SELECT PLAYER" pair at $0803. */
    count = allstar_caption_layout_07e3(pack, 1u, draws,
                                        ALLSTAR_ROM_CAPTION_RECORDS);
    if (count != 2 || draws[0].rom_pointer != 0x0968u ||
        draws[1].rom_pointer != 0x096Eu ||
        draws[0].row != 0x06u || draws[0].column != 0x07u ||
        draws[1].row != 0x08u || draws[1].column != 0x07u) {
        fprintf(stderr,
                "[Test] layout 1 diverged (%d records, first $%04X at row "
                "%u col %u)\n", count, draws[0].rom_pointer, draws[0].row,
                draws[0].column);
        free(pack);
        return 1;
    }
    /* "SELECT" is six tiles and the sixth carries bit 7. */
    if (draws[0].length != 6u ||
        allstar_caption_tile(draws[0].tiles[0]) != 'S' ||
        allstar_caption_tile(draws[0].tiles[5]) != 'T' ||
        (draws[0].tiles[5] & 0x80u) == 0 ||
        (draws[0].tiles[4] & 0x80u) != 0) {
        fprintf(stderr, "[Test] the $0968 stream is not \"SELECT\"\n");
        free(pack);
        return 1;
    }

    /* Layout 13 is the bracket's four GAME labels, three tiles each. */
    count = allstar_caption_layout_07e3(pack, 13u, draws,
                                        ALLSTAR_ROM_CAPTION_RECORDS);
    if (count != 12) {
        fprintf(stderr, "[Test] the bracket layout has %d records\n", count);
        free(pack);
        return 1;
    }
    for (i = 0; i < 12; i++) {
        if (draws[i].column != 0x0Au) {
            fprintf(stderr, "[Test] bracket record %d left column $0A\n", i);
            free(pack);
            return 1;
        }
    }
    /* $09D8 opens with the bracket tile $63, not a letter. */
    if (draws[1].length != 8u || draws[1].tiles[0] != 0x63u ||
        allstar_caption_tile(draws[1].tiles[7]) != '1') {
        fprintf(stderr, "[Test] the $09D8 GAME 1 stream diverged\n");
        free(pack);
        return 1;
    }

    /* Layout 21 is the champion banner. */
    count = allstar_caption_layout_07e3(pack, 21u, draws,
                                        ALLSTAR_ROM_CAPTION_RECORDS);
    if (count != 3 || draws[0].length != 15u ||
        allstar_caption_tile(draws[0].tiles[0]) != 'C' ||
        allstar_caption_tile(draws[0].tiles[14]) != 'S') {
        fprintf(stderr, "[Test] the champion banner diverged\n");
        free(pack);
        return 1;
    }

    /* Every layout the cartridge defines has to be non-empty and bounded. */
    for (i = 1; i < ALLSTAR_ROM_CAPTION_LAYOUTS; i++) {
        int n = allstar_caption_layout_07e3(pack, (uint8_t)i, draws,
                                            ALLSTAR_ROM_CAPTION_RECORDS);
        int j;
        if (n <= 0) {
            fprintf(stderr, "[Test] layout %d is empty\n", i);
            free(pack);
            return 1;
        }
        for (j = 0; j < n; j++) {
            if (draws[j].length == 0u || draws[j].tiles == NULL ||
                (draws[j].tiles[draws[j].length - 1] & 0x80u) == 0) {
                fprintf(stderr,
                        "[Test] layout %d record %d is not terminated\n",
                        i, j);
                free(pack);
                return 1;
            }
        }
    }

    printf("  $07E3 walks %u layouts, one-based, every stream bit-7 "
           "terminated\n", (unsigned)pack->rom_captions.layout_count - 1u);
    printf("  $0D2B -> $FFDA -> $6E1B carries the shooter to its slot\n");
    printf("  $3264 walks voices 3..0 and $327F 7..4, both downwards\n");
    free(pack);
    printf("[Test] PASSED: $07E3, $0D2B, $6E1B, $3264, $327F\n");
    return 0;
}

/*
 * ROM CPU steering head, from $73C9..$7410, and the two route tables at $7391
 * and $73AD that $7367 picks between.
 *
 * $7190's exits at $7411/$742E/$7476/$7496/$749E were ported earlier.  The
 * 251-byte block the coverage tool reported at $73AD is really three things:
 * the second coordinate table, this head, and those exits.  The tables are
 * asserted here against the cartridge's own bytes, and the head is driven
 * through every branch.
 */
int allstar_cli_test_cpu_head_rom(void) {
    /* $7391 and $73AD, byte for byte: first byte $C101, second $C102. */
    static const uint8_t ROM_7391[14][2] = {
        {0x84,0x90},{0x98,0x28},{0x98,0x48},{0x60,0x0c},{0x74,0x9c},
        {0x94,0x0c},{0x98,0x70},{0x8c,0x9c},{0x88,0x14},{0x60,0x9c},
        {0x98,0x5c},{0x70,0x0c},{0x98,0x90},{0x98,0x54}
    };
    static const uint8_t ROM_73AD[14][2] = {
        {0x70,0x34},{0x8c,0x60},{0x84,0x28},{0x68,0x5c},{0x74,0x80},
        {0x64,0x48},{0x74,0x68},{0x64,0x80},{0x64,0x24},{0x88,0x50},
        {0x64,0x70},{0x84,0x3c},{0x84,0x78},{0x70,0x50}
    };
    /* The annulus: inside the $1E box but outside the $1A one. */
    const float LANE_Y = 96.0f;          /* $60, the exact row $73E0 wants */
    const float ANNULUS_X = 56.0f;       /* in [55,114) but not [59,110)   */
    const float WIDE_X = 84.0f;          /* in the $12 box                 */
    const float OUTSIDE_X = 32.0f;       /* in none of them                */
    AllStarCpuHead head;
    uint8_t x = 0;
    uint8_t y = 0;
    int i;

    printf("[Test] Running ROM CPU Steering Head Tests ($73C9/$7367)...\n");

    /* $73CC: $C0FA of $02 leaves for the shot decision before anything else. */
    allstar_cpu_head_73c9(ALLSTAR_CPU_HEAD_SHOOT_STATE, 0x2Au, ANNULUS_X,
                          LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_SHOOT) {
        fprintf(stderr, "[Test] $73CE did not leave for $756C\n");
        return 1;
    }

    /* $73D5: no commit timer means the defensive branch. */
    allstar_cpu_head_73c9(0x00u, 0x00u, ANNULUS_X, LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_DEFEND || head.timer != 0) {
        fprintf(stderr, "[Test] $73D5 did not fall to $742E\n");
        return 1;
    }

    /* $73E0-$73F8: the annulus commits. */
    allstar_cpu_head_73c9(0x00u, 0x01u, ANNULUS_X, LANE_Y, 0x2Fu, &head);
    if (head.route != ALLSTAR_CPU_HEAD_COMMIT ||
        head.timer != ALLSTAR_CPU_HEAD_COMMIT_TIMER) {
        fprintf(stderr,
                "[Test] $7401 annulus did not commit (route=%d timer=$%02X)\n",
                (int)head.route, head.timer);
        return 1;
    }
    /* $73E6: one past the roll threshold and the annulus is shut.  This spot
       is outside the $12 box too, so the fallback cannot rescue it. */
    allstar_cpu_head_73c9(0x00u, 0x01u, ANNULUS_X, LANE_Y,
                          ALLSTAR_CPU_HEAD_LANE_ROLL, &head);
    if (head.route != ALLSTAR_CPU_HEAD_DEFEND) {
        fprintf(stderr, "[Test] $73E8 committed on a roll of $30\n");
        return 1;
    }
    /* $73E0: the lane row is an exact compare, not a range. */
    allstar_cpu_head_73c9(0x00u, 0x01u, ANNULUS_X, LANE_Y - 1.0f, 0x00u,
                          &head);
    if (head.route != ALLSTAR_CPU_HEAD_DEFEND) {
        fprintf(stderr, "[Test] $73E2 accepted a row other than $60\n");
        return 1;
    }
    /* $73F1: inside the $1A box the annulus is closed -- but $73FA's wider
       box then catches it, so this commits by the other path. */
    allstar_cpu_head_73c9(0x00u, 0x01u, WIDE_X, LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_COMMIT) {
        fprintf(stderr, "[Test] $73FA wide box did not commit\n");
        return 1;
    }
    if (!allstar_ai_rom_inside_07b4(WIDE_X, LANE_Y,
                                    ALLSTAR_CPU_HEAD_INNER_MARGIN)) {
        fprintf(stderr,
                "[Test] the wide-box case was supposed to sit inside $1A\n");
        return 1;
    }
    /* $73FF: outside every box, defend. */
    allstar_cpu_head_73c9(0x00u, 0x01u, OUTSIDE_X, LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_DEFEND) {
        fprintf(stderr, "[Test] $73FF did not defend from outside the lane\n");
        return 1;
    }

    /* $7409: the timer runs down one frame at a time, and only the exact
       value $25 drops into $7411's release check. */
    allstar_cpu_head_73c9(0x00u, ALLSTAR_CPU_HEAD_COMMIT_TIMER, WIDE_X,
                          LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_COUNTDOWN || head.timer != 0x29u) {
        fprintf(stderr, "[Test] $7409 countdown diverged (timer=$%02X)\n",
                head.timer);
        return 1;
    }
    allstar_cpu_head_73c9(0x00u, 0x26u, WIDE_X, LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_RELEASE_CHECK ||
        head.timer != ALLSTAR_CPU_HEAD_RELEASE_AT) {
        fprintf(stderr, "[Test] $740D did not reach the release check\n");
        return 1;
    }
    allstar_cpu_head_73c9(0x00u, 0x27u, WIDE_X, LANE_Y, 0x00u, &head);
    if (head.route != ALLSTAR_CPU_HEAD_COUNTDOWN) {
        fprintf(stderr, "[Test] $740D released one frame early\n");
        return 1;
    }
    /* Walking a whole commit down reaches the release check exactly once. */
    {
        uint8_t timer = ALLSTAR_CPU_HEAD_COMMIT_TIMER;
        int releases = 0;
        for (i = 0; i < 64 && timer > 1u; i++) {
            allstar_cpu_head_73c9(0x00u, timer, WIDE_X, LANE_Y, 0x00u, &head);
            timer = head.timer;
            if (head.route == ALLSTAR_CPU_HEAD_RELEASE_CHECK) releases++;
        }
        if (releases != 1) {
            fprintf(stderr,
                    "[Test] a $2A commit hit the release check %d times\n",
                    releases);
            return 1;
        }
    }

    /*
     * $7367: family $80 is the fourteen pairs at $7391 and family $81 the
     * fourteen at $73AD.  $736C walks $FFFE in nineteen-unit steps, so a roll
     * of 19*p lands on pair p.  Roster key $01's families are {1, 0, 2}, which
     * reaches both tables and the fixed spot from one entry.
     */
    for (i = 0; i < 14; i++) {
        uint8_t position_roll = (uint8_t)(0x13u * i);
        /* $FFFD below $A2 takes family index 0, which for key $01 is $81. */
        allstar_ai_rom_route_target_732c(0x01u, 0x00u, position_roll, &x, &y);
        if (y != ROM_73AD[i][0] || x != ROM_73AD[i][1]) {
            fprintf(stderr,
                    "[Test] $73AD pair %d is $%02X/$%02X, the ROM has "
                    "$%02X/$%02X\n", i, y, x, ROM_73AD[i][0], ROM_73AD[i][1]);
            return 1;
        }
        /* $A2..$EF takes family index 1, which for key $01 is $80. */
        allstar_ai_rom_route_target_732c(0x01u, 0xA2u, position_roll, &x, &y);
        if (y != ROM_7391[i][0] || x != ROM_7391[i][1]) {
            fprintf(stderr,
                    "[Test] $7391 pair %d is $%02X/$%02X, the ROM has "
                    "$%02X/$%02X\n", i, y, x, ROM_7391[i][0], ROM_7391[i][1]);
            return 1;
        }
    }
    /* $734C: family $82 never reaches $738D; it is the fixed centre spot. */
    allstar_ai_rom_route_target_732c(0x01u, 0xF0u, 0x00u, &x, &y);
    if (x != 0x54u || y != 0x5Du) {
        fprintf(stderr,
                "[Test] $7350 fixed spot is $%02X/$%02X, expected $54/$5D\n",
                x, y);
        return 1;
    }
    /* $733F: the thresholds are $A2 and $F0, not one below either. */
    allstar_ai_rom_route_target_732c(0x01u, 0xA1u, 0x00u, &x, &y);
    if (y != ROM_73AD[0][0]) {
        fprintf(stderr, "[Test] $7341 moved family at $A1\n");
        return 1;
    }
    allstar_ai_rom_route_target_732c(0x01u, 0xEFu, 0x00u, &x, &y);
    if (y != ROM_7391[0][0]) {
        fprintf(stderr, "[Test] $7346 moved family at $EF\n");
        return 1;
    }

    printf("  $73C9 routes shoot/defend/commit/countdown, and the commit "
           "window is an annulus\n");
    printf("  $7391 and $73AD both match the cartridge across all 28 pairs\n");
    printf("[Test] PASSED: $73C9, $7367, $7391, $73AD\n");
    return 0;
}

/*
 * ROM frame spine and lifecycle, from $2729, $276D, $279E, $0271, $1F7A,
 * $1FA4, $1FE1 and $1699.
 *
 * These seven are one story.  $2729 is the vblank handler; it copies OAM and
 * calls $2757, which trampolines into bank 1 for $276D.  $276D runs the pad
 * poll and the link send in a role-dependent order and bumps $FF8B.  $279E
 * decides which byte the link actually transmits.  $0271, $1F7A, $1FA4 and
 * $1FE1 are the lifecycle around all of that, and $1699 flashes the winner
 * banner off the same $FF8B counter $276D increments.
 */
int allstar_cli_test_frame_rom(void) {
    AllStarFrameVblank vblank;
    AllStarFrameBody body;
    AllStarFrameLinkSend send;
    AllStarCreditsScreen credits;
    AllStarBanner banner;
    AllStarLinkTransmit tx;
    const uint16_t *cleared;
    int count = 0;
    int i;
    int j;

    printf("[Test] Running ROM Frame Spine Tests ($2729/$276D/$279E)...\n");

    /* $2743: every role but $03 copies OAM and runs the frame. */
    allstar_frame_vblank_2729(0x02u, 0u, &vblank);
    if (!vblank.runs_oam_dma || !vblank.runs_update || vblank.advances_stall) {
        fprintf(stderr, "[Test] $2746 normal vblank path diverged\n");
        return 1;
    }
    /* $273E: role $03 copies OAM but never calls $2757. */
    allstar_frame_vblank_2729(ALLSTAR_FRAME_LINK_ROLE_3, 0u, &vblank);
    if (!vblank.runs_oam_dma || vblank.runs_update) {
        fprintf(stderr, "[Test] $2730 role $03 ran the frame update\n");
        return 1;
    }
    /* $2738: a stalled role $03 skips the OAM copy as well. */
    allstar_frame_vblank_2729(ALLSTAR_FRAME_LINK_ROLE_3, 0x04u, &vblank);
    if (vblank.runs_oam_dma || vblank.runs_update ||
        !vblank.advances_stall || vblank.stall_counter != 0x05u) {
        fprintf(stderr,
                "[Test] $2739 stall path diverged (dma=%d update=%d "
                "counter=$%02X)\n",
                vblank.runs_oam_dma ? 1 : 0, vblank.runs_update ? 1 : 0,
                vblank.stall_counter);
        return 1;
    }

    /* $2749: bit 1 of STAT, set in modes 2 and 3. */
    if (allstar_frame_stat_busy_2749(0x00u) ||
        allstar_frame_stat_busy_2749(0x01u) ||
        !allstar_frame_stat_busy_2749(0x02u) ||
        !allstar_frame_stat_busy_2749(0x03u)) {
        fprintf(stderr, "[Test] $274B watched the wrong STAT bit\n");
        return 1;
    }

    /* $2780: the two cartridges do not transmit from the same half. */
    allstar_frame_body_276d(0x02u, 0u, 0x10u, 0x00u, &body);
    if (body.order != ALLSTAR_FRAME_ORDER_INPUT_FIRST ||
        body.serial_spin != 0 || body.frame_counter != 0x11u) {
        fprintf(stderr, "[Test] $278C body diverged\n");
        return 1;
    }
    allstar_frame_body_276d(ALLSTAR_FRAME_LINK_ROLE_3, 1u, 0xFFu, 0x03u,
                            &body);
    if (body.order != ALLSTAR_FRAME_ORDER_LINK_FIRST ||
        body.serial_spin != ALLSTAR_FRAME_SERIAL_SPIN ||
        body.frame_counter != 0x00u || body.delay_counter != 0x02u) {
        fprintf(stderr,
                "[Test] $2784 role $03 body diverged (spin=%d counter=$%02X "
                "delay=$%02X)\n", body.serial_spin, body.frame_counter,
                body.delay_counter);
        return 1;
    }
    /* $2799: the countdown stops at zero rather than wrapping. */
    allstar_frame_body_276d(0x02u, 0u, 0x00u, 0x00u, &body);
    if (body.delay_counter != 0x00u) {
        fprintf(stderr, "[Test] $2799 wrapped $FF8A past zero\n");
        return 1;
    }

    /*
     * $279E: only modes $01 and $03, with two players and $FF90 set, put
     * $C18D on the wire in place of the live pad byte.
     */
    allstar_frame_link_send_279e(0x02u, 2u, 1u, 0x3Cu, 0x77u, &send);
    if (send.substitutes || send.transmitted != 0x3Cu) {
        fprintf(stderr, "[Test] $27A6 substituted outside modes $01/$03\n");
        return 1;
    }
    allstar_frame_link_send_279e(0x01u, 1u, 1u, 0x3Cu, 0x77u, &send);
    if (send.substitutes) {
        fprintf(stderr, "[Test] $27AC substituted in a one-player game\n");
        return 1;
    }
    allstar_frame_link_send_279e(0x03u, 2u, 0u, 0x3Cu, 0x77u, &send);
    if (send.substitutes) {
        fprintf(stderr, "[Test] $27B2 substituted while $FF90 was clear\n");
        return 1;
    }
    allstar_frame_link_send_279e(0x03u, 2u, 1u, 0x3Cu, 0x77u, &send);
    if (!send.substitutes || send.transmitted != 0x77u ||
        send.restored != 0x3Cu) {
        fprintf(stderr,
                "[Test] $27BF substitution diverged (sent $%02X restored "
                "$%02X)\n", send.transmitted, send.restored);
        return 1;
    }
    /* $27BF hands that byte to $2FD0, which is already ported. */
    allstar_link_transmit(1u, 0x02u, send.transmitted, 0u, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_STATE || tx.byte != 0x77u) {
        fprintf(stderr, "[Test] $2FD0 did not send the byte $279E staged\n");
        return 1;
    }

    /* $0271: four seconds, and only on a cold boot. */
    allstar_credits_screen_0271(0u, &credits);
    if (!credits.shown || credits.hold_frames != ALLSTAR_CREDITS_FRAMES ||
        credits.hold_frames != 240u || credits.tile_bank != 1u ||
        credits.tilemap_bank != 3u) {
        fprintf(stderr,
                "[Test] $0292 cold-boot credits diverged (%u frames)\n",
                credits.hold_frames);
        return 1;
    }
    /* $0274: $0263 sends every finished game back through $0156, which sets
       $C191 -- so only the first game of a session sees this screen. */
    allstar_credits_screen_0271(1u, &credits);
    if (credits.shown || credits.hold_frames != 0u) {
        fprintf(stderr, "[Test] $0275 showed the credits on a warm boot\n");
        return 1;
    }

    /* $1F81/$1F84: clearing both mailboxes is what stops the music. */
    cleared = allstar_teardown_cleared_1f7a(&count);
    if (!cleared || count != ALLSTAR_TEARDOWN_CLEARED ||
        cleared[0] != 0xDD72u || cleared[1] != 0xDD73u) {
        fprintf(stderr, "[Test] $1F7A did not clear both sound mailboxes\n");
        return 1;
    }

    /* $1FBB..$1FD9, and two of them matter to routines already ported. */
    cleared = allstar_title_reset_cleared_1fa4(&count);
    if (!cleared || count != ALLSTAR_TITLE_RESET_CLEARED) {
        fprintf(stderr, "[Test] $1FA4 cleared %d bytes, expected %d\n",
                count, ALLSTAR_TITLE_RESET_CLEARED);
        return 1;
    }
    {
        bool clears_outgoing = false;
        bool clears_stall = false;
        for (i = 0; i < count; i++) {
            if (cleared[i] == ALLSTAR_PAD_OUTGOING) clears_outgoing = true;
            if (cleared[i] == 0xC176u) clears_stall = true;
        }
        if (!clears_outgoing) {
            fprintf(stderr,
                    "[Test] $1FC3 left $2639's outgoing byte $C16E set\n");
            return 1;
        }
        if (!clears_stall) {
            fprintf(stderr,
                    "[Test] $1FD4 left $2729's role $03 stall counter set\n");
            return 1;
        }
    }

    /* $1FE1: SC and SB first, then the role the whole serial layer keys on. */
    cleared = allstar_serial_reset_cleared_1fe1(&count);
    if (!cleared || count != ALLSTAR_SERIAL_RESET_CLEARED ||
        cleared[0] != 0xFF02u || cleared[1] != 0xFF01u ||
        cleared[2] != 0xC199u) {
        fprintf(stderr, "[Test] $1FE1 serial reset order diverged\n");
        return 1;
    }
    /* None of the three lists may name the same byte twice. */
    for (j = 0; j < 3; j++) {
        const uint16_t *list = j == 0 ? allstar_teardown_cleared_1f7a(&count)
            : (j == 1 ? allstar_title_reset_cleared_1fa4(&count)
                      : allstar_serial_reset_cleared_1fe1(&count));
        for (i = 0; i < count; i++) {
            int k;
            for (k = i + 1; k < count; k++) {
                if (list[i] == list[k]) {
                    fprintf(stderr,
                            "[Test] clear list %d names $%04X twice\n",
                            j, list[i]);
                    return 1;
                }
            }
        }
    }

    /*
     * $1699.  The caller passes `$FF8B & $08`, so the banner is on for eight
     * frames and off for eight -- and $FF8B is exactly the counter $276D
     * increments, which is the coupling worth holding onto.
     */
    allstar_postgame_banner_1699(0u, 0x01u, 1u, &banner);
    if (banner.kind != ALLSTAR_BANNER_BLANK ||
        banner.source != ALLSTAR_BANNER_BLANK_TEXT) {
        fprintf(stderr, "[Test] $169A off-half of the flash diverged\n");
        return 1;
    }
    allstar_postgame_banner_1699(0x08u, 0x01u, 0u, &banner);
    if (banner.kind != ALLSTAR_BANNER_TIE ||
        banner.source != ALLSTAR_BANNER_TIE_TEXT) {
        fprintf(stderr, "[Test] $16BB tie line diverged\n");
        return 1;
    }
    allstar_postgame_banner_1699(0x08u, 0x01u, 1u, &banner);
    if (banner.kind != ALLSTAR_BANNER_NAME_WINS ||
        banner.name_source != 0xC23Cu || !banner.copies_name ||
        banner.trims_spaces || banner.source != ALLSTAR_BANNER_NAME_BUFFER) {
        fprintf(stderr, "[Test] $16C4 player one banner diverged\n");
        return 1;
    }
    allstar_postgame_banner_1699(0x08u, 0x01u, 2u, &banner);
    if (banner.name_source != 0xC255u) {
        fprintf(stderr, "[Test] $16CA player two name source diverged\n");
        return 1;
    }
    /* $16D1: only H-O-R-S-E trims the leading spaces, and it moves the line. */
    allstar_postgame_banner_1699(0x08u, ALLSTAR_BANNER_HORSE_MODE, 1u,
                                 &banner);
    if (!banner.trims_spaces || banner.d != 0x02u || banner.e != 0x0Au) {
        fprintf(stderr, "[Test] $16EC H-O-R-S-E banner placement diverged\n");
        return 1;
    }
    /* $16C2: in H-O-R-S-E a zero $C17D is not a tie, it is still nothing. */
    allstar_postgame_banner_1699(0x08u, ALLSTAR_BANNER_HORSE_MODE, 0u,
                                 &banner);
    if (banner.kind != ALLSTAR_BANNER_BLANK) {
        fprintf(stderr, "[Test] $16B6 called an unfinished H-O-R-S-E a tie\n");
        return 1;
    }

    /* Every frame the counter advances, the banner alternates. */
    {
        uint8_t counter = 0u;
        int on = 0;
        int off = 0;
        for (i = 0; i < 32; i++) {
            allstar_frame_body_276d(0x02u, 0u, counter, 0u, &body);
            counter = body.frame_counter;
            allstar_postgame_banner_1699((uint8_t)(counter & 0x08u), 0x01u,
                                         1u, &banner);
            if (banner.kind == ALLSTAR_BANNER_BLANK) off++; else on++;
        }
        if (on != 16 || off != 16) {
            fprintf(stderr,
                    "[Test] $FF8B flash was %d on / %d off over 32 frames\n",
                    on, off);
            return 1;
        }
    }

    printf("  $2729 stalls role $03 without even copying OAM; $276D swaps "
           "the pad/link order by role\n");
    printf("  $0271 holds the credits 240 frames and only on a cold boot\n");
    printf("  $1699 flashes 16 on / 16 off against the $FF8B counter $276D "
           "increments\n");
    printf("[Test] PASSED: $2729, $276D, $279E, $0271, $1F7A, $1FA4, $1FE1, "
           "$1699\n");
    return 0;
}

/*
 * ROM title-screen music, from the $029C -> $DD73 -> $3014 -> $35B6 path.
 *
 * $02A7 posts $81 into the sound-command mailbox.  $30A0 sees bit 7, takes
 * song index $01 from $3849/$3823/$386F, and the four music voices then run
 * every frame.  When a voice starts a note, $35B6 reads bits 2-3 of its
 * instrument descriptor and ORs the matching entry of $3777/$377B/$377F/$3783
 * into NR51; when a voice rests, $3587 ANDs those same bits back off.
 *
 * The cartridge's title theme uses that to place square 1 hard right and
 * square 2 hard left.  The port decoded every note correctly but threw the
 * routing away and summed all four voices to mono, which collapses the two
 * lead voices on top of each other.
 *
 * Verified against a Mesen capture of the cartridge's own APU writes
 * (tools/emulator/trace_title_music.lua): 901 audio frames, zero mismatches
 * on frequency, envelope, note timing and NR51.
 */
int allstar_cli_test_title_music_rom(void) {
    static const uint8_t ROM_NR51_FRAMES[][2] = {
        /* frame, NR51 -- read from the Mesen capture of the cartridge. */
        {0u, 0xEDu}, {7u, 0xEDu}, {14u, 0xCCu}, {28u, 0xEDu},
        {42u, 0xCCu}, {55u, 0xCCu}, {56u, 0xEDu}, {70u, 0xCCu},
        {77u, 0xEDu}, {98u, 0xEDu}, {126u, 0xEDu}, {168u, 0xEDu},
        {210u, 0xCCu}, {224u, 0xEDu}
    };
    AllStarAssetPack *pack;
    const AllStarRomMusicProgram *song;
    size_t i;
    int voice;
    int code;
    int frames_with_square1 = 0;
    int frames_with_square2 = 0;

    printf("[Test] Running ROM Title Music Tests ($029C/$35B6)...\n");

    /* $3777/$377B/$377F/$3783 are the same two bits shifted per voice. */
    for (voice = 0; voice < 4; voice++) {
        static const uint8_t expected[4] = {0x00u, 0x01u, 0x10u, 0x11u};
        for (code = 0; code < 4; code++) {
            uint8_t descriptor = (uint8_t)(code << 2);
            uint8_t want = (uint8_t)(expected[code] << voice);
            uint8_t got = allstar_asset_pack_rom_music_voice_panning(
                descriptor, voice);
            if (got != want) {
                fprintf(stderr,
                        "[Test] $35B6 voice %d pan code %d gave $%02X, "
                        "expected $%02X\n", voice, code, got, want);
                return 1;
            }
        }
        /* Only bits 2-3 of the descriptor select the routing. */
        if (allstar_asset_pack_rom_music_voice_panning(0xF3u, voice) != 0 ||
            allstar_asset_pack_rom_music_voice_panning(0xFFu, voice) !=
                (uint8_t)(0x11u << voice)) {
            fprintf(stderr,
                    "[Test] $35B6 read outside descriptor bits 2-3\n");
            return 1;
        }
    }

    pack = (AllStarAssetPack *)calloc(1, sizeof(*pack));
    if (!pack) {
        fprintf(stderr, "[Test] Could not allocate an asset pack\n");
        return 1;
    }
    if (!allstar_asset_pack_load_file(pack, "build/allstar.assetpack") ||
        pack->header.rom_music_program_count !=
            ALLSTAR_ROM_MUSIC_PROGRAM_COUNT) {
        /* The ROM is never committed, so a pack may legitimately be absent. */
        free(pack);
        printf("  $3777/$377B/$377F/$3783 routing verified\n");
        printf("  (no build/allstar.assetpack -- skipped the decoded song)\n");
        printf("[Test] PASSED: $35B6\n");
        return 0;
    }
    song = &pack->rom_music_programs[0];

    /* $3849+2/$3823+2/$386F+1 are song $01's three parameters. */
    if (song->song_id != 1u || song->program_pointer != 0x3b25u ||
        song->offset_pointer != 0x3aabu || song->update_skip != 7u) {
        fprintf(stderr, "[Test] $30A0 latched the wrong song parameters\n");
        free(pack);
        return 1;
    }

    /* The exact NR51 the cartridge holds on these frames. */
    for (i = 0; i < sizeof(ROM_NR51_FRAMES) / sizeof(ROM_NR51_FRAMES[0]);
         i++) {
        uint8_t frame = ROM_NR51_FRAMES[i][0];
        uint8_t want = ROM_NR51_FRAMES[i][1];
        if (song->frames[frame].panning != want) {
            fprintf(stderr,
                    "[Test] frame %u routed $%02X, the cartridge routes "
                    "$%02X\n", frame, song->frames[frame].panning, want);
            free(pack);
            return 1;
        }
    }

    /*
     * The arrangement itself: whenever square 1 sounds it is on the right
     * only, and whenever square 2 sounds it is on the left only.  This is the
     * assertion the mono renderer could not have satisfied.
     */
    for (i = 0; i < song->frame_count; i++) {
        const AllStarRomMusicFrame *f = &song->frames[i];
        uint8_t pan = f->panning;
        if ((f->flags & ALLSTAR_ROM_MUSIC_SQUARE1) != 0) {
            frames_with_square1++;
            if ((pan & 0x01u) == 0 || (pan & 0x10u) != 0) {
                fprintf(stderr,
                        "[Test] frame %u put square 1 at $%02X, not hard "
                        "right\n", (unsigned)i, pan);
                free(pack);
                return 1;
            }
        } else if ((pan & 0x11u) != 0) {
            fprintf(stderr,
                    "[Test] frame %u routed a resting square 1 ($3587)\n",
                    (unsigned)i);
            free(pack);
            return 1;
        }
        if ((f->flags & ALLSTAR_ROM_MUSIC_SQUARE2) != 0) {
            frames_with_square2++;
            if ((pan & 0x20u) == 0 || (pan & 0x02u) != 0) {
                fprintf(stderr,
                        "[Test] frame %u put square 2 at $%02X, not hard "
                        "left\n", (unsigned)i, pan);
                free(pack);
                return 1;
            }
        } else if ((pan & 0x22u) != 0) {
            fprintf(stderr,
                    "[Test] frame %u routed a resting square 2 ($3587)\n",
                    (unsigned)i);
            free(pack);
            return 1;
        }
    }
    if (frames_with_square1 == 0 || frames_with_square2 == 0) {
        fprintf(stderr, "[Test] The decoded song has no square voices\n");
        free(pack);
        return 1;
    }

    printf("  $3777/$377B/$377F/$3783 routing verified\n");
    printf("  song $01 routes square 1 hard right (%d frames) and square 2 "
           "hard left (%d frames)\n", frames_with_square1,
           frames_with_square2);
    printf("  a resting voice contributes no NR51 bits, as $3587 requires\n");
    printf("[Test] PASSED: $029C, $35B6, $3587\n");
    free(pack);
    return 0;
}

/*
 * ROM defensive jump lift, from the $6BF9->$6C27->$6C4D disassembly.
 *
 * $6A8C reaches $6BF9 for the eight protected actions, and $6C27 adds
 * $6C4D[+$03] into player +$05.  $2945 writes +$05 straight to OAM Y, so the
 * jump is what physically lifts the sprite; the ground anchor +$15 stays
 * where it was, which is why $2B6C reads reach back as +$15 - +$05 and why
 * $077D's planar gate keeps testing the floor position.
 *
 * Without the lift a defensive jump is invisible AND immobile for its full
 * 72 frames, which reads as the game having locked up.
 */
int allstar_cli_test_defense_jump_rom(void) {
    static const int rom_6c4d[12] = {
        0, -9, -7, -5, -3, -2, 0, 2, 5, 7, 8, 4
    };
    static const float cumulative[12] = {
        0.0f, 9.0f, 16.0f, 21.0f, 24.0f, 26.0f,
        26.0f, 24.0f, 19.0f, 12.0f, 4.0f, 0.0f
    };
    AllStarGame game;
    AllStarOneOnOneDebugState debug;
    float peak_lift = 0.0f;
    float ground_y = 0.0f;
    float ground_x = 0.0f;
    bool ground_moved = false;
    bool descended = false;
    int frame;
    int record;
    int running = 0;

    printf("[Test] Running ROM Defensive Jump Tests ($6C27/$6C4D)...\n");

    /* $6C4D is twelve signed bytes, and the lift is their running sum. */
    for (record = 0; record < 12; record++) {
        if (allstar_one_on_one_rom_jump_lift_delta_6c4d((uint8_t)record) !=
            rom_6c4d[record]) {
            fprintf(stderr, "[Test] $6C4D+%d is %d, expected %d\n", record,
                    allstar_one_on_one_rom_jump_lift_delta_6c4d(
                        (uint8_t)record),
                    rom_6c4d[record]);
            return 1;
        }
        running += rom_6c4d[record];
        if (allstar_one_on_one_rom_jump_lift_6c27(
                0x0cu, (uint8_t)record) != (float)-running ||
            (float)-running != cumulative[record]) {
            fprintf(stderr,
                    "[Test] $6C27 lift at record %d is %.0f, expected %.0f\n",
                    record,
                    allstar_one_on_one_rom_jump_lift_6c27(
                        0x0cu, (uint8_t)record),
                    cumulative[record]);
            return 1;
        }
    }
    /* The arc leaves the floor and comes back to it. */
    if (allstar_one_on_one_rom_jump_lift_6c27(0x05u, 0u) != 0.0f ||
        allstar_one_on_one_rom_jump_lift_6c27(0x05u, 11u) != 0.0f ||
        allstar_one_on_one_rom_jump_lift_6c27(0x05u, 5u) != 26.0f) {
        fprintf(stderr, "[Test] $6C4D arc does not start, peak and land\n");
        return 1;
    }
    /* All three jump actions share the twelve records; $6BF9 gates the rest
       out, so nothing else is ever lifted by $6C27. */
    if (allstar_one_on_one_rom_jump_lift_6c27(0x14u, 5u) != 26.0f ||
        allstar_one_on_one_rom_jump_lift_6c27(0x06u, 5u) != 0.0f ||
        allstar_one_on_one_rom_jump_lift_6c27(
            ALLSTAR_ROM_SHOT_ACTION_A, 5u) != 0.0f ||
        allstar_one_on_one_rom_jump_lift_6c27(0x0du, 5u) != 0.0f) {
        fprintf(stderr, "[Test] $6BF9 gating admitted a non-jump action\n");
        return 1;
    }
    /* $2B6C reads the same displacement, six frames to the record. */
    for (record = 0; record < 12; record++) {
        if (allstar_one_on_one_rom_jump_height_6c4d((uint16_t)(record * 6)) !=
            cumulative[record]) {
            fprintf(stderr,
                    "[Test] $2B6C reach at frame %d disagrees with $6C4D\n",
                    record * 6);
            return 1;
        }
    }

    /* The scene has to actually put that lift on the sprite. */
    if (!allstar_game_init(&game, NULL)) {
        fprintf(stderr, "[Test] Could not initialise the game\n");
        return 1;
    }
    allstar_game_change_scene(&game, ALLSTAR_SCENE_ONE_ON_ONE);
    allstar_scene_one_on_one_set_test_possession(game.active_scene, &game, 2);
    allstar_scene_one_on_one_set_test_positions(
        game.active_scene, 60.0f, 128.0f, 100.0f, 128.0f);
    allstar_input_update(&game.input, 0);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    ground_x = debug.p1_x;
    ground_y = debug.p1_y;
    if (debug.p1_defense_jump_lift != 0.0f) {
        fprintf(stderr, "[Test] A standing defender is already airborne\n");
        allstar_game_shutdown(&game);
        return 1;
    }

    /* $2639 bit 0 is A, and $702D takes the $70FD branch while +$09 says the
       player does not own the ball. */
    allstar_input_update(&game.input, ALLSTAR_BTN_A);
    allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_input_update(&game.input, 0);
    for (frame = 0; frame < ALLSTAR_ROM_DEFENSE_JUMP_FRAMES; frame++) {
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
        allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
        if (!debug.p1_defense_jump_active) break;
        if (debug.p1_defense_jump_lift > peak_lift)
            peak_lift = debug.p1_defense_jump_lift;
        if (peak_lift > 0.0f &&
            debug.p1_defense_jump_lift < peak_lift) descended = true;
        /* $077D never leaves the floor, so neither may the anchor. */
        if (debug.p1_y != ground_y || debug.p1_x != ground_x)
            ground_moved = true;
    }
    if (peak_lift != 26.0f) {
        fprintf(stderr,
                "[Test] Defensive jump peaked at %.0f, expected the $6C4D "
                "26 -- the defender never left the floor\n", peak_lift);
        allstar_game_shutdown(&game);
        return 1;
    }
    if (!descended) {
        fprintf(stderr, "[Test] Defensive jump never came back down\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    if (ground_moved) {
        fprintf(stderr,
                "[Test] $6C27 moved the $077D/+$15 anchor as well as +$05\n");
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (debug.p1_defense_jump_lift != 0.0f) {
        fprintf(stderr, "[Test] The defender landed %.0f above the floor\n",
                debug.p1_defense_jump_lift);
        allstar_game_shutdown(&game);
        return 1;
    }

    /* $6BF9 leaves the jump family through the twelfth record's $02 control
       byte, so the landed defender has to move again. */
    allstar_input_update(&game.input, ALLSTAR_BTN_LEFT);
    for (frame = 0; frame < 40; frame++)
        allstar_game_tick(&game, ALLSTAR_PHYSICS_STEP_SECONDS);
    allstar_scene_one_on_one_get_debug_state(game.active_scene, &debug);
    if (debug.p1_x >= ground_x || debug.p1_action == 0x05 ||
        debug.p1_action == 0x0c || debug.p1_action == 0x14) {
        fprintf(stderr,
                "[Test] Defender stayed stuck in the jump family "
                "(action=$%02X x=%.0f->%.0f)\n",
                debug.p1_action, ground_x, debug.p1_x);
        allstar_game_shutdown(&game);
        return 1;
    }
    allstar_game_shutdown(&game);

    printf("  $6C4D lifts the sprite 0-9-16-21-24-26-26-24-19-12-4-0 and "
           "lands it\n");
    printf("  the +$15 anchor $077D reads stays on the floor throughout\n");
    printf("[Test] PASSED: $6C27, $6C4D, $6BF9\n");
    return 0;
}

/*
 * ROM joypad poll, from the $2639..$267D disassembly.
 */
int allstar_cli_test_pad_rom(void) {
    AllStarPadDispatch dispatch;

    printf("[Test] Running ROM Joypad Tests ($2639)...\n");

    /* $263D and $264B settle the two rows differently. */
    if (allstar_pad_settle_reads(ALLSTAR_PAD_SELECT_DIRECTIONS) != 2 ||
        allstar_pad_settle_reads(ALLSTAR_PAD_SELECT_BUTTONS) != 6) {
        fprintf(stderr, "[Test] $264B settle counts diverged\n");
        return 1;
    }

    /* $2641: both rows are active-low, so nothing pressed reads as $00. */
    if (allstar_pad_assemble(0x0Fu, 0x0Fu) != 0x00u) {
        fprintf(stderr, "[Test] $2642 idle pad did not read zero\n");
        return 1;
    }
    if (allstar_pad_assemble(0x00u, 0x00u) != 0xFFu) {
        fprintf(stderr, "[Test] $2658 all-pressed did not read $FF\n");
        return 1;
    }

    /*
     * $2644: the direction row is swapped into the high nibble and the button
     * row stays low.  That is what puts A at bit 0 and Right at bit 4.
     */
    if (allstar_pad_assemble(0x0Fu, 0x0Eu) != ALLSTAR_PAD_A) {
        fprintf(stderr, "[Test] A landed at $%02X, expected bit 0\n",
                allstar_pad_assemble(0x0Fu, 0x0Eu));
        return 1;
    }
    if (allstar_pad_assemble(0x0Fu, 0x07u) != ALLSTAR_PAD_START) {
        fprintf(stderr, "[Test] Start landed at $%02X, expected bit 3\n",
                allstar_pad_assemble(0x0Fu, 0x07u));
        return 1;
    }
    if (allstar_pad_assemble(0x0Eu, 0x0Fu) != ALLSTAR_PAD_RIGHT) {
        fprintf(stderr, "[Test] Right landed at $%02X, expected bit 4\n",
                allstar_pad_assemble(0x0Eu, 0x0Fu));
        return 1;
    }
    if (allstar_pad_assemble(0x07u, 0x0Fu) != ALLSTAR_PAD_DOWN) {
        fprintf(stderr, "[Test] Down landed at $%02X, expected bit 7\n",
                allstar_pad_assemble(0x07u, 0x0Fu));
        return 1;
    }

    /*
     * The masks other routines use have to agree with that layout, or every
     * one of them is reading the wrong buttons.
     */
    if (ALLSTAR_SELECT_CONFIRM_MASK != (ALLSTAR_PAD_SELECT | ALLSTAR_PAD_START)) {
        fprintf(stderr, "[Test] $410F does not confirm on Select or Start\n");
        return 1;
    }
    if (ALLSTAR_SELECT_MOVE_MASK !=
        (ALLSTAR_PAD_A | ALLSTAR_PAD_B | ALLSTAR_PAD_RIGHT | ALLSTAR_PAD_LEFT)) {
        fprintf(stderr, "[Test] $4119 does not move on A, B, Right or Left\n");
        return 1;
    }
    if (ALLSTAR_SELECT_BACK_MASK != (ALLSTAR_PAD_B | ALLSTAR_PAD_LEFT)) {
        fprintf(stderr, "[Test] $4127 does not step back on B or Left\n");
        return 1;
    }
    if (ALLSTAR_COURT_PAUSE_BUTTON != ALLSTAR_PAD_START) {
        fprintf(stderr, "[Test] $2BC6 does not pause on Start\n");
        return 1;
    }
    if (ALLSTAR_SETTINGS_UP_MASK != ALLSTAR_PAD_UP ||
        ALLSTAR_SETTINGS_DOWN_MASK != ALLSTAR_PAD_DOWN) {
        fprintf(stderr, "[Test] $233A does not move on Up and Down\n");
        return 1;
    }
    if (ALLSTAR_MENU_CONFIRM_MASK != ALLSTAR_PAD_START) {
        fprintf(stderr, "[Test] $03D4 does not confirm on Start\n");
        return 1;
    }
    if (ALLSTAR_RESET_COMBO !=
        (ALLSTAR_PAD_A | ALLSTAR_PAD_B | ALLSTAR_PAD_SELECT | ALLSTAR_PAD_START)) {
        fprintf(stderr, "[Test] $2D25 is not the four-button combo\n");
        return 1;
    }

    /* $2663: a solo game keeps the byte and sends nothing. */
    allstar_pad_dispatch(0u, 0u, 0x3Cu, 0x11u, &dispatch);
    if (dispatch.route != ALLSTAR_PAD_ROUTE_LOCAL || dispatch.stores_outgoing ||
        dispatch.calls_link_update) {
        fprintf(stderr, "[Test] $2664 solo path diverged\n");
        return 1;
    }

    /*
     * $2666: the fresh byte is swapped into $C16E and the previous one carried
     * on.  $C16E is exactly what $2FD0 transmits for roles $02 and $03.
     */
    allstar_pad_dispatch(0x01u, 0u, 0x3Cu, 0x11u, &dispatch);
    if (!dispatch.stores_outgoing || dispatch.outgoing != 0x3Cu ||
        dispatch.carried != 0x11u) {
        fprintf(stderr, "[Test] $266E swap diverged, out $%02X carried $%02X\n",
                dispatch.outgoing, dispatch.carried);
        return 1;
    }
    {
        AllStarLinkTransmit tx;
        allstar_link_transmit(1u, 0x02u, dispatch.outgoing, 0u, &tx);
        if (tx.kind != ALLSTAR_LINK_TX_STATE || tx.byte != 0x3Cu) {
            fprintf(stderr, "[Test] $2FF1 did not send the byte $2639 stored\n");
            return 1;
        }
    }

    /* $266F: the link update only runs in a link game. */
    if (dispatch.calls_link_update) {
        fprintf(stderr, "[Test] $2673 ran the link update outside a link game\n");
        return 1;
    }
    allstar_pad_dispatch(0x01u, 1u, 0x3Cu, 0x11u, &dispatch);
    if (!dispatch.calls_link_update || dispatch.route != ALLSTAR_PAD_ROUTE_LINK) {
        fprintf(stderr, "[Test] $2673 link update diverged\n");
        return 1;
    }
    /* $2679: role $03 takes the other exit. */
    allstar_pad_dispatch(0x03u, 1u, 0x3Cu, 0x11u, &dispatch);
    if (dispatch.route != ALLSTAR_PAD_ROUTE_LINK_ROLE_3) {
        fprintf(stderr, "[Test] $267B role 3 exit diverged\n");
        return 1;
    }

    printf("  directions swap into the high nibble, buttons stay low: A is bit 0, Right bit 4\n");
    printf("  every raw mask in the port is checked against that layout, not just described\n");
    printf("[Test] PASSED: $2639\n");
    return 0;
}

/*
 * ROM game session driver, from the $0214..$0265 disassembly.
 */
int allstar_cli_test_session_rom(void) {
    static const AllStarSessionStep FULL[9] = {
        ALLSTAR_SESSION_CLEAR_FLAG, ALLSTAR_SESSION_MENU, ALLSTAR_SESSION_SETTINGS,
        ALLSTAR_SESSION_LOAD_TILES, ALLSTAR_SESSION_PICK_PLAYER, ALLSTAR_SESSION_PREPARE,
        ALLSTAR_SESSION_RUN_MODE, ALLSTAR_SESSION_POSTGAME, ALLSTAR_SESSION_RESET
    };
    AllStarSessionStep steps[12];
    const uint16_t *modes;
    int count;
    int i;
    int j;

    printf("[Test] Running ROM Session Tests ($0214)...\n");

    /* $023D: the tournament is the only mode that skips the player pick. */
    if (!allstar_session_picks_player(0x00u) || !allstar_session_picks_player(0x01u) ||
        !allstar_session_picks_player(0x02u) || !allstar_session_picks_player(0x03u) ||
        allstar_session_picks_player(ALLSTAR_SESSION_TOURNAMENT)) {
        fprintf(stderr, "[Test] $023B player-pick gate diverged\n");
        return 1;
    }

    /* Every other mode runs all nine steps in order. */
    count = allstar_session_sequence(0x00u, steps, 12);
    if (count != 9) {
        fprintf(stderr, "[Test] $0214 ran %d steps, expected 9\n", count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (steps[i] != FULL[i]) {
            fprintf(stderr, "[Test] $0214 step %d was %s, expected %s\n", i,
                    allstar_session_step_name(steps[i]),
                    allstar_session_step_name(FULL[i]));
            return 1;
        }
    }

    /* The tournament runs eight, with the pick dropped and nothing reordered. */
    count = allstar_session_sequence(ALLSTAR_SESSION_TOURNAMENT, steps, 12);
    if (count != 8) {
        fprintf(stderr, "[Test] tournament ran %d steps, expected 8\n", count);
        return 1;
    }
    j = 0;
    for (i = 0; i < 9; i++) {
        if (FULL[i] == ALLSTAR_SESSION_PICK_PLAYER) continue;
        if (steps[j] != FULL[i]) {
            fprintf(stderr, "[Test] tournament step %d diverged\n", j);
            return 1;
        }
        j++;
    }

    /* The menu and settings run from bank 3, the pick from bank 2. */
    if (allstar_session_bank(ALLSTAR_SESSION_MENU) != 3u ||
        allstar_session_bank(ALLSTAR_SESSION_SETTINGS) != 3u ||
        allstar_session_bank(ALLSTAR_SESSION_PICK_PLAYER) != 2u ||
        allstar_session_bank(ALLSTAR_SESSION_RUN_MODE) != 1u ||
        allstar_session_bank(ALLSTAR_SESSION_POSTGAME) != 1u) {
        fprintf(stderr, "[Test] $0218/$023F bank switching diverged\n");
        return 1;
    }

    /*
     * $0255 dispatches through the same $0267 table the tournament driver
     * documents, and $0263 hands control to the reset vector rather than
     * returning -- so a finished game wipes RAM every time.
     */
    if (ALLSTAR_SESSION_MODE_TABLE != 0x0267u) {
        fprintf(stderr, "[Test] $0255 mode table address diverged\n");
        return 1;
    }
    if (ALLSTAR_SESSION_RESET_VECTOR != 0x0156u) {
        fprintf(stderr, "[Test] $0263 must return to the reset vector\n");
        return 1;
    }
    /* That vector is the one the boot path treats as a soft reset. */
    if (allstar_boot_entry(false, 0u, 0u) != ALLSTAR_BOOT_RESET) {
        fprintf(stderr, "[Test] $0156 is not the boot path's reset entry\n");
        return 1;
    }

    /* The mode the session dispatches is the mode the postgame table indexes. */
    modes = allstar_postgame_mode_table(&count);
    if (count != 5) {
        fprintf(stderr, "[Test] the mode space is not five wide\n");
        return 1;
    }
    if (ALLSTAR_SESSION_TOURNAMENT >= (uint8_t)count) {
        fprintf(stderr, "[Test] the tournament index falls outside the mode space\n");
        return 1;
    }
    if (modes[ALLSTAR_SESSION_TOURNAMENT] != 0x12A6u) {
        fprintf(stderr, "[Test] the tournament postgame handler moved\n");
        return 1;
    }

    printf("  every mode but the tournament picks one player through the bank 2 selector\n");
    printf("  a finished game jumps to $0156, so the RAM wipe runs after every game\n");
    printf("[Test] PASSED: $0214\n");
    return 0;
}

/*
 * ROM two-player handshake and attract entry, from the $0322..$0383 and
 * $0417..$0443 disassembly.
 */
int allstar_cli_test_handshake_rom(void) {
    AllStarHandshakeAccept accept;
    AllStarHandshakeReady ready;
    AllStarAttract attract;
    uint8_t attempts;
    uint8_t role;
    uint8_t echo;

    printf("[Test] Running ROM Handshake Tests ($0324/$035F/$0417)...\n");

    /* $0331: the peer answering straight away skips the middle exchange. */
    attempts = ALLSTAR_HANDSHAKE_ATTEMPTS;
    role = 0u; echo = 0xFFu;
    if (allstar_handshake_step(0x01u, 0x00u, 0x02u, &attempts, &role, &echo)
            != ALLSTAR_HANDSHAKE_AGREED ||
        role != ALLSTAR_HANDSHAKE_ROLE_LEAD || echo != 0x00u ||
        attempts != ALLSTAR_HANDSHAKE_ATTEMPTS) {
        fprintf(stderr, "[Test] $0358 fast agreement diverged, role %u echo %u\n", role, echo);
        return 1;
    }

    /* $0335: otherwise it echoes a one and looks again. */
    attempts = ALLSTAR_HANDSHAKE_ATTEMPTS;
    if (allstar_handshake_step(0x00u, 0x01u, 0x02u, &attempts, &role, &echo)
            != ALLSTAR_HANDSHAKE_AGREED || echo != 0x01u) {
        fprintf(stderr, "[Test] $033A echo diverged, got %u\n", echo);
        return 1;
    }

    /* $0343: a failure at the middle reading aborts without spending a try. */
    attempts = ALLSTAR_HANDSHAKE_ATTEMPTS;
    if (allstar_handshake_step(0x00u, 0x00u, 0x02u, &attempts, &role, &echo)
            != ALLSTAR_HANDSHAKE_ABORT ||
        attempts != ALLSTAR_HANDSHAKE_ATTEMPTS) {
        fprintf(stderr, "[Test] $0343 abort spent an attempt\n");
        return 1;
    }

    /* $0353: a failure at the last reading does spend one and retries. */
    attempts = 3u;
    if (allstar_handshake_step(0x01u, 0x00u, 0x00u, &attempts, &role, &echo)
            != ALLSTAR_HANDSHAKE_RETRY || attempts != 2u) {
        fprintf(stderr, "[Test] $0354 retry diverged, attempts %u\n", attempts);
        return 1;
    }
    /* The tenth failure aborts rather than retrying. */
    attempts = 1u;
    if (allstar_handshake_step(0x01u, 0x00u, 0x00u, &attempts, &role, &echo)
            != ALLSTAR_HANDSHAKE_ABORT || attempts != 0u) {
        fprintf(stderr, "[Test] $0356 last attempt did not abort\n");
        return 1;
    }
    /* Every attempt re-claims role $01 before anything else. */
    attempts = 5u; role = 0xFFu;
    allstar_handshake_step(0x01u, 0x00u, 0x00u, &attempts, &role, &echo);
    if (role != ALLSTAR_HANDSHAKE_ROLE_INIT) {
        fprintf(stderr, "[Test] $032A did not reclaim role 1 on a retry\n");
        return 1;
    }

    /*
     * $035F: the receiving side takes role $03, which is the value $267F keys
     * on to put the byte it receives into pad 1.
     */
    allstar_handshake_accept(&accept);
    if (accept.role != ALLSTAR_HANDSHAKE_ROLE_JOIN || accept.sends != 0x00u ||
        !accept.unwinds_caller) {
        fprintf(stderr, "[Test] $036C accept path diverged\n");
        return 1;
    }
    /* The two sides must not land on the same role. */
    if (ALLSTAR_HANDSHAKE_ROLE_LEAD == ALLSTAR_HANDSHAKE_ROLE_JOIN) {
        fprintf(stderr, "[Test] both sides took the same role\n");
        return 1;
    }

    /* $0376: both success paths agree on two players. */
    allstar_handshake_ready(&ready);
    if (ready.ffa5 != 0x01u || ready.ffbe != 0x01u ||
        ready.players != ALLSTAR_HANDSHAKE_PLAYERS) {
        fprintf(stderr, "[Test] $037D ready state diverged\n");
        return 1;
    }

    /* $0417: the attract setup, including the countdown $2D1B consumes. */
    allstar_attract_setup(&attract);
    if (attract.mode != 0x00u || attract.link_state != 0x00u ||
        attract.countdown != ALLSTAR_ATTRACT_COUNTDOWN ||
        attract.seed != ALLSTAR_ATTRACT_SEED ||
        attract.skill != ALLSTAR_ATTRACT_SKILL ||
        attract.bank != ALLSTAR_ATTRACT_BANK ||
        attract.selector != ALLSTAR_ATTRACT_SELECTOR) {
        fprintf(stderr, "[Test] $0417 attract setup diverged\n");
        return 1;
    }
    /* Attract runs at the easiest skill, which $761B reads as index zero. */
    if (allstar_cpu_threshold(allstar_cpu_threshold_table(0x7626u), attract.skill) != 0x1Bu) {
        fprintf(stderr, "[Test] $0436 attract skill does not select the first threshold\n");
        return 1;
    }
    /* The countdown really is the one the watchdog spends. */
    {
        uint16_t countdown = attract.countdown;
        if (allstar_watchdog(1u, 0u, 0u, 1u, 0u, &countdown) != ALLSTAR_WATCHDOG_CONTINUE ||
            countdown != (uint16_t)(ALLSTAR_ATTRACT_COUNTDOWN - 1u)) {
            fprintf(stderr, "[Test] $2D39 did not consume the $0420 countdown\n");
            return 1;
        }
    }

    printf("  the side that starts becomes role $02 and the side that answers role $03\n");
    printf("  attract loads $0E10 into the very counter $2D1B spends, at the easiest skill\n");
    printf("[Test] PASSED: $0322, $0324, $035F, $0376, $0417\n");
    return 0;
}

/*
 * ROM boot path and title selector, from the $0150..$0212 and $029C..$030D
 * disassembly.
 */
int allstar_cli_test_boot_rom(void) {
    static const uint16_t PRESERVED[2] = { 0xFFFBu, 0xFFFDu };
    const AllStarBootRegion *regions;
    const uint16_t *preserved;
    AllStarTitleConfirm confirm;
    uint8_t players;
    uint16_t countdown;
    int count;
    int i;

    printf("[Test] Running ROM Boot Tests ($0150/$0156/$02AC)...\n");

    /* $0187/$0195/$01B6: three regions, in this order. */
    regions = allstar_boot_cleared(&count);
    if (count != ALLSTAR_BOOT_REGIONS ||
        regions[0].start != 0xC000u || regions[0].length != 0x02D0u ||
        regions[1].start != 0xDD72u || regions[1].length != 0x00BDu ||
        regions[2].start != 0xFF80u || regions[2].length != 0x007Eu) {
        fprintf(stderr, "[Test] $018D boot clear regions diverged\n");
        return 1;
    }

    /*
     * The RNG seed lives inside the HRAM wipe, which is exactly why $01A8
     * pushes it first.  If it did not, a soft reset would replay the same game.
     */
    preserved = allstar_boot_preserved(&count);
    if (count != 2 || preserved[0] != PRESERVED[0] || preserved[1] != PRESERVED[1]) {
        fprintf(stderr, "[Test] $01A8 preserved words diverged\n");
        return 1;
    }
    for (i = 0; i < 2; i++) {
        if (preserved[i] < regions[2].start ||
            preserved[i] >= (uint16_t)(regions[2].start + regions[2].length)) {
            fprintf(stderr, "[Test] $%04X should sit inside the HRAM wipe\n", preserved[i]);
            return 1;
        }
    }

    /* $0156: only a link game in role $02 announces its reset. */
    if (allstar_boot_entry(true, 1u, ALLSTAR_BOOT_LINK_ROLE) != ALLSTAR_BOOT_COLD) {
        fprintf(stderr, "[Test] $0150 cold entry diverged\n");
        return 1;
    }
    if (allstar_boot_entry(false, 0u, ALLSTAR_BOOT_LINK_ROLE) != ALLSTAR_BOOT_RESET) {
        fprintf(stderr, "[Test] $015A announced outside a link game\n");
        return 1;
    }
    if (allstar_boot_entry(false, 1u, 0x01u) != ALLSTAR_BOOT_RESET) {
        fprintf(stderr, "[Test] $0161 announced from the wrong role\n");
        return 1;
    }
    if (allstar_boot_entry(false, 1u, ALLSTAR_BOOT_LINK_ROLE) != ALLSTAR_BOOT_RESET_NOTIFY) {
        fprintf(stderr, "[Test] $0163 did not announce the reset\n");
        return 1;
    }

    /* $02E4: the player count toggles between one and two. */
    players = 1u;
    countdown = ALLSTAR_TITLE_TIMEOUT;
    if (allstar_title_step(0x01u, &players, &countdown) != ALLSTAR_TITLE_TOGGLED ||
        players != 2u) {
        fprintf(stderr, "[Test] $02EA toggle to two diverged, got %u\n", players);
        return 1;
    }
    if (allstar_title_step(0x02u, &players, &countdown) != ALLSTAR_TITLE_TOGGLED ||
        players != 1u) {
        fprintf(stderr, "[Test] $02EA toggle back diverged, got %u\n", players);
        return 1;
    }
    /* Toggling does not spend the timeout. */
    if (countdown != ALLSTAR_TITLE_TIMEOUT) {
        fprintf(stderr, "[Test] $02E1 spent the countdown on a toggle\n");
        return 1;
    }
    /* A button outside the $33 mask neither toggles nor confirms. */
    if (allstar_title_step(0x40u, &players, &countdown) != ALLSTAR_TITLE_WAITING ||
        players != 1u || countdown != (uint16_t)(ALLSTAR_TITLE_TIMEOUT - 1u)) {
        fprintf(stderr, "[Test] $02DF accepted a button outside the mask\n");
        return 1;
    }

    /* $02FE: Start confirms. */
    if (allstar_title_step(ALLSTAR_TITLE_START_MASK, &players, &countdown)
            != ALLSTAR_TITLE_CONFIRMED) {
        fprintf(stderr, "[Test] $0301 Start did not confirm\n");
        return 1;
    }

    /* $0304: the counter running out drops into attract. */
    countdown = 2u;
    if (allstar_title_step(0x00u, &players, &countdown) != ALLSTAR_TITLE_WAITING ||
        countdown != 1u) {
        fprintf(stderr, "[Test] $0304 countdown diverged\n");
        return 1;
    }
    if (allstar_title_step(0x00u, &players, &countdown) != ALLSTAR_TITLE_ATTRACT) {
        fprintf(stderr, "[Test] $030B did not enter attract\n");
        return 1;
    }

    /* $0318: two players start the link handshake and take role $01. */
    allstar_title_confirm(1u, &confirm);
    if (confirm.starts_link || confirm.role != 0u) {
        fprintf(stderr, "[Test] $031C one player must not start the link\n");
        return 1;
    }
    allstar_title_confirm(2u, &confirm);
    if (!confirm.starts_link || confirm.role != 0x01u || confirm.attempts != 0x0Au) {
        fprintf(stderr, "[Test] $0322 two-player handshake diverged\n");
        return 1;
    }

    printf("  the RNG seed is pushed across the HRAM wipe, so resets do not replay a game\n");
    printf("  a link reset posts $C3 to $C18E, the same mailbox pause uses for $CC\n");
    printf("[Test] PASSED: $0150, $0156, $029C, $02AC\n");
    return 0;
}

/*
 * ROM APU channel programmer, from the $35B6..$3714 disassembly and the
 * tables at $3151, $3159, $3777 and $3FB2.
 */
int allstar_cli_test_apu_program_rom(void) {
    const AllStarApuChannel *channel;
    const uint16_t *regs;
    uint8_t mask;
    uint8_t cached;
    uint16_t source;
    int count;
    int i;

    printf("[Test] Running ROM APU Programmer Tests ($35B6/$3151/$366F)...\n");

    /* $35E0: only $00, $01 and $02 are named; everything else is noise. */
    if (allstar_apu_kind(0x00u) != ALLSTAR_APU_SQUARE_1 ||
        allstar_apu_kind(0x01u) != ALLSTAR_APU_SQUARE_2 ||
        allstar_apu_kind(0x02u) != ALLSTAR_APU_WAVE ||
        allstar_apu_kind(0x03u) != ALLSTAR_APU_NOISE ||
        allstar_apu_kind(0xFFu) != ALLSTAR_APU_NOISE) {
        fprintf(stderr, "[Test] $35ED kind dispatch diverged\n");
        return 1;
    }

    /* $3151 is one bit per kind. */
    for (i = 0; i < 8; i++) {
        if (allstar_apu_claim_bit((uint8_t)i) != (uint8_t)(1u << i)) {
            fprintf(stderr, "[Test] $3151 bit %d diverged\n", i);
            return 1;
        }
    }

    /*
     * $35BB: the arbitration.  A slot below four stands down when the bit is
     * already set; a slot four or above takes the channel.
     */
    mask = 0x00u;
    if (allstar_apu_claim(0u, 0x00u, &mask) != ALLSTAR_APU_CLAIM_FREE || mask != 0x00u) {
        fprintf(stderr, "[Test] $35C9 low slot on a free channel diverged\n");
        return 1;
    }
    if (allstar_apu_claim(4u, 0x00u, &mask) != ALLSTAR_APU_CLAIM_TAKEN || mask != 0x01u) {
        fprintf(stderr, "[Test] $35D5 high slot did not take the channel, mask $%02X\n", mask);
        return 1;
    }
    if (allstar_apu_claim(0u, 0x00u, &mask) != ALLSTAR_APU_CLAIM_BLOCKED || mask != 0x01u) {
        fprintf(stderr, "[Test] $35CB low slot did not stand down\n");
        return 1;
    }
    /* A different kind is a different bit, so it is unaffected. */
    if (allstar_apu_claim(0u, 0x02u, &mask) != ALLSTAR_APU_CLAIM_FREE) {
        fprintf(stderr, "[Test] $35C8 blocked the wrong channel\n");
        return 1;
    }
    /* A high slot re-taking an already-set bit is still TAKEN, not blocked. */
    if (allstar_apu_claim(7u, 0x00u, &mask) != ALLSTAR_APU_CLAIM_TAKEN) {
        fprintf(stderr, "[Test] $35CC high slot must never be blocked\n");
        return 1;
    }

    /* $35FC and its mirrors: each channel keeps a different pair of NR51 bits. */
    if (!allstar_apu_channel(ALLSTAR_APU_SQUARE_1, &channel) || channel->nr51_keep != 0xEEu ||
        !allstar_apu_channel(ALLSTAR_APU_SQUARE_2, &channel) || channel->nr51_keep != 0xDDu ||
        !allstar_apu_channel(ALLSTAR_APU_WAVE, &channel) || channel->nr51_keep != 0xBBu ||
        !allstar_apu_channel(ALLSTAR_APU_NOISE, &channel) || channel->nr51_keep != 0x77u) {
        fprintf(stderr, "[Test] NR51 keep masks diverged\n");
        return 1;
    }
    /* Square 2 and noise read one byte later, because they have no sweep. */
    allstar_apu_channel(ALLSTAR_APU_SQUARE_1, &channel);
    if (channel->block != 0x388Au) {
        fprintf(stderr, "[Test] $360A square 1 block diverged\n");
        return 1;
    }
    allstar_apu_channel(ALLSTAR_APU_SQUARE_2, &channel);
    if (channel->block != 0x388Bu) {
        fprintf(stderr, "[Test] $364B square 2 block diverged\n");
        return 1;
    }

    /* $35F5: the panning index is bits 3 and 2. */
    if (allstar_apu_pan_index(0x00u) != 0u || allstar_apu_pan_index(0x04u) != 1u ||
        allstar_apu_pan_index(0x08u) != 2u || allstar_apu_pan_index(0x0Cu) != 3u ||
        allstar_apu_pan_index(0xF3u) != 0u) {
        fprintf(stderr, "[Test] $35F7 pan index diverged\n");
        return 1;
    }
    /* $3777: silent, right, left, both -- one bit per channel in each nibble. */
    if (allstar_apu_pan_value(ALLSTAR_APU_SQUARE_1, 3u) != 0x11u ||
        allstar_apu_pan_value(ALLSTAR_APU_SQUARE_2, 3u) != 0x22u ||
        allstar_apu_pan_value(ALLSTAR_APU_WAVE, 3u) != 0x44u ||
        allstar_apu_pan_value(ALLSTAR_APU_NOISE, 3u) != 0x88u ||
        allstar_apu_pan_value(ALLSTAR_APU_SQUARE_1, 0u) != 0x00u ||
        allstar_apu_pan_value(ALLSTAR_APU_NOISE, 1u) != 0x08u) {
        fprintf(stderr, "[Test] pan tables diverged\n");
        return 1;
    }
    /* The update clears only this channel's two bits and leaves the rest. */
    if (allstar_apu_nr51(ALLSTAR_APU_SQUARE_1, 0xFFu, 0x00u) != 0xEEu) {
        fprintf(stderr, "[Test] $3603 silent update diverged\n");
        return 1;
    }
    if (allstar_apu_nr51(ALLSTAR_APU_SQUARE_1, 0x00u, 0x0Cu) != 0x11u) {
        fprintf(stderr, "[Test] $3602 both-sides update diverged\n");
        return 1;
    }
    if (allstar_apu_nr51(ALLSTAR_APU_WAVE, 0x22u, 0x04u) != 0x26u) {
        fprintf(stderr, "[Test] $36AA wave update disturbed another channel\n");
        return 1;
    }

    /* The register order, with a marker where the ROM skips a block byte. */
    regs = allstar_apu_registers(ALLSTAR_APU_SQUARE_1, &count);
    if (count != 6 || regs[0] != 0xFF10u || regs[3] != 0x0000u || regs[5] != 0xFF14u) {
        fprintf(stderr, "[Test] $360E square 1 register order diverged\n");
        return 1;
    }
    regs = allstar_apu_registers(ALLSTAR_APU_SQUARE_2, &count);
    if (count != 5 || regs[0] != 0xFF16u || regs[2] != 0x0000u || regs[4] != 0xFF19u) {
        fprintf(stderr, "[Test] $364F square 2 register order diverged\n");
        return 1;
    }
    regs = allstar_apu_registers(ALLSTAR_APU_WAVE, &count);
    if (count != 6 || regs[0] != 0xFF1Au || regs[5] != 0xFF1Eu) {
        fprintf(stderr, "[Test] $36B6 wave register order diverged\n");
        return 1;
    }
    regs = allstar_apu_registers(ALLSTAR_APU_NOISE, &count);
    if (count != 4 || regs[0] != 0xFF20u || regs[3] != 0xFF23u) {
        fprintf(stderr, "[Test] $36F9 noise register order diverged\n");
        return 1;
    }

    /* $366F: the waveform only uploads on a change, and the stride is sixteen. */
    cached = 0xFFu;
    if (!allstar_apu_wave_upload(0x03u, &cached, &source) || cached != 0x03u ||
        source != (uint16_t)(ALLSTAR_APU_WAVE_BANK + 0x30u)) {
        fprintf(stderr, "[Test] $3685 wave source diverged, got $%04X\n", source);
        return 1;
    }
    if (allstar_apu_wave_upload(0x03u, &cached, &source)) {
        fprintf(stderr, "[Test] $367A re-uploaded an unchanged waveform\n");
        return 1;
    }
    /* Only the low nibble is the id. */
    if (allstar_apu_wave_upload(0xB3u, &cached, &source)) {
        fprintf(stderr, "[Test] $3674 did not mask the waveform id\n");
        return 1;
    }
    if (!allstar_apu_wave_upload(0x00u, &cached, &source) ||
        source != ALLSTAR_APU_WAVE_BANK) {
        fprintf(stderr, "[Test] $3682 waveform zero diverged\n");
        return 1;
    }

    /* The frequency-high write keeps only the top five bits. */
    if (allstar_apu_frequency_high(0xFFu, 0x00u) != 0xF8u ||
        allstar_apu_frequency_high(0x07u, 0x00u) != 0x00u ||
        allstar_apu_frequency_high(0x80u, 0x07u) != 0x87u) {
        fprintf(stderr, "[Test] $3619 frequency-high diverged\n");
        return 1;
    }
    /* The noise control keeps only the bottom four. */
    if (allstar_apu_noise_control(0xFFu, 0x00u) != 0x0Fu ||
        allstar_apu_noise_control(0x0Au, 0x70u) != 0x7Au) {
        fprintf(stderr, "[Test] $3703 noise control diverged\n");
        return 1;
    }

    printf("  slots four and up take a channel through $DD7D; lower slots stand down\n");
    printf("  the waveform uploads sixteen bytes to $FF30 only when its id changes\n");
    printf("[Test] PASSED: $35B6, $3151, $3159, $3777, $366F, $36DB\n");
    return 0;
}

/*
 * ROM CPU steering, from the bank 1 $7182, $7190..$7312 and $761B
 * disassembly.
 */
int allstar_cli_test_cpu_target_rom(void) {
    static const uint16_t MODES[ALLSTAR_CPU_MODE_SLOTS] = {
        0x7190u, 0x718Fu, 0x74A8u, 0x718Fu, 0x7190u
    };
    static const uint16_t CLEARED[ALLSTAR_CPU_CLEARED] = {
        0xC0F7u, 0xC0FAu, 0xC0F9u, 0xC0F8u, 0xC100u, 0xC106u
    };
    AllStarCpuTarget target;
    const uint16_t *table;
    const uint8_t *thresholds;
    int count;
    int i;

    printf("[Test] Running ROM CPU Steering Tests ($7182/$7190/$761B)...\n");

    table = allstar_cpu_mode_table(&count);
    if (count != ALLSTAR_CPU_MODE_SLOTS) {
        fprintf(stderr, "[Test] $7185 has %d slots\n", count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != MODES[i]) {
            fprintf(stderr, "[Test] $7185 slot %d is $%04X, expected $%04X\n", i, table[i], MODES[i]);
            return 1;
        }
    }
    /* Modes $01 and $03 share the bare ret; $00 and $04 share the real body. */
    if (table[1] != table[3] || table[0] != table[4] || table[1] != 0x718Fu) {
        fprintf(stderr, "[Test] $7185 aliasing diverged\n");
        return 1;
    }

    /* $761B indexes by the skill level minus one. */
    thresholds = allstar_cpu_threshold_table(0x7626u);
    if (!thresholds || allstar_cpu_threshold(thresholds, 1u) != 0x1Bu ||
        allstar_cpu_threshold(thresholds, 2u) != 0x10u ||
        allstar_cpu_threshold(thresholds, 3u) != 0x07u) {
        fprintf(stderr, "[Test] $761B on the $7626 table diverged\n");
        return 1;
    }
    thresholds = allstar_cpu_threshold_table(0x7629u);
    if (!thresholds || allstar_cpu_threshold(thresholds, 3u) != 0x96u) {
        fprintf(stderr, "[Test] $7629 table diverged\n");
        return 1;
    }
    thresholds = allstar_cpu_threshold_table(0x762Cu);
    if (!thresholds || allstar_cpu_threshold(thresholds, 1u) != 0x04u) {
        fprintf(stderr, "[Test] $762C table diverged\n");
        return 1;
    }
    thresholds = allstar_cpu_threshold_table(0x7635u);
    if (!thresholds || allstar_cpu_threshold(thresholds, 2u) != 0x14u) {
        fprintf(stderr, "[Test] $7635 table diverged\n");
        return 1;
    }
    if (allstar_cpu_threshold_table(0x7000u) != NULL) {
        fprintf(stderr, "[Test] $761B accepted an address that is not a table\n");
        return 1;
    }
    /* The three tables do not agree on which way difficulty runs. */
    if (!(allstar_cpu_threshold(allstar_cpu_threshold_table(0x7626u), 1u) >
          allstar_cpu_threshold(allstar_cpu_threshold_table(0x7626u), 3u)) ||
        !(allstar_cpu_threshold(allstar_cpu_threshold_table(0x7629u), 1u) <
          allstar_cpu_threshold(allstar_cpu_threshold_table(0x7629u), 3u))) {
        fprintf(stderr, "[Test] $7626 and $7629 should slope opposite ways\n");
        return 1;
    }

    /* $7190: the entry compares possession against the stored context. */
    if (allstar_cpu_entry(1u, 1u) != ALLSTAR_CPU_SAME_POSSESSION ||
        allstar_cpu_entry(1u, 2u) != ALLSTAR_CPU_NEW_POSSESSION ||
        allstar_cpu_entry(0u, 0u) != ALLSTAR_CPU_SAME_POSSESSION) {
        fprintf(stderr, "[Test] $7197 entry comparison diverged\n");
        return 1;
    }

    /* $719A clears exactly these six bytes, in this order. */
    table = allstar_cpu_cleared_state(&count);
    if (count != ALLSTAR_CPU_CLEARED) {
        fprintf(stderr, "[Test] $719A clears %d bytes, expected %d\n", count, ALLSTAR_CPU_CLEARED);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != CLEARED[i]) {
            fprintf(stderr, "[Test] $719A byte %d is $%04X, expected $%04X\n", i, table[i], CLEARED[i]);
            return 1;
        }
    }

    /* $7257: the first set direction bit wins. */
    allstar_cpu_step_target(0x40u, 0x40u, 0x01u, &target);
    if (target.field_06 != 0x58u || target.field_15 != 0x44u) {
        fprintf(stderr, "[Test] $7278 bit 0 gave $%02X/$%02X\n", target.field_06, target.field_15);
        return 1;
    }
    allstar_cpu_step_target(0x40u, 0x40u, 0x02u, &target);
    if (target.field_06 != 0x38u || target.field_15 != 0x44u) {
        fprintf(stderr, "[Test] $726F bit 1 gave $%02X/$%02X\n", target.field_06, target.field_15);
        return 1;
    }
    allstar_cpu_step_target(0x40u, 0x40u, 0x04u, &target);
    if (target.field_06 != 0x48u || target.field_15 != 0x3Cu) {
        fprintf(stderr, "[Test] $7269 bit 2 gave $%02X/$%02X\n", target.field_06, target.field_15);
        return 1;
    }
    allstar_cpu_step_target(0x40u, 0x40u, 0x00u, &target);
    if (target.field_06 != 0x48u || target.field_15 != 0x4Cu) {
        fprintf(stderr, "[Test] $7263 default gave $%02X/$%02X\n", target.field_06, target.field_15);
        return 1;
    }
    /* Bit 0 beats bit 1 when both are set. */
    allstar_cpu_step_target(0x40u, 0x40u, 0x07u, &target);
    if (target.field_06 != 0x58u) {
        fprintf(stderr, "[Test] $7257 lost priority to a later bit\n");
        return 1;
    }
    /* $7272: stepping the coarse axis down clamps at zero. */
    allstar_cpu_step_target(0x00u, 0x40u, 0x02u, &target);
    if (target.field_06 != 0x00u) {
        fprintf(stderr, "[Test] $7274 did not clamp, gave $%02X\n", target.field_06);
        return 1;
    }

    /* $729E: three court bands, and the fine axis always loses eight. */
    allstar_cpu_center_target(0x20u, 0x40u, &target);
    if (target.field_06 != 0x30u || target.field_15 != 0x38u) {
        fprintf(stderr, "[Test] $72AE low band gave $%02X\n", target.field_06);
        return 1;
    }
    allstar_cpu_center_target(0x50u, 0x40u, &target);
    if (target.field_06 != 0x58u) {
        fprintf(stderr, "[Test] $72A8 middle band gave $%02X\n", target.field_06);
        return 1;
    }
    allstar_cpu_center_target(0x80u, 0x40u, &target);
    if (target.field_06 != 0x78u) {
        fprintf(stderr, "[Test] $72B4 high band gave $%02X\n", target.field_06);
        return 1;
    }
    /* The band edges are inclusive at $3C and $6C. */
    allstar_cpu_center_target(0x3Cu, 0x40u, &target);
    if (target.field_06 != 0x4Cu) {
        fprintf(stderr, "[Test] $72A1 boundary at $3C diverged\n");
        return 1;
    }
    allstar_cpu_center_target(0x6Cu, 0x40u, &target);
    if (target.field_06 != 0x74u) {
        fprintf(stderr, "[Test] $72A6 boundary at $6C diverged\n");
        return 1;
    }

    /* $72F0: the ball height picks the table. */
    if (allstar_cpu_spot_table(0x53u) != 0x731Cu ||
        allstar_cpu_spot_table(0x54u) != 0x7324u) {
        fprintf(stderr, "[Test] $72F0 height boundary diverged\n");
        return 1;
    }
    /* $72F7: $30 then steps of $40, four buckets. */
    if (allstar_cpu_spot_index(0x00u) != 0 || allstar_cpu_spot_index(0x2Fu) != 0 ||
        allstar_cpu_spot_index(0x30u) != 1 || allstar_cpu_spot_index(0x6Fu) != 1 ||
        allstar_cpu_spot_index(0x70u) != 2 || allstar_cpu_spot_index(0xAFu) != 2 ||
        allstar_cpu_spot_index(0xB0u) != 3 || allstar_cpu_spot_index(0xFFu) != 3) {
        fprintf(stderr, "[Test] $72FD spot bucketing diverged\n");
        return 1;
    }
    /* The two tables share their coarse column and differ in the fine one. */
    allstar_cpu_spot(0x00u, 0x00u, &target);
    if (target.field_06 != 0x68u || target.field_15 != 0x10u) {
        fprintf(stderr, "[Test] $731C spot 0 diverged\n");
        return 1;
    }
    allstar_cpu_spot(0xFFu, 0xFFu, &target);
    if (target.field_06 != 0x98u || target.field_15 != 0x78u) {
        fprintf(stderr, "[Test] $7324 spot 3 diverged\n");
        return 1;
    }
    allstar_cpu_spot(0x60u, 0x80u, &target);
    if (target.field_06 != 0x7Cu || target.field_15 != 0x90u) {
        fprintf(stderr, "[Test] $7324 spot 2 diverged\n");
        return 1;
    }


    /* $7476 and $749E hand $74BB two different pairs. */
    {
        uint16_t coarse;
        uint16_t fine;
        allstar_cpu_steer_source(ALLSTAR_CPU_STEER_BALL, &coarse, &fine);
        if (coarse != ALLSTAR_CPU_BALL_COARSE || fine != ALLSTAR_CPU_BALL_FINE) {
            fprintf(stderr, "[Test] $7476 steer source diverged\n");
            return 1;
        }
        allstar_cpu_steer_source(ALLSTAR_CPU_STEER_TARGET, &coarse, &fine);
        if (coarse != ALLSTAR_CPU_TARGET_X || fine != ALLSTAR_CPU_TARGET_Y) {
            fprintf(stderr, "[Test] $749E did not consume the stored target\n");
            return 1;
        }
    }

    /* $7481: three gates, each of which alone blocks the request. */
    if (!allstar_cpu_requests_action(1u, 0x30u, 0x05u, 0x19u)) {
        fprintf(stderr, "[Test] $7495 refused a request that should pass\n");
        return 1;
    }
    if (allstar_cpu_requests_action(0u, 0x30u, 0x05u, 0x19u)) {
        fprintf(stderr, "[Test] $7485 passed with the gate clear\n");
        return 1;
    }
    if (allstar_cpu_requests_action(1u, 0x27u, 0x05u, 0x19u)) {
        fprintf(stderr, "[Test] $748B passed below the $28 ball state\n");
        return 1;
    }
    if (allstar_cpu_requests_action(1u, 0x28u, 0x19u, 0x19u)) {
        fprintf(stderr, "[Test] $7495 passed on a roll equal to the threshold\n");
        return 1;
    }

    /* $7499 forces bit 0 on without disturbing the rest. */
    if (allstar_cpu_action_request(0x00u) != 0x01u ||
        allstar_cpu_action_request(0x0Eu) != 0x0Fu ||
        allstar_cpu_action_request(0x01u) != 0x01u) {
        fprintf(stderr, "[Test] $7499 request byte diverged\n");
        return 1;
    }

    /* $7443: a high roll skips the facing decision entirely. */
    {
        AllStarCpuFacing facing;
        if (allstar_cpu_face_opponent(ALLSTAR_CPU_FACE_ROLL_MAX, 0x40u, 0x20u, &facing)) {
            fprintf(stderr, "[Test] $7447 did not skip on a high roll\n");
            return 1;
        }
        /* An opponent below us faces $01, above us faces $02. */
        if (!allstar_cpu_face_opponent(0x00u, 0x40u, 0x20u, &facing) ||
            facing.facing != 0x01u || facing.commit_frames != ALLSTAR_CPU_FACE_COMMIT) {
            fprintf(stderr, "[Test] $7461 facing diverged, got %u\n", facing.facing);
            return 1;
        }
        if (!allstar_cpu_face_opponent(0x00u, 0x20u, 0x40u, &facing) || facing.facing != 0x02u) {
            fprintf(stderr, "[Test] $745D facing diverged\n");
            return 1;
        }
        /* Equal positions take the $02 branch, since `cp` clears carry. */
        if (!allstar_cpu_face_opponent(0x00u, 0x40u, 0x40u, &facing) || facing.facing != 0x02u) {
            fprintf(stderr, "[Test] $745B equal case diverged\n");
            return 1;
        }
    }

    /* $7431: the commit counts down before anything else may run. */
    {
        uint8_t frames = 2u;
        if (allstar_cpu_hold(&frames) != ALLSTAR_CPU_HOLD_RUNNING || frames != 1u) {
            fprintf(stderr, "[Test] $7437 hold did not tick\n");
            return 1;
        }
        if (allstar_cpu_hold(&frames) != ALLSTAR_CPU_HOLD_RUNNING || frames != 0u) {
            fprintf(stderr, "[Test] $7437 hold ended early\n");
            return 1;
        }
        if (allstar_cpu_hold(&frames) != ALLSTAR_CPU_HOLD_EXPIRED || frames != 0u) {
            fprintf(stderr, "[Test] $7435 hold did not expire\n");
            return 1;
        }
    }

    /* $7411: an immediate release, or a wait on the $C0FF counter. */
    if (!allstar_cpu_release(0x00u, 0x05u, 0x00u)) {
        fprintf(stderr, "[Test] $7425 immediate release diverged\n");
        return 1;
    }
    if (allstar_cpu_release(ALLSTAR_CPU_RELEASE_STATE, 0x05u, 0x00u)) {
        fprintf(stderr, "[Test] $7418 state $0D must fall through to the counter\n");
        return 1;
    }
    if (!allstar_cpu_release(ALLSTAR_CPU_RELEASE_STATE, 0x05u, 0x01u)) {
        fprintf(stderr, "[Test] $7423 counter of one did not release\n");
        return 1;
    }
    if (allstar_cpu_release(0x00u, 0x0Au, 0x02u)) {
        fprintf(stderr, "[Test] $741E a roll of $0A must fall through to the counter\n");
        return 1;
    }
    if (!allstar_cpu_release(0x00u, 0x0Au, 0x01u)) {
        fprintf(stderr, "[Test] $7423 counter release under a high roll diverged\n");
        return 1;
    }

    printf("  the first set direction bit wins, and stepping down clamps at zero\n");
    printf("  $7626 gets easier with skill while $7629 gets harder -- they slope opposite ways\n");
    printf("[Test] PASSED: $7182, $7190, $7411, $742E, $7476, $7496, $749E, $761B\n");
    return 0;
}

/*
 * ROM serial link layer, from the $267F, $2FD0..$2FFF and $2718 disassembly.
 */
int allstar_cli_test_link_rom(void) {
    static const uint16_t SEND[ALLSTAR_LINK_SEND_SLOTS] = {
        0x2FE2u, 0x2FE8u, 0x2FF1u, 0x2FF6u
    };
    AllStarLinkInject rx;
    AllStarLinkTransmit tx;
    AllStarLinkPadRefresh pad;
    const uint16_t *table;
    int count;
    int i;

    printf("[Test] Running ROM Serial Link Tests ($267F/$2FD0/$2718)...\n");

    /* $269B: a solo game injects nothing. */
    allstar_link_receive(1u, 0u, 0u, 0u, 0u, 0x3Cu, 0x00u, &rx);
    if (rx.target != ALLSTAR_LINK_RX_NOTHING) {
        fprintf(stderr, "[Test] $269F injected in a solo game\n");
        return 1;
    }

    /*
     * $26AA vs $26B8: the received byte is the *other* player's input, so a
     * cartridge that is player 2 writes it into pad 1 and vice versa.
     */
    allstar_link_receive(1u, 0u, 0u, 0u, ALLSTAR_LINK_ROLE_PLAYER_2, 0x3Cu, 0x00u, &rx);
    if (rx.target != ALLSTAR_LINK_RX_PAD_1) {
        fprintf(stderr, "[Test] $26AA player 2 did not write pad 1\n");
        return 1;
    }
    allstar_link_receive(1u, 0u, 0u, 0u, 0x01u, 0x3Cu, 0x00u, &rx);
    if (rx.target != ALLSTAR_LINK_RX_PAD_2) {
        fprintf(stderr, "[Test] $26B8 player 1 did not write pad 2\n");
        return 1;
    }

    /* $26AE: pressed is (held ^ received) & received, so only new bits count. */
    allstar_link_receive(1u, 0u, 0u, 0u, 0x01u, 0x0Fu, 0x03u, &rx);
    if (rx.held != 0x0Fu || rx.pressed != 0x0Cu) {
        fprintf(stderr, "[Test] $26B0 edge detect gave held $%02X pressed $%02X\n",
                rx.held, rx.pressed);
        return 1;
    }
    /* Holding the same bits reports nothing newly pressed. */
    allstar_link_receive(1u, 0u, 0u, 0u, 0x01u, 0x0Fu, 0x0Fu, &rx);
    if (rx.pressed != 0x00u) {
        fprintf(stderr, "[Test] $26B0 reported a repeat as newly pressed\n");
        return 1;
    }
    /* Releasing bits never shows up as a press. */
    allstar_link_receive(1u, 0u, 0u, 0u, 0x01u, 0x00u, 0x0Fu, &rx);
    if (rx.pressed != 0x00u || rx.held != 0x00u) {
        fprintf(stderr, "[Test] $26B0 turned a release into a press\n");
        return 1;
    }

    /* $2685: a dropped link clears the received byte before anything uses it. */
    allstar_link_receive(0u, 0u, 0u, 0u, 0x01u, 0xFFu, 0x00u, &rx);
    if (!rx.clears_received || rx.held != 0x00u || rx.pressed != 0x00u) {
        fprintf(stderr, "[Test] $2685 did not clear the received byte\n");
        return 1;
    }

    /* $2694: a link game that is mid-update hands off to $2EE5. */
    allstar_link_receive(1u, 1u, 0u, 1u, 0x01u, 0x3Cu, 0x00u, &rx);
    if (rx.target != ALLSTAR_LINK_RX_DIVERT) {
        fprintf(stderr, "[Test] $2698 did not divert\n");
        return 1;
    }
    /* Neither condition alone diverts. */
    allstar_link_receive(1u, 1u, 0u, 0u, 0x01u, 0x3Cu, 0x00u, &rx);
    if (rx.target == ALLSTAR_LINK_RX_DIVERT) {
        fprintf(stderr, "[Test] $2694 diverted without a link game\n");
        return 1;
    }
    allstar_link_receive(1u, 0u, 0u, 1u, 0x01u, 0x3Cu, 0x00u, &rx);
    if (rx.target == ALLSTAR_LINK_RX_DIVERT) {
        fprintf(stderr, "[Test] $268F diverted while idle\n");
        return 1;
    }

    /* $2FDA: the send table is indexed by the same role byte. */
    table = allstar_link_send_table(&count);
    if (count != ALLSTAR_LINK_SEND_SLOTS) {
        fprintf(stderr, "[Test] $2FDA has %d slots\n", count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != SEND[i]) {
            fprintf(stderr, "[Test] $2FDA slot %d is $%04X, expected $%04X\n", i, table[i], SEND[i]);
            return 1;
        }
    }

    /* $2FD4: a dropped link sends a zero. */
    allstar_link_transmit(0u, 0x02u, 0x5Au, 0u, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_ZERO || !tx.transmits || tx.byte != 0x00u) {
        fprintf(stderr, "[Test] $2FD4 dropped-link send diverged\n");
        return 1;
    }
    /* Role $00 sends nothing at all. */
    allstar_link_transmit(1u, 0x00u, 0x5Au, 0u, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_IDLE || tx.transmits) {
        fprintf(stderr, "[Test] $2FE2 role 0 transmitted\n");
        return 1;
    }
    /* Role $01 sends the $D5 sync byte. */
    allstar_link_transmit(1u, 0x01u, 0x5Au, 0u, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_SYNC || tx.byte != ALLSTAR_LINK_SYNC_BYTE) {
        fprintf(stderr, "[Test] $2FED sync byte diverged, got $%02X\n", tx.byte);
        return 1;
    }
    /* ...unless a serial interrupt is still pending. */
    allstar_link_transmit(1u, 0x01u, 0x5Au, ALLSTAR_LINK_IF_SERIAL, &tx);
    if (tx.transmits) {
        fprintf(stderr, "[Test] $2FEA sent while an interrupt was pending\n");
        return 1;
    }
    /* Roles $02 and $03 send the state byte from $C16E. */
    allstar_link_transmit(1u, 0x02u, 0x5Au, 0u, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_STATE || tx.byte != 0x5Au) {
        fprintf(stderr, "[Test] $2FF1 state send diverged\n");
        return 1;
    }
    allstar_link_transmit(1u, 0x03u, 0x5Au, ALLSTAR_LINK_IF_SERIAL, &tx);
    if (tx.kind != ALLSTAR_LINK_TX_STATE || tx.byte != 0x5Au) {
        fprintf(stderr, "[Test] $2FF6 must ignore the interrupt flag\n");
        return 1;
    }

    /* $2718: both pad 2 bytes take the same merged value. */
    allstar_link_refresh_pad_2(0x02u, 0x30u, 0x0Cu, &pad);
    if (pad.context != 0x02u || pad.held != 0x3Cu || pad.pressed != 0x3Cu) {
        fprintf(stderr, "[Test] $2724 pad refresh gave $%02X/$%02X\n", pad.held, pad.pressed);
        return 1;
    }

    /* The strays swept up alongside. */
    if (allstar_busy_wait_count() != ALLSTAR_BUSY_WAIT_COUNT) {
        fprintf(stderr, "[Test] $0386 loop count diverged\n");
        return 1;
    }
    allstar_bank1_mode_noop();
    if (allstar_sound_offset_slot(0u) != ALLSTAR_SOUND_OFFSET_TABLE ||
        allstar_sound_offset_slot(3u) != (uint16_t)(ALLSTAR_SOUND_OFFSET_TABLE + 6u)) {
        fprintf(stderr, "[Test] $331B channel doubling diverged\n");
        return 1;
    }

    printf("  the received byte drives the other player's pad, so role $03 writes pad 1\n");
    printf("  role $01 sends $D5 unless a serial interrupt is pending; $02 and $03 send $C16E\n");
    printf("[Test] PASSED: $267F, $2718, $2FD0, $2FE2, $2FE8, $2FF1, $2FF6, $2FF9, $0386, $331A, $718F\n");
    return 0;
}

/*
 * ROM settings-screen cursor and shell helpers, from the $231E..$23A3,
 * $0ADA..$0B4E and $2D1B..$2D4E disassembly.
 */
int allstar_cli_test_shell_rom(void) {
    static const uint16_t SETTINGS[ALLSTAR_SETTINGS_SLOTS] = {
        0x232Bu, 0x235Fu, 0x236Au, 0x2374u, 0x2330u
    };
    static const uint16_t PROBES[ALLSTAR_PROBE_SLOTS] = {
        0x0AE1u, 0x0B01u, 0x0AEAu, 0x0621u, 0x1900u
    };
    AllStarSettingsScreen screen;
    AllStarProbe probe;
    const uint16_t *table;
    uint8_t cursor;
    uint8_t flag;
    uint16_t countdown;
    int count;
    int i;

    printf("[Test] Running ROM Shell Tests ($231E/$0ADB/$2D1B)...\n");

    table = allstar_settings_table(&count);
    if (count != ALLSTAR_SETTINGS_SLOTS) {
        fprintf(stderr, "[Test] $2321 has %d slots\n", count);
        return 1;
    }
    for (i = 0; i < count; i++) {
        if (table[i] != SETTINGS[i]) {
            fprintf(stderr, "[Test] $2321 slot %d is $%04X, expected $%04X\n", i, table[i], SETTINGS[i]);
            return 1;
        }
    }

    /* Each mode has its own cursor table and row count. */
    if (!allstar_settings_screen(0x00u, &screen) || screen.cursor_table != 0x2AC8u ||
        screen.rows != 4u || !screen.wraps_with_mask) {
        fprintf(stderr, "[Test] $232B screen diverged\n");
        return 1;
    }
    if (!allstar_settings_screen(0x04u, &screen) || screen.cursor_table != 0x2ADAu ||
        screen.rows != 4u) {
        fprintf(stderr, "[Test] $2330 screen diverged\n");
        return 1;
    }
    if (!allstar_settings_screen(0x03u, &screen) || screen.cursor_table != 0x2AD4u ||
        screen.rows != 3u || screen.wraps_with_mask) {
        fprintf(stderr, "[Test] $2374 screen diverged\n");
        return 1;
    }
    if (!allstar_settings_screen(0x01u, &screen) || screen.rows != 1u ||
        !allstar_settings_screen(0x02u, &screen) || screen.rows != 1u) {
        fprintf(stderr, "[Test] $235F/$236A single-row screens diverged\n");
        return 1;
    }

    /* $2342/$2347: Down steps forward, Up steps back, four rows wrap by mask. */
    cursor = 0u;
    if (allstar_settings_step(0x00u, ALLSTAR_SETTINGS_DOWN_MASK, &cursor) != ALLSTAR_SETTINGS_MOVED ||
        cursor != 1u) {
        fprintf(stderr, "[Test] $2344 Down diverged\n");
        return 1;
    }
    cursor = 3u;
    if (allstar_settings_step(0x00u, ALLSTAR_SETTINGS_DOWN_MASK, &cursor) != ALLSTAR_SETTINGS_MOVED ||
        cursor != 0u) {
        fprintf(stderr, "[Test] $234A mask wrap forward diverged, cursor %u\n", cursor);
        return 1;
    }
    if (allstar_settings_step(0x00u, ALLSTAR_SETTINGS_UP_MASK, &cursor) != ALLSTAR_SETTINGS_MOVED ||
        cursor != 3u) {
        fprintf(stderr, "[Test] $234A mask wrap back diverged, cursor %u\n", cursor);
        return 1;
    }
    /* Up wins when both are pressed, because $233A is tested first. */
    cursor = 1u;
    if (allstar_settings_step(0x00u, 0xC0u, &cursor) != ALLSTAR_SETTINGS_MOVED || cursor != 0u) {
        fprintf(stderr, "[Test] $233A did not take priority over $233E\n");
        return 1;
    }
    /* Mode $03 has three rows and wraps with explicit compares. */
    cursor = 2u;
    if (allstar_settings_step(0x03u, ALLSTAR_SETTINGS_DOWN_MASK, &cursor) != ALLSTAR_SETTINGS_MOVED ||
        cursor != 0u) {
        fprintf(stderr, "[Test] $238B three-row wrap forward diverged, cursor %u\n", cursor);
        return 1;
    }
    if (allstar_settings_step(0x03u, ALLSTAR_SETTINGS_UP_MASK, &cursor) != ALLSTAR_SETTINGS_MOVED ||
        cursor != 2u) {
        fprintf(stderr, "[Test] $2395 three-row wrap back diverged, cursor %u\n", cursor);
        return 1;
    }
    /* Neither direction leaves the cursor alone. */
    cursor = 1u;
    if (allstar_settings_step(0x00u, 0x01u, &cursor) != ALLSTAR_SETTINGS_IDLE || cursor != 1u) {
        fprintf(stderr, "[Test] $2340 moved on an unrelated button\n");
        return 1;
    }

    /* $0ADB: three probes, two on the X field and one on Y. */
    table = allstar_probe_table(&count);
    for (i = 0; i < ALLSTAR_PROBE_SLOTS; i++) {
        if (table[i] != PROBES[i]) {
            fprintf(stderr, "[Test] $0ADB slot %d is $%04X, expected $%04X\n", i, table[i], PROBES[i]);
            return 1;
        }
    }
    if (!allstar_probe_shape(0x0AE1u, &probe) || probe.field != ALLSTAR_PROBE_FIELD_X ||
        probe.delta != 12) {
        fprintf(stderr, "[Test] $0AE6 probe diverged\n");
        return 1;
    }
    if (!allstar_probe_shape(0x0AEAu, &probe) || probe.field != ALLSTAR_PROBE_FIELD_X ||
        probe.delta != -12) {
        fprintf(stderr, "[Test] $0AEF probe diverged\n");
        return 1;
    }
    if (!allstar_probe_shape(0x0B01u, &probe) || probe.field != ALLSTAR_PROBE_FIELD_Y ||
        probe.delta != -8) {
        fprintf(stderr, "[Test] $0B06 probe diverged\n");
        return 1;
    }
    if (allstar_probe_result(0x40u, 0x40u) != ALLSTAR_PROBE_OK ||
        allstar_probe_result(0x40u, 0x41u) != ALLSTAR_PROBE_BLOCKED) {
        fprintf(stderr, "[Test] $0AFD probe verdict diverged\n");
        return 1;
    }

    /* $0B20 and $0B29 are BCD counters that carry between the bytes. */
    if (allstar_bcd_increment(0x0000u) != 0x0001u ||
        allstar_bcd_increment(0x0009u) != 0x0010u ||
        allstar_bcd_increment(0x0099u) != 0x0100u ||
        allstar_bcd_increment(0x0199u) != 0x0200u) {
        fprintf(stderr, "[Test] $0B21 BCD increment diverged, 99 gave $%04X\n",
                allstar_bcd_increment(0x0099u));
        return 1;
    }
    if (allstar_bcd_decrement(0x0001u) != 0x0000u ||
        allstar_bcd_decrement(0x0010u) != 0x0009u ||
        allstar_bcd_decrement(0x0100u) != 0x0099u ||
        allstar_bcd_decrement(0x0200u) != 0x0199u) {
        fprintf(stderr, "[Test] $0B29 BCD decrement diverged, 100 gave $%04X\n",
                allstar_bcd_decrement(0x0100u));
        return 1;
    }

    /* $0B44: the serial flag is consumed once. */
    flag = 0u;
    if (allstar_serial_ready(&flag)) {
        fprintf(stderr, "[Test] $0B47 reported ready with the flag clear\n");
        return 1;
    }
    flag = 1u;
    if (!allstar_serial_ready(&flag) || flag != 0u) {
        fprintf(stderr, "[Test] $0B4A did not consume the serial flag\n");
        return 1;
    }

    /* $2D25: all four buttons held at once is the soft reset. */
    if (allstar_watchdog(0u, 0u, ALLSTAR_RESET_COMBO, 0u, 0u, NULL) != ALLSTAR_WATCHDOG_RESET) {
        fprintf(stderr, "[Test] $2D2B soft reset did not fire\n");
        return 1;
    }
    if (allstar_watchdog(0u, 0u, 0x0Eu, 0u, 0u, NULL) != ALLSTAR_WATCHDOG_CONTINUE) {
        fprintf(stderr, "[Test] $2D29 fired on a partial combo\n");
        return 1;
    }
    if (allstar_watchdog(0u, 1u, ALLSTAR_RESET_COMBO, 0u, 0u, NULL) != ALLSTAR_WATCHDOG_CONTINUE) {
        fprintf(stderr, "[Test] $2D24 suppression did not block the reset\n");
        return 1;
    }
    /* $2D2F: attract mode counts down instead, and Select or Start cuts it short. */
    countdown = 3u;
    if (allstar_watchdog(1u, 0u, 0u, 1u, 0u, &countdown) != ALLSTAR_WATCHDOG_CONTINUE ||
        countdown != 2u) {
        fprintf(stderr, "[Test] $2D39 attract countdown diverged\n");
        return 1;
    }
    countdown = 1u;
    if (allstar_watchdog(1u, 0u, 0u, 1u, 0u, &countdown) != ALLSTAR_WATCHDOG_RESET) {
        fprintf(stderr, "[Test] $2D44 attract expiry did not reset\n");
        return 1;
    }
    countdown = 9u;
    if (allstar_watchdog(1u, 0u, 0u, 1u, ALLSTAR_RESET_ATTRACT, &countdown) != ALLSTAR_WATCHDOG_RESET) {
        fprintf(stderr, "[Test] $2D4B attract button did not reset\n");
        return 1;
    }
    countdown = 9u;
    if (allstar_watchdog(1u, 0u, 0u, 0u, 0u, &countdown) != ALLSTAR_WATCHDOG_CONTINUE ||
        countdown != 9u) {
        fprintf(stderr, "[Test] $2D32 counted down while disarmed\n");
        return 1;
    }

    printf("  four-row modes wrap with and $03, mode $03 wraps three rows by compare\n");
    printf("  A+B+Select+Start is the soft reset, and attract adds a countdown\n");
    printf("[Test] PASSED: $231E, $232B, $2330, $235F, $236A, $2374, $0ADB, $0AE1, $0AEA, $0B01, $0B20, $0B29, $0B44, $2D1B\n");
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
    failed += allstar_cli_test_shell_rom();
    failed += allstar_cli_test_link_rom();
    failed += allstar_cli_test_cpu_target_rom();
    failed += allstar_cli_test_apu_program_rom();
    failed += allstar_cli_test_boot_rom();
    failed += allstar_cli_test_handshake_rom();
    failed += allstar_cli_test_session_rom();
    failed += allstar_cli_test_pad_rom();
    failed += allstar_cli_test_defense_jump_rom();
    failed += allstar_cli_test_title_music_rom();
    failed += allstar_cli_test_frame_rom();
    failed += allstar_cli_test_cpu_head_rom();
    failed += allstar_cli_test_caption_rom();
    failed += allstar_cli_test_kernel_rom();
    failed += allstar_cli_test_sfx_envelope_rom();
    failed += allstar_cli_test_rom_art();
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
    } else if (strcmp(cmd, "--test-shell") == 0) {
        return allstar_cli_test_shell_rom();
    } else if (strcmp(cmd, "--test-link") == 0) {
        return allstar_cli_test_link_rom();
    } else if (strcmp(cmd, "--test-cpu-target") == 0) {
        return allstar_cli_test_cpu_target_rom();
    } else if (strcmp(cmd, "--test-apu-program") == 0) {
        return allstar_cli_test_apu_program_rom();
    } else if (strcmp(cmd, "--test-boot") == 0) {
        return allstar_cli_test_boot_rom();
    } else if (strcmp(cmd, "--test-handshake") == 0) {
        return allstar_cli_test_handshake_rom();
    } else if (strcmp(cmd, "--test-session") == 0) {
        return allstar_cli_test_session_rom();
    } else if (strcmp(cmd, "--test-pad") == 0) {
        return allstar_cli_test_pad_rom();
    } else if (strcmp(cmd, "--test-defense-jump") == 0) {
        return allstar_cli_test_defense_jump_rom();
    } else if (strcmp(cmd, "--test-title-music") == 0) {
        return allstar_cli_test_title_music_rom();
    } else if (strcmp(cmd, "--test-frame") == 0) {
        return allstar_cli_test_frame_rom();
    } else if (strcmp(cmd, "--test-cpu-head") == 0) {
        return allstar_cli_test_cpu_head_rom();
    } else if (strcmp(cmd, "--test-captions") == 0) {
        return allstar_cli_test_caption_rom();
    } else if (strcmp(cmd, "--test-kernel") == 0) {
        return allstar_cli_test_kernel_rom();
    } else if (strcmp(cmd, "--test-sfx-envelope") == 0) {
        return allstar_cli_test_sfx_envelope_rom();
    } else if (strcmp(cmd, "--test-rom-art") == 0) {
        return allstar_cli_test_rom_art();
    } else if (strcmp(cmd, "--export-title-music") == 0) {
        AllStarAssetPack pack;
        if (argc < 4) {
            fprintf(stderr,
                    "Error: --export-title-music <pack> <out.wav>\n");
            return 1;
        }
        if (!allstar_asset_pack_load_file(&pack, argv[2])) return 1;
        return allstar_audio_export_rom_music_wav(&pack, argv[3]) ? 0 : 1;
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
