#include "allstar_tournament.h"
#include <string.h>

/*
 * Fixed-bank tournament driver, ported from $0F2E..$0FBA plus its two helpers
 * $22DE and $0FBB.  Control flow follows the cartridge exactly:
 *
 *   $0F2E  LCD on, BG and OBJ display off
 *   $0F37  call $22DE            clear the bracket block
 *   $0F3A  call $2890            pick the eight entrants
 *   $0F3D  call $2835            bank the stage win counters
 *   $0F40  $DD73 = 0             silence the menu music
 *   $0F44  HL = $C0BF, DE = $C0C3
 *   $0F4A  four matches, each: $FF40 &= $FC, load [HL+]/[DE+], $0FBB,
 *          $C0BE++, $0B80, $10A5, $284D -- until $C0BE == $04
 *   $0F72  call $2897 then $2835
 *   $0F78  HL = $C0CB, DE = $C0CD
 *   $0F7E  two matches, same body but $286E -- until $C0BE == $06
 *   $0FA0  call $28B0 then $2835
 *   $0FA6  final: entrants read straight from $C0D1/$C0D2, $0FBB, $C0BE++,
 *          $0B80, then ret.  The final runs no $10A5 and no advance routine.
 */

/* Position inside a match body, shared by all three stages. */
enum {
    MATCH_LOAD = 0,     /* $0FBB */
    MATCH_PLAY,         /* $0B80 */
    MATCH_POSTGAME,     /* $10A5 */
    MATCH_ADVANCE       /* $284D or $286E */
};

uint8_t allstar_tournament_rom_peek(const AllStarTournamentRom *rom, uint16_t address) {
    if (!rom) return 0;
    if (address < ALLSTAR_TOURNAMENT_RAM_BASE || address > ALLSTAR_TOURNAMENT_RAM_END) return 0;
    return rom->ram[address - ALLSTAR_TOURNAMENT_RAM_BASE];
}

void allstar_tournament_rom_poke(AllStarTournamentRom *rom, uint16_t address, uint8_t value) {
    if (!rom) return;
    if (address < ALLSTAR_TOURNAMENT_RAM_BASE || address > ALLSTAR_TOURNAMENT_RAM_END) return;
    rom->ram[address - ALLSTAR_TOURNAMENT_RAM_BASE] = value;
}

/* $22DE */
void allstar_tournament_rom_reset_block(AllStarTournamentRom *rom) {
    if (!rom) return;
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_MATCH_COUNT, 0);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_TOTAL_WINS_LEFT, 0);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_TOTAL_WINS_RIGHT, 0);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_STAGE_WINS_LEFT, 0);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_STAGE_WINS_RIGHT, 0);
}

/* $0FBB */
void allstar_tournament_rom_load_players(AllStarTournamentRom *rom) {
    if (!rom) return;
    /* $0FBB switches to bank 2, loads slot 1 then slot 2, and restores bank 1. */
    rom->loaded_slot[0] = rom->current_left;
    rom->loaded_slot[1] = rom->current_right;
    rom->loaded_bank = 1u;
}

/* $2835 */
void allstar_tournament_rom_bank_wins(AllStarTournamentRom *rom) {
    uint8_t left;
    uint8_t right;
    if (!rom) return;
    left = (uint8_t)(allstar_tournament_rom_peek(rom, ALLSTAR_TR_TOTAL_WINS_LEFT) +
                     allstar_tournament_rom_peek(rom, ALLSTAR_TR_STAGE_WINS_LEFT));
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_TOTAL_WINS_LEFT, left);
    right = (uint8_t)(allstar_tournament_rom_peek(rom, ALLSTAR_TR_TOTAL_WINS_RIGHT) +
                      allstar_tournament_rom_peek(rom, ALLSTAR_TR_STAGE_WINS_RIGHT));
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_TOTAL_WINS_RIGHT, right);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_STAGE_WINS_LEFT, 0);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_STAGE_WINS_RIGHT, 0);
}

/* $28E1: unsigned compare of $C133/$C134 against $C135/$C136. */
uint8_t allstar_tournament_rom_match_winner(const AllStarTournamentRom *rom) {
    if (!rom) return 0;
    if (rom->score_left == rom->score_right) return 0;
    return rom->score_left > rom->score_right ? 1u : 2u;
}

