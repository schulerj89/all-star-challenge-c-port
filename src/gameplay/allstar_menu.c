#include "allstar_menu.h"

/* $0397..$039E */
bool allstar_menu_plays_music(uint8_t player_count) {
    return player_count != 0x01u;
}

/* $03AE..$03B6 */
void allstar_menu_seed_input(AllStarMenuInputSeed *out) {
    if (!out) return;
    out->new_player_1 = 0x00u;                                      /* $03AF */
    out->new_player_2 = 0x00u;                                      /* $03B1 */
    out->held_player_1 = 0xFFu;                                     /* $03B4 */
    out->held_player_2 = 0xFFu;                                     /* $03B6 */
}

/* $03BA..$0415 */
AllStarMenuResult allstar_menu_step(uint8_t lock, uint8_t new_buttons,
                                    uint8_t player_count, uint8_t *mode,
                                    uint8_t *link_flag) {
    bool one_player;
    bool can_confirm;
    uint8_t accepted;

    if (!mode) return ALLSTAR_MENU_IDLE;

    one_player = (player_count == 0x01u);

    /* $03C2-$03D6: three separate reasons the confirm is refused. */
    can_confirm = true;
    if (!one_player && *mode == ALLSTAR_MENU_TOURNAMENT) can_confirm = false;
    if (lock != 0) can_confirm = false;
    if ((new_buttons & ALLSTAR_MENU_CONFIRM_MASK) == 0) can_confirm = false;

    if (can_confirm) {
        /* $03D8-$03E7: a two-player Free Throw or Accuracy game is a link game. */
        if (!one_player && (*mode == 0x01u || *mode == 0x03u)) {
            if (link_flag) *link_flag = *mode;
        }
        return ALLSTAR_MENU_CONFIRMED;                              /* $03EA-$03F0 */
    }

    accepted = (uint8_t)(new_buttons & ALLSTAR_MENU_MOVE_MASK);     /* $03F5 */
    if (accepted == 0) return ALLSTAR_MENU_IDLE;                    /* $03F7 */

    if ((accepted & ALLSTAR_MENU_BACK_MASK) != 0) {                 /* $03F9-$03FB */
        /* $0407-$040E: stepping below zero wraps to the last entry. */
        *mode = (*mode == 0) ? (uint8_t)(ALLSTAR_MENU_MODES - 1u)
                             : (uint8_t)(*mode - 1u);
    } else {
        /* $03FD-$0405: stepping past the last entry wraps to zero. */
        *mode = (uint8_t)(*mode + 1u);
        if (*mode >= ALLSTAR_MENU_MODES) *mode = 0u;
    }
    return ALLSTAR_MENU_MOVED;                                      /* $0410-$0415 */
}
