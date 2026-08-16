#ifndef ALLSTAR_ACCURACY_H
#define ALLSTAR_ACCURACY_H

#include "allstar_types.h"

#define ALLSTAR_ACCURACY_SPOTS_PER_GROUP 10
#define ALLSTAR_ACCURACY_SPOT_GROUPS 5

typedef struct {
    uint8_t group;                 /* ROM $FFDF. */
    uint8_t position_index;        /* ROM $FFE0, 0..10. */
    uint8_t target_x;              /* Player center written by $6CA2. */
    uint8_t target_y;              /* Player ground Y written by $6CA2. */
    uint8_t attempts_bcd[2];       /* $C139/$C13A. */
    uint8_t makes_bcd[2];          /* $C137/$C138. */
    uint8_t custom_positions[ALLSTAR_ACCURACY_SPOTS_PER_GROUP][2];
    uint8_t custom_count;
    bool computer_positions;       /* Settings byte $FF9A. */
} AllStarAccuracyState;

/* Fixed $0E51 and bank 1 $6C9B mode initialization. */
void allstar_accuracy_init_0e51_6c9b(AllStarAccuracyState *state,
                                    bool computer_positions);
/* Bank 1 $6CA2/$6CAB: select a group at each ten-shot boundary, then
   return the next exact ROM-authored center/ground pair. */
void allstar_accuracy_next_position_6ca2(AllStarAccuracyState *state,
                                         uint8_t rng);
/* Bank 1 $6D57 custom-position cursor and ten-entry recorder. */
void allstar_accuracy_move_custom_cursor_6d57(uint8_t held,
                                              uint8_t *x, uint8_t *y);
bool allstar_accuracy_record_custom_position_6d57(
    AllStarAccuracyState *state, uint8_t x, uint8_t y);
/* Fixed $0B20 increments a two-byte packed-BCD display value. */
void allstar_accuracy_bcd_increment_0b20(uint8_t value[2]);
uint16_t allstar_accuracy_bcd_value(const uint8_t value[2]);

#endif