/* Shared body of $284D and $286E: bump a stage counter, then copy one entrant. */
static void tournament_credit_winner(AllStarTournamentRom *rom,
                                     uint8_t winner,
                                     uint16_t left_source,
                                     uint16_t right_source,
                                     uint16_t destination) {
    uint16_t counter = (winner == 1u) ? ALLSTAR_TR_STAGE_WINS_LEFT : ALLSTAR_TR_STAGE_WINS_RIGHT;
    uint16_t source = (winner == 1u) ? left_source : right_source;
    allstar_tournament_rom_poke(rom, counter,
                                (uint8_t)(allstar_tournament_rom_peek(rom, counter) + 1u));
    allstar_tournament_rom_poke(rom, destination, allstar_tournament_rom_peek(rom, source));
}

/*
 * $284D.  The ROM walks DE and HL forward $C0BE times from $C0BE/$C0C2 and
 * $C0C6, which lands on the entrant that just played and on winner slot N.
 */
void allstar_tournament_rom_advance_round_1(AllStarTournamentRom *rom) {
    uint8_t winner;
    uint8_t match;
    if (!rom) return;
    winner = allstar_tournament_rom_match_winner(rom);
    if (winner == 0) return;                                        /* $2850-$2851 */
    match = allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT);
    tournament_credit_winner(rom, winner,
                             (uint16_t)(ALLSTAR_TR_MATCH_COUNT + match),
                             (uint16_t)(ALLSTAR_TR_R1_RIGHT - 1u + match),
                             (uint16_t)(ALLSTAR_TR_R1_WINNERS - 1u + match));
}

/*
 * $286E.  Match $05 fills $C0CF from the first semifinal pair, match $06 fills
 * $C0D0 from the second.
 */
void allstar_tournament_rom_advance_round_2(AllStarTournamentRom *rom) {
    uint8_t winner;
    uint8_t slot;
    if (!rom) return;
    winner = allstar_tournament_rom_match_winner(rom);
    if (winner == 0) return;                                        /* $2871-$2872 */
    slot = (allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT) == 0x05u) ? 0u : 1u;
    tournament_credit_winner(rom, winner,
                             (uint16_t)(ALLSTAR_TR_R2_LEFT + slot),
                             (uint16_t)(ALLSTAR_TR_R2_RIGHT + slot),
                             (uint16_t)(ALLSTAR_TR_R2_WINNERS + slot));
}

/*
 * $0B35.  The ROM writes $FF at $C0F4, then walks down from $C0F3 storing a
 * counter that starts at $1A and ends at $00, and closes with $FF at $C0D8.
 * In address order that leaves $C0D9..$C0F3 holding roster ids 0..26.
 */
void allstar_tournament_rom_build_roster_list(AllStarTournamentRom *rom) {
    uint8_t i;
    if (!rom) return;
    allstar_tournament_rom_poke(rom, 0xC0F4u, 0xFFu);
    for (i = 0; i < ALLSTAR_TR_ROSTER_SIZE; i++) {
        allstar_tournament_rom_poke(rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 1u + i), i);
    }
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_SELECT_LIST, 0xFFu);
}

/* $28CA: hand the list to the bank 2 selector and raise $C185 around the call. */
static void tournament_request_select(AllStarTournamentRom *rom,
                                      uint8_t count,
                                      uint16_t destination,
                                      uint8_t list_length) {
    rom->select.pending = true;
    rom->select.count = count;
    rom->select.destination = destination;
    rom->select.list_length = list_length;
    rom->select_flag = 1u;
}

/* $2890 */
void allstar_tournament_rom_pick_field(AllStarTournamentRom *rom) {
    if (!rom) return;
    allstar_tournament_rom_build_roster_list(rom);                  /* $2890 -> $0B35 */
    tournament_request_select(rom, 0x04u, ALLSTAR_TR_SELECT_LIST, ALLSTAR_TR_ROSTER_SIZE);
}

/* $2897: copy the four round 1 winners into the list, then ask for two pairs. */
void allstar_tournament_rom_seed_semifinals(AllStarTournamentRom *rom) {
    uint8_t i;
    if (!rom) return;
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_SELECT_LIST, 0xFFu);
    for (i = 0; i < 4u; i++) {
        allstar_tournament_rom_poke(rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 1u + i),
                                    allstar_tournament_rom_peek(rom, (uint16_t)(ALLSTAR_TR_R1_WINNERS + i)));
    }
    allstar_tournament_rom_poke(rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 5u), 0xFFu);
    /* HL has advanced past the four winners and now points at $C0CB. */
    tournament_request_select(rom, 0x02u, ALLSTAR_TR_R2_LEFT, 4u);
}

