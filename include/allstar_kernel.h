#ifndef ALLSTAR_KERNEL_H
#define ALLSTAR_KERNEL_H

#include "allstar_types.h"

/*
 * The cartridge's small kernel: the vector table at $0000..$005F and the
 * handful of helpers hanging off it.
 *
 * Nothing here is more than a couple of dozen bytes, but between them they own
 * the dispatch mechanism every ported routine's comments refer to, the OAM DMA
 * routine, the interrupt mask, and the frame waits the rest of the game is
 * built on.
 */

/* ---- $0000..$005F: the vectors ---- */

typedef enum {
    ALLSTAR_VECTOR_UNUSED = 0,   /* the slot is $FF filler or a bare reti */
    ALLSTAR_VECTOR_DISPATCH,     /* $08 and $10, the pointer-table pair    */
    ALLSTAR_VECTOR_BANK,         /* $20, the MBC1 trampoline               */
    ALLSTAR_VECTOR_VBLANK_WAIT,  /* $28                                    */
    ALLSTAR_VECTOR_BANKED_CALL,  /* $30                                    */
    ALLSTAR_VECTOR_INTERRUPT     /* $40..$60                               */
} AllStarVectorKind;

typedef struct {
    uint16_t address;
    AllStarVectorKind kind;
    uint16_t target;   /* where it goes, or 0 when it returns immediately */
} AllStarVector;

#define ALLSTAR_VECTOR_COUNT 12

/* Every vector the cartridge defines, in address order. */
const AllStarVector* allstar_kernel_vectors(int *count);

/*
 * $0008 pops the return address -- which points at the inline pointer table
 * following the `rst $08` -- runs the $0010 lookup on it and jumps to the
 * entry.  $0010 does the lookup alone and leaves the entry in HL, which is why
 * `rst $10` sites read as "and then HL is the target".
 */
#define ALLSTAR_KERNEL_MBC1_REGISTER 0x2150u  /* $0020 */
#define ALLSTAR_KERNEL_BANK_SHADOW   0xC13Bu  /* $0023 */
#define ALLSTAR_KERNEL_HRAM_DMA      0xFF80u  /* $0466's destination */
#define ALLSTAR_KERNEL_DMA_BYTES     10       /* $0468 */
#define ALLSTAR_KERNEL_OAM_PAGE      0xC0u    /* $0474 */

/* $0010: the index is doubled and added to the table base. */
uint16_t allstar_kernel_dispatch_0010(uint16_t table, uint8_t index);

/* The ten bytes $0466 copies from $0474 into $FF80. */
const uint8_t* allstar_kernel_dma_routine_0466(int *count);

/* ---- small helpers ---- */

/*
 * $045E.  The pending-interrupt flags are cleared BEFORE the new mask is
 * written, so enabling a source cannot immediately fire on a stale flag.
 */
typedef struct {
    uint8_t interrupt_flags;   /* $FF0F, always zero */
    uint8_t interrupt_enable;  /* $FFFF, the mask passed in */
} AllStarKernelInterrupts;

void allstar_kernel_set_interrupts_045e(uint8_t mask,
                                        AllStarKernelInterrupts *out);

/*
 * $0A91.  A one-player game clears the frame counter and the whole RNG state;
 * a two-player game returns without touching either, so a link session keeps
 * the seed both cartridges agreed on.
 */
#define ALLSTAR_KERNEL_RNG_CLEARED 6
bool allstar_kernel_clears_rng_0a91(uint8_t players);
const uint16_t* allstar_kernel_rng_cleared_0a91(int *count);

/* $0773 and bank 1 $7914 pick between the same two entity slots. */
#define ALLSTAR_KERNEL_SLOT_TWO 0xFF9Du  /* $0773 */
#define ALLSTAR_KERNEL_SLOT_ONE 0xFFB6u  /* $0779 */
uint16_t allstar_kernel_entity_slot_0773(uint8_t which);

/*
 * $2D0C.  Parks a frame count in $FF8A -- the counter $276D decrements -- and
 * spins on $100F until it reaches zero, then clears $FFEB.
 */
#define ALLSTAR_KERNEL_WAIT_COUNTER 0xFF8Au  /* $2D0D */
typedef struct {
    uint8_t frames;      /* what lands in $FF8A       */
    bool clears_ffeb;    /* $2D18                     */
} AllStarKernelWait;
void allstar_kernel_wait_2d0c(uint8_t frames, AllStarKernelWait *out);

/*
 * $2F79.  A two-stage countdown: $C194 runs down every call and only the frame
 * it reaches zero clears $C193.
 */
