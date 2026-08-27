#ifndef ALLSTAR_TOURNAMENT_H
#define ALLSTAR_TOURNAMENT_H

#include "allstar_types.h"

/*
 * Native model of the cartridge tournament (menu mode $04, driver at $0F2E).
 *
 * The ROM keeps the whole bracket in one contiguous work-RAM block and walks it
 * with pointer arithmetic, so the block is modelled here at its real addresses
 * instead of as named fields.  Routines that index it ($284D, $286E, $2835)
 * depend on the exact adjacency of these slots.
 *
 *   $C0BE            match counter, 1-based while a match is running
 *   $C0BF..$C0C2     round 1 left entrants   (4)
 *   $C0C3..$C0C6     round 1 right entrants  (4)
 *   $C0C7..$C0CA     round 1 winners         (4)
 *   $C0CB..$C0CC     round 2 left entrants   (2)
 *   $C0CD..$C0CE     round 2 right entrants  (2)
 *   $C0CF..$C0D0     round 2 winners         (2)
 *   $C0D1            final left entrant
 *   $C0D2            final right entrant
 *   $C0D4/$C0D5      wins for the left/right slot in the current stage
 *   $C0D6/$C0D7      wins for the left/right slot banked across stages
 *   $C0D8..$C0F4     candidate list handed to the bank 2 selector, $FF-terminated
 */
#define ALLSTAR_TOURNAMENT_RAM_BASE   0xC0BEu
#define ALLSTAR_TOURNAMENT_RAM_END    0xC0F4u
#define ALLSTAR_TOURNAMENT_RAM_SIZE   ((ALLSTAR_TOURNAMENT_RAM_END - ALLSTAR_TOURNAMENT_RAM_BASE) + 1u)

#define ALLSTAR_TR_MATCH_COUNT        0xC0BEu
#define ALLSTAR_TR_R1_LEFT            0xC0BFu
#define ALLSTAR_TR_R1_RIGHT           0xC0C3u
#define ALLSTAR_TR_R1_WINNERS         0xC0C7u
#define ALLSTAR_TR_R2_LEFT            0xC0CBu
#define ALLSTAR_TR_R2_RIGHT           0xC0CDu
#define ALLSTAR_TR_R2_WINNERS         0xC0CFu
#define ALLSTAR_TR_FINAL_LEFT         0xC0D1u
#define ALLSTAR_TR_FINAL_RIGHT        0xC0D2u
#define ALLSTAR_TR_STAGE_WINS_LEFT    0xC0D4u
#define ALLSTAR_TR_STAGE_WINS_RIGHT   0xC0D5u
#define ALLSTAR_TR_TOTAL_WINS_LEFT    0xC0D6u
#define ALLSTAR_TR_TOTAL_WINS_RIGHT   0xC0D7u
#define ALLSTAR_TR_SELECT_LIST        0xC0D8u
#define ALLSTAR_TR_ROSTER_SIZE        27u

/* One ROM call made by the $0F2E driver, in the order the driver makes it. */
typedef enum {
    ALLSTAR_TR_STEP_RESET = 0,      /* $0F37 -> $22DE  clear the bracket block  */
    ALLSTAR_TR_STEP_PICK_FIELD,     /* $0F3A -> $2890  choose the eight entrants */
    ALLSTAR_TR_STEP_BANK_WINS,      /* $2835           fold stage wins into totals */
    ALLSTAR_TR_STEP_LOAD_PLAYERS,   /* $0FBB           load both player records  */
    ALLSTAR_TR_STEP_PLAY_MATCH,     /* $0B80           run the One-on-One match  */
    ALLSTAR_TR_STEP_POSTGAME,       /* $10A5           postgame screen           */
    ALLSTAR_TR_STEP_ADVANCE_R1,     /* $284D           record a round 1 winner   */
    ALLSTAR_TR_STEP_SEED_SEMIS,     /* $2897           show and seed round 2     */
    ALLSTAR_TR_STEP_ADVANCE_R2,     /* $286E           record a round 2 winner   */
    ALLSTAR_TR_STEP_SEED_FINAL,     /* $28B0           show and seed the final   */
    ALLSTAR_TR_STEP_DONE            /* $0FBA           driver returns            */
} AllStarTournamentStep;

