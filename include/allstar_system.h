#ifndef ALLSTAR_SYSTEM_H
#define ALLSTAR_SYSTEM_H

#include "allstar_types.h"

/*
 * Shell-level helpers: the $0ADB movement probes, the BCD counters at $0B20 and
 * $0B29, the serial wait at $0B44, and the soft-reset watchdog at $2D1B.
 */

/* ---- $0ADA: three probes selected by a jump table ---- */

#define ALLSTAR_PROBE_FIELD_X    0x06u  /* offset into the entity record */
#define ALLSTAR_PROBE_FIELD_Y    0x15u
#define ALLSTAR_PROBE_STEP_X     0x0Cu
#define ALLSTAR_PROBE_STEP_Y     0x08u
#define ALLSTAR_PROBE_OK         0x00u  /* $0B1C */
#define ALLSTAR_PROBE_BLOCKED    0x04u  /* $0B16 */
#define ALLSTAR_PROBE_SLOTS      5

/* $0ADB, indexed by the value $0AD9 fetched. */
const uint16_t* allstar_probe_table(int *count);

typedef struct {
    uint8_t field;     /* which record offset the probe moves */
    int8_t delta;      /* how far, signed */
} AllStarProbe;

/* $0AE1, $0AEA and $0B01. */
bool allstar_probe_shape(uint16_t entry, AllStarProbe *out);

/*
 * $0AF1..$0B1F.  The probe writes the offset field, runs the movement check,
 * then compares the field against what it wrote: unchanged means the move took,
 * and any difference reports blocked.  Both exits pop two register pairs, so
 * they return past their own caller.
 */
uint8_t allstar_probe_result(uint8_t written, uint8_t observed);

/* ---- $0B20 / $0B29: BCD counters on a 16-bit value ---- */

uint16_t allstar_bcd_increment(uint16_t value);
uint16_t allstar_bcd_decrement(uint16_t value);

/* ---- $0B44: spin until the serial handler raises $C19C, then clear it ---- */

bool allstar_serial_ready(uint8_t *flag);

/* ---- $2D1B: the soft-reset and attract watchdog ---- */

#define ALLSTAR_RESET_COMBO    0x0Fu   /* $FFAF, all of A, B, Select and Start */
#define ALLSTAR_RESET_ATTRACT  0x0Cu   /* $FFAE, Select or Start               */
#define ALLSTAR_RESET_VECTOR   0x0156u

typedef enum {
    ALLSTAR_WATCHDOG_CONTINUE = 0,
    ALLSTAR_WATCHDOG_RESET
} AllStarWatchdogResult;

/*
 * $2D1B..$2D4E.  In normal play, holding all four buttons at once resets, and
 * $C270 suppresses even that.  In attract mode ($FFE4) a countdown at $C26D
 * runs down and either its expiry or a Select/Start press resets instead.
 */
AllStarWatchdogResult allstar_watchdog(uint8_t attract, uint8_t suppress,
                                       uint8_t held, uint8_t attract_armed,
                                       uint8_t new_buttons, uint16_t *countdown);

/* ---- small helpers swept up with the rest ---- */

#define ALLSTAR_BUSY_WAIT_COUNT 0x02BCu  /* $0386 */

/* $0386..$038E: a plain busy loop of $02BC iterations. */
uint16_t allstar_busy_wait_count(void);

/* $718F: the no-op slot modes $01 and $03 take in the bank 1 $7185 table. */
void allstar_bank1_mode_noop(void);

/* $331A..$3325: c doubled indexes the $3111 table, whose entry is added to $DD7A. */
#define ALLSTAR_SOUND_OFFSET_TABLE 0x3111u
uint16_t allstar_sound_offset_slot(uint8_t channel);

#endif /* ALLSTAR_SYSTEM_H */
