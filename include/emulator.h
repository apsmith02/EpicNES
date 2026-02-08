#ifdef __cplusplus
extern "C" {
#endif

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

    char save_dir[256];

    int is_rom_loaded;

    char save_path[256];
    FILE *save_file;
} Emulator;

Emulator* Emu_Create();

void Emu_Free(Emulator* emu);

/**
* Set the path of the directory to load and save battery saves in.
* The path must end with a trailing slash (/).
* The name of the save for a ROM will be <romname>.sav.
*/
void Emu_SetSavePath(Emulator *emu, const char* filepath);

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

//Get the output volume of an APU channel. Volume is a value between 0.0 and 1.0.
double Emu_GetAudioChannelVolume(Emulator* emu, APU_Channel channel);
//Set the output volume of an APU channel. Volume is a value between 0.0 and 1.0.
void Emu_SetAudioChannelVolume(Emulator* emu, APU_Channel channel, double volume);
//Get the output volume mute status of an APU channel.
bool Emu_GetAudioChannelMute(Emulator* emu, APU_Channel channel);
//Set the output volume mute status of an APU channel.
void Emu_SetAudioChannelMute(Emulator* emu, APU_Channel channel, bool mute);

RGBAPixel* Emu_GetPixelBuffer(Emulator* emu, int* width, int* height);

void* Emu_GetAudioBuffer(Emulator* emu, size_t* len);
void Emu_ClearAudioBuffer(Emulator* emu);

#endif //#ifndef EMULATOR_H
#ifdef __cplusplus
}
#endif