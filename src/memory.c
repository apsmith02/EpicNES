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
    memset(&mem->prg_page_type[0], MEM_NONE, sizeof(mem->prg_page_type));
    memset(&mem->chr_page_type[0], MEM_NONE, sizeof(mem->chr_page_type));
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

void Mem_MapPRGPages(Memory *memory, MemoryType src, unsigned src_offset, uint8_t start_page, uint8_t end_page)
{
    assert(src >= 0 && src < MEM_NUM_TYPES);
    assert(src_offset + ((end_page - start_page) * 0x100) + 0x100 <= memory->memory_size[src]);

    for (unsigned page = start_page; page <= end_page; page++, src_offset += 0x100) {
        memory->prg_page_type[page] = src;
        memory->prg_pages[page] = src_offset;
    }
}

void Mem_UnmapPRGPages(Memory *memory, uint8_t start_page, uint8_t end_page)
{
    for (unsigned page = start_page; page <= end_page; page++) {
        memory->prg_page_type[page] = MEM_NONE;
        memory->prg_pages[page] = 0;
    }
}

void Mem_MapCHRPages(Memory *memory, MemoryType src, unsigned src_offset, uint8_t start_page, uint8_t end_page)
{
    assert(end_page < CHR_PAGE_COUNT);
    assert(src >= 0 && src < MEM_NUM_TYPES);
    assert(src_offset + ((end_page - start_page) * 0x100) + 0x100 <= memory->memory_size[src]);

    for (unsigned page = start_page; page <= end_page; page++, src_offset += 0x100) {
        memory->chr_page_type[page] = src;
        memory->chr_pages[page] = src_offset;
    }
}

void Mem_UnmapCHRPages(Memory *memory, uint8_t start_page, uint8_t end_page)
{
    assert(end_page < CHR_PAGE_COUNT);
    for (unsigned page = start_page; page <= end_page; page++) {
        memory->chr_page_type[page] = MEM_NONE;
        memory->chr_pages[page] = 0;
    }
}

void Mem_MapCPUReadDevice(Memory *memory, void *device, CPUReadFn readfn, uint16_t start, uint16_t end)
{
    for (unsigned addr = start; addr <= end; addr++) {
        memory->cpu_read_handler_data[addr] = device;
        memory->cpu_read_handler_fn[addr] = readfn;
    }
}

void Mem_MapCPUWriteDevice(Memory *memory, void *device, CPUWriteFn writefn, uint16_t start, uint16_t end)
{
    for (unsigned addr = start; addr <= end; addr++) {
        memory->cpu_write_handler_data[addr] = device;
        memory->cpu_write_handler_fn[addr] = writefn;
    }
}

uint8_t Mem_CPURead(Memory *mem, uint16_t addr)
{
    void* fndata = mem->cpu_read_handler_data[addr];
    CPUReadFn readfn = mem->cpu_read_handler_fn[addr];
    MemoryType src = mem->prg_page_type[addr >> 8];
    unsigned srcoffset = mem->prg_pages[addr >> 8];

    if (fndata != NULL && readfn != NULL) {
        return readfn(fndata, addr);
    }
    if (src >= 0) {
        return mem->memory[src][srcoffset + (addr & 0xFF)];
    }
    return 0;
}

void Mem_CPUWrite(Memory *mem, uint16_t addr, uint8_t data)
{
    void* fndata = mem->cpu_write_handler_data[addr];
    CPUWriteFn writefn = mem->cpu_write_handler_fn[addr];
    MemoryType src = mem->prg_page_type[addr >> 8];
    unsigned srcoffset = mem->prg_pages[addr >> 8];

    if (fndata != NULL && writefn != NULL) {
        writefn(fndata, addr, data);
    } else if (src >= 0 && MEMORY_IS_RAM[src]) {
        mem->memory[src][srcoffset + (addr & 0xFF)] = data;
    }
}

uint8_t Mem_CPUPeek(Memory *mem, uint16_t addr)
{
    MemoryType src = mem->prg_page_type[addr >> 8];
    unsigned srcoffset = mem->prg_pages[addr >> 8];
    
    if (src >= 0) {
        return mem->memory[src][srcoffset + (addr & 0xFF)];
    }
    return 0;
}

