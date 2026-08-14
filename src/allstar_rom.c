#include "allstar_rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool allstar_rom_load_file(AllStarRom *rom, const char *filepath) {
    if (!rom || !filepath) return false;
    memset(rom, 0, sizeof(AllStarRom));

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "[ROM] Failed to open file: %s\n", filepath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 0x150) {
        fprintf(stderr, "[ROM] File too small (%ld bytes)\n", file_size);
        fclose(f);
        return false;
    }

    rom->size = (size_t)file_size;
    rom->data = (uint8_t*)malloc(rom->size);
    if (!rom->data) {
        fprintf(stderr, "[ROM] Out of memory allocating %zu bytes\n", rom->size);
        fclose(f);
        return false;
    }

    if (fread(rom->data, 1, rom->size, f) != rom->size) {
        fprintf(stderr, "[ROM] Failed reading complete ROM data\n");
        free(rom->data);
        rom->data = NULL;
        fclose(f);
        return false;
    }
    fclose(f);

    /* Parse Game Boy Header */
    memcpy(rom->header.title, &rom->data[0x134], 16);
    rom->header.title[16] = '\0';
    rom->header.cart_type = rom->data[0x147];
    rom->header.rom_size_code = rom->data[0x148];
    rom->header.ram_size_code = rom->data[0x149];
    rom->header.dest_code = rom->data[0x14A];
    rom->header.header_checksum = rom->data[0x14D];
    rom->header.global_checksum = ((uint16_t)rom->data[0x14E] << 8) | rom->data[0x14F];

    /* Calculate header checksum */
    uint8_t chk = 0;
    for (int i = 0x134; i <= 0x14C; i++) {
        chk = (uint8_t)(chk - rom->data[i] - 1);
    }
    rom->header.is_valid_header = (chk == rom->header.header_checksum);
    rom->is_loaded = true;

    return true;
}

void allstar_rom_free(AllStarRom *rom) {
    if (rom && rom->data) {
        free(rom->data);
        rom->data = NULL;
        rom->size = 0;
        rom->is_loaded = false;
    }
}

bool allstar_rom_verify(const AllStarRom *rom) {
    if (!rom || !rom->is_loaded || !rom->data) return false;
    if (!rom->header.is_valid_header) {
        fprintf(stderr, "[ROM] Warning: Header checksum mismatch!\n");
    }
    return true;
}

uint8_t allstar_rom_read8(const AllStarRom *rom, uint16_t address) {
    if (!rom || !rom->data || address >= rom->size) return 0xFF;
    return rom->data[address];
}

uint16_t allstar_rom_read16(const AllStarRom *rom, uint16_t address) {
    if (!rom || !rom->data || (size_t)address + 1 >= rom->size) return 0xFFFF;
    return (uint16_t)rom->data[address] | ((uint16_t)rom->data[address + 1] << 8);
}
