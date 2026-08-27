#ifndef ALLSTAR_GAME_CLOCK_H
#define ALLSTAR_GAME_CLOCK_H

#include "allstar_types.h"

/*
 * The three court clocks, ported from bank 1 $79EE..$7A70 and the shared BCD
 * decrementer at $7A71.
 *
 * Each clock is a little-endian pair holding BCD seconds in the low byte and
 * BCD minutes in the high byte, and each is gated by one bit of $C0BD:
 *
 *   $C0B6/$C0B7   bit 0   the game clock
 *   $C0B8/$C0B9   bit 1   player 1
 *   $C0BA/$C0BB   bit 2   player 2
 *
 * $C0BC counts twenty calls between ticks.  Possession in $FFCF decides which
 * player clock runs: the other player's is reset to $24 -- twenty-four seconds,
 * the shot clock -- and its enable bit is cleared, so only one of the two ever
 * advances.  Bit 0 is preserved across that, because the game clock is enabled
 * elsewhere.
 */

#define ALLSTAR_CLOCK_GAME          0xC0B6u
#define ALLSTAR_CLOCK_PLAYER_1      0xC0B8u
#define ALLSTAR_CLOCK_PLAYER_2      0xC0BAu
#define ALLSTAR_CLOCK_TICK_COUNTER  0xC0BCu
#define ALLSTAR_CLOCK_ENABLE        0xC0BDu

#define ALLSTAR_CLOCK_TICK_RELOAD   0x14u  /* $7A2F, twenty calls per tick   */
#define ALLSTAR_CLOCK_SHOT_RESET    0x24u  /* twenty-four seconds, BCD       */
#define ALLSTAR_CLOCK_SECONDS_WRAP  0x59u  /* $7A83, fifty-nine seconds, BCD */
#define ALLSTAR_CLOCK_WARN_BELOW    0x12u  /* $7A4E                          */
#define ALLSTAR_CLOCK_WARN_SOUND    0x0Bu  /* $7A55                          */

#define ALLSTAR_CLOCK_ENABLE_GAME     0x01u
#define ALLSTAR_CLOCK_ENABLE_PLAYER_1 0x02u
#define ALLSTAR_CLOCK_ENABLE_PLAYER_2 0x04u

/* $79EE..$79F6: two flags stop the clocks entirely. */
bool allstar_clock_suppressed(uint8_t freeze, uint8_t halt);

typedef struct {
    bool resets_player_1;  /* that clock goes back to $24 */
    bool resets_player_2;
    uint8_t enable;        /* what $C0BD becomes; bit 0 is preserved */
} AllStarClockOwnership;

/*
 * $79F7..$7A29.  Accuracy, or nobody holding the ball, resets both player
 * clocks and clears both enable bits.  Otherwise the player without the ball is
 * reset and only the holder's clock is left running.
 */
void allstar_clock_ownership(uint8_t mode, uint8_t possession, uint8_t enable,
                             AllStarClockOwnership *out);

/* $7A2A..$7A2F: returns true on the call that reloads and advances the clocks. */
bool allstar_clock_tick(uint8_t *counter);

/*
 * $7A38..$7A5A.  The warning beeps while the game clock's seconds read below
 * $12, except at exactly 1.
 *
 * ROM BUG, not reproduced.  The mode $01 and $02 tests branch to $7A5A, which
 * is the `pop hl` paired with the `push hl` at $7A42 -- and those branches skip
 * that push.  Both displacements are one short:
 *
 *     $7A3C  28 1C   jr z,$7A5A     should be  28 1D   jr z,$7A5B
 *     $7A40  28 18   jr z,$7A5A     should be  28 19   jr z,$7A5B
 *
 * Landing on $7A5B instead skips the unbalanced pop and rejoins the same code.
 * As written the routine would take its own return address into HL and then
 * return through whatever lay beneath it.
 *
 * It is unreachable in practice -- bit 0 of $C0BD is never set in those modes,
 * so $7A34 always branches away first -- so the port keeps the observable
 * behaviour (no warning for those modes) and drops the stack damage.  That
 * makes it deliberately not bug-compatible: if the path were ever reached the
 * cartridge would crash and this would not.
 */
bool allstar_clock_warns(uint8_t mode, uint16_t game_clock);

/*
 * $7A71.  One BCD step down, seconds wrapping to $59 with a minute borrow.  A
 * clock already reading zero is left alone.  The ROM's `daa` runs with carry
 * clear on both paths, so only the half-carry adjustment ever applies.
 */
uint16_t allstar_clock_decrement(uint16_t clock);

#endif /* ALLSTAR_GAME_CLOCK_H */
