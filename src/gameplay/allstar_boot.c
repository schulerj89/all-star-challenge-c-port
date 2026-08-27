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
