#ifndef ALLSTAR_VOICE_STATE_H
#define ALLSTAR_VOICE_STATE_H

#include "allstar_types.h"

/*
 * Sound-driver voice context switch, ported from $32B8 and $32E9.
 *
 * The driver keeps one working voice in a scratch block at $DE28..$DE2D and a
 * per-channel copy of each field in six arrays that start eight bytes apart at
 * $DDBF.  BC indexes the channel.  $32B8 stores the working voice into a
 * channel's slots and $32E9 loads it back, so the two are exact mirrors.
 *
 * The field order is not sequential: the third and fourth entries swap, mapping
 * $DE2D to $DDCF and $DE2C to $DDD7.  Reproduce that or two of the six fields
 * end up in each other's slots.
 */

#define ALLSTAR_VOICE_FIELDS       6
#define ALLSTAR_VOICE_SCRATCH_BASE 0xDE28u
#define ALLSTAR_VOICE_TABLE_BASE   0xDDBFu
#define ALLSTAR_VOICE_TABLE_STRIDE 8u

typedef struct {
    uint16_t scratch;  /* the $DE2x byte this field lives in */
    uint16_t table;    /* the per-channel array it copies to */
} AllStarVoiceField;

/* $32B8's copy order, which $32E9 mirrors exactly. */
const AllStarVoiceField* allstar_voice_fields(int *count);

/* The slot a field uses for one channel. */
uint16_t allstar_voice_slot(int field, uint8_t channel);

/*
 * $32B8 and $32E9.  Both buffers are indexed by field, in the order
 * allstar_voice_fields reports, so the permutation lives in that table rather
 * than in these two loops.
 */
void allstar_voice_save(const uint8_t *working, uint8_t *slots);
void allstar_voice_load(const uint8_t *slots, uint8_t *working);

/* ---- $3264 and $327F: the two per-frame voice loops ---- */

#define ALLSTAR_VOICE_ACTIVE_BASE 0xDD7Fu  /* $3266/$3281, indexed by channel */
#define ALLSTAR_VOICE_MUSIC_FIRST 3        /* $3264 counts 3 down to 0        */
#define ALLSTAR_VOICE_MUSIC_LAST  0
#define ALLSTAR_VOICE_SFX_FIRST   7        /* $327F counts 7 down to 4        */
#define ALLSTAR_VOICE_SFX_LAST    4

typedef enum {
    ALLSTAR_VOICE_BANK_MUSIC = 0,  /* $3264, channels 3..0 */
    ALLSTAR_VOICE_BANK_SFX         /* $327F, channels 7..4 */
} AllStarVoiceBank;

/*
 * $3264 and $327F are the same loop over different halves of the eight voices,
 * and both walk DOWNWARDS.  A channel is skipped entirely when its $DD7F byte
 * is zero; otherwise it runs $32E9 (load), $347B (advance), $32B8 (save).
 *
 * $3264 stops when bit 7 of C is set -- that is, after channel 0 wraps to $FF
 * -- while $327F stops on a plain `cp $04`, so neither loop can run off its
 * half into the other.
 */
int allstar_voice_bank_order(AllStarVoiceBank bank, uint8_t *out, int max);

/* The $DD7F slot a channel's active flag lives in. */
uint16_t allstar_voice_active_slot(uint8_t channel);

/* $326C/$3287: a zero flag means the channel is skipped this frame. */
bool allstar_voice_channel_runs(uint8_t active_flag);

/* ---- $3119 and $329B: resetting the APU, and starting a song ---- */

#define ALLSTAR_APU_RESET_NR51    0x00u    /* $311B, everything unrouted   */
#define ALLSTAR_APU_RESET_NR50    0x77u    /* $3124, both sides at maximum */
#define ALLSTAR_APU_RESET_NR52    0x8Fu    /* $3128                        */
#define ALLSTAR_APU_WAVE_CACHE    0xDD78u  /* $311F                        */
#define ALLSTAR_APU_WAVE_INVALID  0xFFu    /* forces $366F to reload       */

typedef struct {
    uint8_t nr51;
    uint8_t nr50;
    uint8_t nr52;
    uint8_t wave_cache;
} AllStarApuReset;

/*
 * $3119.  Run by $1F7D on the way back to the title.  NR50 going to $77 is
 * worth recording: master volume is maximum and symmetric, so the stereo image
 * a song sets up through NR51 is the only thing placing its voices.
 */
void allstar_apu_reset_3119(AllStarApuReset *out);

/*
 * $329B.  Starting a song walks the same four music voices as $3264, in the
 * same downward order, but runs $331A instead of the advance -- so a song
 * begins with every voice initialised before any of them ticks.
 */
int allstar_voice_start_order_329b(uint8_t *out, int max);

#endif /* ALLSTAR_VOICE_STATE_H */
