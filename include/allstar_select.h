#ifndef ALLSTAR_SELECT_H
#define ALLSTAR_SELECT_H

#include "allstar_types.h"

/*
 * Bank 2 entrant selector, ported from $4000..$40F3 plus the cursor loop at
 * $40F4.  This is the screen the fixed-bank tournament code hands its candidate
 * list to: $2890, $2897 and $28B0 fill $C0D8.. and call in with a count in A,
 * and the selector writes the chosen ids straight into the bracket slots.
 *
 * $C17F holds that count, and the destination tables at $4358 and $4360 turn it
 * into the very slots the $0F2E driver later reads:
 *
 *   $C17F  pass 1 -> ...   pass 2 -> ...   meaning
 *     4      $C0BF           $C0C3         four entrants each, eight in total
 *     2      $C0CB           $C0CD         two semifinalists each
 *     1      $C0D1           $C0D2         the two finalists
 *     3      unused          unused
 *
 * IMPORTANT: the input bytes this file works with ($FFAE, $FFAF, $FFC7, $FFC8)
 * use the cartridge's own bit packing, which is NOT AllStarButtonMask:
 *
 *   bit 0 A   bit 1 B   bit 2 Select   bit 3 Start
 *   bit 4 Right   bit 5 Left   bit 6 Up   bit 7 Down
 *
 * Convert at the boundary; do not mix these masks with ALLSTAR_BTN_*.
 */

#define ALLSTAR_SELECT_ROM_A       0x01u
#define ALLSTAR_SELECT_ROM_B       0x02u
#define ALLSTAR_SELECT_ROM_SELECT  0x04u
#define ALLSTAR_SELECT_ROM_START   0x08u
#define ALLSTAR_SELECT_ROM_RIGHT   0x10u
#define ALLSTAR_SELECT_ROM_LEFT    0x20u

#define ALLSTAR_SELECT_CONFIRM_MASK 0x0Cu  /* $410F: Select or Start          */
#define ALLSTAR_SELECT_MOVE_MASK    0x33u  /* $4119: A, B, Right or Left      */
#define ALLSTAR_SELECT_BACK_MASK    0x22u  /* $4127: B or Left steps backward */

#define ALLSTAR_SELECT_LIST_BASE    0xC0D9u  /* first candidate after $C0D8   */
#define ALLSTAR_SELECT_CURSOR       0xC0F5u  /* $C0F5/$C0F6 hold the pointer  */
#define ALLSTAR_SELECT_WRITE_CURSOR 0xC182u  /* $C182/$C183 hold the output   */
#define ALLSTAR_SELECT_SENTINEL     0xFFu

#define ALLSTAR_SELECT_PASS_1  1u
#define ALLSTAR_SELECT_PASS_2  2u

/* $400F..$401D: modes $01 and $03 with one player run pass 1 only. */
bool allstar_select_runs_second_pass(uint8_t player_count, uint8_t mode);

/* $4045 and $40C8: where a pass writes, from the $4358 / $4360 tables. */
uint16_t allstar_select_destination(uint8_t stage, uint8_t pass);

/* $406A: the prompt id handed to $2DEA before a pass. */
uint8_t allstar_select_prompt(uint8_t mode, uint8_t stage, uint8_t picker,
                              uint8_t player_count);

/* $40C4: the duplicate scan covers both passes for this stage. */
uint8_t allstar_select_scan_length(uint8_t stage);

/* $4100..$410D: whose held-input byte the cursor loop reads. */
uint8_t allstar_select_buttons(uint8_t player_count, uint8_t picker,
                               uint8_t player_1_held, uint8_t player_2_held);

typedef enum {
    ALLSTAR_SELECT_IDLE = 0,
    ALLSTAR_SELECT_MOVED,
    ALLSTAR_SELECT_CONFIRMED
} AllStarSelectInput;

/*
 * $40F7..$4149.  One pass of the cursor loop over the $FF-bounded candidate
 * list.  Confirm is tested before movement, both read held input rather than
 * new input, and running off either end wraps to the other.
 */
AllStarSelectInput allstar_select_step(uint8_t hold_lock, uint8_t held,
                                       uint8_t length, uint8_t *index);

