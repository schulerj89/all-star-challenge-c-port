#include "allstar_system.h"

/* $0ADB */
static const uint16_t PROBE_TABLE[ALLSTAR_PROBE_SLOTS] = {
    0x0AE1u, 0x0B01u, 0x0AEAu, 0x0621u, 0x1900u
};

const uint16_t* allstar_probe_table(int *count) {
    if (count) *count = ALLSTAR_PROBE_SLOTS;
    return PROBE_TABLE;
}

/* $0AE1, $0AEA, $0B01 */
bool allstar_probe_shape(uint16_t entry, AllStarProbe *out) {
    if (!out) return false;
    switch (entry) {
    case 0x0AE1u:                                                   /* $0AE6 add $0C */
        out->field = ALLSTAR_PROBE_FIELD_X;
        out->delta = (int8_t)ALLSTAR_PROBE_STEP_X;
        return true;
    case 0x0AEAu:                                                   /* $0AEF sub $0C */
        out->field = ALLSTAR_PROBE_FIELD_X;
        out->delta = (int8_t)(-(int)ALLSTAR_PROBE_STEP_X);
        return true;
    case 0x0B01u:                                                   /* $0B06 sub $08 */
        out->field = ALLSTAR_PROBE_FIELD_Y;
        out->delta = (int8_t)(-(int)ALLSTAR_PROBE_STEP_Y);
        return true;
    default:
        return false;
    }
}

/* $0AFB..$0B1F */
uint8_t allstar_probe_result(uint8_t written, uint8_t observed) {
    return (written == observed) ? ALLSTAR_PROBE_OK                 /* $0AFD / $0B14 */
                                 : ALLSTAR_PROBE_BLOCKED;           /* $0B16-$0B18 */
}

/*
 * The GB `daa` after an addition, which is what both counters below rely on.
 * The $60 correction is decided from the value before the $06 correction is
 * applied, and it is that carry the ROM tests with `ret c` and `ret nc`.
 */
static uint8_t allstar_daa_add(uint8_t value, uint8_t operand, bool *carry_out) {
    uint16_t sum = (uint16_t)value + (uint16_t)operand;
    bool half = (((value & 0x0Fu) + (operand & 0x0Fu)) > 0x0Fu);
    bool carry = (sum > 0xFFu);
    uint8_t result = (uint8_t)sum;
    uint8_t correction = 0;

    if (half || (result & 0x0Fu) > 0x09u) correction = (uint8_t)(correction | 0x06u);
    if (carry || result > 0x99u) {
        correction = (uint8_t)(correction | 0x60u);
        carry = true;
    }
    result = (uint8_t)(result + correction);
    if (carry_out) *carry_out = carry;
    return result;
}

/* $0B20..$0B28: one BCD step up, carrying into the high byte. */
uint16_t allstar_bcd_increment(uint16_t value) {
    uint8_t low = (uint8_t)(value & 0xFFu);
    uint8_t high = (uint8_t)(value >> 8);
    bool carry = false;

    low = allstar_daa_add(low, 0x01u, &carry);                      /* $0B22-$0B25 */
    if (carry) high = (uint8_t)(high + 1u);                         /* $0B26-$0B27 */
    return (uint16_t)(((uint16_t)high << 8) | low);
}

/*
 * $0B29..$0B34.  Adding $99 in BCD subtracts one; a carry out means the low
 * byte did not underflow, which is why $0B2E returns on carry and only the
 * borrowing case reaches the high byte.
 */
uint16_t allstar_bcd_decrement(uint16_t value) {
    uint8_t low = (uint8_t)(value & 0xFFu);
    uint8_t high = (uint8_t)(value >> 8);
    bool carry = false;

    low = allstar_daa_add(low, 0x99u, &carry);                      /* $0B2A-$0B2D */
    if (carry) return (uint16_t)(((uint16_t)high << 8) | low);      /* $0B2E ret c */
    high = allstar_daa_add(high, 0x99u, NULL);                      /* $0B2F-$0B33 */
    return (uint16_t)(((uint16_t)high << 8) | low);
}

/* $0B44..$0B4E */
bool allstar_serial_ready(uint8_t *flag) {
    if (!flag) return false;
    if (*flag == 0) return false;                                   /* $0B47-$0B48 */
    *flag = 0;                                                      /* $0B4A-$0B4B */
    return true;
}

/* $2D1B..$2D4E */
AllStarWatchdogResult allstar_watchdog(uint8_t attract, uint8_t suppress,
                                       uint8_t held, uint8_t attract_armed,
                                       uint8_t new_buttons, uint16_t *countdown) {
    if (attract == 0) {                                             /* $2D1B-$2D1E */
        if (suppress != 0) return ALLSTAR_WATCHDOG_CONTINUE;        /* $2D20-$2D24 */
        if ((held & ALLSTAR_RESET_COMBO) == ALLSTAR_RESET_COMBO) {  /* $2D25-$2D2B */
            return ALLSTAR_WATCHDOG_RESET;
        }
        return ALLSTAR_WATCHDOG_CONTINUE;
    }

    if (attract_armed == 0) return ALLSTAR_WATCHDOG_CONTINUE;       /* $2D2F-$2D32 */
    if (!countdown) return ALLSTAR_WATCHDOG_CONTINUE;

    *countdown = (uint16_t)(*countdown - 1u);                       /* $2D39 */
    if (*countdown == 0) return ALLSTAR_WATCHDOG_RESET;             /* $2D42-$2D44 */
    if ((new_buttons & ALLSTAR_RESET_ATTRACT) != 0) {               /* $2D47-$2D4B */
        return ALLSTAR_WATCHDOG_RESET;
    }
    return ALLSTAR_WATCHDOG_CONTINUE;
}

/* $0386..$038E */
uint16_t allstar_busy_wait_count(void) {
    return ALLSTAR_BUSY_WAIT_COUNT;
}

/* $718F */
void allstar_bank1_mode_noop(void) {
}

/* $331A..$3323: `ld a,c / rlca` doubles the channel before indexing. */
uint16_t allstar_sound_offset_slot(uint8_t channel) {
    return (uint16_t)(ALLSTAR_SOUND_OFFSET_TABLE + (uint16_t)((uint8_t)(channel << 1)));
}
