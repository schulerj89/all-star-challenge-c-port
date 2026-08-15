#ifndef ALLSTAR_RNG_H
#define ALLSTAR_RNG_H

#include <stdint.h>

/* Fixed-bank $0714/$072F random state.  One-on-One consumers read the low
   byte at $FFFB; they do not consume or advance the stream themselves. */
typedef struct {
    uint16_t seed;
    uint16_t alternate_seed;
    uint8_t frame_phase;
} AllStarRomRng;

void allstar_rom_rng_init(AllStarRomRng *rng, uint16_t seed);
uint16_t allstar_rom_rng_step_072f(uint16_t seed,
                                  uint8_t score_low_bcd,
                                  uint8_t clock_seconds_bcd);
uint8_t allstar_rom_rng_current(const AllStarRomRng *rng);
uint8_t allstar_rom_rng_high(const AllStarRomRng *rng);
uint8_t allstar_rom_rng_alternate(const AllStarRomRng *rng);
uint8_t allstar_rom_rng_alternate_high(const AllStarRomRng *rng);
uint8_t allstar_rom_rng_end_frame_0714(AllStarRomRng *rng,
                                      uint8_t score_low_bcd,
                                      uint8_t clock_seconds_bcd);
uint8_t allstar_rom_bcd_byte(uint8_t value);

#endif /* ALLSTAR_RNG_H */