/* $40BA..$40DC: reject a pick already present in this stage's output. */
bool allstar_select_is_duplicate(const uint8_t *picked, uint8_t scan_length, uint8_t candidate);

/* ---- $414B: the player info card the cursor redraws on every move ---- */

#define ALLSTAR_SELECT_CARD_RULE      0x430Eu  /* 19 copies of tile $69      */
#define ALLSTAR_SELECT_CARD_ROW       0x4322u  /* framed blank row           */
#define ALLSTAR_SELECT_CARD_ROWS      0x10u    /* rows 1..16 get the frame   */
#define ALLSTAR_SELECT_PORTRAIT_TABLE 0x2D4Fu
#define ALLSTAR_SELECT_PORTRAIT_VRAM  0x9000u
#define ALLSTAR_SELECT_TILE_ARRAY     0xC1A1u
#define ALLSTAR_SELECT_TILE_COUNT     24u
#define ALLSTAR_SELECT_BLANK_TABLE    0x42A2u  /* one byte per roster id     */
#define ALLSTAR_SELECT_MARK_STREAM    0x42BDu  /* $FE-delimited, one per id  */
#define ALLSTAR_SELECT_MARK_DELIM     0xFEu
#define ALLSTAR_SELECT_LABEL_HEIGHT   0x4336u  /* "HEIGHT   :"               */
#define ALLSTAR_SELECT_LABEL_WEIGHT   0x4341u  /* "WEIGHT   :"               */
#define ALLSTAR_SELECT_LABEL_PPG      0x434Cu  /* "PPG AVG  :"               */
#define ALLSTAR_SELECT_HEIGHT_SUFFIX  0x4357u
#define ALLSTAR_SELECT_FIELD_HEIGHT   0x0Au    /* record offsets             */
#define ALLSTAR_SELECT_FIELD_WEIGHT   0x0Eu
#define ALLSTAR_SELECT_FIELD_PPG      0x12u
#define ALLSTAR_SELECT_FIELD_NAME     0x16u
#define ALLSTAR_SELECT_CARD_HOLD      0x0Eu    /* $429B waits 14 frames      */
#define ALLSTAR_SELECT_CARD_OPS       10

typedef enum {
    ALLSTAR_SELECT_CARD_RULE_TOP = 0,   /* $415A at (0,0)                    */
    ALLSTAR_SELECT_CARD_FRAME,          /* $417A, one row each at (0,1..16)  */
    ALLSTAR_SELECT_CARD_RULE_BOTTOM,    /* $4184 at (0,17)                   */
    ALLSTAR_SELECT_CARD_BLOCK_A,        /* $41C7, six rows of four at (4,1)  */
    ALLSTAR_SELECT_CARD_BLOCK_B,        /* $4217, four rows of four at (12,2)*/
    ALLSTAR_SELECT_CARD_NAME_FIELD,     /* $4230, record + $16 at (1,8)      */
    ALLSTAR_SELECT_CARD_SURNAME,        /* $423B, continues where that ended */
    ALLSTAR_SELECT_CARD_HEIGHT,         /* $4250 at (2,11), then record + $0A*/
    ALLSTAR_SELECT_CARD_WEIGHT,         /* $426B at (2,13), then record + $0E*/
    ALLSTAR_SELECT_CARD_PPG             /* $4280 at (2,15), then record + $12*/
} AllStarSelectCardStep;

typedef struct {
    AllStarSelectCardStep step;
    uint16_t source;      /* string or label address, 0 when it comes from a record */
    uint8_t record_field; /* record offset, or $FF when unused                     */
    uint8_t d;
    uint8_t e;
    uint8_t rows;         /* repeat count for the block and frame steps            */
    uint8_t per_row;
} AllStarSelectCardOp;

/* $414B..$4292, in the order the ROM draws them. */
int allstar_select_card_layout(AllStarSelectCardOp *out, int max);

/* $418D: the portrait pointer table, indexed by the highlighted id. */
uint16_t allstar_select_portrait_slot(uint8_t roster_id);

/*
 * $4199..$41C5.  The array starts as 1..24; a blank index from $42A2 punches a
 * zero at that position and decrements everything after it.  $FF means no hole.
 */
