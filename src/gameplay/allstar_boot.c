#include "allstar_boot.h"

/* $0187, $0195, $01B6 */
static const AllStarBootRegion BOOT_REGIONS[ALLSTAR_BOOT_REGIONS] = {
    { 0xC000u, 0x02D0u },   /* $018D, work RAM */
    { 0xDD72u, 0x00BDu },   /* $019B, the sound state */
    { 0xFF80u, 0x007Eu }    /* $01BC, HRAM */
};

/* $01A8 and $01AF push these two words before the HRAM wipe reaches them. */
static const uint16_t BOOT_PRESERVED[2] = { 0xFFFBu, 0xFFFDu };

const AllStarBootRegion* allstar_boot_cleared(int *count) {
    if (count) *count = ALLSTAR_BOOT_REGIONS;
    return BOOT_REGIONS;
}

const uint16_t* allstar_boot_preserved(int *count) {
    if (count) *count = 2;
    return BOOT_PRESERVED;
}

/* $0150..$016F */
AllStarBootEntry allstar_boot_entry(bool cold, uint8_t link_game, uint8_t role) {
    if (cold) return ALLSTAR_BOOT_COLD;                             /* $0150 */
    if (link_game == 0) return ALLSTAR_BOOT_RESET;                  /* $0159-$015A */
    if (role != ALLSTAR_BOOT_LINK_ROLE) return ALLSTAR_BOOT_RESET;  /* $015F-$0161 */
    return ALLSTAR_BOOT_RESET_NOTIFY;                               /* $0163 */
}

/* $02D6..$030D */
AllStarTitleResult allstar_title_step(uint8_t new_buttons, uint8_t *players,
                                      uint16_t *countdown) {
    if (!players || !countdown) return ALLSTAR_TITLE_WAITING;

    if ((new_buttons & ALLSTAR_TITLE_MOVE_MASK) != 0) {             /* $02DF-$02E1 */
        /* $02E4-$02EA: dec, xor $01, inc -- a toggle between one and two. */
        uint8_t value = (uint8_t)(*players - 1u);
        value = (uint8_t)(value ^ 0x01u);
        *players = (uint8_t)(value + 1u);
        return ALLSTAR_TITLE_TOGGLED;
    }

    if ((new_buttons & ALLSTAR_TITLE_START_MASK) != 0) {            /* $02FE-$0301 */
        return ALLSTAR_TITLE_CONFIRMED;
    }

    *countdown = (uint16_t)(*countdown - 1u);                       /* $0304 */
    if (*countdown == 0) return ALLSTAR_TITLE_ATTRACT;              /* $0307-$030B */
    return ALLSTAR_TITLE_WAITING;
}

/* $0311..$0327 */
void allstar_title_confirm(uint8_t players, AllStarTitleConfirm *out) {
    if (!out) return;
    if (players == ALLSTAR_TITLE_PLAYERS_MIN) {                     /* $0318-$031A */
        out->starts_link = false;                                   /* $031C-$0321 */
        out->role = 0;
        out->attempts = 0;
        return;
    }
    out->starts_link = true;
    out->role = 0x01u;                                              /* $0329 */
    out->attempts = 0x0Au;                                          /* $0322 */
}

/* $0271..$029B */
void allstar_credits_screen_0271(uint8_t warm_flag,
                                 AllStarCreditsScreen *out) {
    if (!out) return;
    out->hold_frames = ALLSTAR_CREDITS_FRAMES;                  /* $0292 */
    out->tile_bank = 1u;                                        /* $0276 */
    out->tilemap_bank = 3u;                                     /* $0282 */
    /* $0271-$0275: a warm boot returns before any of it runs. */
    out->shown = warm_flag == 0;
    if (!out->shown) out->hold_frames = 0u;
}

/* $1F80..$1F91: what the teardown zeroes, in ROM order. */
const uint16_t* allstar_teardown_cleared_1f7a(int *count) {
    static const uint16_t cleared[ALLSTAR_TEARDOWN_CLEARED] = {
        0xDD72u,  /* $1F81, the effect command  */
        0xDD73u,  /* $1F84, the song command    */
        0xC194u,  /* $1F87                      */
        0xC193u,  /* $1F8A                      */
        0xFF90u,  /* $1F8D                      */
        0xC19Au   /* $1F8F                      */
    };
    if (count) *count = ALLSTAR_TEARDOWN_CLEARED;
    return cleared;
}

/* $1FBB..$1FD9: what the title reset zeroes, in ROM order. */
const uint16_t* allstar_title_reset_cleared_1fa4(int *count) {
    static const uint16_t cleared[ALLSTAR_TITLE_RESET_CLEARED] = {
        0xFFE5u,  /* $1FBC */
        0xFF90u,  /* $1FBE */
        0xC174u,  /* $1FC0 */
        0xC16Eu,  /* $1FC3, the outgoing pad byte $2FD0 sends */
        0xC19Cu,  /* $1FC6 */
        0xC177u,  /* $1FC9 */
        0xFFECu,  /* $1FCC */
        0xC16Fu,  /* $1FCE */
        0xC170u,  /* $1FD1 */
        0xC176u,  /* $1FD4, the role $03 stall counter $2729 bumps */
        0xFFF9u,  /* $1FD7 */
        0xC18Bu   /* $1FD9, the link-game flag */
    };
    if (count) *count = ALLSTAR_TITLE_RESET_CLEARED;
    return cleared;
}

/* $1FE1..$1FF1: what the serial reset zeroes, in ROM order. */
const uint16_t* allstar_serial_reset_cleared_1fe1(int *count) {
    static const uint16_t cleared[ALLSTAR_SERIAL_RESET_CLEARED] = {
        0xFF02u,  /* $1FE2, SC */
        0xFF01u,  /* $1FE4, SB */
        0xC199u,  /* $1FE6, the role the whole serial layer keys on */
        0xC19Du,  /* $1FE9 */
        0xC19Eu,  /* $1FEC */
        0xC19Fu   /* $1FEF */
    };
    if (count) *count = ALLSTAR_SERIAL_RESET_CLEARED;
    return cleared;
}
