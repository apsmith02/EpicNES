#include "emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* PRIVATE FUNCTIONS */


void MapNTHorizontal(Emulator* emu) {
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x000, 0x20, 0x23);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x000, 0x24, 0x27);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x400, 0x28, 0x2B);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x400, 0x2C, 0x2F);
}

void MapNTVertical(Emulator* emu) {
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x000, 0x20, 0x23);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x400, 0x24, 0x27);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x000, 0x28, 0x2B);
    Mem_MapCHRPages(&emu->memory, MEM_VRAM, 0x400, 0x2C, 0x2F);
}

uint8_t OnCPURead(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;

    //Debug: Break on step CPU cycle, break on read breakpoint
    if (emu->debug_enable) {
        if (emu->debug_step == DEBUG_STEP_CPUCYCLE) {
            TriggerDebugPause(emu);
        }
        TestBreakpoint(emu, addr, DEBUG_BREAK_ON_R);
    }
    

    bool iNmi = PPU_NMISignal(&emu->ppu);

    uint8_t val = Mem_CPURead(&emu->memory, addr);

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);

    if (iNmi && !PPU_NMISignal(&emu->ppu))
        CPU_NMI(&emu->cpu);
    return val;
}

void OnCPUWrite(void* emulator, uint16_t addr, uint8_t data) {
    Emulator* emu = (Emulator*)emulator;

    bool iNmi = PPU_NMISignal(&emu->ppu);

    Mem_CPUWrite(&emu->memory, addr, data);

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);

    if (iNmi && !PPU_NMISignal(&emu->ppu))
        CPU_NMI(&emu->cpu);
}

uint8_t CPUPeek(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;
    return Mem_CPUPeek(&emu->memory, addr);
}

uint8_t OnPPURead(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;
    return Mem_PPURead(&emu->memory, addr);
}

void OnPPUWrite(void* emulator, uint16_t addr, uint8_t data) {
    Emulator* emu = (Emulator*)emulator;
    Mem_PPUWrite(&emu->memory, addr, data);
}

//Write $4016: Controller strobe
void Write4016(Emulator* emu, uint16_t addr, uint8_t data) {
    //Write to both controller ports (currently only 1 standard controller)
    StdController_Write(&emu->controller, addr, data);
}

void TriggerDebugPause(Emulator* emu) {
    emu->debug_step = emu->debug_pause_callback(emu->debug_pause_userdata);
}

//Triggers a debug pause if the memory at addr has a breakpoint of access type break_on
void TestBreakpoint(Emulator* emu, uint16_t addr, DebugBreakOn break_on) {
    Memory* memory = &emu->memory;
    uint8_t cpuDebugFlags = *Mem_CPUDebugFlagsAt(memory, addr);
    uint8_t prgDebugFlags = *Mem_PRGDebugFlagsAt(memory, addr);

    if (cpuDebugFlags & break_on)
        TriggerDebugPause(emu);
    if (prgDebugFlags & break_on)
        TriggerDebugPause(emu);
}

void SetBPDebugFlags(Memory* memory, Breakpoint* bp) {
    for(unsigned addr = bp->start; addr <= bp->end; addr++) {
        *Mem_DebugFlagsAtType(memory, bp->type, addr) |= (bp->rwx & (DEBUG_BREAK_ON_MASK));
    }
}

void ClearBPDebugFlags(Memory* memory, Breakpoint* bp) {
    for (unsigned addr = bp->start; addr <= bp->end; addr++) {
        *Mem_DebugFlagsAtPhysical(memory, bp->type, addr) &= ~(DEBUG_BREAK_ON_MASK);
    }
}


/* FUNCTION DEFINITIONS */

void Vec_BP_Init(Vec_Breakpoint *vec)
{
    vec->capacity = 16;
    vec->size = 0;
    vec->arr = malloc(vec->capacity * sizeof(Breakpoint));
}

void Vec_BP_Free(Vec_Breakpoint *vec)
{
    free(vec->arr);
}

void Vec_BP_Add(Vec_Breakpoint *vec, Breakpoint bp)
{
    vec->arr[vec->size++] = bp;
    if (vec->size == vec->capacity) {
        vec->capacity *= 2;
        vec->arr = realloc(vec->arr, vec->capacity);
    }
}

