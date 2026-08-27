#ifndef ALLSTAR_SESSION_H
#define ALLSTAR_SESSION_H

#include "allstar_types.h"

/*
 * One game session, ported from $0214..$0265.
 *
 * This is the routine the outer loop at $01F1 calls to play a game, and it is
 * where the whole control flow finally joins up: menu, settings, player pick,
 * the mode dispatch through $0267, the postgame screen, and then a jump back to
 * the reset vector.
 *
 * Two things are worth knowing before reading it.
 *
 * The tournament is the only mode that does **not** pick a player here.  Every
 * other mode calls the bank 2 selector with a count of one; mode $04 skips
 * straight past, because $2890 will later run its own eight-entrant selection.
 *
 * A finished game does not return to the caller.  $0263 jumps to $0156, the
 * soft-reset vector, so every completed game wipes work RAM, the sound state
 * and HRAM.  That is why the seed preservation in $0150 matters -- it is not a
 * rare path taken when someone holds four buttons, it runs after every game.
 */

#define ALLSTAR_SESSION_TILE_SOURCE  0x640Fu  /* $022D */
#define ALLSTAR_SESSION_TILE_TARGET  0x8C00u
#define ALLSTAR_SESSION_SELECT_COUNT 0x01u    /* $0245 */
#define ALLSTAR_SESSION_BGP          0xE4u    /* $025C */
#define ALLSTAR_SESSION_MODE_TABLE   0x0267u  /* $0255 */
#define ALLSTAR_SESSION_RESET_VECTOR 0x0156u  /* $0263 */
#define ALLSTAR_SESSION_TOURNAMENT   0x04u
#define ALLSTAR_SESSION_MAX_STEPS    9

typedef enum {
    ALLSTAR_SESSION_CLEAR_FLAG = 0,  /* $0214, $C270 is cleared        */
    ALLSTAR_SESSION_MENU,            /* $0221 -> $038F                 */
    ALLSTAR_SESSION_SETTINGS,        /* $0224 -> $22EF                 */
    ALLSTAR_SESSION_LOAD_TILES,      /* $022D, $640F into $8C00        */
    ALLSTAR_SESSION_PICK_PLAYER,     /* $023F, every mode but $04      */
    ALLSTAR_SESSION_PREPARE,         /* $0250 -> $1FFA                 */
    ALLSTAR_SESSION_RUN_MODE,        /* $0259, the $0267 dispatch      */
    ALLSTAR_SESSION_POSTGAME,        /* $0260 -> $10A5                 */
    ALLSTAR_SESSION_RESET            /* $0263 -> $0156, never returns  */
} AllStarSessionStep;

/* $0239..$023D: only the tournament skips the single-player pick. */
bool allstar_session_picks_player(uint8_t mode);

/* The steps a session runs, in order, for a given mode. */
int allstar_session_sequence(uint8_t mode, AllStarSessionStep *out, int max);

/* Which ROM bank is paged in when each step runs. */
uint8_t allstar_session_bank(AllStarSessionStep step);

const char* allstar_session_step_name(AllStarSessionStep step);

#endif /* ALLSTAR_SESSION_H */
