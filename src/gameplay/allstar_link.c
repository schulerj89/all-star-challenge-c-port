#include "allstar_link.h"

/* $2FDA, indexed by $C199. */
static const uint16_t LINK_SEND_TABLE[ALLSTAR_LINK_SEND_SLOTS] = {
    0x2FE2u, 0x2FE8u, 0x2FF1u, 0x2FF6u
};

const uint16_t* allstar_link_send_table(int *count) {
    if (count) *count = ALLSTAR_LINK_SEND_SLOTS;
    return LINK_SEND_TABLE;
}

/* $267F..$26C1 */
void allstar_link_receive(uint8_t link_up, uint8_t busy, uint8_t vblank_flag,
                          uint8_t link_game, uint8_t role,
                          uint8_t received, uint8_t previous_held,
                          AllStarLinkInject *out) {
    if (!out) return;

    out->target = ALLSTAR_LINK_RX_NOTHING;
    out->clears_received = false;
    out->held = previous_held;
    out->pressed = 0;

    if (link_up == 0) {                                             /* $267F-$2683 */
        out->clears_received = true;                                /* $2685 */
        received = 0;
    }

    /* $2689-$2698: a link game that is mid-update hands off to $2EE5. */
    if ((busy != 0 || vblank_flag != 0) && link_game != 0) {
        out->target = ALLSTAR_LINK_RX_DIVERT;
        return;
    }

    if (role == 0) return;                                          /* $269B-$269F */

    /* $26AE and $26B8: only bits newly set count as pressed. */
    out->pressed = (uint8_t)((previous_held ^ received) & received);
    out->held = received;
    out->target = (role == ALLSTAR_LINK_ROLE_PLAYER_2) ? ALLSTAR_LINK_RX_PAD_1
                                                       : ALLSTAR_LINK_RX_PAD_2;
}

/* $2FD0..$2FFF */
void allstar_link_transmit(uint8_t link_up, uint8_t role, uint8_t state_byte,
                           uint8_t interrupt_flags, AllStarLinkTransmit *out) {
    if (!out) return;

    out->transmits = true;
    out->byte = 0;

    if (link_up == 0) {                                             /* $2FD3-$2FD4 */
        out->kind = ALLSTAR_LINK_TX_ZERO;
        return;
    }

    switch (role) {
    case 0x00u:                                                     /* $2FE2 */
        out->kind = ALLSTAR_LINK_TX_IDLE;
        out->transmits = false;
        return;
    case 0x01u:                                                     /* $2FE8 */
        if ((interrupt_flags & ALLSTAR_LINK_IF_SERIAL) != 0) {      /* $2FEA-$2FEC */
            out->kind = ALLSTAR_LINK_TX_IDLE;
            out->transmits = false;
            return;
        }
        out->kind = ALLSTAR_LINK_TX_SYNC;
        out->byte = ALLSTAR_LINK_SYNC_BYTE;                         /* $2FED */
        return;
    case 0x02u:                                                     /* $2FF1 */
    case 0x03u:                                                     /* $2FF6 */
        out->kind = ALLSTAR_LINK_TX_STATE;
        out->byte = state_byte;
        return;
    default:
        out->kind = ALLSTAR_LINK_TX_IDLE;
        out->transmits = false;
        return;
    }
}

/* $2718..$2728 */
void allstar_link_refresh_pad_2(uint8_t context, uint8_t half_a, uint8_t half_b,
                                AllStarLinkPadRefresh *out) {
    if (!out) return;
    out->context = context;                                         /* $2718 */
    out->held = (uint8_t)(half_a | half_b);                         /* $271E-$2724 */
    out->pressed = out->held;                                       /* $2726 */
}