void Vec_BP_Remove(Vec_Breakpoint *vec, size_t index)
{
    for (size_t i = index + 1; i < vec->size; i++) {
        vec->arr[i - 1] = vec->arr[i];
    }
    vec->size--;
}

void Vec_BP_Clear(Vec_Breakpoint *vec)
{
    vec->size = 0;
}

Breakpoint *Vec_BP_At(Vec_Breakpoint *vec, size_t index)
{
    assert(index < vec->size);
    return &vec->arr[index];
}


Emulator *Emu_Create()
{
    Emulator* emu = malloc(sizeof(Emulator));
    memset(emu, 0, sizeof(Emulator));
    Mem_Init(&emu->memory);
    CPU_Init(&emu->cpu, &OnCPURead, &OnCPUWrite, emu);
    CPU_SetPeekFn(&emu->cpu, &CPUPeek);
    PPU_Init(&emu->ppu, &OnPPURead, &OnPPUWrite, emu);
    APU_Init(&emu->apu, NTSC_CPU_CLOCK, 44100);
    StdController_Init(&emu->controller);
    Vec_BP_Init(&emu->breakpoints);

    Memory* memory = &emu->memory;
    Mem_Create_RAM(memory);

    //RAM: $0000-$07FF (with mirrors at $0800-$1FFF)
    Mem_MapPRGPages(memory, MEM_RAM, 0x000, 0x00, 0x07);
    Mem_MapPRGPages(memory, MEM_RAM, 0x000, 0x08, 0x0F);
    Mem_MapPRGPages(memory, MEM_RAM, 0x000, 0x10, 0x17);
    Mem_MapPRGPages(memory, MEM_RAM, 0x000, 0x18, 0x1F);
    /*
    MapPRGPages(emu, &emu->ram[0], 0x00, 0x07, true, DEBUG_MEMTYPE_RAM);
    MapPRGPages(emu, &emu->ram[0], 0x08, 0x0F, true, DEBUG_MEMTYPE_RAM);
    MapPRGPages(emu, &emu->ram[0], 0x10, 0x17, true, DEBUG_MEMTYPE_RAM);
    MapPRGPages(emu, &emu->ram[0], 0x18, 0x1F, true, DEBUG_MEMTYPE_RAM);
    */

    //PPU: $2000-$3FFF
    Mem_MapCPUReadDevice(memory, &emu->ppu, (CPUReadFn)&PPU_RegRead, 0x2000, 0x3FFF);
    Mem_MapCPUWriteDevice(memory, &emu->ppu, (CPUWriteFn)&PPU_RegWrite, 0x2000, 0x3FFF);
    /*
    MapCPUReadDevice(emu, &emu->ppu, (CPUReadFn)&PPU_RegRead, 0x2000, 0x3FFF);
    MapCPUWriteDevice(emu, &emu->ppu, (CPUWriteFn)&PPU_RegWrite, 0x2000, 0x3FFF);
    */

    //APU
    Mem_MapCPUReadDevice(memory, &emu->apu, (CPUReadFn)&APU_Read, 0x4015, 0x4015);
    Mem_MapCPUWriteDevice(memory, &emu->apu, (CPUWriteFn)&APU_Write, 0x4000, 0x4013);
    Mem_MapCPUWriteDevice(memory, &emu->apu, (CPUWriteFn)&APU_Write, 0x4015, 0x4015);
    Mem_MapCPUWriteDevice(memory, &emu->apu, (CPUWriteFn)&APU_Write, 0x4017, 0x4017);
    /*
    MapCPUReadDevice(emu, &emu->apu, (CPUReadFn)&APU_Read, 0x4015, 0x4015);
    MapCPUWriteDevice(emu, &emu->apu, (CPUWriteFn)&APU_Write, 0x4000, 0x4013);
    MapCPUWriteDevice(emu, &emu->apu, (CPUWriteFn)&APU_Write, 0x4015, 0x4015);
    MapCPUWriteDevice(emu, &emu->apu, (CPUWriteFn)&APU_Write, 0x4017, 0x4017);
    */

    //Controller Strobe: Write $4016
    Mem_MapCPUWriteDevice(memory, emu, (CPUWriteFn)&Write4016, 0x4016, 0x4016);
    //MapCPUWriteDevice(emu, emu, (CPUWriteFn)&Write4016, 0x4016, 0x4016);

    //Controller output: Read $4016-$4017
    Mem_MapCPUReadDevice(memory, &emu->controller, (CPUReadFn)&StdController_Read, 0x4016, 0x4016);
    //MapCPUReadDevice(emu, &emu->controller, (CPUReadFn)&StdController_Read, 0x4016, 0x4016);

    return emu;
}

