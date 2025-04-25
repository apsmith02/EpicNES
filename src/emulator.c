#include "emulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* PRIVATE FUNCTIONS */

void TriggerDebugPause(Emulator* emu);


void MapNTHorizontal(Emulator* emu) {
    Mem_PPUMapPages(&emu->memory, 0x20, 0x23, MEM_VRAM, 0x00);
    Mem_PPUMapPages(&emu->memory, 0x24, 0x27, MEM_VRAM, 0x00);
    Mem_PPUMapPages(&emu->memory, 0x28, 0x2B, MEM_VRAM, 0x04);
    Mem_PPUMapPages(&emu->memory, 0x2C, 0x2F, MEM_VRAM, 0x04);
}

void MapNTVertical(Emulator* emu) {
    Mem_PPUMapPages(&emu->memory, 0x20, 0x23, MEM_VRAM, 0x00);
    Mem_PPUMapPages(&emu->memory, 0x24, 0x27, MEM_VRAM, 0x04);
    Mem_PPUMapPages(&emu->memory, 0x28, 0x2B, MEM_VRAM, 0x00);
    Mem_PPUMapPages(&emu->memory, 0x2C, 0x2F, MEM_VRAM, 0x04);
}

uint8_t OnCPURead(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;
    Memory* memory = &emu->memory;
    Mapper_Base* mapper = emu->mapper;

    //Debug: Break on step or breakpoint hit, flag opcodes on opcode fetch
    if (emu->debug_enable) {
        if (emu->cpu.instr_cycle == 1) {
            //Opcode fetch cycle: Flag memory as opcode
            MemoryType memtype;
            unsigned memaddr;
            Mem_CPUToMemAddr(memory, addr, &memtype, &memaddr);
            Mem_FlagOpcode(memory, memtype, memaddr);
        }
        //Break on...
        if (emu->debug_step == DEBUG_STEP_CPUCYCLE ||                           //Step CPU cycle
        (emu->cpu.instr_cycle == 1 && (emu->debug_step == DEBUG_STEP_INTO)) ||  //Step instruction
        Mem_TestCPUBreakpoint(memory, addr, emu->cpu.access_type)               //Breakpoint hit
        ) {
            TriggerDebugPause(emu);
        }
    }

    bool iNmi = PPU_NMISignal(&emu->ppu);

    uint8_t data = mapper->vtable->cpuRead(mapper, emu, addr);

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    
    if (iNmi && !PPU_NMISignal(&emu->ppu))
        CPU_NMI(&emu->cpu);
    return data;
}

void OnCPUWrite(void* emulator, uint16_t addr, uint8_t data) {
    Emulator* emu = (Emulator*)emulator;
    Memory* memory = &emu->memory;
    Mapper_Base* mapper = emu->mapper;

    //Debug: Break on step or breakpoint hit
    if (emu->debug_enable) {
        //Break on...
        if (emu->debug_step == DEBUG_STEP_CPUCYCLE ||                           //Step CPU cycle
        Mem_TestCPUBreakpoint(memory, addr, emu->cpu.access_type)               //Breakpoint hit
        ) {
            TriggerDebugPause(emu);
        }
    }

    bool iNmi = PPU_NMISignal(&emu->ppu);
    
    mapper->vtable->cpuWrite(mapper, emu, addr, data);

    APU_CPUCycle(&emu->apu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);
    PPU_Cycle(&emu->ppu);

    if (iNmi && !PPU_NMISignal(&emu->ppu))
        CPU_NMI(&emu->cpu);
}

uint8_t OnCPUPeek(void* emulator, uint16_t addr) {
    Emulator* emu = (Emulator*)emulator;
    return Mem_CPUPeek(&emu->memory, addr);
}

