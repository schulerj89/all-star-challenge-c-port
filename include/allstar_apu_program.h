#ifndef ALLSTAR_APU_PROGRAM_H
#define ALLSTAR_APU_PROGRAM_H

#include "allstar_types.h"

/*
 * The APU channel programmer, ported from $35B6..$3714.
 *
 * This is the routine that actually writes the Game Boy sound registers.  It
 * takes a voice slot in BC, reads that slot's kind from $DD9F, arbitrates for
 * the hardware channel through the mask in $DD7D, and then programs one of the
 * four channels from a byte block at $388A.
 *
 * The arbitration is the interesting part.  $3151 gives each kind a single bit;
 * a slot numbered below four tests that bit and **gives up** if it is already
 * set, while a slot four or above **sets** it.  So the upper slots take a
 * channel and the lower ones stand down, which is how one set of voices talks
 * over the other.
 */

#define ALLSTAR_APU_KINDS        4
#define ALLSTAR_APU_PRIORITY_SLOT 4      /* $35BC compares against this */
#define ALLSTAR_APU_CLAIM_MASK   0xDD7Du
#define ALLSTAR_APU_KIND_TABLE   0xDD9Fu
#define ALLSTAR_APU_PROGRAM_TABLE 0xDDB7u
#define ALLSTAR_APU_NOTE_TABLE   0xDDEFu
#define ALLSTAR_APU_NR51         0xFF25u
#define ALLSTAR_APU_WAVE_RAM     0xFF30u
#define ALLSTAR_APU_WAVE_BANK    0x3FB2u
#define ALLSTAR_APU_WAVE_CACHE   0xDD78u
#define ALLSTAR_APU_WAVE_BYTES   16
#define ALLSTAR_APU_FREQ_LO_TABLE 0x31C6u
#define ALLSTAR_APU_FREQ_HI_TABLE 0x3159u
#define ALLSTAR_APU_NOISE_TABLE  0x3233u

typedef enum {
    ALLSTAR_APU_SQUARE_1 = 0,   /* $35F0 */
    ALLSTAR_APU_SQUARE_2,       /* $3631 */
    ALLSTAR_APU_WAVE,           /* $366F */
    ALLSTAR_APU_NOISE           /* $36DB, the catch-all */
} AllStarApuKind;

/* $35E0..$35ED: values above $02 all fall to the noise branch. */
AllStarApuKind allstar_apu_kind(uint8_t kind_byte);

/* $3151: one bit per kind, $01 $02 $04 $08 $10 $20 $40 $80. */
uint8_t allstar_apu_claim_bit(uint8_t kind_byte);

typedef enum {
    ALLSTAR_APU_CLAIM_BLOCKED = 0,  /* $35CB, the channel is already spoken for */
    ALLSTAR_APU_CLAIM_FREE,         /* $35D6, proceeds and leaves the mask alone */
    ALLSTAR_APU_CLAIM_TAKEN         /* $35D5, the slot sets the bit itself       */
} AllStarApuClaim;

/* $35BB..$35D5 */
AllStarApuClaim allstar_apu_claim(uint8_t slot, uint8_t kind_byte, uint8_t *mask);

typedef struct {
    uint8_t nr51_keep;    /* what $FF25 keeps: $EE, $DD, $BB or $77 */
    uint16_t pan_table;   /* $3777, $377B, $377F or $3783           */
    uint16_t block;       /* $388A or $388B                          */
} AllStarApuChannel;

bool allstar_apu_channel(AllStarApuKind kind, const AllStarApuChannel **out);

/* The panning index is bits 3 and 2 of the program byte, rotated down. */
uint8_t allstar_apu_pan_index(uint8_t program_byte);

/* $3777 and friends: 0 silent, 1 right, 2 left, 3 both. */
uint8_t allstar_apu_pan_value(AllStarApuKind kind, uint8_t index);

/* The full $FF25 update for one channel. */
uint8_t allstar_apu_nr51(AllStarApuKind kind, uint8_t current, uint8_t program_byte);

/*
 * The registers each branch writes, in order.  A zero entry marks the byte the
 * ROM skips over with a bare `inc hl`.
 */
const uint16_t* allstar_apu_registers(AllStarApuKind kind, int *count);

/*
 * $366F..$3692.  The low nibble of the wave program byte is a waveform id.  It
 * is compared against the cache in $DD78 and, only on a change, sixteen bytes
 * are copied from $3FB2 + (id << 4) into wave RAM.
 */
bool allstar_apu_wave_upload(uint8_t program_byte, uint8_t *cached, uint16_t *source);

/*
 * The frequency-high write is the program byte masked with $F8, ORed with the
 * note's entry in $3159.  Every byte of that table in this ROM is zero, so the
 * OR never changes anything -- the lookup is reproduced anyway.
 */
uint8_t allstar_apu_frequency_high(uint8_t program_byte, uint8_t note_bits);

/* $3702..$370F: the noise channel masks with $0F and ORs the $3233 entry. */
uint8_t allstar_apu_noise_control(uint8_t program_byte, uint8_t note_bits);

#endif /* ALLSTAR_APU_PROGRAM_H */
