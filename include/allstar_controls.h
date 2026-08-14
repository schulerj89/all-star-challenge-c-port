#ifndef ALLSTAR_CONTROLS_H
#define ALLSTAR_CONTROLS_H

#include "allstar_types.h"

/* Ghidra: Call_000_2639 - Joypad hardware polling (rP1 register) */
void allstar_input_init(AllStarInput *input);
void allstar_input_update(AllStarInput *input, uint8_t current_raw_buttons);
void allstar_controls_poll(AllStarInput *input, uint8_t raw_buttons);
bool allstar_input_is_pressed(const AllStarInput *input, AllStarButtonMask btn);
bool allstar_input_is_held(const AllStarInput *input, AllStarButtonMask btn);
bool allstar_input_is_released(const AllStarInput *input, AllStarButtonMask btn);

#endif /* ALLSTAR_CONTROLS_H */
