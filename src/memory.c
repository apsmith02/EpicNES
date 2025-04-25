#include "memory.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>

void _Mem_Move(Memory* mem, MemoryType type, uint8_t* memory, unsigned size) {
    if (mem->memory[type] != NULL) {
        free(mem->memory[type]);
    }
    if (mem->memory_debug_flags[type] != NULL) {
        free(mem->memory_debug_flags[type]);
    }

    uint8_t* debug_flags = (uint8_t*)malloc(size);
    memset(debug_flags, 0, size);

    mem->memory_size[type] = size;
    mem->memory[type] = memory;
    mem->memory_debug_flags[type] = debug_flags;

}

void _Mem_Create(Memory* mem, MemoryType type, unsigned size) {
    uint8_t* memory = (uint8_t*)malloc(size);
    memset(memory, 0, size);
    _Mem_Move(mem, type, memory, size);
}



void Mem_Init(Memory *mem)
{
    memset(mem, 0, sizeof(Memory));
    memset(&mem->cpu_page_type[0], MEM_NONE, sizeof(mem->cpu_page_type));
    memset(&mem->ppu_page_type[0], MEM_NONE, sizeof(mem->ppu_page_type));
}

void Mem_Free(Memory *mem)
{
    for (int i = 0; i < MEM_NUM_TYPES; i++) {
        free(mem->memory[i]);
        free(mem->memory_debug_flags[i]);
    }
}

void Mem_Create_RAM(Memory *mem)
{
    _Mem_Create(mem, MEM_RAM, 0x800);
}

void Mem_Create_VRAM(Memory *mem, unsigned size)
{
    _Mem_Create(mem, MEM_VRAM, size);
}

void Mem_Load_PRG_ROM(Memory *mem, INESHeader *ines, FILE *rom_file)
{
    uint8_t* prg_rom = (uint8_t*)INES_ReadPRG(ines, rom_file);
    _Mem_Move(mem, MEM_PRG_ROM, prg_rom, ines->prg_bytes);
}

void Mem_Load_CHR_ROM(Memory *mem, INESHeader *ines, FILE *rom_file)
{
    uint8_t* chr_rom = (uint8_t*)INES_ReadCHR(ines, rom_file);
    _Mem_Move(mem, MEM_CHR_ROM, chr_rom, ines->chr_bytes);
}

void Mem_CPUMapPages(Memory *memory, uint8_t cpuStartPg, uint8_t cpuEndPg, MemoryType memType, unsigned memStartPg)
{
    assert(memType >= 0 && memType < MEM_NUM_TYPES);
    assert(((memStartPg + (cpuEndPg - cpuStartPg + 1)) << 8) <= memory->memory_size[memType]);

    unsigned memAddr = memStartPg << 8;
    for (unsigned p = cpuStartPg; p <= cpuEndPg; p++, memAddr += 0x100) {
        memory->cpu_page_type[p] = memType;
        memory->cpu_page_addr[p] = memAddr;
    }
}

void Mem_CPUUnmapPages(Memory *memory, uint8_t cpuStartPg, uint8_t cpuEndPg)
{
    for  (unsigned p = cpuStartPg; p <= cpuEndPg; p++) {
        memory->cpu_page_type[p] = MEM_NONE;
        memory->cpu_page_addr[p] = 0;
    }
}

void Mem_PPUMapPages(Memory *memory, uint8_t ppuStartPg, uint8_t ppuEndPg, MemoryType memType, unsigned memStartPg)
{
    assert(ppuEndPg < PPU_PAGE_COUNT);
    assert(memType >= 0 && memType < MEM_NUM_TYPES);
    assert(((memStartPg + (ppuEndPg - ppuStartPg + 1)) << 8) <= memory->memory_size[memType]);

    unsigned memAddr = memStartPg << 8;
    for (unsigned p = ppuStartPg; p <= ppuEndPg; p++, memAddr += 0x100) {
        memory->ppu_page_type[p] = memType;
        memory->ppu_page_addr[p] = memAddr;
    }
}

void Mem_PPUUnmapPages(Memory *memory, uint8_t ppuStartPg, uint8_t ppuEndPg)
{
    for  (unsigned p = ppuStartPg; p <= ppuEndPg; p++) {
        memory->ppu_page_type[p] = MEM_NONE;
        memory->ppu_page_addr[p] = 0;
    }
}

void Mem_MapNTMirror(Memory *memory, NTMirrorType mirroring)
{
    switch (mirroring) {
        case NT_HORIZONTAL:
            Mem_PPUMapPages(memory, 0x20, 0x23, MEM_VRAM, 0x00);
            Mem_PPUMapPages(memory, 0x24, 0x27, MEM_VRAM, 0x00);
            Mem_PPUMapPages(memory, 0x28, 0x2B, MEM_VRAM, 0x04);
            Mem_PPUMapPages(memory, 0x2C, 0x2F, MEM_VRAM, 0x04);
            break;
        case NT_VERTICAL:
            Mem_PPUMapPages(memory, 0x20, 0x23, MEM_VRAM, 0x00);
            Mem_PPUMapPages(memory, 0x24, 0x27, MEM_VRAM, 0x04);
            Mem_PPUMapPages(memory, 0x28, 0x2B, MEM_VRAM, 0x00);
            Mem_PPUMapPages(memory, 0x2C, 0x2F, MEM_VRAM, 0x04);
            break;
    }
}

