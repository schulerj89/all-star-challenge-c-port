#ifndef ALLSTAR_LINK_H
#define ALLSTAR_LINK_H

#include "allstar_types.h"

/*
 * The serial link layer, ported from $267F (receive), $2FD0 and its table at
 * $2FDA (transmit), and $2718 (pad refresh).  Together with the $0B44 wait in
 * allstar_system.h this is the whole of the cartridge's two-player plumbing.
 *
 * How the pieces found earlier fit together:
 *
 *   $038F   a two-player Free Throw or Accuracy game writes the mode to $C18B
 *   $2FD0   sends a byte from $C16E, or the $D5 sync byte, chosen by $C199
 *   $007B   the serial interrupt receives into $C19B and raises $C19C
 *   $0B44   the game spins until $C19C is raised, then clears it
 *   $267F   injects $C19B as the remote pad's held state
 *   $1121   the postgame screens hand-shake on the $F0 score-high flags
 *   $2BC6   pause posts $CC into $C18E instead of pausing directly
 *
 * $C199 is this cartridge's role.  $03 means it is player 2, so the byte it
 * receives is player 1's input and belongs in pad 1; any other non-zero role
 * means the reverse.  Role $00 is a solo game and injects nothing.
 */

#define ALLSTAR_LINK_ROLE_PLAYER_2  0x03u   /* $C199 */
#define ALLSTAR_LINK_SYNC_BYTE      0xD5u   /* $2FED */
#define ALLSTAR_LINK_SERIAL_DATA    0xFF01u
#define ALLSTAR_LINK_SERIAL_CONTROL 0xFF02u
#define ALLSTAR_LINK_IF_SERIAL      0x08u   /* $FF0F bit 3 */
#define ALLSTAR_LINK_DIVERT         0x2EE5u /* $2698 */
#define ALLSTAR_LINK_SEND_SLOTS     4

/* ---- $267F: turn a received byte into a pad state ---- */

typedef enum {
    ALLSTAR_LINK_RX_NOTHING = 0,  /* $269F, a solo game            */
    ALLSTAR_LINK_RX_DIVERT,       /* $2698, hands off to $2EE5     */
    ALLSTAR_LINK_RX_PAD_1,        /* $26AA, this side is player 2  */
    ALLSTAR_LINK_RX_PAD_2         /* $26B8                         */
} AllStarLinkReceive;

typedef struct {
    AllStarLinkReceive target;
    bool clears_received;   /* $2685, when $C19A says the link is down */
    uint8_t held;           /* what the pad's held byte becomes        */
    uint8_t pressed;        /* and its newly-pressed byte              */
} AllStarLinkInject;

/*
 * $267F..$26C1.  The newly-pressed byte is `(held ^ received) & received`, so
 * only bits that are set now and were clear before survive.
 */
void allstar_link_receive(uint8_t link_up, uint8_t busy, uint8_t vblank_flag,
                          uint8_t link_game, uint8_t role,
                          uint8_t received, uint8_t previous_held,
                          AllStarLinkInject *out);

/* ---- $2FD0: choose and send a byte ---- */

/* $2FDA, indexed by $C199. */
const uint16_t* allstar_link_send_table(int *count);

typedef enum {
    ALLSTAR_LINK_TX_IDLE = 0,     /* $2FE2, role $00 sends nothing        */
    ALLSTAR_LINK_TX_SYNC,         /* $2FE8, the $D5 byte                  */
    ALLSTAR_LINK_TX_STATE,        /* $2FF1 and $2FF6, the byte in $C16E   */
    ALLSTAR_LINK_TX_ZERO          /* $2FD4, the link is down              */
} AllStarLinkSend;

typedef struct {
    AllStarLinkSend kind;
    bool transmits;
    uint8_t byte;
} AllStarLinkTransmit;

/*
 * $2FD0..$2FFF.  Role $01 skips the send entirely while a serial interrupt is
 * still pending in $FF0F.  A transmit clears $FF02 before writing $FF01.
 */
void allstar_link_transmit(uint8_t link_up, uint8_t role, uint8_t state_byte,
                           uint8_t interrupt_flags, AllStarLinkTransmit *out);

/* ---- $2718: refresh pad 2 from the raw joypad halves ---- */

typedef struct {
    uint8_t context;   /* $C127 */
    uint8_t held;      /* $FFC8 */
    uint8_t pressed;   /* $FFC7 */
} AllStarLinkPadRefresh;

/* $2718..$2728.  Both pad-2 bytes receive the same merged value. */
void allstar_link_refresh_pad_2(uint8_t context, uint8_t half_a, uint8_t half_b,
                                AllStarLinkPadRefresh *out);

#endif /* ALLSTAR_LINK_H */
