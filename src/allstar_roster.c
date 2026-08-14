#include "allstar_roster.h"
#include <string.h>

static const AllStarPlayerStats DEFAULT_PLAYERS[ALLSTAR_DEFAULT_ROSTER_COUNT] = {
    {"DANNY AINGE",       "DANNY",     "AINGE",     "KINGS",    "6'5\"",  "185", "17.9",  7, 85, 90, 88, 89, 82, 0x91,  0},
    {"CHARLES BARKLEY",   "CHARLES",   "BARKLEY",   "76ERS",    "6'6\"",  "253", "25.2", 34, 88, 74, 96, 75, 93, 0x90,  1},
    {"LARRY BIRD",        "LARRY",     "BIRD",      "CELTICS",  "6'9\"",  "220", "24.3", 33, 82, 98, 95, 93, 86, 0x91,  2},
    {"ROLANDO BLACKMAN",  "ROLANDO",   "BLACKMAN",  "MAVS",     "6'6\"",  "201", "19.4", 22, 88, 84, 89, 88, 85, 0x90,  3},
    {"TONY CAMPBELL",     "TONY",      "CAMPBELL",  "T-WOLVES", "6'7\"",  "215", "23.2",  0, 86, 75, 88, 80, 82, 0x90,  4},
    {"TOM CHAMBERS",      "TOM",       "CHAMBERS",  "SUNS",     "6'10\"", "230", "27.2", 24, 84, 78, 94, 86, 84, 0x91,  5},
    {"TERRY CUMMINGS",    "TERRY",     "CUMMINGS",  "SPURS",    "6'9\"",  "235", "22.4", 34, 83, 70, 91, 78, 88, 0x90,  6},
    {"BRAD DAUGHERTY",    "BRAD",      "DAUGHERTY", "CAVS",     "7'0\"",  "263", "16.8", 43, 78, 55, 90, 75, 90, 0x90,  7},
    {"CLYDE DREXLER",     "CLYDE",     "DREXLER",   "BLAZERS",  "6'7\"",  "215", "23.9", 22, 95, 84, 95, 82, 92, 0x90,  8},
    {"DALE ELLIS",        "DALE",      "ELLIS",     "SONICS",   "6'7\"",  "215", "23.5",  3, 87, 96, 92, 89, 83, 0x90,  9},
    {"ALEX ENGLISH",      "ALEX",      "ENGLISH",   "MAVS",     "6'7\"",  "190", "17.9",  2, 84, 72, 93, 85, 81, 0x90, 10},
    {"PATRICK EWING",     "PATRICK",   "EWING",     "KNICKS",   "7'0\"",  "240", "28.6", 33, 82, 50, 94, 78, 97, 0x90, 11},
    {"ROY HINSON",        "ROY",       "HINSON",    "NETS",     "6'9\"",  "215", "15.0", 32, 80, 52, 85, 72, 84, 0x90, 12},
    {"MICHAEL JORDAN",    "MICHAEL",   "JORDAN",    "BULLS",    "6'6\"",  "198", "33.6", 23, 98, 88, 99, 86, 97, 0x90, 13},
    {"JEFF MALONE",       "JEFF",      "MALONE",    "JAZZ",     "6'4\"",  "205", "24.3", 24, 89, 82, 93, 89, 84, 0x90, 14},
    {"KARL MALONE",       "KARL",      "MALONE",    "JAZZ",     "6'9\"",  "252", "31.0", 32, 88, 60, 97, 79, 94, 0x90, 15},
    {"DANNY MANNING",     "DANNY",     "MANNING",   "CLIPPERS", "6'10\"", "230", "16.3", 25, 84, 68, 88, 77, 85, 0x90, 16},
    {"CHRIS MULLIN",      "CHRIS",     "MULLIN",    "WARRIORS", "6'7\"",  "215", "25.7", 17, 85, 96, 95, 92, 83, 0x91, 17},
    {"AKEEM OLAJUWON",    "AKEEM",     "OLAJUWON",  "ROCKETS",  "7'0\"",  "252", "24.3", 34, 86, 50, 96, 75, 98, 0x90, 18},
    {"CHUCK PERSON",      "CHUCK",     "PERSON",    "PACERS",   "6'8\"",  "225", "19.7", 45, 85, 92, 89, 84, 82, 0x90, 19},
    {"ALVIN ROBERTSON",   "ALVIN",     "ROBERTSON", "BUCKS",    "6'4\"",  "190", "14.2", 21, 91, 76, 85, 78, 95, 0x90, 20},
    {"RONY SEIKALY",      "RONY",      "SEIKALY",   "HEAT",     "6'11\"", "240", "16.6",  4, 79, 50, 87, 71, 88, 0x91, 21},
    {"REGGIE THEUS",      "REGGIE",    "THEUS",     "NETS",     "6'7\"",  "213", "18.9", 24, 87, 80, 89, 83, 82, 0x90, 22},
    {"ISIAH THOMAS",      "ISIAH",     "THOMAS",    "PISTONS",  "6'1\"",  "175", "18.4", 11, 94, 82, 90, 84, 91, 0x90, 23},
    {"KELLY TRIPUCKA",    "KELLY",     "TRIPUCKA",  "HORNETS",  "6'6\"",  "225", "15.6",  7, 82, 88, 88, 89, 79, 0x91, 24},
    {"DOMINIQUE WILKINS", "DOMINIQUE", "WILKINS",   "HAWKS",    "6'8\"",  "200", "26.7", 21, 95, 82, 97, 82, 88, 0x90, 25},
    {"JAMES WORTHY",      "JAMES",     "WORTHY",    "LAKERS",   "6'9\"",  "225", "21.7", 42, 91, 70, 94, 80, 87, 0x90, 26}
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
