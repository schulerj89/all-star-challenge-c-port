#include "allstar_accuracy.h"
#include "allstar_controls.h"
#include <string.h>

/* Bank 1 $6DB7: five random groups of ten exact player center/ground pairs.
   Accuracy and HORSE share this cartridge data, but use different controllers. */
static const uint8_t ACCURACY_POSITIONS[ALLSTAR_ACCURACY_SPOT_GROUPS]
                                       [ALLSTAR_ACCURACY_SPOTS_PER_GROUP][2] = {
    {{0x0c,0x94},{0x70,0x88},{0x38,0x70},{0x50,0x90},{0x74,0x6c},
     {0x30,0x60},{0x6c,0x80},{0x64,0x68},{0x1c,0x74},{0x48,0x64}},
    {{0x90,0x60},{0x54,0x78},{0x50,0x60},{0x40,0x98},{0x0c,0x78},
     {0x68,0x68},{0x84,0x98},{0x5c,0x7c},{0x38,0x6c},{0x54,0x6c}},
    {{0xa0,0x64},{0x6c,0x84},{0x18,0x7c},{0x4c,0x74},{0x0c,0x60},
     {0x44,0x70},{0x7c,0x84},{0x54,0x8c},{0x40,0x68},{0xa0,0x64}},
    {{0x88,0x94},{0x90,0x7c},{0x34,0x84},{0x54,0x68},{0x34,0x98},
     {0x74,0x64},{0x88,0x6c},{0x3c,0x64},{0x68,0x7c},{0x88,0x88}},
    {{0xa0,0x60},{0x40,0x60},{0x28,0x74},{0x8c,0x64},{0x40,0x88},
     {0x0c,0x98},{0x60,0x70},{0x1c,0x70},{0x68,0x70},{0x8c,0x78}}
};

static uint8_t accuracy_group_6cab(uint8_t rng) {
    return rng < 0x30 ? 0 : rng < 0x5e ? 1 :
           rng < 0x8c ? 2 : rng < 0xba ? 3 : 4;
}

void allstar_accuracy_init_0e51_6c9b(AllStarAccuracyState *state,
                                    bool computer_positions) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->computer_positions = computer_positions;
    /* $6C9B seeds ten so the first $6CA2 selects a group. */
    state->position_index = ALLSTAR_ACCURACY_SPOTS_PER_GROUP;
}

void allstar_accuracy_next_position_6ca2(AllStarAccuracyState *state,
                                         uint8_t rng) {
    uint8_t index;
    if (!state) return;
    if (!state->computer_positions) {
        index = state->position_index % ALLSTAR_ACCURACY_SPOTS_PER_GROUP;
        state->target_x = state->custom_positions[index][0];
        state->target_y = state->custom_positions[index][1];
        state->position_index = (uint8_t)((index + 1) %
            ALLSTAR_ACCURACY_SPOTS_PER_GROUP);
        return;
    }
    if (state->position_index >= ALLSTAR_ACCURACY_SPOTS_PER_GROUP) {
        state->group = accuracy_group_6cab(rng);
        state->position_index = 0;
    }
    index = state->position_index++;
    state->target_x = ACCURACY_POSITIONS[state->group][index][0];
    state->target_y = ACCURACY_POSITIONS[state->group][index][1];
}

void allstar_accuracy_move_custom_cursor_6d57(uint8_t held,
                                              uint8_t *x, uint8_t *y) {
    if (!x || !y) return;
    /* $6D57 changes one axis by four and clamps the same center/ground
       rectangle used by the cartridge marker editor. */
    if ((held & ALLSTAR_BTN_RIGHT) != 0 && *x < 0x9c) *x += 4;
    else if ((held & ALLSTAR_BTN_LEFT) != 0 && *x > 0x10) *x -= 4;
    if ((held & ALLSTAR_BTN_DOWN) != 0 && *y < 0x98) *y += 4;
    else if ((held & ALLSTAR_BTN_UP) != 0 && *y > 0x64) *y -= 4;
}

bool allstar_accuracy_record_custom_position_6d57(
    AllStarAccuracyState *state, uint8_t x, uint8_t y) {
    if (!state || state->custom_count >= ALLSTAR_ACCURACY_SPOTS_PER_GROUP)
        return false;
    state->custom_positions[state->custom_count][0] = x;
    state->custom_positions[state->custom_count][1] = y;
    state->custom_count++;
    return state->custom_count == ALLSTAR_ACCURACY_SPOTS_PER_GROUP;
}

void allstar_accuracy_bcd_increment_0b20(uint8_t value[2]) {
    uint16_t decimal;
    if (!value) return;
    decimal = allstar_accuracy_bcd_value(value);
    decimal = (uint16_t)((decimal + 1) % 10000);
    value[0] = (uint8_t)(((decimal / 1000) << 4) |
                         ((decimal / 100) % 10));
    value[1] = (uint8_t)((((decimal / 10) % 10) << 4) | (decimal % 10));
}

uint16_t allstar_accuracy_bcd_value(const uint8_t value[2]) {
    if (!value) return 0;
    return (uint16_t)(((value[0] >> 4) & 0x0f) * 1000 +
        (value[0] & 0x0f) * 100 + ((value[1] >> 4) & 0x0f) * 10 +
        (value[1] & 0x0f));
}