typedef struct {
    uint8_t counter;     /* $C194 after the call */
    bool clears_state;   /* $C193 cleared        */
} AllStarKernelCountdown;
void allstar_kernel_countdown_2f79(uint8_t counter,
                                   AllStarKernelCountdown *out);

/*
 * $20BA.  Writes the byte, then when $FF8C has run down to one it blanks the
 * next sixteen bytes and reloads the counter with $10.
 */
#define ALLSTAR_KERNEL_OAM_STRIDE 0x10u  /* $20C2 and $20CB */
typedef struct {
    bool blanks;         /* $20C1 */
    uint8_t blank_bytes; /* $20C2 */
    uint8_t counter;     /* $FF8C on return */
} AllStarKernelOamFill;
void allstar_kernel_oam_fill_20ba(uint8_t counter, AllStarKernelOamFill *out);

/*
 * $27EA.  Waits $C195 vblanks, reloads it with $0B, then dispatches through
 * the pointer in $C197 with the step counter in $C196 -- which it
 * post-increments, so the step that runs is the value before the bump.
 */
#define ALLSTAR_KERNEL_SEQUENCE_RELOAD 0x0Bu   /* $27F1 */
#define ALLSTAR_KERNEL_SEQUENCE_STEP   0xC196u /* $27FA */
#define ALLSTAR_KERNEL_SEQUENCE_TABLE  0xC197u /* $27F4 */
typedef struct {
    bool waits;          /* $27EE, the counter had not reached zero */
    uint8_t counter;     /* $C195 on return                         */
    uint8_t step;        /* the index handed to the dispatch        */
    uint8_t next_step;   /* $C196 on return                         */
} AllStarKernelSequence;
void allstar_kernel_sequence_27ea(uint8_t counter, uint8_t step,
                                  AllStarKernelSequence *out);

/*
 * $1982 and $1984 are two-byte trampolines -- `inc hl; ret` and `dec hl; ret`
 * -- that exist only to be entries in a pointer table.
 */
int allstar_kernel_step_1982(bool forward);

/* ---- $05FA, $18D0 and $2AA5: VRAM and OAM plumbing ---- */

#define ALLSTAR_KERNEL_STATUS_SOURCE 0xC271u  /* $05FB */
#define ALLSTAR_KERNEL_STATUS_DEST   0x98E0u  /* $05FE */
#define ALLSTAR_KERNEL_STATUS_GROUPS 0x20u    /* $0601 */
#define ALLSTAR_KERNEL_STATUS_STRIDE 3        /* $0606..$060E */

/*
 * $05FA.  Waits a vblank, then copies thirty-two three-byte groups from $C271
 * into $98E0 -- the bottom row of the background map -- pausing on $049F
 * between groups so the transfer stays inside the blanking window.
 */
typedef struct {
    uint16_t source;
    uint16_t destination;
    uint8_t groups;
    uint8_t bytes_per_group;
    uint16_t total_bytes;
    bool waits_vblank;   /* the leading rst $28 */
} AllStarKernelStatusCopy;

void allstar_kernel_status_copy_05fa(AllStarKernelStatusCopy *out);

/*
 * $18D0.  Writes one OAM entry per iteration -- Y from $FF8C, X from C, tile
 * from the source stream -- and then the attribute byte, which is forced to
 * zero only while $FF8D is clear.  X advances eight pixels per entry.
 */
#define ALLSTAR_KERNEL_OAM_X_STEP 0x08u  /* $18E0 */
typedef struct {
    uint8_t y;
    uint8_t x;
    uint8_t tile;
    uint8_t attributes;
    bool wrote_attributes;  /* $18DD only runs while $FF8D is clear */
} AllStarKernelOamEntry;

int allstar_kernel_oam_row_18d0(uint8_t y, uint8_t first_x,
                                const uint8_t *tiles, int count,
                                uint8_t attribute_flag,
                                AllStarKernelOamEntry *out, int max);

/*
 * $2AA5.  Builds the first OAM entry at $C000 from a two-byte source, with
 * attribute $01 and a zero fourth byte.
 */
#define ALLSTAR_KERNEL_OAM_BASE      0xC000u  /* $2AA5 */
#define ALLSTAR_KERNEL_OAM_ATTRIBUTE 0x01u    /* $2AAE */
void allstar_kernel_oam_seed_2aa5(uint8_t first, uint8_t second,
                                  uint8_t out[4]);

#endif /* ALLSTAR_KERNEL_H */
