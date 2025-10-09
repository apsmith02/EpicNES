#include "mapper_base.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

void Mapper_Init(MapperBase *mapper, const INESHeader *romHeader, FILE *romFile)
{
    Mapper_Init_PRG_CHR_ROM(mapper, romHeader, romFile);
    if (romHeader->chr_units == 0) {
        //If no CHR ROM, create and map CHR RAM instead
        Mapper_InitCHRRAM(mapper, 0x2000);
        Mapper_MapCHRRAMPages(mapper, 0x00, 0x1F, 0x0);
    } else {
        //Else map 8k CHR ROM by default
        Mapper_MapCHRROMPages(mapper, 0x00, 0x1F, 0x0);
    }
    mapper->vtable->Init(mapper, romHeader, romFile);
}

void Mapper_Free(MapperBase *mapper)
{
    if (mapper->vtable->Deinit)
        mapper->vtable->Deinit(mapper);
    free(mapper);
}

uint8_t Mapper_CPURead(MapperBase *mapper, uint16_t addr)
{
    if (mapper->prg_pages[addr >> 8] == NULL)
        return 0;
    return mapper->prg_pages[addr >> 8][addr & 0xFF];
}

void Mapper_CPUWrite(MapperBase *mapper, uint16_t addr, uint8_t data)
{
    if (mapper->prg_pages[addr >> 8] != NULL &&
    mapper->prg_page_is_rom[addr >> 8] == false) {
        mapper->prg_pages[addr >> 8][addr & 0xFF] = data;
    }
    if (mapper->vtable->RegWrite)
        mapper->vtable->RegWrite(mapper, addr, data);
}

uint8_t Mapper_PPURead(MapperBase *mapper, uint16_t addr)
{
    if (mapper->chr_pages[addr >> 8] == NULL)
        return 0;
    return mapper->chr_pages[addr >> 8][addr & 0xFF];
}

void Mapper_PPUWrite(MapperBase *mapper, uint16_t addr, uint8_t data)
{
    if (mapper->chr_pages[addr >> 8] != NULL &&
    mapper->chr_page_is_rom[addr >> 8] == false) {
        mapper->chr_pages[addr >> 8][addr & 0xFF] = data;
    }
}

void Mapper_Init_PRG_CHR_ROM(MapperBase *mapper, const INESHeader *romHeader, FILE *romFile)
{
    mapper->prg_rom_size = romHeader->prg_bytes;
    mapper->prg_rom = INES_ReadPRG(romHeader, romFile);
    mapper->chr_rom_size = romHeader->chr_bytes;
    mapper->chr_rom = INES_ReadCHR(romHeader, romFile);
}

void Mapper_InitVRAM(MapperBase *mapper, unsigned vram_size)
{
    mapper->vram = malloc(vram_size);
    mapper->vram_size = vram_size;
}

void Mapper_InitCHRRAM(MapperBase *mapper, unsigned chr_ram_size)
{
    mapper->chr_ram = malloc(chr_ram_size);
    mapper->chr_ram_size = chr_ram_size;
}

static void Mapper_MapPRGPages(MapperBase *mapper, char *physical_memory, unsigned physical_size, bool is_rom, uint8_t cpu_page_start, uint8_t cpu_page_end, unsigned physical_address) {
    assert(physical_address + (cpu_page_end - cpu_page_start + 1) * 0x100 <= physical_size);
    for (unsigned page = cpu_page_start; page <= cpu_page_end; page++, physical_address += 0x100) {
        mapper->prg_pages[page] = physical_memory + physical_address;
        mapper->prg_page_is_rom[page] = is_rom;
    }
}

static void Mapper_MapCHRPages(MapperBase *mapper, char *physical_memory, unsigned physical_size, bool is_rom, uint8_t ppu_page_start, uint8_t ppu_page_end, unsigned physical_address) {
    assert(physical_address + (ppu_page_end - ppu_page_start + 1) * 0x100 <= physical_size);
    for (unsigned page = ppu_page_start; page <= ppu_page_end; page++, physical_address += 0x100) {
        mapper->chr_pages[page] = physical_memory + physical_address;
        mapper->chr_page_is_rom[page] = is_rom;
    }
}

void Mapper_MapPRGROMPages(MapperBase *mapper, uint8_t cpu_page_start, uint8_t cpu_page_end, unsigned physical_address)
{
    Mapper_MapPRGPages(mapper, mapper->prg_rom, mapper->prg_rom_size, true, cpu_page_start, cpu_page_end, physical_address);
}

void Mapper_MapCHRROMPages(MapperBase *mapper, uint8_t ppu_page_start, uint8_t ppu_page_end, unsigned physical_address)
{
    Mapper_MapCHRPages(mapper, mapper->chr_rom, mapper->chr_rom_size, true, ppu_page_start, ppu_page_end, physical_address);
}

void Mapper_MapCHRRAMPages(MapperBase *mapper, uint8_t ppu_page_start, uint8_t ppu_page_end, unsigned physical_address)
{
    Mapper_MapCHRPages(mapper, mapper->chr_ram, mapper->chr_ram_size, false, ppu_page_start, ppu_page_end, physical_address);
}

void Mapper_MapNTMirroring(MapperBase *mapper, NTMirroring mirroring)
{
    switch (mirroring) {
        case NT_MIRROR_HORIZONTAL:
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x20, 0x23, 0x0);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x24, 0x27, 0x0);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x28, 0x2B, 0x400);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x2C, 0x2F, 0x400);
        break;
        case NT_MIRROR_VERTICAL:
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x20, 0x23, 0);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x24, 0x27, 0x400);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x28, 0x2B, 0);
            Mapper_MapCHRPages(mapper, mapper->vram, mapper->vram_size, false, 0x2C, 0x2F, 0x400);
        break;
        default: break;
    }
}

void Mapper_MapPRGROM_16k(MapperBase *mapper, bool upperHalf, unsigned bank)
{
    uint8_t pageStart = upperHalf ? 0xC0 : 0x80;
    Mapper_MapPRGROMPages(mapper, pageStart, pageStart + 0x3F, bank * 0x4000);
}

unsigned Mapper_NumBanks_PRGROM_16k(MapperBase *mapper)
{
    return mapper->prg_rom_size / 0x4000;
}