/* $28B0: same, with the two round 2 winners, and HL set explicitly to $C0D1. */
void allstar_tournament_rom_seed_final(AllStarTournamentRom *rom) {
    uint8_t i;
    if (!rom) return;
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_SELECT_LIST, 0xFFu);
    for (i = 0; i < 2u; i++) {
        allstar_tournament_rom_poke(rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 1u + i),
                                    allstar_tournament_rom_peek(rom, (uint16_t)(ALLSTAR_TR_R2_WINNERS + i)));
    }
    allstar_tournament_rom_poke(rom, (uint16_t)(ALLSTAR_TR_SELECT_LIST + 3u), 0xFFu);
    tournament_request_select(rom, 0x01u, ALLSTAR_TR_FINAL_LEFT, 2u);
}

void allstar_tournament_rom_begin(AllStarTournamentRom *rom) {
    if (!rom) return;
    memset(rom, 0, sizeof(*rom));
    rom->lcdc = 0x91u;              /* whatever the menu left behind */
    rom->loaded_bank = 1u;
    rom->phase = ALLSTAR_TR_PHASE_ENTRY;
    rom->sub = 0;
}

uint8_t allstar_tournament_rom_match_number(const AllStarTournamentRom *rom) {
    return allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT);
}

int allstar_tournament_rom_round(const AllStarTournamentRom *rom) {
    uint8_t count = allstar_tournament_rom_match_number(rom);
    if (count <= 4u) return 0;
    if (count <= 6u) return 1;
    return 2;
}

const char* allstar_tournament_rom_step_name(AllStarTournamentStep step) {
    switch (step) {
    case ALLSTAR_TR_STEP_RESET:        return "$22DE";
    case ALLSTAR_TR_STEP_PICK_FIELD:   return "$2890";
    case ALLSTAR_TR_STEP_BANK_WINS:    return "$2835";
    case ALLSTAR_TR_STEP_LOAD_PLAYERS: return "$0FBB";
    case ALLSTAR_TR_STEP_PLAY_MATCH:   return "$0B80";
    case ALLSTAR_TR_STEP_POSTGAME:     return "$10A5";
    case ALLSTAR_TR_STEP_ADVANCE_R1:   return "$284D";
    case ALLSTAR_TR_STEP_SEED_SEMIS:   return "$2897";
    case ALLSTAR_TR_STEP_ADVANCE_R2:   return "$286E";
    case ALLSTAR_TR_STEP_SEED_FINAL:   return "$28B0";
    case ALLSTAR_TR_STEP_DONE:         return "$0FBA";
    default:                           return "?";
    }
}

/* $0F50..$0F55 and $0F7E..$0F83: read one entrant from each cursor. */
static void tournament_fetch_entrants(AllStarTournamentRom *rom) {
    rom->lcdc &= (uint8_t)~0x03u;                                   /* $0F4A */
    rom->current_left = allstar_tournament_rom_peek(rom, rom->cursor_left);
    rom->cursor_left++;
    rom->current_right = allstar_tournament_rom_peek(rom, rom->cursor_right);
    rom->cursor_right++;
}

static void tournament_bump_counter(AllStarTournamentRom *rom) {
    uint8_t count = allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT);
    allstar_tournament_rom_poke(rom, ALLSTAR_TR_MATCH_COUNT, (uint8_t)(count + 1u));
}

