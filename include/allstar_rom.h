#ifndef ALLSTAR_ROM_H
#define ALLSTAR_ROM_H

#include "allstar_types.h"

typedef struct {
    char title[17];
    uint8_t cart_type;
    uint8_t rom_size_code;
    uint8_t ram_size_code;
    uint8_t dest_code;
    uint8_t header_checksum;
    uint16_t global_checksum;
    bool is_valid_header;
} AllStarRomHeader;

typedef struct {
    uint8_t *data;
    size_t size;
    AllStarRomHeader header;
    bool is_loaded;
} AllStarRom;

bool allstar_rom_load_file(AllStarRom *rom, const char *filepath);
void allstar_rom_free(AllStarRom *rom);
bool allstar_rom_verify(const AllStarRom *rom);
uint8_t allstar_rom_read8(const AllStarRom *rom, uint16_t address);
uint16_t allstar_rom_read16(const AllStarRom *rom, uint16_t address);

#endif /* ALLSTAR_ROM_H */
