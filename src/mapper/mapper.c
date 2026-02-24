#include "mapper.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

/* Default mapper methods */

void DefaultCleanup(Mapper* mapper) {}

uint8_t DefaultCPURead(Mapper* mapper, uint16_t addr) {
    MapperMemory* mem = &mapper->memory;

    if (mem->prg_pages[addr >> 8] != NULL)
        return mem->prg_pages[addr >> 8][addr & 0x00FF];
    return 0;
}

void DefaultCPUWrite(Mapper* mapper, uint16_t addr, uint8_t data) {
    MapperMemory* mem = &mapper->memory;

    if (mem->prg_pages[addr >> 8] != NULL && !mem->prg_page_is_rom[addr >> 8])
        mem->prg_pages[addr >> 8][addr & 0x00FF] = data;
    //TODO: Emulate bus conflicts
    mapper->f.WriteRegisters(mapper, addr, data);
}

uint8_t DefaultPPURead(Mapper* mapper, uint16_t addr) {
    assert(addr < 0x4000);
    MapperMemory* mem = &mapper->memory;

    if (mem->chr_pages[addr >> 8] != NULL)
        return mem->chr_pages[addr >> 8][addr & 0x00FF];
    return 0;
}

void DefaultPPUWrite(Mapper* mapper, uint16_t addr, uint8_t data) {
    assert(addr < 0x4000);
    MapperMemory* mem = &mapper->memory;

    if (mem->chr_pages[addr >> 8] != NULL && !mem->chr_page_is_rom[addr >> 8])
        mem->chr_pages[addr >> 8][addr & 0x00FF] = data;
}

void DefaultWriteRegisters(Mapper* mapper, uint16_t addr, uint8_t data) {}

size_t DefaultSaveBattery(Mapper* mapper, FILE* file) {
    MapperMemory* mem = &mapper->memory;

    if (!mapper->hasBattery)
        return 0;
    return fwrite(mem->prg_ram, 1, mem->prg_ram_size, file);
}

size_t DefaultLoadBattery(Mapper* mapper, FILE* file) {
    MapperMemory* mem = &mapper->memory;

    if (!mapper->hasBattery)
        return 0;
    return fread(mem->prg_ram, 1, mem->prg_ram_size, file);
}

/* Public functions */

int Mapper_Init(Mapper *mapper, const INESHeader *ines, FILE* romFile)
{
    MapperMemory* mem = &mapper->memory;

    memset(mapper, 0, sizeof(Mapper));

    mapper->f.Cleanup = &DefaultCleanup;
    mapper->f.CPURead = &DefaultCPURead;
    mapper->f.CPUWrite = &DefaultCPUWrite;
    mapper->f.PPURead = &DefaultPPURead;
    mapper->f.PPUWrite = &DefaultPPUWrite;
    mapper->f.LoadBattery = &DefaultLoadBattery;
    mapper->f.SaveBattery = &DefaultSaveBattery;

    //Extract some information from iNES header
    mapper->hasBattery = ines->has_battery_saves;

    //Load PRG-ROM
    mem->prg_rom_size = ines->prg_bytes;
    mem->prg_rom = INES_ReadPRG(ines, romFile);
    //If CHR-ROM is present, load it. Else, default to 8KB of CHR-RAM instead
    mem->chr_rom_size = ines->chr_bytes;
    if (mem->chr_rom_size > 0)
        mem->chr_rom = INES_ReadCHR(ines, romFile);
    else
        Mapper_ResizeCHRRAM(mapper, 0x2000);
    
    Mapper_ResizeVRAM(mapper, 0x800);
    
    //Select mapper
    switch (ines->mapper) {
        case 0:     mapper->type = MAPPER_NROM;     return NROM_Init(mapper, ines);
        case 1:     mapper->type = MAPPER_MMC1;     return MMC1_Init(mapper, ines);
        case 2:     mapper->type = MAPPER_UxROM;    return UxROM_Init(mapper, ines);
        default:    printf("Error: Mapper #%u is not supported by this emulator.\n", ines->mapper); Mapper_Cleanup(mapper); return -1;
    }
}

void Mapper_Cleanup(Mapper *mapper)
{
    MapperMemory* mem = &mapper->memory;

    mapper->f.Cleanup(mapper);

    free(mem->prg_rom);
    free(mem->chr_rom);
    free(mem->prg_ram);
    free(mem->chr_ram);
    free(mem->vram);

    memset(mapper, 0, sizeof(Mapper));
}

/* Helper functions */

