#include "allstar_voice_state.h"

/* $32B8..$32E7, in the order the ROM copies them. */
static const AllStarVoiceField VOICE_FIELDS[ALLSTAR_VOICE_FIELDS] = {
    { 0xDE2Au, 0xDDBFu },   /* $32B8 */
    { 0xDE2Bu, 0xDDC7u },   /* $32C0 */
    { 0xDE2Du, 0xDDCFu },   /* $32C8, the swapped pair */
    { 0xDE2Cu, 0xDDD7u },   /* $32D0 */
    { 0xDE28u, 0xDDDFu },   /* $32D8 */
    { 0xDE29u, 0xDDE7u }    /* $32E0 */
};

const AllStarVoiceField* allstar_voice_fields(int *count) {
    if (count) *count = ALLSTAR_VOICE_FIELDS;
    return VOICE_FIELDS;
}

uint16_t allstar_voice_slot(int field, uint8_t channel) {
    if (field < 0 || field >= ALLSTAR_VOICE_FIELDS) return 0;
    return (uint16_t)(VOICE_FIELDS[field].table + channel);
}

/* $32B8 */
void allstar_voice_save(const uint8_t *working, uint8_t *slots) {
    int i;
    if (!working || !slots) return;
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) slots[i] = working[i];
}

/* $32E9 */
void allstar_voice_load(const uint8_t *slots, uint8_t *working) {
    int i;
    if (!slots || !working) return;
    for (i = 0; i < ALLSTAR_VOICE_FIELDS; i++) working[i] = slots[i];
}

/* $3264 and $327F */
int allstar_voice_bank_order(AllStarVoiceBank bank, uint8_t *out, int max) {
    int first = bank == ALLSTAR_VOICE_BANK_SFX
        ? ALLSTAR_VOICE_SFX_FIRST : ALLSTAR_VOICE_MUSIC_FIRST;
    int last = bank == ALLSTAR_VOICE_BANK_SFX
        ? ALLSTAR_VOICE_SFX_LAST : ALLSTAR_VOICE_MUSIC_LAST;
    int count = 0;
    int channel;
    if (!out || max <= 0) return 0;
    /* $3279/$3294 both decrement, so the walk is high channel to low. */
    for (channel = first; channel >= last && count < max; channel--) {
        out[count++] = (uint8_t)channel;
    }
    return count;
}

/* $3266 and $3281 index $DD7F by the channel. */
uint16_t allstar_voice_active_slot(uint8_t channel) {
    return (uint16_t)(ALLSTAR_VOICE_ACTIVE_BASE + channel);
}

/* $326B/$3286: `and a` then `jr z` skips the channel. */
bool allstar_voice_channel_runs(uint8_t active_flag) {
    return active_flag != 0;
}
