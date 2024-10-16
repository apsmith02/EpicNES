#ifndef EMULATOR_H
#define EMULATOR_H

#include "rom.h"
#include "cpu.h"
#include "ppu.h"
#include "memory_map.h"

typedef struct {
    CPU cpu;
    PPU ppu;
    uint8_t ram[0x800];
    uint8_t vram[0x1000];

    ROM rom;
    CPUMemoryMap cpu_memory;
    PPUMemoryMap ppu_memory;

    int is_rom_loaded;
} Emulator;

Emulator* Emu_Create();

void Emu_Free(Emulator* emu);

/*
* Load a ROM from a file.
*
* @return 0 on success, -1 on error.
*/
int Emu_LoadROM(Emulator* emu, const char* filename);

void Emu_CloseROM(Emulator* emu);

int Emu_IsROMLoaded(Emulator* emu);

/**
* Run one frame.
*
* @return 0 on success, -1 on error.
*/
int Emu_RunFrame(Emulator* emu);

RGBAPixel* Emu_GetPixelBuffer(Emulator* emu, int* width, int* height);

#endif