void OnCPUDMCLoad(Emulator* emu, uint8_t sampleData) {
    APU_DMCLoadSample(&emu->apu, sampleData);
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
    CPU_SetPeekFn(&emu->cpu, &OnCPUPeek);
    CPU_SetDMCLoadFn(&emu->cpu, (CPU_DMCLoadFn)&OnCPUDMCLoad);

    PPU_Init(&emu->ppu, &OnPPURead, &OnPPUWrite, emu);

    APU_Init(&emu->apu, &CPU_DMCDMA, &emu->cpu, NTSC_CPU_CLOCK, 44100);

    StdController_Init(&emu->controller);

    Vec_BP_Init(&emu->breakpoints);

    Memory* memory = &emu->memory;
    Mem_Create_RAM(memory);

    //RAM: $0000-$07FF (with mirrors at $0800-$1FFF)
    Mem_CPUMapPages(memory, 0x00, 0x07, MEM_RAM, 0x00);
    Mem_CPUMapPages(memory, 0x08, 0x0F, MEM_RAM, 0x00);
    Mem_CPUMapPages(memory, 0x10, 0x17, MEM_RAM, 0x00);
    Mem_CPUMapPages(memory, 0x18, 0x1F, MEM_RAM, 0x00);

    //PPU: $2000-$3FFF
    Mem_CPUMapDevice(memory, 0x2000, 0x3FFF, &emu->ppu, (CPUReadFn)&PPU_RegRead, (CPUWriteFn)&PPU_RegWrite);

    //APU
    Mem_CPUMapReadDevice(memory, 0x4015, 0x4015, &emu->apu, (CPUReadFn)&APU_Read);
    Mem_CPUMapWriteDevice(memory, 0x4000, 0x4013, &emu->apu, (CPUWriteFn)&APU_Write);
    Mem_CPUMapWriteDevice(memory, 0x4015, 0x4015, &emu->apu, (CPUWriteFn)&APU_Write);
    Mem_CPUMapWriteDevice(memory, 0x4017, 0x4017, &emu->apu, (CPUWriteFn)&APU_Write);

    //Controller Strobe: Write $4016
    Mem_CPUMapWriteDevice(memory, 0x4016, 0x4016, emu, (CPUWriteFn)&Write4016);
    
    //Controller output: Read $4016-$4017
    Mem_CPUMapReadDevice(memory, 0x4016, 0x4016, &emu->controller, (CPUReadFn)&StdController_Read);

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
    if (ines->prg_units == 0) {
        fprintf(stderr, "Error: ROM has no PRG ROM.\n");
        return -1;
    }
    Mem_Load_PRG_ROM(memory, ines, rom_file);
    if (ines->chr_units == 0) {
        fprintf(stderr, "Error: ROM has no CHR ROM.\n");
        return -1;
    }
    Mem_Load_CHR_ROM(memory, ines, rom_file);
    fclose(rom_file);

    const Mapper_Vtable* mapper_vtbl;
    if (ines->mapper >= NUM_MAPPERS || (mapper_vtbl = MAPPER_VTABLES[ines->mapper]) == NULL) {
        fprintf(stderr, "Error: This ROM uses mapper %u, which is not supported by this emulator.\n", ines->mapper);
        return -1;
    }
    emu->mapper = Mapper_CreateFromVtable(mapper_vtbl, emu);
    
    /*
    //Check mapper
    if (ines->mapper == 0) { //NROM
        //Create VRAM
        Mem_Create_VRAM(memory, 0x800);

        //Map PRG ROM
        Mem_CPUMapPages(memory, 0x80, 0xBF, MEM_PRG_ROM, 0);
        Mem_CPUMapPages(memory, 0xC0, 0xFF, MEM_PRG_ROM, 0x40 % (ines->prg_bytes >> 8));

        //Map CHR ROM
        Mem_PPUMapPages(memory, 0x00, 0x1F, MEM_CHR_ROM, 0);

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
    */
    
    emu->is_rom_loaded = 1;

    //Power on system
    Emu_PowerOn(emu);

    return 0;
}

void Emu_CloseROM(Emulator *emu)
{
    emu->is_rom_loaded = 0;
    Mapper_Destroy(emu->mapper, emu);
    emu->mapper = NULL;
}

int Emu_IsROMLoaded(Emulator *emu)
{
    return emu->is_rom_loaded;
}

void Emu_PowerOn(Emulator *emu)
{
    if (!emu->in_cpu_exec) {
        PPU_PowerOn(&emu->ppu);
        APU_PowerOn(&emu->apu);
        CPU_PowerOn(&emu->cpu);

        emu->power_on_queued = false;
        if (emu->debug_enable && emu->debug_break_on_reset)
            emu->debug_step = DEBUG_STEP_INTO;
    } else {
        emu->power_on_queued = true;
    }
}

