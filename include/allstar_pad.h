#ifndef ALLSTAR_PAD_H
#define ALLSTAR_PAD_H

#include "allstar_types.h"

/*
 * The joypad poll, ported from $2639..$267D.
 *
 * This routine is the authority on the cartridge's button packing.  Earlier
 * work inferred the layout from the masks other routines use -- $0C confirming,
 * $33 moving, $22 stepping backward -- and this confirms it from the hardware
 * read itself:
 *
 *   $263B writes $20, which pulls P14 low and selects the **directions**.  The
 *   four bits come back inverted, are masked, and are then `swap`ped into the
 *   HIGH nibble.
 *
 *   $2649 writes $10, which selects the **buttons**.  Those four bits stay in
 *   the LOW nibble and are ORed in.
 *
 * So the assembled byte is:
 *
 *   bit 0 A   bit 1 B   bit 2 Select   bit 3 Start
 *   bit 4 Right   bit 5 Left   bit 6 Up   bit 7 Down
 *
 * which is NOT AllStarButtonMask.  Every raw mask elsewhere in the port --
 * allstar_select.h, allstar_menu.h, allstar_court_state.h and the rest -- is in
 * these terms.  Convert at the boundary.
 *
 * The two rows are not settled the same way: the direction row takes two dummy
 * reads and the button row takes six.
 */

#define ALLSTAR_PAD_A       0x01u
#define ALLSTAR_PAD_B       0x02u
#define ALLSTAR_PAD_SELECT  0x04u
#define ALLSTAR_PAD_START   0x08u
#define ALLSTAR_PAD_RIGHT   0x10u
#define ALLSTAR_PAD_LEFT    0x20u
#define ALLSTAR_PAD_UP      0x40u
#define ALLSTAR_PAD_DOWN    0x80u

#define ALLSTAR_PAD_SELECT_DIRECTIONS 0x20u  /* $2639 */
#define ALLSTAR_PAD_SELECT_BUTTONS    0x10u  /* $2647 */
#define ALLSTAR_PAD_DESELECT          0x30u  /* $265C */
#define ALLSTAR_PAD_SETTLE_DIRECTIONS 2      /* $263D..$263F */
#define ALLSTAR_PAD_SETTLE_BUTTONS    6      /* $264B..$2655 */
#define ALLSTAR_PAD_OUTGOING          0xC16Eu

/* How many dummy reads each row takes before the value is trusted. */
int allstar_pad_settle_reads(uint8_t row_select);

/*
 * $2641..$265B.  Both rows arrive active-low, so each is complemented and
 * masked; the direction nibble is then shifted up and the two are combined.
 */
uint8_t allstar_pad_assemble(uint8_t direction_row, uint8_t button_row);

typedef enum {
    ALLSTAR_PAD_ROUTE_LOCAL = 0,  /* $2664, a solo game keeps the byte    */
    ALLSTAR_PAD_ROUTE_LINK,       /* $267D, a link game that is not role 3 */
    ALLSTAR_PAD_ROUTE_LINK_ROLE_3 /* $267B                                 */
} AllStarPadRoute;

typedef struct {
    AllStarPadRoute route;
    bool stores_outgoing;    /* $C16E takes the freshly polled byte       */
    uint8_t outgoing;        /* what $2FD0 will transmit                  */
    uint8_t carried;         /* C is left holding the previous byte       */
    bool calls_link_update;  /* $2673 calls $2EA8 when $C18B is set       */
} AllStarPadDispatch;

/*
 * $2660..$267D.  In a link game the fresh byte is swapped into $C16E and the
 * previous one is carried on in C.  $C16E is exactly what $2FD0 sends for roles
 * $02 and $03, so this is where the outgoing half of the link starts.
 */
void allstar_pad_dispatch(uint8_t role, uint8_t link_game, uint8_t fresh,
                          uint8_t previous, AllStarPadDispatch *out);

#endif /* ALLSTAR_PAD_H */