void Emu_Free(Emulator *emu)
{
    Emu_CloseROM(emu);
    Mem_Free(&emu->memory);
    free(emu);
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

    Memory* memory = &emu->memory;
    //Load PRG and CHR ROM
    Mem_Load_PRG_ROM(memory, ines, rom_file);
    Mem_Load_CHR_ROM(memory, ines, rom_file);
    fclose(rom_file);

    //Check mapper
    if (ines->mapper == 0) { //NROM
        if (ines->prg_units == 0) {
            fprintf(stderr, "Error: ROM has no PRG ROM.\n");
            return -1;
        }
        if (ines->chr_units == 0) {
            fprintf(stderr, "Error: ROM has no CHR ROM.\n");
            return -1;
        }

        //Create VRAM
        Mem_Create_VRAM(memory, 0x800);

        //Map PRG ROM
        Mem_MapPRGPages(memory, MEM_PRG_ROM, 0, 0x80, 0xBF);
        Mem_MapPRGPages(memory, MEM_PRG_ROM, 0x4000 % ines->prg_bytes, 0xC0, 0xFF);
        /*
        MapPRGPages(emu, &rom->prg_rom[0], 0x80, 0xBF, false, DEBUG_MEMTYPE_PRGROM);
        MapPRGPages(emu, &rom->prg_rom[0x4000 % rom->prg_bytes], 0xC0, 0xFF, false, DEBUG_MEMTYPE_PRGROM);
        */
        //Map CHR ROM
        Mem_MapCHRPages(memory, MEM_CHR_ROM, 0, 0x00, 0x1F);
        //MapCHRPages(emu, rom->chr_rom, 0x00, 0x1F, false);

        //NT Mirroring
        if (ines->nt_mirroring == 0)
            MapNTHorizontal(emu);
        else
            MapNTVertical(emu);
    } else {
        printf("Error: ROM mapper %d is not supported by this emulator.\n", ines->mapper);
        Emu_CloseROM(emu);
        return -1;
    }
    
    emu->is_rom_loaded = 1;

    //Power on system
    PPU_PowerOn(&emu->ppu);
    APU_PowerOn(&emu->apu);
    CPU_PowerOn(&emu->cpu);

    return 0;
}

void Emu_CloseROM(Emulator *emu)
{
    emu->is_rom_loaded = 0;
}

int Emu_IsROMLoaded(Emulator *emu)
{
    return emu->is_rom_loaded;
}

