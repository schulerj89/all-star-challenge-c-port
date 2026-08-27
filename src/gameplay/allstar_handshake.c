#include "allstar_handshake.h"

/* $0324..$0356 */
AllStarHandshakeResult allstar_handshake_step(uint8_t peer_first, uint8_t peer_middle,
                                              uint8_t peer_last, uint8_t *attempts,
                                              uint8_t *role, uint8_t *echo) {
    uint8_t local_echo = 0;

    if (!attempts) return ALLSTAR_HANDSHAKE_ABORT;

    if (role) *role = ALLSTAR_HANDSHAKE_ROLE_INIT;                  /* $0328-$032A */

    if (peer_first != 0x01u) {                                      /* $0331-$0333 */
        local_echo = 0x01u;                                         /* $0335-$033A */
        if (peer_middle != 0x01u) {                                 /* $0341-$0343 */
            if (echo) *echo = local_echo;
            return ALLSTAR_HANDSHAKE_ABORT;                         /* $0380 */
        }
    }
    if (echo) *echo = local_echo;                                   /* $0345-$0348 */

    if (peer_last == 0x02u) {                                       /* $034F-$0351 */
        if (role) *role = ALLSTAR_HANDSHAKE_ROLE_LEAD;              /* $0358-$035A */
        return ALLSTAR_HANDSHAKE_AGREED;
    }

    *attempts = (uint8_t)(*attempts - 1u);                          /* $0353 */
    if (*attempts != 0) return ALLSTAR_HANDSHAKE_RETRY;             /* $0354 */
    return ALLSTAR_HANDSHAKE_ABORT;                                 /* $0356 */
}

/* $035F..$0373 */
void allstar_handshake_accept(AllStarHandshakeAccept *out) {
    if (!out) return;
    out->role = ALLSTAR_HANDSHAKE_ROLE_JOIN;                        /* $036C */
    out->sends = 0x00u;                                             /* $036F-$0370 */
    out->unwinds_caller = true;                                     /* $035F */
}

/* $0376..$037F */
void allstar_handshake_ready(AllStarHandshakeReady *out) {
    if (!out) return;
    out->ffa5 = 0x01u;                                              /* $0378 */
    out->ffbe = 0x01u;                                              /* $037A */
    out->players = ALLSTAR_HANDSHAKE_PLAYERS;                       /* $037C-$037D */
}

/* $0417..$0443 */
void allstar_attract_setup(AllStarAttract *out) {
    if (!out) return;
    out->mode = 0x00u;                                              /* $041B */
    out->link_state = 0x00u;                                        /* $041D */
    out->countdown = ALLSTAR_ATTRACT_COUNTDOWN;                     /* $0420-$0428 */
    out->seed = ALLSTAR_ATTRACT_SEED;                               /* $042B-$0432 */
    out->skill = ALLSTAR_ATTRACT_SKILL;                             /* $0434-$0436 */
    out->bank = ALLSTAR_ATTRACT_BANK;                               /* $0438-$043A */
    out->selector = ALLSTAR_ATTRACT_SELECTOR;                       /* $043B */
}
