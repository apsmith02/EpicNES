#include "emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PRIVATE FUNCTIONS */

uint8_t _OnCPURead(void* emu, uint16_t addr) {
    CPUMemoryMap* memoryMap = &((Emulator*)emu)->cpu_memory;
    return memoryMap->read_handlers[addr >> 8](memoryMap->read_data[addr >> 8], addr);
}

void _OnCPUWrite(void* emu, uint16_t addr, uint8_t data) {
    CPUMemoryMap* memoryMap = &((Emulator*)emu)->cpu_memory;
    memoryMap->write_handlers[addr >> 8](memoryMap->write_data[addr >> 8], addr, data);
}

uint8_t _RAMRead(void* ram, uint16_t addr) {
    return ((uint8_t*)ram)[addr % 0x800];
}

void _RAMWrite(void* ram, uint16_t addr, uint8_t data) {
    ((uint8_t*)ram)[addr % 0x800] = data;
}


/* FUNCTION DEFINITIONS */

Emulator* Emu_Create()
{
    Emulator* emu = malloc(sizeof(Emulator));
    emu->is_rom_loaded = 0;
    CPU_Init(&emu->cpu, &_OnCPURead, &_OnCPUWrite, emu);
    //PPU_Init(&emu->ppu, &_OnPPURead, &_OnPPUWrite, emu);
    memset(&emu->cpu_memory, 0, sizeof(CPUMemoryMap));
    memset(&emu->ppu_memory, 0, sizeof(PPUMemoryMap));

    return emu;
}

void Emu_Free(Emulator *emu)
{
    Emu_CloseROM(emu);
    free(emu);
}

int Emu_LoadROM(Emulator *emu, const char *filename)
{
    ROM* rom = &emu->rom;
    if (ROM_Load(rom, filename) != 0) {
        printf("Error loading ROM: File not found or invalid.\n");
        return -1;
    }

    // Set up console memory map
    CPUMap_Init(&emu->cpu_memory);
    PPUMap_Init(&emu->ppu_memory);

    MapCPUPages(&emu->cpu_memory, &emu->ram[0], &_RAMRead, &_RAMWrite, 0x00, 0x20);         //RAM
    //MapCPUPages(&emu->cpu_memory, &emu->ppu, &PPU_Read, &PPU_Write, 0x20, 0x20);          //PPU
    //MapCPUPages(&emu->cpu_memory, &emu, &_Read4000_4FFF, &_Write4000_4FFF, 0x40, 0x01);  //APU, IO, OAMDMA ($4000-$4017)

    // Set up mapper, or return error if mapper is not implemented
    switch (rom->mapper) {
        case 0:
            //Map PRG
            MapPRGROMPages(&emu->cpu_memory, rom, 0x0000, 0x80, 0x40);
            MapPRGROMPages(&emu->cpu_memory, rom, 0x4000, 0xC0, 0x40);
            //Map CHR
            MapCHRROMPages(&emu->ppu_memory, rom, 0, 0x00, 0x20);
            //Map nametable
            MapNametable(&emu->ppu_memory, &emu->vram[0], rom->nt_mirroring);
            break;
        default:
            printf("Error loading ROM: This game is not currently supported by this emulator - Mapper %u is not implemented.\n", rom->mapper);
            return -1;
    }

    emu->is_rom_loaded = 1;
    return 0;
}

void Emu_CloseROM(Emulator *emu)
{
    ROM_Destroy(&emu->rom);
    emu->is_rom_loaded = 0;
}

int Emu_IsROMLoaded(Emulator *emu)
{
    return emu->is_rom_loaded;
}
