#include "allstar_kernel.h"

/* $0000..$005F, in address order. */
const AllStarVector* allstar_kernel_vectors(int *count) {
    static const AllStarVector VECTORS[ALLSTAR_VECTOR_COUNT] = {
        {0x0000u, ALLSTAR_VECTOR_UNUSED,      0x0000u},
        {0x0008u, ALLSTAR_VECTOR_DISPATCH,    0x0010u}, /* $000A runs $0010 */
        {0x0010u, ALLSTAR_VECTOR_DISPATCH,    0x0000u},
        {0x0018u, ALLSTAR_VECTOR_UNUSED,      0x0000u},
        {0x0020u, ALLSTAR_VECTOR_BANK,        0x0000u},
        {0x0028u, ALLSTAR_VECTOR_VBLANK_WAIT, 0x0000u},
        {0x0030u, ALLSTAR_VECTOR_BANKED_CALL, 0x0B4Fu}, /* $0035 */
        {0x0038u, ALLSTAR_VECTOR_UNUSED,      0x0000u}, /* $FF filler       */
        {0x0040u, ALLSTAR_VECTOR_INTERRUPT,   0x2729u}, /* vblank           */
        {0x0048u, ALLSTAR_VECTOR_INTERRUPT,   0x0000u}, /* STAT, bare reti  */
        {0x0050u, ALLSTAR_VECTOR_INTERRUPT,   0x0000u}, /* timer, bare reti */
        {0x0058u, ALLSTAR_VECTOR_INTERRUPT,   0x0061u}  /* serial           */
    };
    if (count) *count = ALLSTAR_VECTOR_COUNT;
    return VECTORS;
}

/* $0010: `add a` doubles the index before it is added to the table. */
uint16_t allstar_kernel_dispatch_0010(uint16_t table, uint8_t index) {
    return (uint16_t)(table + (uint16_t)index * 2u);
}

/* $0474: the canonical OAM DMA routine, run from HRAM because the bus is
   otherwise unavailable during the transfer. */
const uint8_t* allstar_kernel_dma_routine_0466(int *count) {
    static const uint8_t ROUTINE[ALLSTAR_KERNEL_DMA_BYTES] = {
        0x3Eu, ALLSTAR_KERNEL_OAM_PAGE,  /* ld a,$C0      */
        0xE0u, 0x46u,                    /* ldh [rDMA],a  */
        0x3Eu, 0x28u,                    /* ld a,$28      */
        0x3Du,                           /* dec a         */
        0x20u, 0xFDu,                    /* jr nz,-3      */
        0xC9u                            /* ret           */
    };
    if (count) *count = ALLSTAR_KERNEL_DMA_BYTES;
    return ROUTINE;
}

/* $045E..$0465 */
void allstar_kernel_set_interrupts_045e(uint8_t mask,
                                        AllStarKernelInterrupts *out) {
    if (!out) return;
    out->interrupt_flags = 0u;    /* $0460, cleared first */
    out->interrupt_enable = mask; /* $0463 */
}

/* $0A91..$0A94: `dec a` then `ret z`, so only an exact one player continues. */
bool allstar_kernel_clears_rng_0a91(uint8_t players) {
    return players != 1u;
}

/* $0A95..$0AA2, in ROM order. */
const uint16_t* allstar_kernel_rng_cleared_0a91(int *count) {
    static const uint16_t CLEARED[ALLSTAR_KERNEL_RNG_CLEARED] = {
        0xFF8Bu,  /* $0A96, the frame counter $276D increments */
        0xFFFBu,  /* $0A98 */
        0xFFFCu,  /* $0A9A */
        0xFFFDu,  /* $0A9C */
        0xFFFEu,  /* $0A9E */
        0xFFF9u   /* $0AA0 */
    };
    if (count) *count = ALLSTAR_KERNEL_RNG_CLEARED;
    return CLEARED;
}

/* $0773..$077C: `cp $02` then `ret z`, so two takes the first branch. */
uint16_t allstar_kernel_entity_slot_0773(uint8_t which) {
    return which == 0x02u ? ALLSTAR_KERNEL_SLOT_TWO   /* $0773 */
                          : ALLSTAR_KERNEL_SLOT_ONE;  /* $0779 */
}

