#ifndef ALLSTAR_HORSE_H
#define ALLSTAR_HORSE_H

#include "allstar_types.h"

#define ALLSTAR_HORSE_LETTER_COUNT 5
#define ALLSTAR_HORSE_CPU_SPOTS_PER_GROUP 10
#define ALLSTAR_HORSE_CPU_SPOT_GROUPS 5

typedef enum {
    ALLSTAR_HORSE_EVENT_NONE = 0,
    ALLSTAR_HORSE_EVENT_CALLED_MAKE = (1 << 0),
    ALLSTAR_HORSE_EVENT_CALLER_CHANGED = (1 << 1),
    ALLSTAR_HORSE_EVENT_LETTER = (1 << 2),
    ALLSTAR_HORSE_EVENT_COMPLETE = (1 << 3)
} AllStarHorseEvent;

typedef struct {
    uint8_t letters_remaining[2]; /* ROM player +$0E: $FFAB/$FFC4. */
    uint8_t current_player;       /* $FFDA, one based. */
    uint8_t caller;               /* $C172, one based. */
    uint8_t saved_x;              /* $FFDB, aligned player center. */
    uint8_t saved_y;              /* $FFDC, aligned ground Y. */
    uint8_t cpu_spot_index;
    uint8_t winner;
    bool called_shot_made;        /* $C180. */
    bool complete;
} AllStarHorseState;

/* Fixed $0CDF->$22B9 and bank 1 $7A90 mode initialization. */
void allstar_horse_init_0cdf(AllStarHorseState *state);
/* Fixed $0E36 stores center/ground coordinates rounded down to four pixels. */
void allstar_horse_save_spot_0e36(AllStarHorseState *state,
                                  float player_center_x,
                                  float player_ground_y);
/* Bank 1 $6CAB and its five groups of ten ROM-authored CPU shot spots. */
void allstar_horse_cpu_spot_6cab(uint8_t rng, uint8_t sequence_index,
                                uint8_t *player_center_x,
                                uint8_t *player_ground_y);
/* Fixed $0D57 caller/matcher result dispatcher and $0E26 letter penalty. */
uint32_t allstar_horse_resolve_shot_0d57(AllStarHorseState *state,
                                        bool made,
                                        float player_center_x,
                                        float player_ground_y);
/* Bank 1 $7BC0 selects the incurred H/O/R/S/E prefix from remaining +$0E. */
const char *allstar_horse_letters_7bc0(uint8_t letters_remaining);
bool allstar_horse_current_is_matcher(const AllStarHorseState *state);

/* ---- $0D2B: handing the court to a shooter ---- */

#define ALLSTAR_HORSE_SHOOTER_SLOT 0xC179u  /* $0D2B */
#define ALLSTAR_HORSE_SHOOTER_HRAM 0xFFDAu  /* $0D41, what $6E1B reads */
#define ALLSTAR_HORSE_HANDOFF_WAIT 0x04u    /* $0D4C, b for $2D08      */
#define ALLSTAR_HORSE_LCDC_OBJ_BIT 0x02u    /* $0D54, `set 1,[hl]`     */

typedef struct {
    uint8_t shooter;        /* $C179 and then $FFDA          */
    uint8_t set_flags[3];   /* $FFE7, $FFE6, $C12C all take 1 */
    uint8_t cleared[2];     /* $C0FD and $C145 both cleared   */
    uint8_t wait_frames;    /* $0D4C                          */
    bool enables_objects;   /* $0D51 sets LCDC bit 1          */
} AllStarHorseHandoff;

/*
 * $0D2B.  Called with the shooter in A -- $0D14 passes 2 and $0D21 passes 1 --
 * it parks that value in $C179, mirrors it into $FFDA for $6E1B to pick up,
 * raises three flags, clears two, waits four frames through $2D08 and turns
 * objects back on.
 */
void allstar_horse_handoff_0d2b(uint8_t shooter, AllStarHorseHandoff *out);

#endif /* ALLSTAR_HORSE_H */
