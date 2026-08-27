#include "allstar_apu_program.h"

/* $3151 */
static const uint8_t APU_CLAIM_BITS[8] = {
    0x01u, 0x02u, 0x04u, 0x08u, 0x10u, 0x20u, 0x40u, 0x80u
};

/* $3777, $377B, $377F, $3783 -- NR51 pairs: none, right, left, both. */
static const uint8_t APU_PAN[ALLSTAR_APU_KINDS][4] = {
    { 0x00u, 0x01u, 0x10u, 0x11u },
    { 0x00u, 0x02u, 0x20u, 0x22u },
    { 0x00u, 0x04u, 0x40u, 0x44u },
    { 0x00u, 0x08u, 0x80u, 0x88u }
};

static const AllStarApuChannel APU_CHANNELS[ALLSTAR_APU_KINDS] = {
    { 0xEEu, 0x3777u, 0x388Au },   /* $35FC, square 1 keeps everything but bits 0 and 4 */
    { 0xDDu, 0x377Bu, 0x388Bu },   /* $363D, square 2 has no sweep byte, so the block starts later */
    { 0xBBu, 0x377Fu, 0x388Au },   /* $36A4 */
    { 0x77u, 0x3783u, 0x388Bu }    /* $36E7 */
};

/* The register each branch writes, in order; 0 marks a skipped block byte. */
static const uint16_t APU_REGS_SQUARE_1[6] = { 0xFF10u, 0xFF11u, 0xFF12u, 0x0000u, 0xFF13u, 0xFF14u };
static const uint16_t APU_REGS_SQUARE_2[5] = { 0xFF16u, 0xFF17u, 0x0000u, 0xFF18u, 0xFF19u };
static const uint16_t APU_REGS_WAVE[6]     = { 0xFF1Au, 0xFF1Bu, 0xFF1Cu, 0x0000u, 0xFF1Du, 0xFF1Eu };
static const uint16_t APU_REGS_NOISE[4]    = { 0xFF20u, 0xFF21u, 0xFF22u, 0xFF23u };

/* $35E0..$35ED */
AllStarApuKind allstar_apu_kind(uint8_t kind_byte) {
    switch (kind_byte) {
    case 0x00u: return ALLSTAR_APU_SQUARE_1;
    case 0x01u: return ALLSTAR_APU_SQUARE_2;
    case 0x02u: return ALLSTAR_APU_WAVE;
    default:    return ALLSTAR_APU_NOISE;                           /* $35ED */
    }
}

/* $3151 */
uint8_t allstar_apu_claim_bit(uint8_t kind_byte) {
    return APU_CLAIM_BITS[kind_byte & 0x07u];
}

/* $35BB..$35D5 */
AllStarApuClaim allstar_apu_claim(uint8_t slot, uint8_t kind_byte, uint8_t *mask) {
    uint8_t bit = allstar_apu_claim_bit(kind_byte);

    if (!mask) return ALLSTAR_APU_CLAIM_FREE;

    if (slot < ALLSTAR_APU_PRIORITY_SLOT) {                         /* $35BC-$35BE */
        if ((*mask & bit) != 0) return ALLSTAR_APU_CLAIM_BLOCKED;   /* $35C8-$35CB */
        return ALLSTAR_APU_CLAIM_FREE;                              /* $35C9 */
    }

    *mask = (uint8_t)(*mask | bit);                                 /* $35D4-$35D5 */
    return ALLSTAR_APU_CLAIM_TAKEN;
}

bool allstar_apu_channel(AllStarApuKind kind, const AllStarApuChannel **out) {
    if (!out) return false;
    if ((int)kind < 0 || (int)kind >= ALLSTAR_APU_KINDS) return false;
    *out = &APU_CHANNELS[(int)kind];
    return true;
}

/* $35F5..$35F9: `rrca` twice then `and $03`. */
uint8_t allstar_apu_pan_index(uint8_t program_byte) {
    return (uint8_t)((program_byte >> 2) & 0x03u);
}

uint8_t allstar_apu_pan_value(AllStarApuKind kind, uint8_t index) {
    if ((int)kind < 0 || (int)kind >= ALLSTAR_APU_KINDS) return 0;
    return APU_PAN[(int)kind][index & 0x03u];
}

/* $35FA..$3603 and its three mirrors. */
uint8_t allstar_apu_nr51(AllStarApuKind kind, uint8_t current, uint8_t program_byte) {
    const AllStarApuChannel *channel;
    if (!allstar_apu_channel(kind, &channel)) return current;
    return (uint8_t)((current & channel->nr51_keep) |
                     allstar_apu_pan_value(kind, allstar_apu_pan_index(program_byte)));
}

const uint16_t* allstar_apu_registers(AllStarApuKind kind, int *count) {
    switch (kind) {
    case ALLSTAR_APU_SQUARE_1: if (count) *count = 6; return APU_REGS_SQUARE_1;
    case ALLSTAR_APU_SQUARE_2: if (count) *count = 5; return APU_REGS_SQUARE_2;
    case ALLSTAR_APU_WAVE:     if (count) *count = 6; return APU_REGS_WAVE;
    default:                   if (count) *count = 4; return APU_REGS_NOISE;
    }
}

/* $3673..$3692 */
bool allstar_apu_wave_upload(uint8_t program_byte, uint8_t *cached, uint16_t *source) {
    uint8_t id = (uint8_t)(program_byte & 0x0Fu);                   /* $3674 */
    if (!cached) return false;
    if (id == *cached) return false;                                /* $3679-$367A */
    *cached = id;                                                   /* $367C */
    /* $367D-$3685: four rotates left is a shift by four, so the stride is 16. */
    if (source) *source = (uint16_t)(ALLSTAR_APU_WAVE_BANK + (uint16_t)(id << 4));
    return true;
}

uint8_t allstar_apu_frequency_high(uint8_t program_byte, uint8_t note_bits) {
    return (uint8_t)((program_byte & 0xF8u) | note_bits);           /* $3619 / $362D */
}

/* $3702..$370E */
uint8_t allstar_apu_noise_control(uint8_t program_byte, uint8_t note_bits) {
    return (uint8_t)((program_byte & 0x0Fu) | note_bits);
}
