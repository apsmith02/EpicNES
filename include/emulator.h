#ifndef EMULATOR_H
#define EMULATOR_H

#include "nes_defs.h"
#include "rom.h"
#include "mapper/mapper_base.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "dma.h"
#include "standard_controller.h"

typedef struct {
    INESHeader rom_ines;
    CPU cpu;
    PPU ppu;
    APU apu;
    StandardController controller;
    DMAController dma;
    MapperBase *mapper;
    uint8_t ram[0x800];

    int is_rom_loaded;
} Emulator;

Emulator* Emu_Create();

void Emu_Free(Emulator* emu);

/**
* Load a ROM from a file and power on the console.
*
* @return 0 on success, -1 on error.
*/
int Emu_LoadROM(Emulator* emu, const char* filename);

void Emu_CloseROM(Emulator* emu);

int Emu_IsROMLoaded(Emulator* emu);

/**
 * Power on the console.
*/
void Emu_PowerOn(Emulator* emu);

/**
* Run one frame.
*
* @return 0 on success, -1 on error.
*/
int Emu_RunFrame(Emulator* emu);

/**
* Press a button on the standard controller connected to port 1.
*/
void Emu_PressButton(Emulator* emu, ControllerButton button);
/**
* Release a button on the standard controller connected to port 1.
*/
void Emu_ReleaseButton(Emulator* emu, ControllerButton button);

RGBAPixel* Emu_GetPixelBuffer(Emulator* emu, int* width, int* height);

void* Emu_GetAudioBuffer(Emulator* emu, size_t* len);
void Emu_ClearAudioBuffer(Emulator* emu);

#endif