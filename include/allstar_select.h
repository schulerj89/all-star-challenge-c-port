#ifndef ALLSTAR_SELECT_H
#define ALLSTAR_SELECT_H

#include "allstar_types.h"

/*
 * Bank 2 entrant selector, ported from $4000..$40F3 plus the cursor loop at
 * $40F4.  This is the screen the fixed-bank tournament code hands its candidate
 * list to: $2890, $2897 and $28B0 fill $C0D8.. and call in with a count in A,
 * and the selector writes the chosen ids straight into the bracket slots.
 *
 * $C17F holds that count, and the destination tables at $4358 and $4360 turn it
 * into the very slots the $0F2E driver later reads:
 *
 *   $C17F  pass 1 -> ...   pass 2 -> ...   meaning
 *     4      $C0BF           $C0C3         four entrants each, eight in total
 *     2      $C0CB           $C0CD         two semifinalists each
 *     1      $C0D1           $C0D2         the two finalists
 *     3      unused          unused
 *
 * IMPORTANT: the input bytes this file works with ($FFAE, $FFAF, $FFC7, $FFC8)
 * use the cartridge's own bit packing, which is NOT AllStarButtonMask:
 *
 *   bit 0 A   bit 1 B   bit 2 Select   bit 3 Start
 *   bit 4 Right   bit 5 Left   bit 6 Up   bit 7 Down
 *
 * Convert at the boundary; do not mix these masks with ALLSTAR_BTN_*.
 */

#define ALLSTAR_SELECT_ROM_A       0x01u
#define ALLSTAR_SELECT_ROM_B       0x02u
#define ALLSTAR_SELECT_ROM_SELECT  0x04u
#define ALLSTAR_SELECT_ROM_START   0x08u
#define ALLSTAR_SELECT_ROM_RIGHT   0x10u
#define ALLSTAR_SELECT_ROM_LEFT    0x20u

#define ALLSTAR_SELECT_CONFIRM_MASK 0x0Cu  /* $410F: Select or Start          */
#define ALLSTAR_SELECT_MOVE_MASK    0x33u  /* $4119: A, B, Right or Left      */
#define ALLSTAR_SELECT_BACK_MASK    0x22u  /* $4127: B or Left steps backward */

#define ALLSTAR_SELECT_LIST_BASE    0xC0D9u  /* first candidate after $C0D8   */
#define ALLSTAR_SELECT_CURSOR       0xC0F5u  /* $C0F5/$C0F6 hold the pointer  */
#define ALLSTAR_SELECT_WRITE_CURSOR 0xC182u  /* $C182/$C183 hold the output   */
#define ALLSTAR_SELECT_SENTINEL     0xFFu

#define ALLSTAR_SELECT_PASS_1  1u
#define ALLSTAR_SELECT_PASS_2  2u

/* $400F..$401D: modes $01 and $03 with one player run pass 1 only. */
bool allstar_select_runs_second_pass(uint8_t player_count, uint8_t mode);

/* $4045 and $40C8: where a pass writes, from the $4358 / $4360 tables. */
uint16_t allstar_select_destination(uint8_t stage, uint8_t pass);

/* $406A: the prompt id handed to $2DEA before a pass. */
uint8_t allstar_select_prompt(uint8_t mode, uint8_t stage, uint8_t picker,
                              uint8_t player_count);

/* $40C4: the duplicate scan covers both passes for this stage. */
uint8_t allstar_select_scan_length(uint8_t stage);

/* $4100..$410D: whose held-input byte the cursor loop reads. */
uint8_t allstar_select_buttons(uint8_t player_count, uint8_t picker,
                               uint8_t player_1_held, uint8_t player_2_held);

typedef enum {
    ALLSTAR_SELECT_IDLE = 0,
    ALLSTAR_SELECT_MOVED,
    ALLSTAR_SELECT_CONFIRMED
} AllStarSelectInput;

/*
 * $40F7..$4149.  One pass of the cursor loop over the $FF-bounded candidate
 * list.  Confirm is tested before movement, both read held input rather than
 * new input, and running off either end wraps to the other.
 */
AllStarSelectInput allstar_select_step(uint8_t hold_lock, uint8_t held,
                                       uint8_t length, uint8_t *index);

/* $40BA..$40DC: reject a pick already present in this stage's output. */
bool allstar_select_is_duplicate(const uint8_t *picked, uint8_t scan_length, uint8_t candidate);

#endif /* ALLSTAR_SELECT_H */
