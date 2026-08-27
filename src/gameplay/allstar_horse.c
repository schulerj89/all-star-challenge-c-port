#include "allstar_horse.h"
#include <string.h>

/* Bank 1 $6DB7: five RNG-selected groups, ten X/Y player positions each. */
static const uint8_t HORSE_CPU_SPOTS[ALLSTAR_HORSE_CPU_SPOT_GROUPS]
                                    [ALLSTAR_HORSE_CPU_SPOTS_PER_GROUP][2] = {
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

void allstar_horse_init_0cdf(AllStarHorseState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
    state->letters_remaining[0] = ALLSTAR_HORSE_LETTER_COUNT;
    state->letters_remaining[1] = ALLSTAR_HORSE_LETTER_COUNT;
    state->current_player = 1;
    state->caller = 1;
}

void allstar_horse_save_spot_0e36(AllStarHorseState *state,
                                  float player_center_x,
                                  float player_ground_y) {
    uint8_t center;
    uint8_t ground;
    if (!state) return;
    center = (uint8_t)player_center_x;
    ground = (uint8_t)player_ground_y;
    /* $0E36 subtracts four only from the lower court limit $98 before
       applying AND $FC. Native movement clamps the same edge to $98. */
    if (ground == 0x98) ground = (uint8_t)(ground - 4);
    state->saved_x = center & 0xfcu;
    state->saved_y = ground & 0xfcu;
}

void allstar_horse_cpu_spot_6cab(uint8_t rng, uint8_t sequence_index,
                                uint8_t *player_center_x,
                                uint8_t *player_ground_y) {
    uint8_t group = rng < 0x30 ? 0 :
                    rng < 0x5e ? 1 :
                    rng < 0x8c ? 2 :
                    rng < 0xba ? 3 : 4;
    uint8_t spot = sequence_index % ALLSTAR_HORSE_CPU_SPOTS_PER_GROUP;
    if (player_center_x)
        *player_center_x = HORSE_CPU_SPOTS[group][spot][0];
    if (player_ground_y)
        *player_ground_y = HORSE_CPU_SPOTS[group][spot][1];
}

bool allstar_horse_current_is_matcher(const AllStarHorseState *state) {
    return state && state->current_player != state->caller;
}

uint32_t allstar_horse_resolve_shot_0d57(AllStarHorseState *state,
                                        bool made,
                                        float player_center_x,
                                        float player_ground_y) {
    uint32_t events = ALLSTAR_HORSE_EVENT_NONE;
    uint8_t index;
    bool matcher;
    if (!state || state->complete || state->current_player < 1 ||
        state->current_player > 2) return events;
    index = (uint8_t)(state->current_player - 1);
    matcher = allstar_horse_current_is_matcher(state);

    if (!matcher) {
        /* $0D9C always saves the caller's release spot; $0DD8 keeps the
           caller only on a make and otherwise gives the call to the next
           player. */
        allstar_horse_save_spot_0e36(
            state, player_center_x, player_ground_y);
        if (made) {
            state->called_shot_made = true;
            events |= ALLSTAR_HORSE_EVENT_CALLED_MAKE;
        } else {
            state->called_shot_made = false;
            state->caller = state->current_player == 1 ? 2 : 1;
            events |= ALLSTAR_HORSE_EVENT_CALLER_CHANGED;
        }
    } else if (!made) {
        /* $0E26 decrements the selected player +$0E, calls $7BA8, then
           issues audio command $07. The first zero counter ends the mode. */
        if (state->letters_remaining[index] != 0)
            state->letters_remaining[index]--;
        events |= ALLSTAR_HORSE_EVENT_LETTER;
        if (state->letters_remaining[index] == 0) {
            state->complete = true;
            state->winner = state->current_player == 1 ? 2 : 1;
            events |= ALLSTAR_HORSE_EVENT_COMPLETE;
        }
    } else if (!state->called_shot_made) {
        /* Preserve the cartridge's defensive branch: a made shot after a
           cleared call becomes the new called spot. */
        allstar_horse_save_spot_0e36(
            state, player_center_x, player_ground_y);
        state->called_shot_made = true;
        state->caller = state->current_player;
        events |= ALLSTAR_HORSE_EVENT_CALLED_MAKE |
                  ALLSTAR_HORSE_EVENT_CALLER_CHANGED;
    }

    if (!state->complete)
        state->current_player = state->current_player == 1 ? 2 : 1;
    return events;
}

const char *allstar_horse_letters_7bc0(uint8_t letters_remaining) {
    static const char *const incurred[ALLSTAR_HORSE_LETTER_COUNT + 1] = {
        "HORSE", "HORS", "HOR", "HO", "H", ""
    };
    if (letters_remaining > ALLSTAR_HORSE_LETTER_COUNT)
        letters_remaining = ALLSTAR_HORSE_LETTER_COUNT;
    return incurred[letters_remaining];
}

/* $0D2B..$0D56 */
void allstar_horse_handoff_0d2b(uint8_t shooter, AllStarHorseHandoff *out) {
    if (!out) return;
    out->shooter = shooter;                              /* $0D2B, $0D41 */
    out->set_flags[0] = 1u;                              /* $0D30, $FFE7 */
    out->set_flags[1] = 1u;                              /* $0D32, $FFE6 */
    out->set_flags[2] = 1u;                              /* $0D34, $C12C */
    out->cleared[0] = 0u;                                /* $0D38, $C0FD */
    out->cleared[1] = 0u;                                /* $0D3B, $C145 */
    out->wait_frames = ALLSTAR_HORSE_HANDOFF_WAIT;       /* $0D4C */
    out->enables_objects = true;                         /* $0D54 */
}

/* Bank 1 $7AEA..$7AFC */
void allstar_horse_pre_shot_7aea(uint8_t players, uint8_t shooter,
                                 AllStarHorsePreShot *out) {
    if (!out) return;
    out->cleared[0] = 0xC0FDu;                    /* $7AF6 */
    out->cleared[1] = 0xC145u;                    /* $7AF9 */
    out->calls_6cab = false;
    out->runs = false;
    if (players != 1u) return;                    /* $7AED */
    if (shooter == 1u) return;                    /* $7AF1 */
    out->runs = true;
    out->calls_6cab = true;                       /* $7AF2 */
}
