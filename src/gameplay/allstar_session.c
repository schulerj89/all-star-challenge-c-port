#include "allstar_session.h"

/* $0239..$023D */
bool allstar_session_picks_player(uint8_t mode) {
    return mode != ALLSTAR_SESSION_TOURNAMENT;
}

/* $0214..$0263 */
int allstar_session_sequence(uint8_t mode, AllStarSessionStep *out, int max) {
    static const AllStarSessionStep STEPS[ALLSTAR_SESSION_MAX_STEPS] = {
        ALLSTAR_SESSION_CLEAR_FLAG,
        ALLSTAR_SESSION_MENU,
        ALLSTAR_SESSION_SETTINGS,
        ALLSTAR_SESSION_LOAD_TILES,
        ALLSTAR_SESSION_PICK_PLAYER,
        ALLSTAR_SESSION_PREPARE,
        ALLSTAR_SESSION_RUN_MODE,
        ALLSTAR_SESSION_POSTGAME,
        ALLSTAR_SESSION_RESET
    };
    int count = 0;
    int i;

    if (!out || max <= 0) return 0;

    for (i = 0; i < ALLSTAR_SESSION_MAX_STEPS; i++) {
        if (STEPS[i] == ALLSTAR_SESSION_PICK_PLAYER &&
            !allstar_session_picks_player(mode)) {
            continue;                                               /* $023D */
        }
        if (count >= max) break;
        out[count++] = STEPS[i];
    }
    return count;
}

/* The `rst $20` before each stage. */
uint8_t allstar_session_bank(AllStarSessionStep step) {
    switch (step) {
    case ALLSTAR_SESSION_MENU:                                      /* $0218 */
    case ALLSTAR_SESSION_SETTINGS:
        return 3u;
    case ALLSTAR_SESSION_PICK_PLAYER:                               /* $023F */
        return 2u;
    default:                                                        /* $0227, $024A */
        return 1u;
    }
}

const char* allstar_session_step_name(AllStarSessionStep step) {
    switch (step) {
    case ALLSTAR_SESSION_CLEAR_FLAG:  return "$0214";
    case ALLSTAR_SESSION_MENU:        return "$038F";
    case ALLSTAR_SESSION_SETTINGS:    return "$22EF";
    case ALLSTAR_SESSION_LOAD_TILES:  return "$050F";
    case ALLSTAR_SESSION_PICK_PLAYER: return "$4000";
    case ALLSTAR_SESSION_PREPARE:     return "$1FFA";
    case ALLSTAR_SESSION_RUN_MODE:    return "$0266";
    case ALLSTAR_SESSION_POSTGAME:    return "$10A5";
    case ALLSTAR_SESSION_RESET:       return "$0156";
    default:                          return "?";
    }
}