AllStarTournamentStep allstar_tournament_rom_step(AllStarTournamentRom *rom) {
    if (!rom) return ALLSTAR_TR_STEP_DONE;

    switch (rom->phase) {

    case ALLSTAR_TR_PHASE_ENTRY:
        switch (rom->sub++) {
        case 0:
            /* $0F2E-$0F35: set 7, res 0, res 1 on $FF40. */
            rom->lcdc = (uint8_t)((rom->lcdc | 0x80u) & (uint8_t)~0x03u);
            allstar_tournament_rom_reset_block(rom);                /* $0F37 */
            return ALLSTAR_TR_STEP_RESET;
        case 1:
            allstar_tournament_rom_pick_field(rom);                 /* $0F3A */
            return ALLSTAR_TR_STEP_PICK_FIELD;
        default:
            rom->music_command = 0;                                 /* $0F40 */
            rom->cursor_left = ALLSTAR_TR_R1_LEFT;                  /* $0F44 */
            rom->cursor_right = ALLSTAR_TR_R1_RIGHT;                /* $0F47 */
            rom->phase = ALLSTAR_TR_PHASE_ROUND_1;
            rom->sub = MATCH_LOAD;
            allstar_tournament_rom_bank_wins(rom);                  /* $0F3D */
            return ALLSTAR_TR_STEP_BANK_WINS;
        }

    case ALLSTAR_TR_PHASE_ROUND_1:
        switch (rom->sub) {
        case MATCH_LOAD:
            tournament_fetch_entrants(rom);
            allstar_tournament_rom_load_players(rom);               /* $0F57 */
            rom->sub = MATCH_PLAY;
            return ALLSTAR_TR_STEP_LOAD_PLAYERS;
        case MATCH_PLAY:
            tournament_bump_counter(rom);                           /* $0F5C-$0F5F */
            rom->sub = MATCH_POSTGAME;
            return ALLSTAR_TR_STEP_PLAY_MATCH;                      /* $0F60 */
        case MATCH_POSTGAME:
            rom->sub = MATCH_ADVANCE;
            return ALLSTAR_TR_STEP_POSTGAME;                        /* $0F63 */
        default:
            allstar_tournament_rom_advance_round_1(rom);            /* $0F66 */
            /* $0F6B-$0F70: loop while $C0BE != $04. */
            if (allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT) != 0x04u) {
                rom->sub = MATCH_LOAD;
            } else {
                rom->phase = ALLSTAR_TR_PHASE_ROUND_2;
                rom->sub = 0;
            }
            return ALLSTAR_TR_STEP_ADVANCE_R1;
        }

    case ALLSTAR_TR_PHASE_ROUND_2:
        if (rom->sub == 0) {
            rom->sub = 1;
            allstar_tournament_rom_seed_semifinals(rom);            /* $0F72 */
            return ALLSTAR_TR_STEP_SEED_SEMIS;
        }
        if (rom->sub == 1) {
            rom->cursor_left = ALLSTAR_TR_R2_LEFT;                  /* $0F78 */
            rom->cursor_right = ALLSTAR_TR_R2_RIGHT;                /* $0F7B */
            rom->sub = MATCH_LOAD + 2u;
            allstar_tournament_rom_bank_wins(rom);                  /* $0F75 */
            return ALLSTAR_TR_STEP_BANK_WINS;
        }
        switch (rom->sub - 2u) {
        case MATCH_LOAD:
            tournament_fetch_entrants(rom);
            allstar_tournament_rom_load_players(rom);               /* $0F85 */
            rom->sub = MATCH_PLAY + 2u;
            return ALLSTAR_TR_STEP_LOAD_PLAYERS;
        case MATCH_PLAY:
            tournament_bump_counter(rom);                           /* $0F8A-$0F8D */
            rom->sub = MATCH_POSTGAME + 2u;
            return ALLSTAR_TR_STEP_PLAY_MATCH;                      /* $0F8E */
        case MATCH_POSTGAME:
            rom->sub = MATCH_ADVANCE + 2u;
            return ALLSTAR_TR_STEP_POSTGAME;                        /* $0F91 */
        default:
            allstar_tournament_rom_advance_round_2(rom);            /* $0F94 */
            /* $0F99-$0F9E: loop while $C0BE != $06. */
            if (allstar_tournament_rom_peek(rom, ALLSTAR_TR_MATCH_COUNT) != 0x06u) {
                rom->sub = MATCH_LOAD + 2u;
            } else {
                rom->phase = ALLSTAR_TR_PHASE_FINAL;
                rom->sub = 0;
            }
            return ALLSTAR_TR_STEP_ADVANCE_R2;
        }

    case ALLSTAR_TR_PHASE_FINAL:
        switch (rom->sub++) {
        case 0:
            allstar_tournament_rom_seed_final(rom);                 /* $0FA0 */
            return ALLSTAR_TR_STEP_SEED_FINAL;
        case 1:
            allstar_tournament_rom_bank_wins(rom);                  /* $0FA3 */
            return ALLSTAR_TR_STEP_BANK_WINS;
        case 2:
            /* $0FA6-$0FB0: the final reads both entrants directly, no cursors. */
            rom->current_left = allstar_tournament_rom_peek(rom, ALLSTAR_TR_FINAL_LEFT);
            rom->current_right = allstar_tournament_rom_peek(rom, ALLSTAR_TR_FINAL_RIGHT);
            allstar_tournament_rom_load_players(rom);
            return ALLSTAR_TR_STEP_LOAD_PLAYERS;
        case 3:
            tournament_bump_counter(rom);                           /* $0FB3-$0FB6 */
            rom->phase = ALLSTAR_TR_PHASE_DONE;
            return ALLSTAR_TR_STEP_PLAY_MATCH;                      /* $0FB7 */
        default:
            rom->phase = ALLSTAR_TR_PHASE_DONE;
            return ALLSTAR_TR_STEP_DONE;
        }

    default:
        return ALLSTAR_TR_STEP_DONE;                                /* $0FBA */
    }
}


