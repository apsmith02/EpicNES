#ifndef MEMORY_H
#define MEMORY_H

#include "rom.h"
#include "cpu.h"
#include "ppu.h"

#define CPU_ADDR_SPACE 0x10000
#define PPU_ADDR_SPACE 0x4000
#define CPU_PAGE_COUNT 0x100
#define PPU_PAGE_COUNT 0x40

//Type of memory. Nonnegative values are physical memory.
typedef enum {
    MEM_PPU = -3, //PPU address space. Used for debugging memory accesses at PPU addresses regardless of what is mapped there.
    MEM_CPU = -2, //CPU address space. Used for debugging memory accesses at CPU addresses regardless of what is mapped there, and for debugging device register I/O.
    MEM_NONE = -1,
    MEM_RAM = 0,
    MEM_VRAM,
    MEM_PRG_ROM,
    MEM_CHR_ROM,
    MEM_NUM_TYPES //Number of types of physical memory
} MemoryType;

//Flags used by the debugger to determine if memory is code or has a breakpoint
typedef enum {
    MEMDEBUG_BREAK_R    = ACCESS_READ,
    MEMDEBUG_BREAK_W    = ACCESS_WRITE,
    MEMDEBUG_BREAK_X    = ACCESS_EXECUTE,
    MEMDEBUG_BREAK_D    = ACCESS_DUMMY,
    MEMDEBUG_BREAK_MASK = ACCESS_MASK,
    MEMDEBUG_IS_OPCODE  = ACCESS_FLAGS_END
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

    //CPU and PPU memory maps (mapped in pages)

    MemoryType cpu_page_type[CPU_PAGE_COUNT];//Physical memory type mapped to a page in CPU memory
    unsigned cpu_page_addr[CPU_PAGE_COUNT];      //Physical address mapped to a page in CPU memory
    MemoryType ppu_page_type[PPU_PAGE_COUNT]; //Physical memory type mapped to a page in PPU memory
    unsigned ppu_page_addr[PPU_PAGE_COUNT];       //Physical address mapped to a page in PPU memory

    //CPU IO map (use for memory-mapped IO registers)

    CPUReadFn cpu_io_readfn[CPU_ADDR_SPACE];
    void* cpu_io_readdevice[CPU_ADDR_SPACE];
    CPUWriteFn cpu_io_writefn[CPU_ADDR_SPACE];
    void* cpu_io_writedevice[CPU_ADDR_SPACE];
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

//Map physical memory pages to a range of pages in CPU address space.
void Mem_CPUMapPages(Memory* memory, uint8_t cpuStartPg, uint8_t cpuEndPg, MemoryType memType, unsigned memStartPg);
void Mem_CPUUnmapPages(Memory* memory, uint8_t cpuStartPg, uint8_t cpuEndPg);
void Mem_PPUMapPages(Memory* memory, uint8_t cpuStartPg, uint8_t cpuEndPg, MemoryType memType, unsigned memStartPg);
void Mem_PPUUnmapPages(Memory* memory, uint8_t cpuStartPg, uint8_t cpuEndPg);

//Map an IO device to CPU address space.
void Mem_CPUMapDevice(Memory* memory, uint16_t start, uint16_t end, void* device, CPUReadFn readfn, CPUWriteFn writefn);
//Map an IO read device to CPU address space.
void Mem_CPUMapReadDevice(Memory* memory, uint16_t start, uint16_t end, void* device, CPUReadFn readfn);
//Map an IO write device to CPU address space.
void Mem_CPUMapWriteDevice(Memory* memory, uint16_t start, uint16_t end, void* device, CPUWriteFn writefn);
void Mem_CPUUnmapDevice(Memory* memory, uint16_t start, uint16_t end);
void Mem_CPUUnmapReadDevice(Memory* memory, uint16_t start, uint16_t end);
void Mem_CPUUnmapWriteDevice(Memory* memory, uint16_t start, uint16_t end);

uint8_t Mem_CPURead(Memory* memory, uint16_t addr);
void Mem_CPUWrite(Memory* memory, uint16_t addr, uint8_t data);
uint8_t Mem_CPUPeek(Memory* memory, uint16_t addr);
uint8_t Mem_PPURead(Memory* memory, uint16_t addr);
void Mem_PPUWrite(Memory* memory, uint16_t addr, uint8_t data);
uint8_t Mem_PPUPeek(Memory* memory, uint16_t addr);

//Get the physical memory type and address mapped to a CPU address.
void Mem_CPUToMemAddr(Memory* memory, uint16_t cpuAddr, MemoryType* memType, unsigned* memAddr);
//Get the first CPU address that a physical memory location is mapped to. Returns 0 on success, -1 if the memory is not mapped to a CPU address.
int Mem_BeginMemToCPUAddr(Memory* memory, MemoryType memType, unsigned memAddr, uint16_t* cpuAddr);
//Iterate all CPU address that a physical memory location is mapped to.
int Mem_IterMemToCPUAddr(Memory* memory, MemoryType memType, unsigned memAddr, uint16_t* cpuAddr);

//Get the debug flags of a given memory type and address within that type.
uint8_t Mem_GetDebugFlags(Memory* memory, MemoryType type, unsigned addr);
//Set the breakpoint access type flags for a range of addresses of a given memory type.
void Mem_SetBreakFlags(Memory* memory, MemoryType type, unsigned start, unsigned end, uint8_t breakFlags);
//Clear the breakpoint access type flags for a range of addresses of a given memory type.
void Mem_ClearBreakFlags(Memory* memory, MemoryType type, unsigned start, unsigned end);
//Flag a physical memory location as an opcode. (NOTE: Does not translate CPU to physical addresses. To flag physical memory by its CPU address mapping, use Mem_CPUToMemAddr() to get the physical memory location.)
void Mem_FlagOpcode(Memory* memory, MemoryType type, unsigned addr);
//Test both the CPU address space and physical memory debug flags at a CPU address. Returns the CPU debug flags at addr OR'd with the debug flags of the physical memory mapped to addr.
uint8_t Mem_TestCPUDebugFlags(Memory* memory, uint16_t addr);
//Test whether a breakpoint should trigger on a CPU memory access given a CPU address and access type.
bool Mem_TestCPUBreakpoint(Memory* memory, uint16_t addr, AccessType access);
//Returns true if an opcode is mapped to a given CPU address.
bool Mem_CPUHasOpcode(Memory* memory, uint16_t addr);

// OLD FUNCTIONS

/*
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
*/
/*
* Access the debug flags of a CPU address.
* NOTE: This is for CPU "virtual" address accesses, not physical memory accesses, and is intended for
* debugging device register I/O and CPU accesses regardless of mapping.
*/
//uint8_t* Mem_CPUDebugFlagsAt(Memory* mem, uint16_t addr);
/*
* Access the debug flags of a PPU address.
* NOTE: This is for PPU "virtual" address accesses, not physical memory accesses, and is intended for
* debugging memory accesses from the PPU's perspective regardless of mapping.
*/
//uint8_t* Mem_PPUDebugFlagsAt(Memory* mem, uint16_t addr);

#endif