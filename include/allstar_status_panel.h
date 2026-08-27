#ifndef ALLSTAR_STATUS_PANEL_H
#define ALLSTAR_STATUS_PANEL_H

#include "allstar_types.h"

/*
 * The settings summary panel, ported from $2578 and its handlers at $2585,
 * $25ED, $2607 and $2608, plus the shared string writer at $2517 and the BCD
 * digit renderer at $24E4/$2500/$250D.
 *
 * $2578 is the third and last mode-indexed dispatcher in the cartridge, after
 * $10D9/$10E4 and $147B.  With this one ported, every `ldh a,[$ff8f]` followed
 * by `rst $08` in the ROM is accounted for.
 *
 * Each field is a short run of tiles terminated by a byte with bit 7 set.
 * $FF8D selects which entry of a list to draw, and the settings themselves live
 * in $FF92 (a 16-bit BCD value) and the bytes $FF95..$FF9B.
 */

#define ALLSTAR_STATUS_SLOTS        5
#define ALLSTAR_STATUS_TERMINATOR   0x80u
#define ALLSTAR_STATUS_DIGIT_TABLE  0x250Du
#define ALLSTAR_STATUS_DIGIT_TILES  4
#define ALLSTAR_STATUS_DIGIT_BUFFER 0xC1A1u
#define ALLSTAR_STATUS_VALUE        0xFF92u  /* the 16-bit BCD setting */
#define ALLSTAR_STATUS_FILLER       0x2559u  /* drawn when it reads zero */
#define ALLSTAR_STATUS_MAX_OPS      4

/* $257B: mode $02 draws nothing, and mode $04 shares One-on-One's handler. */
const uint16_t* allstar_status_table(int *count);

typedef enum {
    ALLSTAR_STATUS_FIELD_DIGITS = 0,  /* $2598, the $FF92 word rendered  */
    ALLSTAR_STATUS_FIELD_FILLER,      /* $258E, four tiles from $2559    */
    ALLSTAR_STATUS_FIELD_STRING       /* $2517                           */
} AllStarStatusFieldKind;

typedef enum {
    ALLSTAR_STATUS_INDEX_NONE = 0,
    ALLSTAR_STATUS_INDEX_DIRECT,      /* the byte is the entry number     */
    ALLSTAR_STATUS_INDEX_MINUS_ONE,   /* $25AC decrements it first        */
    ALLSTAR_STATUS_INDEX_BUCKET_3,    /* $25C5, three thresholds          */
    ALLSTAR_STATUS_INDEX_BUCKET_2     /* $25EF, two thresholds            */
} AllStarStatusIndexKind;

typedef struct {
    AllStarStatusFieldKind kind;
    uint16_t source;   /* the string list, or $2559 for the filler */
    uint16_t io;       /* which $FFxx byte selects the entry       */
    AllStarStatusIndexKind index;
    uint8_t d;
    uint8_t e;
} AllStarStatusOp;

/*
 * The fields each mode draws, in order.  `value_is_zero` chooses between the
 * rendered digits and the $2559 filler for the first One-on-One field, which is
 * the only field whose kind depends on the data.
 */
int allstar_status_layout(uint8_t mode, bool value_is_zero,
                          AllStarStatusOp *out, int max);

/*
 * $2517.  Walk a bit-7 terminated list to entry `index`, then measure that
 * entry.  Returns false when the list runs out.
 */
bool allstar_status_entry(const uint8_t *list, uint16_t length, uint8_t index,
                          uint16_t *offset, uint8_t *entry_length);

/* $25C5: $02 maps to 0, $05 to 1, $08 to 2, anything else to 3. */
uint8_t allstar_status_bucket_3(uint8_t value);

/* $25EF: $05 maps to 0, $10 to 1, anything else to 2. */
uint8_t allstar_status_bucket_2(uint8_t value);

/*
 * $24E4 through $2500.  Four nibbles of a BCD word, high to low, each looked up
 * in the $250D table, which simply maps digit N to tile N + 1.
 */
void allstar_status_digits(uint16_t bcd_value, uint8_t *tiles);

#endif /* ALLSTAR_STATUS_PANEL_H */
