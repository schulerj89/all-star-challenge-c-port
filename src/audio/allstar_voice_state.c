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
