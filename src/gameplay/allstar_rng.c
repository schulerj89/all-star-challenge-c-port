#include "allstar_rng.h"

void allstar_rom_rng_init(AllStarRomRng *rng, uint16_t seed) {
    if (!rng) return;
    rng->seed = seed;
    rng->alternate_seed = 0x974a;
    rng->frame_phase = 0;
}

/* $072F computes seed*9+$002B, then adds $C133 and $C0B6 to L without
   carrying into H.  That last detail differs from a conventional 16-bit LCG. */
uint16_t allstar_rom_rng_step_072f(uint16_t seed,
                                  uint8_t score_low_bcd,
                                  uint8_t clock_seconds_bcd) {
    uint16_t scaled = (uint16_t)(seed * 9u + 0x002bu);
    uint8_t low = (uint8_t)scaled;
    low = (uint8_t)(low + score_low_bcd);
    low = (uint8_t)(low + clock_seconds_bcd);
    return (uint16_t)((scaled & 0xff00u) | low);
}

uint8_t allstar_rom_rng_current(const AllStarRomRng *rng) {
    return rng ? (uint8_t)rng->seed : 0;
}

uint8_t allstar_rom_rng_high(const AllStarRomRng *rng) {
    return rng ? (uint8_t)(rng->seed >> 8) : 0;
}

uint8_t allstar_rom_rng_alternate(const AllStarRomRng *rng) {
    return rng ? (uint8_t)rng->alternate_seed : 0;
}

uint8_t allstar_rom_rng_alternate_high(const AllStarRomRng *rng) {
    return rng ? (uint8_t)(rng->alternate_seed >> 8) : 0;
}

/* The $FF8B bit-zero gate makes the gameplay-visible $FFFB stream advance on
   every second 60 Hz update.  Every random branch in between sees one shared
   byte, matching the traced cartridge rather than host rand() call order. */
uint8_t allstar_rom_rng_end_frame_0714(AllStarRomRng *rng,
                                      uint8_t score_low_bcd,
                                      uint8_t clock_seconds_bcd) {
    if (!rng) return 0;
    rng->frame_phase ^= 1u;
    if (rng->frame_phase != 0) {
        rng->alternate_seed = allstar_rom_rng_step_072f(
            rng->alternate_seed, score_low_bcd, clock_seconds_bcd);
    } else {
        rng->seed = allstar_rom_rng_step_072f(
            rng->seed, score_low_bcd, clock_seconds_bcd);
    }
    return (uint8_t)rng->seed;
}

uint8_t allstar_rom_bcd_byte(uint8_t value) {
    value = (uint8_t)(value % 100u);
    return (uint8_t)(((value / 10u) << 4) | (value % 10u));
}
