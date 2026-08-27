#ifndef ALLSTAR_MENU_H
#define ALLSTAR_MENU_H

#include "allstar_types.h"

/*
 * The mode-select menu, ported from $038F..$0416.
 *
 * Five entries in $FF8F, wrapping both ways.  Start confirms, but a two-player
 * game cannot select mode $04 -- the tournament -- and when it confirms mode
 * $01 or $03 it records that mode in $C18B, which is the same link-game flag
 * the pause handler at $2BE7 reads.  Free Throw and Accuracy are exactly the
 * two modes whose postgame screens run the $F0 link handshake, so this is where
 * that decision is made.
 *
 * The input masks use the cartridge's own packing, not AllStarButtonMask:
 * bit 0 A, 1 B, 2 Select, 3 Start, 4 Right, 5 Left, 6 Up, 7 Down.
 */

#define ALLSTAR_MENU_MODES        5
#define ALLSTAR_MENU_TILEMAP      0x2ABAu
#define ALLSTAR_MENU_MUSIC        0x81u   /* $039C writes this to $DD73 */
#define ALLSTAR_MENU_LCDC         0x83u   /* $03AA */
#define ALLSTAR_MENU_LINK_FLAG    0xC18Bu
#define ALLSTAR_MENU_TOURNAMENT   0x04u

#define ALLSTAR_MENU_CONFIRM_MASK 0x08u   /* $03D4 bit 3, Start        */
#define ALLSTAR_MENU_MOVE_MASK    0xC3u   /* $03F5, A, B, Up or Down   */
#define ALLSTAR_MENU_BACK_MASK    0x42u   /* $03F9, B or Up steps back */

/* $0397: a two-player game starts the menu music, a one-player game does not. */
bool allstar_menu_plays_music(uint8_t player_count);

/* $03AE..$03B6: the input bytes the menu primes before its first frame. */
typedef struct {
    uint8_t new_player_1;   /* $FFAE */
    uint8_t new_player_2;   /* $FFC7 */
    uint8_t held_player_1;  /* $FFAF */
    uint8_t held_player_2;  /* $FFC8 */
} AllStarMenuInputSeed;

void allstar_menu_seed_input(AllStarMenuInputSeed *out);

typedef enum {
    ALLSTAR_MENU_IDLE = 0,
    ALLSTAR_MENU_MOVED,
    ALLSTAR_MENU_CONFIRMED
} AllStarMenuResult;

/*
 * $03BA..$0415.  One pass of the menu loop.  `lock` is $FFEC, which blocks the
 * confirm but not the cursor.  On a confirm, `link_flag` receives the mode when
 * a two-player game picks $01 or $03, and is left alone otherwise.
 */
AllStarMenuResult allstar_menu_step(uint8_t lock, uint8_t new_buttons,
                                    uint8_t player_count, uint8_t *mode,
                                    uint8_t *link_flag);

#endif /* ALLSTAR_MENU_H */