uint8_t Mem_PPURead(Memory *mem, uint16_t addr)
{
    assert(addr < PPU_ADDR_SPACE);

    MemoryType src = mem->chr_page_type[addr >> 8];
    unsigned srcoffset = mem->chr_pages[addr >> 8];

    if (src >= 0) {
        return mem->memory[src][srcoffset + (addr & 0xFF)];
    }
    return 0;
}

void Mem_PPUWrite(Memory *mem, uint16_t addr, uint8_t data)
{
    assert(addr < PPU_ADDR_SPACE);

    MemoryType src = mem->chr_page_type[addr >> 8];
    unsigned srcoffset = mem->chr_pages[addr >> 8];

    if (src >= 0) {
        mem->memory[src][srcoffset + (addr & 0xFF)] = data;
    }
}

uint8_t Mem_PPUPeek(Memory *mem, uint16_t addr)
{
    assert(addr < PPU_ADDR_SPACE);

    MemoryType src = mem->chr_page_type[addr >> 8];
    unsigned srcoffset = mem->chr_pages[addr >> 8];

    if (src >= 0) {
        return mem->memory[src][srcoffset + (addr & 0xFF)];
    }
    return 0;
}

uint8_t *Mem_DebugFlagsAtType(Memory *mem, MemoryType type, unsigned addr)
{
    if (type == MEM_CPU)        return Mem_CPUDebugFlagsAt(mem, addr);
    else if (type == MEM_PPU)   return Mem_PPUDebugFlagsAt(mem, addr);
    else                        return Mem_DebugFlagsAtPhysical(mem, type, addr);
}

uint8_t *Mem_DebugFlagsAtPhysical(Memory *mem, MemoryType type, unsigned addr)
{
    assert(type >= 0 && type < MEM_NUM_TYPES);
    return &mem->memory_debug_flags[type][addr];
}

uint8_t *Mem_PRGDebugFlagsAt(Memory *mem, uint16_t addr)
{
    MemoryType src = mem->prg_page_type[addr >> 8];
    unsigned srcoffset = mem->prg_pages[addr >> 8];
    
    if (src >= 0) {
        return &mem->memory_debug_flags[src][srcoffset + (addr & 0xFF)];
    }
    return NULL;
}

uint8_t *Mem_CHRDebugFlagsAt(Memory *mem, uint16_t addr)
{
    assert(addr < PPU_ADDR_SPACE);

    MemoryType src = mem->chr_page_type[addr >> 8];
    unsigned srcoffset = mem->chr_pages[addr >> 8];
    
    if (src >= 0) {
        return &mem->memory_debug_flags[src][srcoffset + (addr & 0xFF)];
    }
    return NULL;
}

MemoryType Mem_GetPRGMemType(Memory *mem, uint16_t addr)
{
    return mem->prg_page_type[addr >> 8];
}

MemoryType Mem_GetChrMemType(Memory *mem, uint16_t addr)
{
    assert(addr < PPU_ADDR_SPACE);
    return mem->chr_page_type[addr >> 8];
}

unsigned Mem_GetPRGMemPhysAddr(Memory *mem, uint16_t addr)
{
    return mem->prg_pages[addr >> 8] + (addr & 0xFF);
}

unsigned Mem_GetCHRMemPhysAddr(Memory *mem, uint16_t addr)
{
    return mem->chr_pages[addr >> 8] + (addr & 0xFF);
}

int Mem_IterPRGMirrors(Memory* mem, MemoryType type, int physical_addr, int* cpu_addr)
{
    *cpu_addr += 0x100;
    assert(0 <= *cpu_addr);

    while (*cpu_addr < CPU_ADDR_SPACE) {
        if (Mem_GetPRGMemType(mem, *cpu_addr) == type) {
            unsigned pageOffset = mem->prg_pages[*cpu_addr >> 8];
            if (pageOffset <= physical_addr && physical_addr < pageOffset + 0x100) {
                *cpu_addr = (*cpu_addr & 0xFF00) | physical_addr - pageOffset;
                return 0;
            }
        }
        *cpu_addr += 0x100;
    }
    return -1;
}

uint8_t *Mem_CPUDebugFlagsAt(Memory *mem, uint16_t addr)
{
    return &mem->cpu_debug_flags[addr];
}

uint8_t *Mem_PPUDebugFlagsAt(Memory *mem, uint16_t addr)
{
    assert(addr < PPU_ADDR_SPACE);
    return &mem->ppu_debug_flags[addr];
}
