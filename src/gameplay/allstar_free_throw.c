#include "allstar_free_throw.h"
#include <string.h>

static uint8_t high_byte(uint16_t value) {
    return (uint8_t)(value >> 8);
}

static uint16_t with_high_byte(uint16_t value, uint8_t high) {
    return (uint16_t)(((uint16_t)high << 8) | (value & 0x00ffu));
}

uint8_t allstar_free_throw_player_profile_2f40(uint8_t roster_id) {
    static const uint8_t groups[] = {
        0x02,0x0d,0x0b,0x05,0x0f,0xff,
        0x19,0x11,0x12,0x1a,0x04,0x01,0x08,0x09,0x0e,0x06,0xff,
        0x18,0x07,0x03,0x0a,0x17,0x13,0x15,0x0c,0x16,0x00,0x14,0x10,0xff
    };
    uint8_t group = 0;
    size_t i;
    for (i = 0; i < sizeof(groups); ++i) {
        if (groups[i] == roster_id) return group;
        if (groups[i] == 0xff) group++;
    }
    return 2;
}

void allstar_free_throw_aim_init_18e7(AllStarFreeThrowState *state,
                                      uint8_t rng) {
    if (!state) return;
    state->aim_timer = 6;
    if (rng < 0x40) {
        state->aim_x = 0x5b00; state->aim_y = 0x3800;
        state->aim_vx = 0x0045; state->aim_vy = 0x0040;
    } else if (rng < 0x90) {
        state->aim_x = 0x4700; state->aim_y = 0x3400;
        state->aim_vx = 0x0050; state->aim_vy = (int16_t)0xffb0;
    } else if (rng < 0xe0) {
        state->aim_x = 0x5600; state->aim_y = 0x4300;
        state->aim_vx = 0x0060; state->aim_vy = 0x0078;
    } else {
        state->aim_x = 0x4900; state->aim_y = 0x3300;
        state->aim_vx = (int16_t)0xffb0; state->aim_vy = 0x0055;
    }
}

void allstar_free_throw_reset_attempt_17aa(AllStarFreeThrowState *state,
                                           uint8_t rng) {
    uint8_t attempts_limit, attempts_remaining, attempts_taken, makes, profile;
    if (!state) return;
    attempts_limit = state->attempts_limit;
    attempts_remaining = state->attempts_remaining;
    attempts_taken = state->attempts_taken;
    makes = state->makes;
    profile = state->player_profile;
    memset(state, 0, sizeof(*state));
    state->attempts_limit = attempts_limit;
    state->attempts_remaining = attempts_remaining;
    state->attempts_taken = attempts_taken;
    state->makes = makes;
    state->player_profile = profile;
    state->phase = ALLSTAR_FREE_THROW_AIMING;
    state->ball.x = 0x4f00;
    state->ball.y = 0xe000;
    state->ball.z = 0x7200;
    state->net_state = 7;
    state->net_timer = 0x10;
    allstar_free_throw_aim_init_18e7(state, rng);
}

void allstar_free_throw_init(AllStarFreeThrowState *state,
                             uint8_t attempts, uint8_t rng,
                             uint8_t roster_id) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    if (attempts != 5 && attempts != 10 && attempts != 20) attempts = 5;
    state->attempts_limit = attempts;
    state->attempts_remaining = attempts;
    state->player_profile = allstar_free_throw_player_profile_2f40(roster_id);
    allstar_free_throw_reset_attempt_17aa(state, rng);
}

void allstar_free_throw_aim_input_1942(AllStarFreeThrowState *state,
                                       uint8_t held_buttons) {
    if (!state) return;
    if (held_buttons & ALLSTAR_BTN_RIGHT) state->aim_vx++;
    if (held_buttons & ALLSTAR_BTN_LEFT) state->aim_vx--;
    if (held_buttons & ALLSTAR_BTN_DOWN) state->aim_vy++;
    if (held_buttons & ALLSTAR_BTN_UP) state->aim_vy--;
}

static int16_t move_away_from_zero(int16_t value) {
    if (value < 0) return (int16_t)(value - 1);
    if (value > 0) return (int16_t)(value + 1);
    return value;
}

void allstar_free_throw_aim_step_1986(AllStarFreeThrowState *state) {
    uint8_t x, y;
    if (!state) return;
    x = high_byte(state->aim_x);
    y = high_byte(state->aim_y);
    state->aim_vx = (int16_t)(state->aim_vx + (x <= 0x50 ? 3 : -3));
    state->aim_vy = (int16_t)(state->aim_vy + (y <= 0x3f ? 3 : -3));
    if (state->aim_timer > 0) state->aim_timer--;
    if (state->aim_timer == 1) {
        state->aim_vx = move_away_from_zero(state->aim_vx);
        state->aim_vy = move_away_from_zero(state->aim_vy);
    }
    if (state->aim_timer == 0) state->aim_timer = 6;
    state->aim_x = (uint16_t)(state->aim_x + (uint16_t)state->aim_vx);
    state->aim_y = (uint16_t)(state->aim_y + (uint16_t)state->aim_vy);
    x = high_byte(state->aim_x);
    y = high_byte(state->aim_y);
    if (x < 0x39 || x > 0x68) state->aim_vx = 0;
    if (y < 0x28 || y > 0x57) state->aim_vy = 0;
}

