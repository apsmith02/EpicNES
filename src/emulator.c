#include "emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "mappers.h"

/* PRIVATE FUNCTIONS */

//Write $4014: Schedule OAM DMA
void Write4014(Emulator *emu, uint16_t addr, uint8_t data) {
    DMA_ScheduleOAMDMA(&emu->dma, &emu->cpu, data);
}

//Write $4016: Controller strobe
void Write4016(Emulator* emu, uint16_t addr, uint8_t data) {
    //Write to both controller ports (currently only 1 standard controller)
    StdController_Write(&emu->controller, addr, data);
}

uint8_t OnCPURead(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;

    uint8_t data = 0;
    if (addr <= 0x1FFF) {
        //RAM
        data = emu->ram[addr % 0x800];
    } else if (addr <= 0x3FFF) {
        //PPU registers
        data = PPU_RegRead(&emu->ppu, addr);
    } else if (addr == 0x4015) {
        //APU register $4015
        data = APU_Read(&emu->apu, addr);
    } else if (addr == 0x4016) {
        //Controller 1
        data = StdController_Read(&emu->controller, addr);
    } else if (addr >= 0x4020) {
        //Cartridge
        data = Mapper_CPURead(emu->mapper, addr);
    }

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);

    CPU_SetNMISignal(&emu->cpu, PPU_NMISignal(&emu->ppu));
    CPU_SetIRQSignal(&emu->cpu, APU_IRQSignal(&emu->apu));

    return data;
}

void OnCPUWrite(void* emulator, uint16_t addr, uint8_t data) {
    Emulator* emu = (Emulator*)emulator;
    
    if (addr <= 0x1FFF) {
        //RAM
        emu->ram[addr % 0x800] = data;
    } else if (addr <= 0x3FFF) {
        //PPU registers
        PPU_RegWrite(&emu->ppu, addr, data);
    } else if (addr == 0x4014) {
        //OAM DMA
        Write4014(emu, addr, data);
    } else if (addr <= 0x4015 || addr == 0x4017) {
        //APU registers
        APU_Write(&emu->apu, addr, data);
    } else if (addr == 0x4016) {
        //Controller strobe
        Write4016(emu, addr, data);
    } else if (addr >= 0x4020) {
        //Cartridge
        Mapper_CPUWrite(emu->mapper, addr, data);
    }

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);

    CPU_SetNMISignal(&emu->cpu, PPU_NMISignal(&emu->ppu));
    CPU_SetIRQSignal(&emu->cpu, APU_IRQSignal(&emu->apu));
}

void OnCPUHalt(void *emulator, CPU *cpu, uint16_t nextAddr) {
    Emulator* emu = (Emulator*)emulator;
    DMA_Process(&emu->dma, &emu->cpu, &emu->apu, nextAddr);
}

uint8_t OnPPURead(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;
    return Mapper_PPURead(emu->mapper, addr);
}

void OnPPUWrite(void* emulator, uint16_t addr, uint8_t data) {
    Emulator* emu = (Emulator*)emulator;
    Mapper_PPUWrite(emu->mapper, addr, data);
}

void OnDMCDMA(void *emulator, uint16_t addr) {
    Emulator *emu = (Emulator*)emulator;
    DMA_ScheduleDMCDMA(&emu->dma, &emu->cpu, addr);
}


/* FUNCTION DEFINITIONS */

Emulator *Emu_Create()
{
    Emulator* emu = malloc(sizeof(Emulator));
    memset(emu, 0, sizeof(Emulator));

    //Console component initialization

    CPU_Init(&emu->cpu, (CPUCallbacks){
        .context = emu,
        .onread = &OnCPURead,
        .onwrite = &OnCPUWrite,
        //.onpeek = &OnCPUPeek,
        .onhalt = &OnCPUHalt
    });

    PPU_Init(&emu->ppu, &OnPPURead, &OnPPUWrite, emu);

    APU_Init(&emu->apu, (APUCallbacks){
        .context = emu,
        .ondma = &OnDMCDMA,
    }, NTSC_CPU_CLOCK, 44100);
    
    StdController_Init(&emu->controller);

    return emu;
}

void Emu_Free(Emulator *emu)
{
    Emu_CloseROM(emu);
    free(emu);
}

void Emu_SetSavePath(Emulator *emu, const char *filepath)
{
    strncpy(emu->save_dir, filepath, sizeof(emu->save_dir));
    emu->save_dir[sizeof(emu->save_dir) - 1] = '\0';
}

