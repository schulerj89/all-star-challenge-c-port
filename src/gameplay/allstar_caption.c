#include "allstar_caption.h"

/* $07E3..$0801 */
int allstar_caption_layout_07e3(const AllStarAssetPack *pack, uint8_t index,
                                AllStarCaptionDraw *out, int max) {
    const AllStarRomCaptionLayout *layout;
    int count = 0;
    int i;

    if (!pack || !out || max <= 0) return 0;
    if ((pack->header.feature_flags &
            ALLSTAR_ASSET_FEATURE_ROM_CAPTIONS) == 0) return 0;
    /* $07E7-$07ED: index is one-based, because $0802 is itself a marker. */
    if (index == 0 || index >= pack->rom_captions.layout_count) return 0;

    layout = &pack->rom_captions.layouts[index];
    for (i = 0; i < layout->record_count && count < max; i++) {
        const AllStarRomCaptionRecord *record = &layout->records[i];
        /* $07EF-$07F7: E, then D, then the stream pointer, then $06C0. */
        out[count].column = record->column;
        out[count].row = record->row;
        out[count].tiles = pack->rom_captions.pool + record->offset;
        out[count].length = record->length;
        out[count].rom_pointer = record->rom_pointer;
        count++;
    }
    return count;
}

/* $07DE differs from $07E3 only by the $047E in front of it. */
bool allstar_caption_clears_first_07de(uint16_t entry) {
    return entry == 0x07DEu;
}

/* $07E8/$07FD: bit 7 marks the last tile of a stream. */
uint8_t allstar_caption_tile(uint8_t stream_byte) {
    return (uint8_t)(stream_byte & 0x7Fu);
}