bool allstar_free_throw_launch_1caa_7c58(AllStarFreeThrowState *state,
                                        uint8_t rng) {
    uint8_t target_x, target_y;
    int16_t dx, dy, dz;
    if (!state || state->phase != ALLSTAR_FREE_THROW_AIMING) return false;
    target_x = high_byte(state->aim_x);
    target_y = high_byte(state->aim_y);
    if (rng < 0x13 && target_x >= 0x48 && target_x <= 0x57 &&
        target_y >= 0x36 && target_y <= 0x43) {
        target_x = 0x52;
        target_y = 0x3c;
    }
    dx = (int16_t)((int16_t)target_x - 12 - (int16_t)high_byte(state->ball.x));
    dy = (int16_t)(0xb8 - (int16_t)high_byte(state->ball.y));
    dz = (int16_t)(0x01f4 +
        (((int16_t)0xb8 - ((int16_t)target_y + 0x0e) -
          (int16_t)high_byte(state->ball.z)) << 2));
    state->ball.x &= 0xff00;
    state->ball.y &= 0xff00;
    state->ball.z &= 0xff00;
    state->ball.vx = (int16_t)(dx << 2);
    state->ball.vy = (int16_t)(dy << 2);
    state->ball.vz = dz;
    state->phase = ALLSTAR_FREE_THROW_PRESENTATION;
    state->presentation_frame = 0;
    state->physics_enabled = false;
    state->made_current = false;
    state->score_pending = false;
    state->attempts_taken++;
    if (state->attempts_remaining > 0) state->attempts_remaining--;

    /* $17E2 overwrites these integer coordinates before its 10-frame pause. */
    state->ball.x = with_high_byte(state->ball.x, 0x50);
    state->ball.z = with_high_byte(state->ball.z, 0x79);
    return true;
}

static uint32_t net_step_1c61(AllStarFreeThrowState *state) {
    uint32_t events = 0;
    if (state->net_state == 7) return 0;
    if (state->net_timer > 0) state->net_timer--;
    if (state->net_timer != 0) return 0;
    state->net_timer = 11;
    state->net_state++;
    if (state->net_state == 2) events |= ALLSTAR_FREE_THROW_EVENT_NET;
    if (state->net_state == 6 && state->score_pending) {
        state->makes++;
        state->score_pending = false;
        events |= ALLSTAR_FREE_THROW_EVENT_SCORE;
    }
    if (state->net_state > 7) state->net_state = 7;
    return events;
}

static void bounce_1e77(AllStarFreeThrowState *state, uint16_t loss) {
    state->ball.vz = (int16_t)(-state->ball.vz - (int16_t)loss);
    state->contact_cooldown = 4;
}

static void make_1e0e(AllStarFreeThrowState *state) {
    state->ball.vx = 0;
    state->ball.vy = 0;
    state->made_current = true;
    state->score_pending = true;
    state->net_state = 0;
    /* $C0B4 remains the reset value 16 until $1C61 consumes it. */
    if (state->net_timer == 0) state->net_timer = 0x10;
}

static uint8_t rim_group_1b0f(uint8_t x) {
    if (x <= 0x36) return 0;
    if (x <= 0x3a) return 1;
    if (x <= 0x3d) return 2;
    if (x <= 0x3f) return 3;
    if (x <= 0x42) return 4;
    if (x <= 0x45) return 5;
    if (x <= 0x48) return 6;
    if (x <= 0x4a) return 7;
    if (x <= 0x4d) return 8;
    if (x <= 0x51) return 9;
    return 10;
}

