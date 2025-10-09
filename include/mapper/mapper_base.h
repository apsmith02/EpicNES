#ifndef MAPPER_BASE_H
#define MAPPER_BASE_H

#include "../rom.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    NT_MIRROR_HORIZONTAL,
    NT_MIRROR_VERTICAL
} NTMirroring;

typedef struct MapperVtable MapperVtable;
typedef struct {
    char* prg_rom;
    unsigned prg_rom_size;
    char* chr_rom;
    unsigned chr_rom_size;
    char* chr_ram;
    unsigned chr_ram_size;
    char* vram;
    unsigned vram_size;

    char* prg_pages[0x100];
    bool prg_page_is_rom[0x100];
    char* chr_pages[0x40];
    bool chr_page_is_rom[0x40];

    const MapperVtable* vtable;
} MapperBase;

struct MapperVtable {
    void(*Init)(MapperBase*, const INESHeader* romHeader, FILE* romFile);
    void(*Deinit)(MapperBase*);
    void(*RegWrite)(MapperBase*, uint16_t addr, uint8_t data);
};

//Load PRG and CHR ROM, then call the mapper's virtual Init() function.
void Mapper_Init(MapperBase* mapper, const INESHeader* romHeader, FILE* romFile);
//Call the mapper's virtual Deinit() function, then free mapper.
void Mapper_Free(MapperBase* mapper);

uint8_t Mapper_CPURead(MapperBase* mapper, uint16_t addr);
//Write to PRG and mapper registers.
void Mapper_CPUWrite(MapperBase* mapper, uint16_t addr, uint8_t data);

uint8_t Mapper_PPURead(MapperBase* mapper, uint16_t addr);
void Mapper_PPUWrite(MapperBase* mapper, uint16_t addr, uint8_t data);

void Mapper_Init_PRG_CHR_ROM(MapperBase* mapper, const INESHeader* romHeader, FILE* romFile);
void Mapper_InitVRAM(MapperBase* mapper, unsigned vram_size);
void Mapper_InitCHRRAM(MapperBase* mapper, unsigned chr_ram_size);

void Mapper_MapPRGROMPages(MapperBase* mapper, uint8_t cpu_page_start, uint8_t cpu_page_end, unsigned physical_address);
void Mapper_MapCHRROMPages(MapperBase* mapper, uint8_t ppu_page_start, uint8_t ppu_page_end, unsigned physical_address);
void Mapper_MapCHRRAMPages(MapperBase* mapper, uint8_t ppu_page_start, uint8_t ppu_page_end, unsigned physical_address);
void Mapper_MapNTMirroring(MapperBase* mapper, NTMirroring mirroring);

/*Map a 16KB PRG ROM bank into the lower half of program memory ($8000-$BFFF) or the upper half ($C000-$FFFF).
* Be sure to check the number of banks with Mapper_NumBanks_PRGROM_16k() first.
*/
void Mapper_MapPRGROM_16k(MapperBase* mapper, bool upperHalf, unsigned bank);
unsigned Mapper_NumBanks_PRGROM_16k(MapperBase* mapper);

#endif