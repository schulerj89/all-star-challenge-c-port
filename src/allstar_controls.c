#include "allstar_controls.h"
#include <string.h>

void allstar_input_init(AllStarInput *input) {
    if (input) {
        memset(input, 0, sizeof(AllStarInput));
    }
}

/* Ghidra: Call_000_2639 - Joypad hardware reading and debouncing */
void allstar_input_update(AllStarInput *input, uint8_t current_raw_buttons) {
    if (!input) return;
    uint8_t prev = input->buttons_held;
    input->buttons_held = current_raw_buttons;
    input->buttons_pressed = (uint8_t)(current_raw_buttons & ~prev);
    input->buttons_released = (uint8_t)(prev & ~current_raw_buttons);
}

void allstar_controls_poll(AllStarInput *input, uint8_t raw_buttons) {
    allstar_input_update(input, raw_buttons);
}

bool allstar_input_is_pressed(const AllStarInput *input, AllStarButtonMask btn) {
    if (!input) return false;
    return (input->buttons_pressed & (uint8_t)btn) != 0;
}

bool allstar_input_is_held(const AllStarInput *input, AllStarButtonMask btn) {
    if (!input) return false;
    return (input->buttons_held & (uint8_t)btn) != 0;
}

bool allstar_input_is_released(const AllStarInput *input, AllStarButtonMask btn) {
    if (!input) return false;
    return (input->buttons_released & (uint8_t)btn) != 0;
}
