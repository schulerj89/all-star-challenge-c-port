#ifndef ALLSTAR_ROSTER_H
#define ALLSTAR_ROSTER_H

#include "allstar_types.h"
#include "allstar_asset_pack.h"

#define ALLSTAR_DEFAULT_ROSTER_COUNT 27

typedef struct {
    AllStarPlayerStats players[ALLSTAR_MAX_ROSTER];
    size_t count;
} AllStarRoster;

void allstar_roster_init_default(AllStarRoster *roster);
void allstar_roster_load_from_asset_pack(AllStarRoster *roster, const AllStarAssetPack *pack);
const AllStarPlayerStats* allstar_roster_get_player(const AllStarRoster *roster, size_t index);

#endif /* ALLSTAR_ROSTER_H */
