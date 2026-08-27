#include "allstar_pad.h"

/* $263D and $264B */
int allstar_pad_settle_reads(uint8_t row_select) {
    if (row_select == ALLSTAR_PAD_SELECT_DIRECTIONS) return ALLSTAR_PAD_SETTLE_DIRECTIONS;
    if (row_select == ALLSTAR_PAD_SELECT_BUTTONS) return ALLSTAR_PAD_SETTLE_BUTTONS;
    return 0;
}

/* $2641..$265B */
uint8_t allstar_pad_assemble(uint8_t direction_row, uint8_t button_row) {
    uint8_t directions = (uint8_t)((uint8_t)~direction_row & 0x0Fu);  /* $2641-$2642 */
    uint8_t buttons = (uint8_t)((uint8_t)~button_row & 0x0Fu);        /* $2657-$2658 */
    return (uint8_t)((uint8_t)(directions << 4) | buttons);           /* $2644, $265A */
}

/* $2660..$267D */
void allstar_pad_dispatch(uint8_t role, uint8_t link_game, uint8_t fresh,
                          uint8_t previous, AllStarPadDispatch *out) {
    if (!out) return;

    out->stores_outgoing = false;
    out->outgoing = previous;
    out->carried = fresh;
    out->calls_link_update = false;

    if (role == 0) {                                                /* $2663-$2664 */
        out->route = ALLSTAR_PAD_ROUTE_LOCAL;
        return;
    }

    /* $2666-$266E: the fresh byte goes out, the previous one is carried on. */
    out->stores_outgoing = true;
    out->outgoing = fresh;
    out->carried = previous;

    out->calls_link_update = (link_game != 0);                      /* $266F-$2673 */

    out->route = (role == 0x03u) ? ALLSTAR_PAD_ROUTE_LINK_ROLE_3    /* $2679-$267B */
                                 : ALLSTAR_PAD_ROUTE_LINK;          /* $267D */
}
