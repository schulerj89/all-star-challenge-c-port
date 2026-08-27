#ifndef ALLSTAR_HANDSHAKE_H
#define ALLSTAR_HANDSHAKE_H

#include "allstar_types.h"

/*
 * The two-player handshake and the attract entry, ported from $0322..$0383 and
 * $0417..$0443.
 *
 * These are the last two pieces of the link subsystem, and each one closes a
 * loop with something ported earlier:
 *
 *   the cartridge that presses Start first runs $0324 and ends up role $02;
 *   the one that receives runs $035F and ends up role $03, which is the exact
 *   value $267F keys on to decide that the byte it receives belongs in pad 1.
 *
 *   $0417 loads $0E10 into $C26D, which is the countdown $2D1B decrements in
 *   attract mode, and calls the bank 2 selector, whose $4003 check on $FFE4
 *   sends it to the $2D85 CPU auto-pick.
 */

/* ---- $0322..$0383: initiating ---- */

#define ALLSTAR_HANDSHAKE_ATTEMPTS 0x0Au   /* $0322 loads B with this */
#define ALLSTAR_HANDSHAKE_ROLE_INIT 0x01u  /* $032A */
#define ALLSTAR_HANDSHAKE_ROLE_LEAD 0x02u  /* $0358 */
#define ALLSTAR_HANDSHAKE_ROLE_JOIN 0x03u  /* $036C */
#define ALLSTAR_HANDSHAKE_PLAYERS   0x02u  /* $037D */

typedef enum {
    ALLSTAR_HANDSHAKE_RETRY = 0,   /* $0354, an attempt was spent          */
    ALLSTAR_HANDSHAKE_AGREED,      /* $0358, this side becomes role $02    */
    ALLSTAR_HANDSHAKE_ABORT        /* $0380, back to the title screen      */
} AllStarHandshakeResult;

/*
 * One pass of $0324.  Each of the three peer readings is taken after its own
 * vblank, so they are separate observations of $C19E rather than one value.
 *
 * The middle reading only happens when the first did not already answer, and a
 * failure there aborts immediately rather than spending an attempt.
 */
AllStarHandshakeResult allstar_handshake_step(uint8_t peer_first, uint8_t peer_middle,
                                              uint8_t peer_last, uint8_t *attempts,
                                              uint8_t *role, uint8_t *echo);

/* $035F..$0373: the receiving side spins on $C19C, then takes role $03. */
typedef struct {
    uint8_t role;
    uint8_t sends;        /* $0370 transmits a zero through $2FF9 */
    bool unwinds_caller;  /* $035F pops the title loop's counter  */
} AllStarHandshakeAccept;

void allstar_handshake_accept(AllStarHandshakeAccept *out);

/* $0376..$037F: what both success paths leave behind. */
typedef struct {
    uint8_t ffa5;
    uint8_t ffbe;
    uint8_t players;
} AllStarHandshakeReady;

void allstar_handshake_ready(AllStarHandshakeReady *out);

/* ---- $0417..$0443: the attract entry ---- */

#define ALLSTAR_ATTRACT_COUNTDOWN 0x0E10u  /* $0420, sixty seconds of frames */
#define ALLSTAR_ATTRACT_SEED      0x0200u  /* $042B */
#define ALLSTAR_ATTRACT_SKILL     0x01u    /* $0434 */
#define ALLSTAR_ATTRACT_BANK      0x02u    /* $0438 */
#define ALLSTAR_ATTRACT_SELECTOR  0x4000u  /* $043B */

typedef struct {
    uint8_t mode;         /* $FF8F */
    uint8_t link_state;   /* $C19A */
    uint16_t countdown;   /* $C26D, the pair $2D1B counts down */
    uint16_t seed;        /* $FF94 */
    uint8_t skill;        /* $FF97 */
    uint8_t bank;
    uint16_t selector;
} AllStarAttract;

void allstar_attract_setup(AllStarAttract *out);

#endif /* ALLSTAR_HANDSHAKE_H */