void Mem_CPUMapDevice(Memory *memory, uint16_t start, uint16_t end, void *device, CPUReadFn readfn, CPUWriteFn writefn)
{
    Mem_CPUMapReadDevice(memory, start, end, device, readfn);
    Mem_CPUMapWriteDevice(memory, start, end, device, writefn);
}

void Mem_CPUMapReadDevice(Memory *memory, uint16_t start, uint16_t end, void *device, CPUReadFn readfn)
{
    for (unsigned a = start; a <= end; a++) {
        memory->cpu_io_readdevice[a] = device;
        memory->cpu_io_readfn[a] = readfn;
    }
}

void Mem_CPUMapWriteDevice(Memory *memory, uint16_t start, uint16_t end, void *device, CPUWriteFn writefn)
{
    for (unsigned a = start; a <= end; a++) {
        memory->cpu_io_writedevice[a] = device;
        memory->cpu_io_writefn[a] = writefn;
    }
}

void Mem_CPUUnmapDevice(Memory *memory, uint16_t start, uint16_t end)
{
    Mem_CPUUnmapReadDevice(memory, start, end);
    Mem_CPUUnmapWriteDevice(memory, start, end);
}

void Mem_CPUUnmapReadDevice(Memory *memory, uint16_t start, uint16_t end)
{
    for (unsigned a = start; a <= end; a++) {
        memory->cpu_io_readdevice[a] = NULL;
        memory->cpu_io_readfn[a] = NULL;
    }
}

void Mem_CPUUnmapWriteDevice(Memory *memory, uint16_t start, uint16_t end)
{
    for (unsigned a = start; a <= end; a++) {
        memory->cpu_io_writedevice[a] = NULL;
        memory->cpu_io_writefn[a] = NULL;
    }
}

uint8_t Mem_CPURead(Memory *memory, uint16_t addr)
{
    void* device = memory->cpu_io_readdevice[addr];
    CPUReadFn readfn = memory->cpu_io_readfn[addr];
    if (device != NULL && readfn != NULL)
        return readfn(device, addr);

    uint8_t page = addr >> 8;
    MemoryType type = memory->cpu_page_type[page];
    if (type != MEM_NONE)
        return memory->memory[type][memory->cpu_page_addr[page] + (addr & 0xFF)];

    return 0;
}

void Mem_CPUWrite(Memory *memory, uint16_t addr, uint8_t data)
{
    void* device = memory->cpu_io_writedevice[addr];
    CPUWriteFn writefn = memory->cpu_io_writefn[addr];
    if (device != NULL && writefn != NULL) {
        writefn(device, addr, data);
    } else {
        uint8_t page = addr >> 8;
        MemoryType type = memory->cpu_page_type[page];
        if (type != MEM_NONE && MEMORY_IS_RAM[type])
            memory->memory[type][memory->cpu_page_addr[page] + (addr & 0xFF)] = data;
    }
}

uint8_t Mem_CPUPeek(Memory *memory, uint16_t addr)
{
    uint8_t page = addr >> 8;
    MemoryType type = memory->cpu_page_type[page];
    if (type != MEM_NONE)
        return memory->memory[type][memory->cpu_page_addr[page] + (addr & 0xFF)];
    return 0;
}

uint8_t Mem_PPURead(Memory *memory, uint16_t addr)
{
    uint8_t page = addr >> 8;
    MemoryType type = memory->ppu_page_type[page];
    if (type != MEM_NONE)
        return memory->memory[type][memory->ppu_page_addr[page] + (addr & 0xFF)];
    return 0;
}

void Mem_PPUWrite(Memory *memory, uint16_t addr, uint8_t data)
{
    uint8_t page = addr >> 8;
    MemoryType type = memory->ppu_page_type[page];
    if (type != MEM_NONE && MEMORY_IS_RAM[type])
        memory->memory[type][memory->ppu_page_addr[page] + (addr & 0xFF)] = data;
}

uint8_t Mem_PPUPeek(Memory *memory, uint16_t addr)
{
    uint8_t page = addr >> 8;
    MemoryType type = memory->ppu_page_type[page];
    if (type != MEM_NONE)
        return memory->memory[type][memory->ppu_page_addr[page] + (addr & 0xFF)];
    return 0;
}

unsigned Mem_Size(Memory *memory, MemoryType memType)
{
    assert(0 <= memType && memType <= MEM_NUM_TYPES);
    return memory->memory_size[memType];
}