void Mapper_ResizePRGRAM(Mapper *mapper, unsigned size)
{
    MapperMemory* mem = &mapper->memory;
    mem->prg_ram = realloc(mem->prg_ram, size);
    mem->prg_ram_size = size;
}

void Mapper_ResizeCHRRAM(Mapper *mapper, unsigned size)
{
    MapperMemory* mem = &mapper->memory;
    mem->chr_ram = realloc(mem->chr_ram, size);
    mem->chr_ram_size = size;
}

void Mapper_ResizeVRAM(Mapper *mapper, unsigned size)
{
    MapperMemory* mem = &mapper->memory;
    mem->vram = realloc(mem->vram, size);
    mem->vram_size = size;
}

void MapPRGPages(Mapper *mapper, uint8_t startPage, uint8_t endPage, int srcPage, PRGType type)
{
    MapperMemory* mem = &mapper->memory;
    uint8_t* src;
    size_t srcCount;
    bool isRom;

    switch (type) {
        case PRGTYPE_PRG_ROM: src = mem->prg_rom; srcCount = mem->prg_rom_size / 0x100; isRom = true; break;
        case PRGTYPE_PRG_RAM: src = mem->prg_ram; srcCount = mem->prg_ram_size / 0x100; isRom = false; break;
    }

    srcPage %= srcCount;
    if (srcPage < 0)
        srcPage += srcCount;

    for (unsigned p = startPage; p <= endPage; p++) {
        mem->prg_pages[p] = &src[srcPage * 0x100];
        mem->prg_page_is_rom[p] = isRom;
        srcPage = (srcPage + 1) % srcCount;
    }
}

void MapCHRPages(Mapper *mapper, uint8_t startPage, uint8_t endPage, int srcPage, CHRType type)
{
    MapperMemory* mem = &mapper->memory;
    uint8_t* src;
    size_t srcCount;
    bool isRom;

    if (type == CHRTYPE_DEFAULT)
        type = ((mem->chr_rom_size == 0) ? CHRTYPE_CHR_RAM : CHRTYPE_CHR_ROM);
    switch (type) {
        case CHRTYPE_CHR_ROM:   src = mem->chr_rom; srcCount = mem->chr_rom_size / 0x100;   isRom = true; break;
        case CHRTYPE_CHR_RAM:   src = mem->chr_ram; srcCount = mem->chr_ram_size / 0x100;   isRom = false; break;
        case CHRTYPE_VRAM:      src = mem->vram;    srcCount = mem->vram_size / 0x100;      isRom = false; break;
    }
    
    srcPage %= srcCount;
    if (srcPage < 0)
        srcPage += srcCount;

    for (unsigned p = startPage; p <= endPage; p++) {
        mem->chr_pages[p] = &src[srcPage * 0x100];
        mem->chr_page_is_rom[p] = isRom;
        srcPage = (srcPage + 1) % srcCount;
    }
}

void MapNametable(Mapper *mapper, NTMirroring mirroring)
{
    MapperMemory* mem = &mapper->memory;

    switch (mirroring) {
        case NT_MIRROR_HORIZONTAL:
            MapCHRPages(mapper, 0x20, 0x23, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x24, 0x27, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x28, 0x2B, 0x0400 / 0x100, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x2C, 0x2F, 0x0400 / 0x100, CHRTYPE_VRAM);
            break;
        case NT_MIRROR_VERTICAL:
            MapCHRPages(mapper, 0x20, 0x23, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x24, 0x27, 0x0400 / 0x100, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x28, 0x2B, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x2C, 0x2F, 0x0400 / 0x100, CHRTYPE_VRAM);
            break;
        case NT_MIRROR_FOURSCREEN:
            MapCHRPages(mapper, 0x20, 0x2F, 0x0000, CHRTYPE_VRAM);
            break;
        case NT_MIRROR_ONESCREEN_A:
            MapCHRPages(mapper, 0x20, 0x23, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x24, 0x27, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x28, 0x2B, 0x0000, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x2C, 0x2F, 0x0000, CHRTYPE_VRAM);
            break;
        case NT_MIRROR_ONESCREEN_B:
            MapCHRPages(mapper, 0x20, 0x23, 0x0400 / 0x100, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x24, 0x27, 0x0400 / 0x100, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x28, 0x2B, 0x0400 / 0x100, CHRTYPE_VRAM);
            MapCHRPages(mapper, 0x2C, 0x2F, 0x0400 / 0x100, CHRTYPE_VRAM);
            break;
    }
}
