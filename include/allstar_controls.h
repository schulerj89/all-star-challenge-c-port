#ifndef ALLSTAR_CONTROLS_H
#define ALLSTAR_CONTROLS_H

#include "allstar_types.h"

void allstar_input_init(AllStarInput *input);
void allstar_input_update(AllStarInput *input, uint8_t current_raw_buttons);
bool allstar_input_is_pressed(const AllStarInput *input, AllStarButtonMask btn);
bool allstar_input_is_held(const AllStarInput *input, AllStarButtonMask btn);
bool allstar_input_is_released(const AllStarInput *input, AllStarButtonMask btn);

#endif /* ALLSTAR_CONTROLS_H */
