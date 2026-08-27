#include "allstar_status_panel.h"

/* $257B, indexed by $FF8F. */
static const uint16_t STATUS_TABLE[ALLSTAR_STATUS_SLOTS] = {
    0x2585u, 0x25EDu, 0x2607u, 0x2608u, 0x2585u
};

const uint16_t* allstar_status_table(int *count) {
    if (count) *count = ALLSTAR_STATUS_SLOTS;
    return STATUS_TABLE;
}

/*
 * $2585 for One-on-One and the tournament, $25ED for Free Throw, $2607 for
 * H-O-R-S-E (a bare `ret`), and $2608 for Accuracy, which draws two fields of
 * its own and then jumps into $2585's tail at $25C5 to share the last one.
 */
int allstar_status_layout(uint8_t mode, bool value_is_zero,
                          AllStarStatusOp *out, int max) {
    int count = 0;

    if (!out || max <= 0) return 0;

    if (mode == 0x02u) return 0;                                    /* $2607 */

    if (mode == 0x03u) {
        if (count < max) {                                          /* $2608-$2612 */
            out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
            out[count].source = 0x253Du; out[count].io = 0xFF9Bu;
            out[count].index = ALLSTAR_STATUS_INDEX_DIRECT;
            out[count].d = 0x0Fu; out[count].e = 0x03u; count++;
        }
        if (count < max) {                                          /* $2615-$261F */
            out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
            out[count].source = 0x253Du; out[count].io = 0xFF9Au;
            out[count].index = ALLSTAR_STATUS_INDEX_DIRECT;
            out[count].d = 0x0Fu; out[count].e = 0x04u; count++;
        }
    } else if (mode == 0x01u) {
        if (count < max) {                                          /* $25ED-$2604 */
            out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
            out[count].source = 0x2543u; out[count].io = 0xFF98u;
            out[count].index = ALLSTAR_STATUS_INDEX_BUCKET_2;
            out[count].d = 0x09u; out[count].e = 0x04u; count++;
        }
        return count;
    } else {
        if (count < max) {                                          /* $258B-$25A7 */
            out[count].kind = value_is_zero ? ALLSTAR_STATUS_FIELD_FILLER
                                            : ALLSTAR_STATUS_FIELD_DIGITS;
            out[count].source = value_is_zero ? ALLSTAR_STATUS_FILLER
                                              : ALLSTAR_STATUS_DIGIT_BUFFER;
            out[count].io = ALLSTAR_STATUS_VALUE;
            out[count].index = ALLSTAR_STATUS_INDEX_NONE;
            out[count].d = 0x0Fu; out[count].e = 0x09u; count++;
        }
        if (count < max) {                                          /* $25AA-$25B5 */
            out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
            out[count].source = 0x253Au; out[count].io = 0xFF97u;
            out[count].index = ALLSTAR_STATUS_INDEX_MINUS_ONE;
            out[count].d = 0x0Fu; out[count].e = 0x0Au; count++;
        }
        if (count < max) {                                          /* $25B8-$25C2 */
            out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
            out[count].source = 0x253Du; out[count].io = 0xFF96u;
            out[count].index = ALLSTAR_STATUS_INDEX_DIRECT;
            out[count].d = 0x0Fu; out[count].e = 0x0Bu; count++;
        }
    }

    /* $25C5..$25EA: shared by One-on-One, the tournament and Accuracy. */
    if (count < max) {
        out[count].kind = ALLSTAR_STATUS_FIELD_STRING;
        out[count].source = 0x2551u; out[count].io = 0xFF95u;
        out[count].index = ALLSTAR_STATUS_INDEX_BUCKET_3;
        if (mode == 0x03u) {                                        /* $25DB-$25E2 */
            out[count].d = 0x0Du; out[count].e = 0x05u;
        } else {                                                    /* $25E4 */
            out[count].d = 0x0Fu; out[count].e = 0x0Cu;
        }
        count++;
    }
    return count;
}

/* $2517..$2531 */
bool allstar_status_entry(const uint8_t *list, uint16_t length, uint8_t index,
                          uint16_t *offset, uint8_t *entry_length) {
    uint16_t i = 0;
    uint8_t remaining = index;
    uint8_t span = 0;

    if (!list) return false;

    while (remaining > 0) {                                         /* $251C-$2526 */
        if (i >= length) return false;
        if ((list[i] & ALLSTAR_STATUS_TERMINATOR) != 0) remaining--;
        i++;
    }
    if (offset) *offset = i;

    do {                                                            /* $2528-$2530 */
        if (i >= length) return false;
        span++;
        if ((list[i] & ALLSTAR_STATUS_TERMINATOR) != 0) break;
        i++;
    } while (1);

    if (entry_length) *entry_length = span;
    return true;
}

/* $25C5..$25D8 */
uint8_t allstar_status_bucket_3(uint8_t value) {
    if (value == 0x02u) return 0u;
    if (value == 0x05u) return 1u;
    if (value == 0x08u) return 2u;
    return 3u;
}

/* $25EF..$25FB */
uint8_t allstar_status_bucket_2(uint8_t value) {
    if (value == 0x05u) return 0u;
    if (value == 0x10u) return 1u;
    return 2u;
}

/*
 * $24E4 splits the word into four nibbles, high to low, and $2500 looks each up
 * in the table at $250D.  That table holds $01..$0A, so the lookup is simply
 * the digit plus one.
 */
void allstar_status_digits(uint16_t bcd_value, uint8_t *tiles) {
    uint8_t high = (uint8_t)(bcd_value >> 8);
    uint8_t low = (uint8_t)(bcd_value & 0xFFu);
    if (!tiles) return;
    tiles[0] = (uint8_t)(((high & 0xF0u) >> 4) + 1u);               /* $24E7-$24EC */
    tiles[1] = (uint8_t)((high & 0x0Fu) + 1u);                      /* $24EF-$24F2 */
    tiles[2] = (uint8_t)(((low & 0xF0u) >> 4) + 1u);                /* $24F5-$24FA */
    tiles[3] = (uint8_t)((low & 0x0Fu) + 1u);                       /* $24FD-$2500 */
}