int Emu_LoadROM(Emulator *emu, const char *filename)
{
    FILE* rom_file = fopen(filename, "rb");
    if (rom_file == NULL) {
        perror("Error opening ROM file");
        return -1;
    }
    INESHeader* ines = &emu->rom_ines;
    if (INES_ReadHeader(ines, rom_file) != 0) {
        fprintf(stderr, "Error opening ROM file: Invalid iNES ROM file format.\n");
        fclose(rom_file);
        return -1;
    }

    //Check PRG and CHR ROM
    if (ines->prg_units == 0) {
        fprintf(stderr, "Error: ROM has no PRG ROM.\n");
        fclose(rom_file);
        return -1;
    }
    
    //Check and initialize mapper
    if ((emu->mapper = Mapper_New(ines->mapper)) == NULL) {
        printf("Error: ROM mapper #%u is not supported by this emulator.\n", ines->mapper);
        fclose(rom_file);
        Emu_CloseROM(emu);
        return -1;
    }
    Mapper_Init(emu->mapper, ines, rom_file);

    //Load PRG RAM save if there is one
    if (ines->has_battery_saves) {
        if (emu->save_dir[0] == '\0') {
            printf("Battery save path is not set, cannot load or save battery saves.\n");
        } else {
            //Build save path: save dir + ROM name + ".sav"
            strncpy(emu->save_path, emu->save_dir, sizeof(emu->save_path));

            const char *rom_name = strrchr(filename, '/');
            const char *rom_name_backslash = strrchr(filename, '\\');
            if (rom_name_backslash > rom_name)
                rom_name = rom_name_backslash;
            if (rom_name == NULL) {
                rom_name = filename;
            }
            else
                rom_name++;
            strncat(emu->save_path, rom_name, sizeof(emu->save_path) - 1);
            
            char *extension = strrchr(emu->save_path, '.');
            if (extension != NULL)
                *extension = '\0';
            strncat(emu->save_path, ".sav", sizeof(emu->save_path));

            //Open save file
            emu->save_file = fopen(emu->save_path, "ab+");
            if (!emu->save_file) {
                fprintf(stderr, "Error opening save file ");
                perror(emu->save_path);
            } else {
                if (fseek(emu->save_file, 0, SEEK_SET) != 0) {
                    perror("Error seeking to beginning of PRG RAM save file");
                }
                Mapper_LoadPRGRAM(emu->mapper, emu->save_file);
            }
        }
    }

    fclose(rom_file);
    
    emu->is_rom_loaded = 1;

    //Power on system
    Emu_PowerOn(emu);

    return 0;
}

void Emu_CloseROM(Emulator *emu)
{
    if (emu->save_file) {
        fclose(emu->save_file);
        emu->save_file = fopen(emu->save_path, "wb");
        if (!emu->save_file) {
            printf("Error reopening save file ");
            perror(emu->save_path);
        } else {
            Mapper_SavePRGRAM(emu->mapper, emu->save_file);
            fclose(emu->save_file);
            emu->save_file = NULL;
        }
    }
    if (emu->mapper) {
        Mapper_Free(emu->mapper);
        emu->mapper = NULL;
    }
    emu->is_rom_loaded = 0;
}

int Emu_IsROMLoaded(Emulator *emu)
{
    return emu->is_rom_loaded;
}

void Emu_PowerOn(Emulator *emu)
{
    PPU_PowerOn(&emu->ppu);
    APU_PowerOn(&emu->apu);
    CPU_PowerOn(&emu->cpu);
}

int Emu_RunFrame(Emulator *emu)
{
    //Execute instructions until a full frame is rendered
    unsigned long long frame = emu->ppu.state.frames;
    while (emu->ppu.state.frames == frame) {
        if (CPU_Exec(&emu->cpu) != 0) {
            printf("Error: CPU crashed.\n");
            return -1;
        }
    }
    return 0;
}

void Emu_PressButton(Emulator *emu, ControllerButton button)
{
    StdController_PressButton(&emu->controller, button);
}

void Emu_ReleaseButton(Emulator *emu, ControllerButton button)
{
    StdController_ReleaseButton(&emu->controller, button);
}

double Emu_GetAudioChannelVolume(Emulator* emu, APU_Channel channel) { return APU_GetChannelVolume(&emu->apu, channel); }

void Emu_SetAudioChannelVolume(Emulator* emu, APU_Channel channel, double volume) { APU_SetChannelVolume(&emu->apu, channel, volume); }

bool Emu_GetAudioChannelMute(Emulator* emu, APU_Channel channel) { return APU_GetChannelMute(&emu->apu, channel); }

void Emu_SetAudioChannelMute(Emulator* emu, APU_Channel channel, bool mute) { APU_SetChannelMute(&emu->apu, channel, mute); }

RGBAPixel *Emu_GetPixelBuffer(Emulator *emu, int *width, int *height)
{
    *width = NES_SCREEN_W;
    *height = NES_SCREEN_H;
    return &emu->ppu.pixelBuffer[0][0];
}

void *Emu_GetAudioBuffer(Emulator *emu, size_t *len)
{
    return APU_GetAudioBuffer(&emu->apu, len);
}

void Emu_ClearAudioBuffer(Emulator *emu)
{
    APU_ClearAudioBuffer(&emu->apu);
}
