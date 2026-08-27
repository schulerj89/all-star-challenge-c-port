#include "allstar_frame.h"

/* $2729..$2756 */
void allstar_frame_vblank_2729(uint8_t link_role, uint8_t stall_counter,
                               AllStarFrameVblank *out) {
    if (!out) return;

    out->runs_oam_dma = true;
    out->runs_update = false;
    out->advances_stall = false;
    out->stall_counter = stall_counter;

    if (link_role == ALLSTAR_FRAME_LINK_ROLE_3) {              /* $272E */
        if (stall_counter != 0) {                              /* $2735 */
            /* $2738: the frame is spent bumping $C176 and nothing else --
               the OAM copy does not run either. */
            out->runs_oam_dma = false;
            out->advances_stall = true;
            out->stall_counter = (uint8_t)(stall_counter + 1u);
            return;
        }
        /* $273E: the copy runs, but $2757 does not. */
        return;
    }

    out->runs_update = true;                                   /* $2746 */
}

/* $2749..$2754: bit 1 of STAT is set in modes 2 and 3. */
bool allstar_frame_stat_busy_2749(uint8_t stat) {
    return (stat & 0x02u) != 0;
}

/* $276D..$279D */
void allstar_frame_body_276d(uint8_t link_role, uint8_t serial_pending,
                             uint8_t frame_counter, uint8_t delay_counter,
                             AllStarFrameBody *out) {
    if (!out) return;

    /* $2770: a pending serial transfer buys 256 iterations of push/pop before
       the frame starts, which is the settle the link needs. */
    out->serial_spin = serial_pending != 0 ? ALLSTAR_FRAME_SERIAL_SPIN : 0;

    out->order = (link_role == ALLSTAR_FRAME_LINK_ROLE_3)      /* $2780 */
        ? ALLSTAR_FRAME_ORDER_LINK_FIRST                       /* $2784 */
        : ALLSTAR_FRAME_ORDER_INPUT_FIRST;                     /* $278C */

    out->frame_counter = (uint8_t)(frame_counter + 1u);        /* $2795 */

    /* $2796: the countdown only moves while it is nonzero. */
    out->delay_counter = delay_counter != 0
        ? (uint8_t)(delay_counter - 1u) : 0u;
}

/* $279E..$27C6 */
void allstar_frame_link_send_279e(uint8_t mode, uint8_t players,
                                  uint8_t link_active, uint8_t outgoing,
                                  uint8_t substitute,
                                  AllStarFrameLinkSend *out) {
    if (!out) return;

    out->substitutes = false;
    out->transmitted = outgoing;
    out->restored = outgoing;

    if (mode != 0x01u && mode != 0x03u) return;                /* $27A0-$27A6 */
    if (players == 1u) return;                                 /* $27AB */
    if (link_active == 0) return;                              /* $27B1 */

    /* $27B5-$27C3: push $C16E, put $C18D there for the one $2FD0 call, pop
       the original back. */
    out->substitutes = true;
    out->transmitted = substitute;
    out->restored = outgoing;
}
