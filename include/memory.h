#ifndef MEMORY_H
#define MEMORY_H

#include "rom.h"
#include "cpu.h"
#include "ppu.h"

#define CPU_ADDR_SPACE 0x10000
#define PPU_ADDR_SPACE 0x4000
#define PRG_PAGE_COUNT 0x100
#define CHR_PAGE_COUNT 0x40

//Type of memory. Nonnegative values are physical memory.
typedef enum {
    MEM_PPU = -3, //PPU addresses. Used for debugging memory accesses at PPU addresses regardless of what is mapped there.
    MEM_CPU = -2, //CPU addresses. Used for debugging memory accesses at CPU addresses regardless of what is mapped there, and for debugging device register I/O.
    MEM_NONE = -1,
    MEM_RAM = 0,
    MEM_VRAM,
    MEM_PRG_ROM,
    MEM_CHR_ROM,
    MEM_NUM_TYPES //Number of types of physical memory
} MemoryType;

//Flags used by the debugger to determine if memory is code or has a breakpoint
typedef enum {
    MEMDEBUG_BREAK_R    = 1,
    MEMDEBUG_BREAK_W    = 1 << 1,
    MEMDEBUG_BREAK_X    = 1 << 2,
    MEMDEBUG_IS_OPCODE  = 1 << 3
} MemoryDebugFlags;



static const bool MEMORY_IS_RAM[MEM_NUM_TYPES] = {
    true,   //RAM
    true,   //VRAM
    false,  //PRG ROM
    false   //CHR ROM
};

/*
* CPU and PPU physical memory storage (console RAM and VRAM, cartridge RAM and ROM),
* memory debug flag storage ("is code" flags, "has R/W/X breakpoint" flags),
* and memory map.
*/
typedef struct {
    uint8_t* memory[MEM_NUM_TYPES]; //Array of physical memory arrays, indexed by memory type
    unsigned memory_size[MEM_NUM_TYPES]; //Sizes of physical memory arrays, indexed by memory type

    uint8_t* memory_debug_flags[MEM_NUM_TYPES];
    uint8_t cpu_debug_flags[CPU_ADDR_SPACE];
    uint8_t ppu_debug_flags[PPU_ADDR_SPACE];

    //PRG and CHR, mapped in pages

    MemoryType prg_page_type[PRG_PAGE_COUNT];//Physical memory type mapped to a page in CPU memory
    unsigned prg_pages[PRG_PAGE_COUNT];      //Physical address mapped to a page in CPU memory
    MemoryType chr_page_type[CHR_PAGE_COUNT]; //Physical memory type mapped to a page in PPU memory
    unsigned chr_pages[CHR_PAGE_COUNT];       //Physical address mapped to a page in PPU memory

    //CPU read/write handler callbacks (use for console component registers and mapper registers)

    CPUReadFn cpu_read_handler_fn[CPU_ADDR_SPACE];
    void* cpu_read_handler_data[CPU_ADDR_SPACE];
    CPUWriteFn cpu_write_handler_fn[CPU_ADDR_SPACE];
    void* cpu_write_handler_data[CPU_ADDR_SPACE];
} Memory;


void Mem_Init(Memory* mem);
void Mem_Free(Memory* mem);

//Create 2KB console RAM
void Mem_Create_RAM(Memory* mem);
//Create VRAM of a given size
void Mem_Create_VRAM(Memory* mem, unsigned size);
//Load PRG ROM from ROM file
void Mem_Load_PRG_ROM(Memory* mem, INESHeader* ines, FILE* rom_file);
//Load CHR ROM from ROM file
void Mem_Load_CHR_ROM(Memory* mem, INESHeader* ines, FILE* rom_file);

//Map physical memory (src,src_offset) to a 256-byte page in CPU address space. Memory type must be physical memory.
void Mem_MapPRGPages(Memory* memory, MemoryType src, unsigned src_offset, uint8_t start_page, uint8_t end_page);
void Mem_UnmapPRGPages(Memory* memory, uint8_t start_page, uint8_t end_page);
//Map physical memory (src,src_offset) to a 256-byte page in PPU address space. Memory type must be physical memory.
void Mem_MapCHRPages(Memory* memory, MemoryType src, unsigned src_offset, uint8_t start_page, uint8_t end_page);
void Mem_UnmapCHRPages(Memory* memory, uint8_t start_page, uint8_t end_page);

void Mem_MapCPUReadDevice(Memory* memory, void* device, CPUReadFn readfn, uint16_t start, uint16_t end);
void Mem_MapCPUWriteDevice(Memory* memory, void* device, CPUWriteFn writefn, uint16_t start, uint16_t end);

uint8_t Mem_CPURead(Memory* mem, uint16_t addr);
void Mem_CPUWrite(Memory* mem, uint16_t addr, uint8_t data);
uint8_t Mem_CPUPeek(Memory* mem, uint16_t addr);
uint8_t Mem_PPURead(Memory* mem, uint16_t addr);
void Mem_PPUWrite(Memory* mem, uint16_t addr, uint8_t data);
uint8_t Mem_PPUPeek(Memory* mem, uint16_t addr);

//Access the debug flags of any memory type
uint8_t* Mem_DebugFlagsAtType(Memory* mem, MemoryType type, unsigned addr);
//Access the debug flags of physical memory
uint8_t* Mem_DebugFlagsAtPhysical(Memory* mem, MemoryType type, unsigned addr);
//Access the debug flags of the physical ROM/RAM mapped to addr in the CPU PRG memory map
uint8_t* Mem_PRGDebugFlagsAt(Memory* mem, uint16_t addr);
//Access the debug flags of the physical ROM/RAM mapped to addr in the PPU CHR memory map
uint8_t* Mem_CHRDebugFlagsAt(Memory* mem, uint16_t addr);

//Get the type of memory mapped to an address in the CPU PRG memory map.
MemoryType Mem_GetPRGMemType(Memory* mem, uint16_t addr);
//Get the type of memory mapped to an address in the PPU CHR memory map.
MemoryType Mem_GetChrMemType(Memory* mem, uint16_t addr);
//Get the physical address mapped to an address in the CPU PRG memory map.
unsigned Mem_GetPRGMemPhysAddr(Memory* mem, uint16_t addr);
//Get the physical address mapped to an address in the PPU CHR memory map.
unsigned Mem_GetCHRMemPhysAddr(Memory* mem, uint16_t addr);

//Iterate all CPU addresses that a byte of physical memory is mapped to. Set cpu_addr to -1 to start. Returns 0 on success, -1 at the end.
int Mem_IterPRGMirrors(Memory* mem, MemoryType type, int physical_addr, int* cpu_addr);

/*
* Access the debug flags of a CPU address.
* NOTE: This is for CPU "virtual" address accesses, not physical memory accesses, and is intended for
* debugging device register I/O and CPU accesses regardless of mapping.
*/
uint8_t* Mem_CPUDebugFlagsAt(Memory* mem, uint16_t addr);
/*
* Access the debug flags of a PPU address.
* NOTE: This is for PPU "virtual" address accesses, not physical memory accesses, and is intended for
* debugging memory accesses from the PPU's perspective regardless of mapping.
*/
uint8_t* Mem_PPUDebugFlagsAt(Memory* mem, uint16_t addr);

#endif