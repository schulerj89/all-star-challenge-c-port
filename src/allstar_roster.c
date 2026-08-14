#include "allstar_roster.h"
#include <string.h>

static const AllStarPlayerStats DEFAULT_PLAYERS[ALLSTAR_DEFAULT_ROSTER_COUNT] = {
    {"M. JORDAN",    "BULLS",    23, 98, 92, 99, 90, 95, 0},
    {"L. BIRD",      "CELTICS",  33, 82, 98, 95, 96, 85, 1},
    {"M. JOHNSON",   "LAKERS",   32, 90, 85, 94, 91, 88, 2},
    {"C. BARKLEY",   "76ERS",    34, 88, 78, 96, 82, 92, 3},
    {"P. EWING",     "KNICKS",   33, 80, 65, 92, 80, 96, 4},
    {"K. MALONE",    "JAZZ",     32, 86, 68, 95, 84, 93, 5},
    {"J. STOCKTON",  "JAZZ",     12, 92, 90, 88, 93, 90, 6},
    {"D. ROBINSON",  "SPURS",    50, 85, 60, 94, 78, 97, 7},
    {"C. DREXLER",   "BLAZERS",  22, 94, 84, 95, 86, 91, 8},
    {"S. PIPPEN",    "BULLS",    33, 92, 80, 90, 82, 96, 9},
    {"C. MULLIN",    "WARRIORS", 17, 84, 96, 94, 95, 82, 10},
    {"D. WILKINS",   "HAWKS",    21, 95, 82, 96, 85, 86, 11},
    {"I. THOMAS",    "PISTONS",  11, 93, 86, 90, 88, 89, 12},
    {"J. WORTHY",    "LAKERS",   42, 90, 72, 93, 81, 87, 13},
    {"B. DAUGHERTY", "CAVS",     43, 78, 55, 90, 85, 88, 14},
    {"T. HARDAWAY",  "WARRIORS", 10, 96, 88, 90, 84, 86, 15},
    {"K. JOHNSON",   "SUNS",      7, 95, 82, 91, 87, 85, 16},
    {"T. PORTER",    "BLAZERS",  30, 89, 90, 88, 88, 86, 17},
    {"R. MILLER",    "PACERS",   31, 88, 97, 91, 95, 83, 18},
    {"H. OLAJUWON",  "ROCKETS",  34, 84, 50, 95, 76, 98, 19},
    {"J. DUMARS",    "PISTONS",   4, 88, 89, 89, 92, 94, 20},
    {"R. BLACKMAN",  "MAVS",     22, 87, 86, 88, 89, 84, 21},
    {"A. GILMORE",   "BULLS",    53, 72, 45, 88, 70, 90, 22},
    {"M. PRICE",     "CAVS",     25, 91, 96, 88, 96, 82, 23},
    {"B. LAIMBEER",  "PISTONS",  40, 74, 82, 84, 86, 91, 24},
    {"D. SCHREMPF",  "PACERS",   11, 84, 88, 89, 85, 84, 25},
    {"C. WEBB",      "HAWKS",     4, 98, 70, 90, 78, 80, 26}
};

void allstar_roster_init_default(AllStarRoster *roster) {
    if (!roster) return;
    memset(roster, 0, sizeof(AllStarRoster));
    roster->count = ALLSTAR_DEFAULT_ROSTER_COUNT;
    for (size_t i = 0; i < roster->count; i++) {
        roster->players[i] = DEFAULT_PLAYERS[i];
    }
}

void allstar_roster_load_from_asset_pack(AllStarRoster *roster, const AllStarAssetPack *pack) {
    if (!roster || !pack) return;
    roster->count = pack->header.player_count;
    if (roster->count > ALLSTAR_MAX_ROSTER) roster->count = ALLSTAR_MAX_ROSTER;
    for (size_t i = 0; i < roster->count; i++) {
        roster->players[i] = pack->players[i];
    }
}

const AllStarPlayerStats* allstar_roster_get_player(const AllStarRoster *roster, size_t index) {
    if (!roster || index >= roster->count) return NULL;
    return &roster->players[index];
}