static uint32_t rim_contact_1a31(AllStarFreeThrowState *state) {
    uint8_t x = high_byte(state->ball.x);
    uint8_t y = high_byte(state->ball.y);
    uint8_t z = high_byte(state->ball.z);
    uint8_t group;
    uint32_t events = 0;

    if (state->made_current) {
        if (z >= 0xe0 || state->ball.z == 0) {
            state->ball.z = 0;
            bounce_1e77(state, 0x0039);
            return ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
        }
        return 0;
    }
    if (state->contact_cooldown > 0) {
        state->contact_cooldown--;
        return 0;
    }
    if (y < 0xb8) {
        state->ball.y = with_high_byte(state->ball.y, 0xba);
        state->ball.vy = 0x0020;
        if (state->ball.vx < 0) state->ball.vx = (int16_t)-0x24;
        else if (state->ball.vx > 0) state->ball.vx = 0x24;
        events |= ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
        y = 0xba;
    }
    x = high_byte(state->ball.x);
    z = high_byte(state->ball.z);
    if (y < 0xbd && z >= 0x77 && z <= 0x79 && x > 0x32 && x <= 0x55) {
        group = rim_group_1b0f(x);
        switch (group) {
            case 0:
                state->ball.vx = (int16_t)0xff92;
                state->contact_cooldown = 4;
                return events | ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
            case 1: state->ball.vx = (int16_t)0xffce; break;
            case 3: state->ball.vx = 0x006e; break;
            case 4:
                state->ball.vx = 0x00a5;
                state->ball.vz = 0x0080;
                state->ball.vy = 0;
                state->contact_cooldown = 4;
                return events | ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
            case 5:
                if (state->center_latch != 0) {
                    make_1e0e(state);
                    return events | ALLSTAR_FREE_THROW_EVENT_MAKE;
                }
                if (high_byte(state->ball.y) == 0xbb) {
                    state->center_latch = state->player_profile == 0 ? 1 : 0;
                    state->ball.vy = (int16_t)0xfff8;
                    state->ball.vx = 0;
                }
                break;
            case 6:
                state->ball.vx = (int16_t)0xff6d;
                state->ricochet_count++;
                if (state->ricochet_count >= 2) {
                    state->ball.x = with_high_byte(
                        state->ball.x, (uint8_t)(high_byte(state->ball.x) - 3));
                    make_1e0e(state);
                    return events | ALLSTAR_FREE_THROW_EVENT_MAKE;
                }
                state->ball.vz = 0x0080;
                state->ball.vy = 0;
                state->contact_cooldown = 4;
                return events | ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
            case 7: state->ball.vx = (int16_t)0xff92; break;
            case 9: state->ball.vx = 0x0032; break;
            case 10:
                state->ball.vx = 0x006e;
                state->contact_cooldown = 4;
                return events | ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
            default: break;
        }
        bounce_1e77(state, 0x0039);
        return events | ALLSTAR_FREE_THROW_EVENT_RIM;
    }
    if (z >= 0xe0 || state->ball.z == 0) {
        state->ball.z = 0;
        bounce_1e77(state, 0x0039);
        events |= ALLSTAR_FREE_THROW_EVENT_BALL_CONTACT;
    }
    return events;
}

uint32_t allstar_free_throw_tick_100f(AllStarFreeThrowState *state,
                                     uint8_t held_buttons,
                                     uint8_t pressed_buttons,
                                     uint8_t rng) {
    uint32_t events = 0;
    if (!state) return 0;
    if (state->phase == ALLSTAR_FREE_THROW_AIMING) {
        allstar_free_throw_aim_input_1942(state, held_buttons);
        allstar_free_throw_aim_step_1986(state);
        if ((pressed_buttons & ALLSTAR_BTN_A) &&
            allstar_free_throw_launch_1caa_7c58(state, rng)) {
            events |= ALLSTAR_FREE_THROW_EVENT_RELEASE;
        }
        /* $1A25 stores C0B3/C0AF in C098/C099 after mode-1 input. */
        state->previous_aim_y = high_byte(state->aim_y);
        state->previous_aim_x = high_byte(state->aim_x);
        return events;
    }
    if (state->phase == ALLSTAR_FREE_THROW_RESULT) return 0;

    /* $100F calls sprite/net work before the gated $7BE8/$1A31 flight work. */
    events |= net_step_1c61(state);
    state->presentation_frame++;
    /* The ten $181A controller calls are bounded by two dispatcher/VBlank
       handoffs in the live trace; flight first advances at release +13. */
    if (state->presentation_frame == 12) {
        state->ball.z = with_high_byte(state->ball.z,
                                       (uint8_t)(high_byte(state->ball.z) + 3));
        state->physics_enabled = true;
    } else if (state->physics_enabled) {
        allstar_physics_rom_step_7be8(&state->ball);
        events |= rim_contact_1a31(state);
    }
    if (state->presentation_frame >= ALLSTAR_FREE_THROW_PRESENTATION_FRAMES) {
        if (state->attempts_remaining == 0) {
            state->phase = ALLSTAR_FREE_THROW_RESULT;
            events |= ALLSTAR_FREE_THROW_EVENT_RESULT;
        } else {
            allstar_free_throw_reset_attempt_17aa(state, rng);
            events |= ALLSTAR_FREE_THROW_EVENT_NEXT_ATTEMPT;
        }
    }
    state->previous_aim_y = high_byte(state->aim_y);
    state->previous_aim_x = high_byte(state->aim_x);
    return events;
}

void allstar_free_throw_ball_screen_1884(const AllStarFreeThrowState *state,
                                         int *screen_x, int *screen_y) {
    if (!state) return;
    if (screen_x) *screen_x = (int)high_byte(state->ball.x);
    if (screen_y) *screen_y = (int)(uint8_t)(
        high_byte(state->ball.y) - high_byte(state->ball.z) - 0x18);
}

void allstar_free_throw_set_test_aim(AllStarFreeThrowState *state,
                                     uint8_t x, uint8_t y) {
    if (!state) return;
    /* Mesen's proof edits only the integer bytes and preserves the existing
       fractions. Mid-fraction avoids an artificial high-byte crossing on
       the following $1986 update. */
    state->aim_x = ((uint16_t)x << 8) | 0x80;
    state->aim_y = ((uint16_t)y << 8) | 0x80;
    state->aim_vx = 0;
    state->aim_vy = 0;
}