uint8_t Mem_Get(Memory *memory, MemoryType memType, unsigned memAddr)
{
    assert(0 <= memType && memType <= MEM_NUM_TYPES);
    assert(memAddr < Mem_Size(memory, memType));
    return memory->memory[memType][memAddr];
}

void Mem_CPUToMemAddr(Memory *memory, uint16_t cpuAddr, MemoryType *memType, unsigned *memAddr)
{
    uint8_t page = cpuAddr >> 8;
    *memType = memory->cpu_page_type[page];
    *memAddr = memory->cpu_page_addr[page] + (cpuAddr & 0xFF);
}

int Mem_BeginMemToCPUAddr(Memory *memory, MemoryType memType, unsigned memAddr, uint16_t* cpuAddr)
{
    for (unsigned _cpuAddr = 0; _cpuAddr < CPU_ADDR_SPACE; _cpuAddr++) {
        MemoryType mappedType;
        unsigned mappedAddr;
        Mem_CPUToMemAddr(memory, _cpuAddr, &mappedType, &mappedAddr);

        if (mappedType != MEM_NONE && mappedType == memType && mappedAddr == memAddr) {
            *cpuAddr = (uint16_t)_cpuAddr;
            return 0;
        }
    }
    return -1;
}

int Mem_IterMemToCPUAddr(Memory *memory, MemoryType memType, unsigned memAddr, uint16_t *cpuAddr)
{
    for (unsigned _cpuAddr = *cpuAddr + 1; _cpuAddr < CPU_ADDR_SPACE; _cpuAddr++) {
        MemoryType mappedType;
        unsigned mappedAddr;
        Mem_CPUToMemAddr(memory, _cpuAddr, &mappedType, &mappedAddr);

        if (mappedType != MEM_NONE && mappedType == memType && mappedAddr == memAddr) {
            *cpuAddr = (uint16_t)_cpuAddr;
            return 0;
        }
    }
    return -1;
}

uint8_t* _Mem_DebugFlagArr(Memory* memory, MemoryType type, unsigned* arrSize) {
    switch (type) {
        case MEM_CPU: *arrSize = CPU_ADDR_SPACE; return memory->cpu_debug_flags;
        case MEM_PPU: *arrSize = PPU_ADDR_SPACE; return memory->ppu_debug_flags;
        case MEM_NONE:
        case MEM_NUM_TYPES: return NULL;
        default: *arrSize = memory->memory_size[type]; return memory->memory_debug_flags[type];
    }
}

uint8_t Mem_GetDebugFlags(Memory *memory, MemoryType type, unsigned addr)
{
    unsigned arrSize;
    uint8_t* arr = _Mem_DebugFlagArr(memory, type, &arrSize);
    if (arr != NULL && addr < arrSize)
        return arr[addr];
    return 0;
}

void Mem_SetBreakFlags(Memory *memory, MemoryType type, unsigned start, unsigned end, uint8_t breakFlags)
{
    unsigned arrSize;
    uint8_t* arr = _Mem_DebugFlagArr(memory, type, &arrSize);
    if (arr != NULL) {
        if (end >= arrSize)
            end = arrSize - 1;
        while (start <= end) {
            arr[start] |= breakFlags & MEMDEBUG_BREAK_MASK;
            start++;
        }
    }
}

void Mem_ClearBreakFlags(Memory *memory, MemoryType type, unsigned start, unsigned end)
{
    unsigned arrSize;
    uint8_t* arr = _Mem_DebugFlagArr(memory, type, &arrSize);
    if (arr != NULL) {
        if (end >= arrSize)
            end = arrSize - 1;
        while (start <= end) {
            arr[start] &= ~MEMDEBUG_BREAK_MASK;
            start++;
        }
    }
}

void Mem_FlagOpcode(Memory *memory, MemoryType type, unsigned addr)
{
    unsigned arrSize;
    uint8_t* arr = _Mem_DebugFlagArr(memory, type, &arrSize);
    if (arr != NULL && addr < arrSize)
        arr[addr] |= MEMDEBUG_IS_OPCODE;
}

uint8_t Mem_TestCPUDebugFlags(Memory *memory, uint16_t addr)
{
    MemoryType memType;
    unsigned memAddr;
    Mem_CPUToMemAddr(memory, addr, &memType, &memAddr);
    return memory->cpu_debug_flags[addr] | 
    ((memType != MEM_NONE) ? memory->memory_debug_flags[memType][memAddr] : 0);
}

bool Mem_TestCPUBreakpoint(Memory *memory, uint16_t addr, AccessType access)
{
    uint8_t bpFlags = Mem_TestCPUDebugFlags(memory, addr);
    return (bpFlags & access & ACCESS_MASK_RWX) &&
    (!(access & ACCESS_DUMMY) || (bpFlags & MEMDEBUG_BREAK_D));
}

bool Mem_CPUHasOpcode(Memory *memory, uint16_t addr)
{
    return (Mem_TestCPUDebugFlags(memory, addr) & MEMDEBUG_IS_OPCODE) > 0;
}