int Emu_RunFrame(Emulator *emu)
{
    //Execute instructions until a full frame is rendered
    unsigned long long frame = emu->ppu.state.frames;
    while (emu->ppu.state.frames == frame) {
        //Debug: Flag instruction byte as code, break on step into, break on x breakpoint
        if (emu->debug_enable) {
            Memory* memory = &emu->memory;
            uint16_t pc = emu->cpu.state.pc;

            *Mem_PRGDebugFlagsAt(memory, pc) |= MEMDEBUG_IS_OPCODE;
            if (emu->debug_step == DEBUG_STEP_INTO) {
                TriggerDebugPause(emu);
            }
            TestBreakpoint(emu, pc, DEBUG_BREAK_ON_X);
        }

        //Execute CPU instruction
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

void Emu_DebugEnable(Emulator *emu, bool enabled)
{
    emu->debug_enable = enabled;
}

void Emu_SetDebugPauseCallback(Emulator *emu, DebugPauseCallback callback, void *userdata)
{
    emu->debug_pause_callback = callback;
    emu->debug_pause_userdata = userdata;
}

void Emu_DebugPause(Emulator *emu)
{
    emu->debug_step = DEBUG_STEP_INTO;
}

void Emu_DebugSetCodeBreakpoint(Emulator *emu, unsigned addr)
{
    Memory* memory = &emu->memory;

    uint8_t* debug_flags = Mem_PRGDebugFlagsAt(memory, addr);
    if (debug_flags != NULL) {
        *debug_flags |= DEBUG_BREAK_ON_X;
        Breakpoint bp = {
            .start = Mem_GetPRGMemPhysAddr(memory, addr),
            .end = bp.start,
            .type = Mem_GetPRGMemType(memory, addr),
            .rwx = DEBUG_BREAK_ON_X
        };
        Vec_BP_Add(&emu->breakpoints, bp);
    }
}

void Emu_DebugSetBreakpoint(Emulator *emu, MemoryType type, int break_on_flags, unsigned addr)
{
    Emu_DebugSetBreakpointRange(emu, type, break_on_flags, addr, addr);
}

void Emu_DebugSetBreakpointRange(Emulator *emu, MemoryType type, int break_on_flags, unsigned start_addr, unsigned end_addr)
{
    Breakpoint bp = {
        .start = start_addr,
        .end = end_addr,
        .rwx = break_on_flags,
        .type = type
    };
    SetBPDebugFlags(&emu->memory, &bp);
    Vec_BP_Add(&emu->breakpoints, bp);
}

void Emu_DebugDeleteBreakpoint(Emulator *emu, size_t index)
{
    Memory* memory = &emu->memory;
    Vec_Breakpoint* bplist = &emu->breakpoints;
    Breakpoint* bp = Vec_BP_At(bplist, index);

    //Clear this breakpoint's debug flags, remove from list
    ClearBPDebugFlags(memory, bp);
    Vec_BP_Remove(bplist, index);
    //Reapply flags from other breakpoints in list
    for (size_t i = 0; i < bplist->size; i++) {
        SetBPDebugFlags(memory, Vec_BP_At(bplist, i));
    }
}

void Emu_DebugClearBreakpoints(Emulator *emu)
{
    Memory* memory = &emu->memory;
    Vec_Breakpoint* bplist = &emu->breakpoints;
    
    for (size_t i = 0; i < bplist->size; i++) {
        ClearBPDebugFlags(memory, Vec_BP_At(bplist, i));
    }
    Vec_BP_Clear(bplist);
}

size_t Emu_DebugGetBreakpointCount(Emulator *emu)
{
    return emu->breakpoints.size;
}

Breakpoint Emu_DebugGetBreakpoint(Emulator *emu, size_t index)
{
    return *Vec_BP_At(&emu->breakpoints, index);
}

bool Emu_DebugAddrHasCodeBreakpoint(Emulator *emu, uint16_t addr)
{
    uint8_t* debug_flags = Mem_PRGDebugFlagsAt(&emu->memory, addr);
    return (debug_flags != NULL && (*debug_flags & MEMDEBUG_BREAK_X) > 0);
}

int Emu_DebugDisassemble(Emulator *emu, uint16_t address, char *buffer, size_t n)
{
    return CPU_Disassemble(&emu->cpu, address, buffer, n);
}

int Emu_DebugIterLastInstruction(Emulator *emu, uint16_t *address)
{
    Memory* memory = &emu->memory;
    int addr = *address;
    uint8_t* debug_flags;
    
    do {
        addr--;
    } while (addr >= 0 && ((debug_flags = Mem_PRGDebugFlagsAt(memory, addr)) == NULL || (*debug_flags & MEMDEBUG_IS_OPCODE) == 0));

    if (addr <= -1)
        return -1;
    *address = addr;
    return 0;
}

int Emu_DebugIterNextInstruction(Emulator *emu, uint16_t *address)
{
    Memory* memory = &emu->memory;
    int addr = *address;
    uint8_t* debug_flags;

    do {
        addr++;
    } while (addr < CPU_ADDR_SPACE && ((debug_flags = Mem_PRGDebugFlagsAt(memory, addr)) == NULL || (*debug_flags & MEMDEBUG_IS_OPCODE) == 0));

    if (addr >= CPU_ADDR_SPACE)
        return -1;
    *address = addr;
    return 0;
}

bool Emu_DebugIsOpcode(Emulator *emu, uint16_t address)
{
    Memory* memory = &emu->memory;
    uint8_t* debug_flags = Mem_PRGDebugFlagsAt(memory, address);
    return debug_flags != NULL && (*debug_flags & MEMDEBUG_IS_OPCODE) > 0;
}

int Emu_DebugIterPRGMirrors(Emulator* emu, MemoryType type, int physical_addr, int *cpu_addr)
{
    return Mem_IterPRGMirrors(&emu->memory, type, physical_addr, cpu_addr);
}