void allstar_select_punch_tiles(uint8_t blank_index, uint8_t *tiles);

/* $41E0: the second block counts from 25 when there was no hole, else 24. */
uint8_t allstar_select_tile_base(uint8_t blank_index);

/* $41E7: skip that many $FE delimiters to reach a player's marker record. */
uint16_t allstar_select_mark_offset(const uint8_t *stream, uint16_t length, uint8_t roster_id);

/*
 * $41F8..$4215.  Walk indices 0..23; an index named by the marker record writes
 * a zero and does not advance the running tile number, every other index writes
 * that number plus the base.
 */
void allstar_select_build_tiles(const uint8_t *marks, uint8_t mark_count,
                                uint8_t base, uint8_t *tiles);

/* ---- $2D85..$2EA7 and bank 1 $780A: the record and prompt helpers ---- */

#define ALLSTAR_SELECT_RECORD_TABLE  0x4368u  /* $FF-delimited player records */
#define ALLSTAR_SELECT_RECORD_DELIM  0xFFu
#define ALLSTAR_SELECT_RECORD_BYTES  0x16u    /* $2DBE copies 22 bytes        */
#define ALLSTAR_SELECT_SLOT_1_BUFFER 0xC23Bu  /* one before the $C23C name    */
#define ALLSTAR_SELECT_SLOT_2_BUFFER 0xC254u
#define ALLSTAR_SELECT_EMPTY_SLOT    0x80u    /* $2E73/$2E8C fill value       */
#define ALLSTAR_SELECT_CPU_STRIDE    0x07u    /* $2D9E steps by seven         */
#define ALLSTAR_SELECT_CPU_RETRY     0x14u    /* $2DAC nudges by twenty       */
#define ALLSTAR_SELECT_CPU_LIMIT     0x1Au    /* $2D95 allows 26 steps        */
#define ALLSTAR_SELECT_PROMPT_HOLD   0x78u    /* $2E0A waits 120 frames       */
#define ALLSTAR_SELECT_PROMPT_P1     0x13u
#define ALLSTAR_SELECT_PROMPT_P2     0x14u
#define ALLSTAR_SELECT_BLIP_SOUND    0x0Fu    /* $2AB5                        */
#define ALLSTAR_SELECT_DIGIT_BASE    0xC1u

/* $2EA3: fill count bytes with value. */
void allstar_select_fill(uint8_t *slots, uint8_t count, uint8_t value);

/* $2E73 / $2E8C: mark one side's bracket slots empty. $2E70 does both. */
typedef struct {
    uint16_t address;
    uint8_t count;
} AllStarSelectClearRun;

#define ALLSTAR_SELECT_CLEAR_RUNS 3
int allstar_select_clear_runs(uint8_t pass, AllStarSelectClearRun *out, int max);

/* $2DD2: walk the $FF-delimited record table to a roster id. */
uint16_t allstar_select_record_offset(const uint8_t *table, uint16_t length, uint8_t roster_id);

/* $2DBE: which buffer a $FF8C slot copies its 22 record bytes into. */
uint16_t allstar_select_record_buffer(uint8_t slot);

/*
 * $2D93: turn the $FFFB seed into a roster index by counting sevens, then nudge
 * the seed by twenty and retry if it collides with the id already in $FFAC.
 */
uint8_t allstar_select_cpu_opponent(uint8_t seed, uint8_t taken, uint8_t *seed_out);

/* $2DEA: the prompt announcement, and whether it names a player. */
typedef struct {
    uint8_t prompt_sound;   /* the id passed in                     */
    bool announces_player;  /* only in a two-player game            */
    uint8_t player_sound;   /* $13 or $14                           */
    uint16_t hold_frames;   /* $78, or fewer if Start is pressed    */
} AllStarSelectPrompt;

void allstar_select_prompt_shape(uint8_t prompt, uint8_t player_count, uint8_t picker,
                                 AllStarSelectPrompt *out);

/* $2AB5: the menu blip sound. */
uint8_t allstar_select_blip_sound(void);

/* $780A: four tile codes for a 16-bit BCD word, with no leading-zero blanking. */
void allstar_select_wide_digits(uint16_t bcd_value, uint8_t *digits);

#endif /* ALLSTAR_SELECT_H */