int Emu_RunFrame(Emulator *emu)
{
    //Execute instructions until a full frame is rendered
    unsigned long long frame = emu->ppu.state.frames;
    while (emu->ppu.state.frames == frame) {
        emu->in_cpu_exec = true;

        if (CPU_Exec(&emu->cpu) != 0) {
            printf("Error: CPU crashed.\n");
            return -1;
        }

        emu->in_cpu_exec = false;
        if (emu->power_on_queued) {
            Emu_PowerOn(emu);
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

void Emu_DebugSetBreakOnReset(Emulator *emu, bool enabled)
{
    emu->debug_break_on_reset = enabled;
}

bool Emu_DebugGetBreakOnReset(Emulator *emu)
{
    return emu->debug_break_on_reset;
}

void Emu_DebugPause(Emulator *emu)
{
    emu->debug_step = DEBUG_STEP_INTO;
}

void Emu_DebugSetCodeBreakpoint(Emulator *emu, unsigned addr)
{
    Memory* memory = &emu->memory;

    Breakpoint bp;
    Mem_CPUToMemAddr(memory, addr, &bp.type, &bp.start);
    bp.end = bp.start;
    bp.access = MEMDEBUG_BREAK_X;

    Mem_SetBreakFlags(memory, bp.type, bp.start, bp.end, bp.access);
    Vec_BP_Add(&emu->breakpoints, bp);
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
        .access = break_on_flags,
        .type = type
    };
    Mem_SetBreakFlags(&emu->memory, bp.type, bp.start, bp.end, bp.access);
    Vec_BP_Add(&emu->breakpoints, bp);
}

void Emu_DebugDeleteBreakpoint(Emulator *emu, size_t index)
{
    Memory* memory = &emu->memory;
    Vec_Breakpoint* bplist = &emu->breakpoints;
    Breakpoint* bp = Vec_BP_At(bplist, index);

    //Clear this breakpoint's debug flags, remove from list
    Mem_ClearBreakFlags(memory, bp->type, bp->start, bp->end);
    Vec_BP_Remove(bplist, index);
    //Reapply flags from other breakpoints in list
    for (size_t i = 0; i < bplist->size; i++) {
        bp = Vec_BP_At(bplist, i);
        Mem_SetBreakFlags(memory, bp->type, bp->start, bp->end, bp->access);
    }
}

void Emu_DebugClearBreakpoints(Emulator *emu)
{
    Memory* memory = &emu->memory;
    Vec_Breakpoint* bplist = &emu->breakpoints;
    
    for (size_t i = 0; i < bplist->size; i++) {
        Breakpoint* bp = Vec_BP_At(bplist, i);
        Mem_ClearBreakFlags(memory, bp->type, bp->start, bp->end);
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
    return (Mem_TestCPUDebugFlags(&emu->memory, addr) & MEMDEBUG_BREAK_X) > 0;
}

int Emu_DebugDisassemble(Emulator *emu, uint16_t address, char *buffer, size_t n)
{
    return CPU_Disassemble(&emu->cpu, address, buffer, n);
}

int Emu_DebugIterLastInstruction(Emulator *emu, uint16_t *address)
{
    Memory* memory = &emu->memory;
    
    for (int addr = *address - 1; addr >= 0; addr--) {
        if (Mem_CPUHasOpcode(memory, addr)) {
            *address = addr;
            return 0;
        }
    }
    return -1;
}

int Emu_DebugIterNextInstruction(Emulator *emu, uint16_t *address)
{
    Memory* memory = &emu->memory;
    
    for (int addr = *address + 1; addr < CPU_ADDR_SPACE; addr++) {
        if (Mem_CPUHasOpcode(memory, addr)) {
            *address = addr;
            return 0;
        }
    }
    return -1;
}

bool Emu_DebugIsOpcode(Emulator *emu, uint16_t address)
{
    return Mem_CPUHasOpcode(&emu->memory, address);
}

int Emu_DebugIterPRGMirrors(Emulator* emu, MemoryType type, int physical_addr, int *cpu_addr)
{
    if (*cpu_addr == -1) {
        uint16_t addr;
        int ret = Mem_BeginMemToCPUAddr(&emu->memory, type, physical_addr, &addr);
        if (ret == 0) {
            *cpu_addr = addr;
        }
        return ret;
    }

    uint16_t addr = *cpu_addr;
    int ret = Mem_IterMemToCPUAddr(&emu->memory, type, physical_addr, &addr);
    *cpu_addr = addr;
    return ret;
}
