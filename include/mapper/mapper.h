#ifndef MAPPER_H
#define MAPPER_H

#include <stdint.h>
#include <stdbool.h>
#include "rom.h"
#include "nrom.h"
#include "mmc1.h"
#include "uxrom.h"

typedef enum {
    MAPPER_NROM = 0,
    MAPPER_MMC1 = 1,
    MAPPER_UxROM = 2
} MapperType;

typedef enum {
    PRGTYPE_PRG_ROM,
    PRGTYPE_PRG_RAM
} PRGType;

typedef enum {
    CHRTYPE_DEFAULT,
    CHRTYPE_CHR_ROM,
    CHRTYPE_CHR_RAM,
    CHRTYPE_VRAM
} CHRType;

typedef enum {
    NT_MIRROR_HORIZONTAL,
    NT_MIRROR_VERTICAL,
    NT_MIRROR_FOURSCREEN,
    NT_MIRROR_ONESCREEN_A,
    NT_MIRROR_ONESCREEN_B
} NTMirroring;

typedef struct {
    char* prg_rom;
    unsigned prg_rom_size;
    char* chr_rom;
    unsigned chr_rom_size;
    char* prg_ram;
    unsigned prg_ram_size;
    char* chr_ram;
    unsigned chr_ram_size;
    char* vram;
    unsigned vram_size;

    char* prg_pages[0x100];
    bool prg_page_is_rom[0x100];
    char* chr_pages[0x40];
    bool chr_page_is_rom[0x40];
} MapperMemory;

typedef struct Mapper Mapper;

typedef struct {
    void(*Cleanup)(Mapper*);

    uint8_t(*CPURead)(Mapper*, uint16_t addr);
    void(*CPUWrite)(Mapper*, uint16_t addr, uint8_t data);
    uint8_t(*PPURead)(Mapper*, uint16_t addr);
    void(*PPUWrite)(Mapper*, uint16_t addr, uint8_t data);

    void(*WriteRegisters)(Mapper*, uint16_t addr, uint8_t data);

    size_t(*SaveBattery)(Mapper*, FILE* file);
    size_t(*LoadBattery)(Mapper*, FILE* file);
} MapperFuncs;

typedef struct Emulator Emulator;

struct Mapper {
    Emulator* emulator;
    MapperMemory memory;
    bool hasBattery;
    MapperType type;
    union {
        MMC1 mmc1;
        UxROM uxrom;
    };
    MapperFuncs f;
};


int Mapper_Init(Mapper* mapper, const INESHeader* ines, FILE* romFile);
void Mapper_Cleanup(Mapper* mapper);

/* Helper functions for use by mappers */

void Mapper_ResizePRGRAM(Mapper* mapper, unsigned size);
void Mapper_ResizeCHRRAM(Mapper* mapper, unsigned size);
void Mapper_ResizeVRAM(Mapper* mapper, unsigned size);

void MapPRGPages(Mapper* mapper, uint8_t startPage, uint8_t endPage, int srcPage, PRGType type);
void MapCHRPages(Mapper* mapper, uint8_t startPage, uint8_t endPage, int srcPage, CHRType type);
void MapNametable(Mapper* mapper, NTMirroring mirroring);

#endif