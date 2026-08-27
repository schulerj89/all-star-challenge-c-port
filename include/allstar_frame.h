#ifndef ALLSTAR_FRAME_H
#define ALLSTAR_FRAME_H

#include "allstar_types.h"

/*
 * The vblank interrupt and the per-frame update it drives, ported from
 * $2729..$27C6.
 *
 * This is the spine every other per-frame routine hangs off: $2729 is the
 * handler itself, $2757 trampolines into bank 1, $276D is the body, and $279E
 * decides which byte the link sends this frame.  $FF8B, the frame counter the
 * body increments, is the same counter the postgame banner flashes off.
 */

#define ALLSTAR_FRAME_OAM_DMA        0xFF80u  /* $273E/$2743, the HRAM copier */
#define ALLSTAR_FRAME_COUNTER        0xFF8Bu  /* $2792                        */
#define ALLSTAR_FRAME_DELAY_COUNTER  0xFF8Au  /* $2796                        */
#define ALLSTAR_FRAME_SERIAL_SPIN    256      /* $2773, b wraps from zero     */
#define ALLSTAR_FRAME_LINK_ROLE_3    0x03u    /* $272E, $277F                 */
#define ALLSTAR_FRAME_SUBSTITUTE     0xC18Du  /* $27B9                        */
#define ALLSTAR_FRAME_OUTGOING       0xC16Eu  /* $27B5, what $2FD0 sends      */

/*
 * $2729.  Role $03 with a nonzero stall counter spends the frame incrementing
 * that counter and does nothing else -- not even the OAM copy.
 */
typedef struct {
    bool runs_oam_dma;      /* $273E/$2743 call $FF80                        */
    bool runs_update;       /* $2746 calls $2757                             */
    bool advances_stall;    /* $2738 bumps $C176 instead of running the frame */
    uint8_t stall_counter;  /* the value $C176 is left holding               */
} AllStarFrameVblank;

void allstar_frame_vblank_2729(uint8_t link_role, uint8_t stall_counter,
                               AllStarFrameVblank *out);

/*
 * $2749..$2754.  After the frame work the handler waits for STAT's mode bit 1
 * to go high and then low again, which parks the return on a fixed point of
 * the scanline.  Bit 1 of $FF41 is set in modes 2 and 3.
 */
bool allstar_frame_stat_busy_2749(uint8_t stat);

/*
 * $276D.  $2624 polls the pad and $279E hands the link its byte; which of the
 * two runs first is decided by the role, so the two cartridges do not both
 * transmit from the same half of the frame.
 */
typedef enum {
    ALLSTAR_FRAME_ORDER_INPUT_FIRST = 0, /* $278C, every role except $03 */
    ALLSTAR_FRAME_ORDER_LINK_FIRST       /* $2784, role $03              */
} AllStarFrameOrder;

typedef struct {
    AllStarFrameOrder order;
    int serial_spin;        /* $2773, iterations burned before anything else */
    uint8_t frame_counter;  /* $FF8B after $2795                             */
    uint8_t delay_counter;  /* $FF8A after $279A                             */
} AllStarFrameBody;

void allstar_frame_body_276d(uint8_t link_role, uint8_t serial_pending,
                             uint8_t frame_counter, uint8_t delay_counter,
                             AllStarFrameBody *out);

/*
 * $279E.  Modes $01 and $03 with two players and $FF90 set send $C18D in place
 * of the live pad byte and put the real one back afterwards; everything else
 * falls straight through to $2FD0 with $C16E untouched.
 */
typedef struct {
    bool substitutes;        /* $27B5..$27C3 swap $C18D in and back out */
    uint8_t transmitted;     /* what $2FD0 sees in $C16E                */
    uint8_t restored;        /* what $C16E holds on return              */
} AllStarFrameLinkSend;

void allstar_frame_link_send_279e(uint8_t mode, uint8_t players,
                                  uint8_t link_active, uint8_t outgoing,
                                  uint8_t substitute,
                                  AllStarFrameLinkSend *out);

#endif /* ALLSTAR_FRAME_H */