typedef enum {
    ALLSTAR_TR_PHASE_ENTRY = 0,
    ALLSTAR_TR_PHASE_ROUND_1,
    ALLSTAR_TR_PHASE_ROUND_2,
    ALLSTAR_TR_PHASE_FINAL,
    ALLSTAR_TR_PHASE_DONE
} AllStarTournamentPhase;

/*
 * What $28CA hands to the bank 2 selector at $4000: a count in A, a $FF-bounded
 * candidate list at $C0D8, and HL as the driver left it.  The selector itself is
 * bank 2 work and is not ported yet, so the request is recorded for the caller
 * to satisfy.
 */
typedef struct {
    bool pending;
    uint8_t count;             /* A: 4 entrants, 2 semifinalists, 1 final pair */
    uint16_t destination;      /* HL as the caller left it                     */
    uint8_t list_length;       /* entries between the $FF sentinels            */
} AllStarTournamentSelect;

typedef struct {
    uint8_t ram[ALLSTAR_TOURNAMENT_RAM_SIZE];

    uint8_t current_left;      /* $FFAC */
    uint8_t current_right;     /* $FFC5 */
    uint8_t loaded_slot[2];    /* records $0FBB pushed through $FFF6/$FF8C     */
    uint8_t loaded_bank;       /* bank selected when $0FBB returns             */

    uint16_t score_left;       /* $C133/$C134 as $0B80 leaves them             */
    uint16_t score_right;      /* $C135/$C136                                  */

    AllStarTournamentSelect select;

    uint8_t lcdc;              /* $FF40 */
    uint8_t music_command;     /* $DD73 */
    uint8_t select_flag;       /* $C185, raised around the bank 2 selector     */

    AllStarTournamentPhase phase;
    uint8_t sub;               /* position inside the current phase            */
    uint16_t cursor_left;      /* driver HL: next left entrant                 */
    uint16_t cursor_right;     /* driver DE: next right entrant                */
} AllStarTournamentRom;

uint8_t allstar_tournament_rom_peek(const AllStarTournamentRom *rom, uint16_t address);
void allstar_tournament_rom_poke(AllStarTournamentRom *rom, uint16_t address, uint8_t value);

/* $22DE: zero the match counter and both win-counter pairs. */
void allstar_tournament_rom_reset_block(AllStarTournamentRom *rom);

/* $0FBB: page in bank 2 and load $FFAC into slot 1 and $FFC5 into slot 2. */
void allstar_tournament_rom_load_players(AllStarTournamentRom *rom);

/* $2835: fold the stage win counters into the running totals and clear them. */
void allstar_tournament_rom_bank_wins(AllStarTournamentRom *rom);

/* $28E1: 0 when the two score words are equal, else 1 or 2 for the higher. */
uint8_t allstar_tournament_rom_match_winner(const AllStarTournamentRom *rom);

/* $284D / $286E: credit the winner and copy it into the next bracket slot. */
void allstar_tournament_rom_advance_round_1(AllStarTournamentRom *rom);
void allstar_tournament_rom_advance_round_2(AllStarTournamentRom *rom);

/* $0B35: build the descending 27-entry candidate list bounded by $FF. */
void allstar_tournament_rom_build_roster_list(AllStarTournamentRom *rom);

/* $2890 / $2897 / $28B0: raise a selector request for the bank 2 screen. */
void allstar_tournament_rom_pick_field(AllStarTournamentRom *rom);
void allstar_tournament_rom_seed_semifinals(AllStarTournamentRom *rom);
void allstar_tournament_rom_seed_final(AllStarTournamentRom *rom);

/* Enter the driver at $0F2E. */
void allstar_tournament_rom_begin(AllStarTournamentRom *rom);

/* Perform the next ROM call the driver makes and report which one it was. */
AllStarTournamentStep allstar_tournament_rom_step(AllStarTournamentRom *rom);

uint8_t allstar_tournament_rom_match_number(const AllStarTournamentRom *rom);
int allstar_tournament_rom_round(const AllStarTournamentRom *rom);
const char* allstar_tournament_rom_step_name(AllStarTournamentStep step);

#endif /* ALLSTAR_TOURNAMENT_H */