/* $2D0C..$2D1A */
void allstar_kernel_wait_2d0c(uint8_t frames, AllStarKernelWait *out) {
    if (!out) return;
    out->frames = frames;      /* $2D0D */
    out->clears_ffeb = true;   /* $2D18, once the spin ends */
}

/* $2F79..$2F87 */
void allstar_kernel_countdown_2f79(uint8_t counter,
                                   AllStarKernelCountdown *out) {
    if (!out) return;
    out->counter = counter;
    out->clears_state = false;
    if (counter == 0) return;                 /* $2F7D */
    out->counter = (uint8_t)(counter - 1u);   /* $2F7E */
    /* $2F82: only the frame it reaches zero clears $C193. */
    out->clears_state = out->counter == 0;
}

/* $20BA..$20CF */
void allstar_kernel_oam_fill_20ba(uint8_t counter, AllStarKernelOamFill *out) {
    if (!out) return;
    /* $20BE: `dec a` then `jr nz`, so only an exact one blanks. */
    out->blanks = counter == 1u;
    out->blank_bytes = out->blanks ? ALLSTAR_KERNEL_OAM_STRIDE : 0u;
    out->counter = out->blanks ? ALLSTAR_KERNEL_OAM_STRIDE
                               : (uint8_t)(counter - 1u);   /* $20CD */
}

/* $27EA..$2803 */
void allstar_kernel_sequence_27ea(uint8_t counter, uint8_t step,
                                  AllStarKernelSequence *out) {
    if (!out) return;
    out->step = step;
    out->next_step = step;
    /* $27EE: the vblank wait repeats until $C195 decrements to zero. */
    if (counter > 1u) {
        out->waits = true;
        out->counter = (uint8_t)(counter - 1u);
        return;
    }
    out->waits = false;
    out->counter = ALLSTAR_KERNEL_SEQUENCE_RELOAD;   /* $27F1 */
    /* $27FD-$2801: $C196 is post-incremented, so the step that dispatches is
       the value from before the bump. */
    out->next_step = (uint8_t)(step + 1u);
}

/* $1982 and $1984 */
int allstar_kernel_step_1982(bool forward) {
    return forward ? 1 : -1;
}

/* $05FA..$0612 */
void allstar_kernel_status_copy_05fa(AllStarKernelStatusCopy *out) {
    if (!out) return;
    out->waits_vblank = true;                              /* $05FA */
    out->source = ALLSTAR_KERNEL_STATUS_SOURCE;            /* $05FB */
    out->destination = ALLSTAR_KERNEL_STATUS_DEST;         /* $05FE */
    out->groups = ALLSTAR_KERNEL_STATUS_GROUPS;            /* $0601 */
    out->bytes_per_group = ALLSTAR_KERNEL_STATUS_STRIDE;
    out->total_bytes = (uint16_t)(ALLSTAR_KERNEL_STATUS_GROUPS *
                                  ALLSTAR_KERNEL_STATUS_STRIDE);
}

/* $18D0..$18E6 */
int allstar_kernel_oam_row_18d0(uint8_t y, uint8_t first_x,
                                const uint8_t *tiles, int count,
                                uint8_t attribute_flag,
                                AllStarKernelOamEntry *out, int max) {
    int i;
    uint8_t x = first_x;
    if (!tiles || !out || max <= 0) return 0;
    for (i = 0; i < count && i < max; i++) {
        out[i].y = y;                                      /* $18D0 */
        out[i].x = x;                                      /* $18D3 */
        out[i].tile = tiles[i];                            /* $18D5 */
        /* $18DA: a set $FF8D leaves the attribute byte alone. */
        out[i].wrote_attributes = attribute_flag == 0;
        out[i].attributes = 0u;
        x = (uint8_t)(x + ALLSTAR_KERNEL_OAM_X_STEP);      /* $18E0 */
    }
    return i;
}

/* $2AA5..$2AB4 */
void allstar_kernel_oam_seed_2aa5(uint8_t first, uint8_t second,
                                  uint8_t out[4]) {
    if (!out) return;
    out[0] = first;                              /* $2AA8 */
    out[1] = second;                             /* $2AAB */
    out[2] = ALLSTAR_KERNEL_OAM_ATTRIBUTE;       /* $2AAE */
    out[3] = 0u;                                 /* $2AB2 */
}
