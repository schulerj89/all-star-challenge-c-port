#ifndef ALLSTAR_BOOT_H
#define ALLSTAR_BOOT_H

#include "allstar_types.h"

/*
 * The boot path and title selector, ported from $0150..$0212 and $029C..$030D.
 *
 * $0150 is the cold entry and $0156 is the soft-reset vector the watchdog at
 * $2D1B jumps to.  Both converge on $0170, which wipes work RAM, the sound
 * state and HRAM -- but carries two things across the wipe on the stack.
 */

/* ---- $0150..$01E1: the wipe ---- */

#define ALLSTAR_BOOT_STACK        0xE000u
#define ALLSTAR_BOOT_LINK_RESET   0xC3u    /* $0163, posted to $C18E */
#define ALLSTAR_BOOT_LINK_ROLE    0x02u    /* $015F */
#define ALLSTAR_BOOT_RESET_WAITS  5        /* $016B, five vblanks */
#define ALLSTAR_BOOT_WARM_FLAG    0xC191u
#define ALLSTAR_BOOT_REGIONS      3

typedef struct {
    uint16_t start;
    uint16_t length;
} AllStarBootRegion;

/* $0187, $0195 and $01B6: what the boot clears, in order. */
const AllStarBootRegion* allstar_boot_cleared(int *count);

/*
 * $01A8..$01D1.  The RNG seed at $FFFB..$FFFE sits inside the HRAM wipe, so it
 * is pushed before and popped after.  $C191 rides through the work-RAM wipe in
 * $FF8C the same way.  Without either, a soft reset would replay the same game.
 */
const uint16_t* allstar_boot_preserved(int *count);

typedef enum {
    ALLSTAR_BOOT_COLD = 0,      /* $0150, the warm flag is cleared first */
    ALLSTAR_BOOT_RESET,         /* $0156 with nothing to tell the other side */
    ALLSTAR_BOOT_RESET_NOTIFY   /* $0163, a link partner has to be told */
} AllStarBootEntry;

/*
 * $0150..$016F.  A soft reset only posts the $C3 notice when this is a link
 * game and this cartridge holds role $02.
 */
AllStarBootEntry allstar_boot_entry(bool cold, uint8_t link_game, uint8_t role);

/* ---- $02AC..$030D: the title screen selector ---- */

#define ALLSTAR_TITLE_TIMEOUT     0x0960u  /* $02D3, frames before attract  */
#define ALLSTAR_TITLE_MOVE_MASK   0x33u    /* $02DF, A, B, Right or Left    */
#define ALLSTAR_TITLE_START_MASK  0x08u    /* $02FE                         */
#define ALLSTAR_TITLE_PLAYERS_MIN 1u
#define ALLSTAR_TITLE_PLAYERS_MAX 2u

typedef enum {
    ALLSTAR_TITLE_WAITING = 0,
    ALLSTAR_TITLE_TOGGLED,     /* $02E4, one or two players                */
    ALLSTAR_TITLE_CONFIRMED,   /* $030E                                    */
    ALLSTAR_TITLE_ATTRACT      /* $0309, the counter ran out and $FFE4 set */
} AllStarTitleResult;

/*
 * $02D6..$030D.  One pass of the title loop.  The player count in $FF91 toggles
 * between one and two through `dec / xor $01 / inc`, Start confirms, and the
 * $0960 frame counter running out drops into attract mode instead.
 */
AllStarTitleResult allstar_title_step(uint8_t new_buttons, uint8_t *players,
                                      uint16_t *countdown);

/* $0311..$0327: confirming two players starts the link handshake. */
typedef struct {
    bool starts_link;   /* two players go to $0322 rather than returning */
    uint8_t role;       /* $C199 becomes $01 there                        */
    uint8_t attempts;   /* $0322 loads B with $0A                         */
} AllStarTitleConfirm;

void allstar_title_confirm(uint8_t players, AllStarTitleConfirm *out);

/* ---- $0271..$029B: the copyright screen ---- */

#define ALLSTAR_CREDITS_FRAMES    0x00F0u  /* $0292, 240 frames = ~4.0 s   */
#define ALLSTAR_CREDITS_TILES     0x640Fu  /* $0279, bank 1 -> $9000       */
#define ALLSTAR_CREDITS_TILEMAP   0x4000u  /* $0285, bank 3 -> $9800       */
#define ALLSTAR_CREDITS_LCDC      0x81u    /* $028E                        */

/*
 * $0271.  The copyright screen is shown for a fixed 240 frames and then falls
 * through to the title.  It is gated on $C191, the warm-boot flag, so it runs
 * on a cold start only -- and because $0263 sends every finished game back
 * through $0156, the second and later games of a session skip it entirely.
 */
typedef struct {
    bool shown;             /* $0274, only while $C191 is clear */
    uint16_t hold_frames;   /* $0292                            */
    uint8_t tile_bank;      /* $0276                            */
    uint8_t tilemap_bank;   /* $0282                            */
} AllStarCreditsScreen;

void allstar_credits_screen_0271(uint8_t warm_flag,
                                 AllStarCreditsScreen *out);

/* ---- $1F7A..$1FE0: leaving a game, and resetting to the title ---- */

/*
 * $1F7A.  Run before the title is drawn.  Zeroing both sound mailboxes is what
 * actually stops the music: $DD73 is the song command $029C later re-posts as
 * $81, and $DD72 is the effect command.
 */
#define ALLSTAR_TEARDOWN_CLEARED  6

const uint16_t* allstar_teardown_cleared_1f7a(int *count);

/*
 * $1FA4.  The full return to the title.  LCDC keeps only bit 7, the player
 * count goes back to one, $048B wipes the $C000 OAM shadow, a dozen link and
 * presentation bytes are cleared, and $C12C is left at one.
 */
#define ALLSTAR_TITLE_RESET_CLEARED 12
#define ALLSTAR_TITLE_RESET_LCDC_MASK 0x80u  /* $1FA9 */
#define ALLSTAR_TITLE_RESET_OAM_SHADOW 0xC000u
#define ALLSTAR_TITLE_RESET_OAM_BYTES  0x00A0u /* $048B..$0495, 160 bytes */

const uint16_t* allstar_title_reset_cleared_1fa4(int *count);

/*
 * $1FE1.  The serial reset both of the above run first: SC and SB are cleared,
 * the role and the three handshake bytes go to zero, and $C1A0 is seeded with
 * $B3 before control passes to $2FE3.
 */
#define ALLSTAR_SERIAL_RESET_SEED 0xB3u    /* $1FF2, into $C1A0 */
#define ALLSTAR_SERIAL_RESET_TAIL 0x2FE3u  /* $1FF7             */
#define ALLSTAR_SERIAL_RESET_CLEARED 6

const uint16_t* allstar_serial_reset_cleared_1fe1(int *count);

#endif /* ALLSTAR_BOOT_H */
