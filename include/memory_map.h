#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include "cpu.h"
#include "ppu.h"

/*
* CPU memory map, implemented as a table of read/write handler callbacks divided into 256-byte pages.
*
* On CPU reads/writes to an address, the read/write handler for the page of the address is called 
* (read_handlers[address / 256](read_data[address / 256], address) or
* write_handlers[address / 256](write_data[address / 256], address, data) is called).
*
* Example usage:

* - To map a PPU register read function to $2000-$3FFF, set read_handlers[0x20 to 0x3F] to
* PPU_ReadRegisters(), and set read_data[0x20 to 0x3F] to a pointer to the PPU.
*
* -PRG bank switching can easily be done by setting read_handlers[0x80 to 0xFF] to a simple
* function that reads from a 256-byte array (indexed by the low address byte i.e. [address % 256])
* once, then switching PRG banks by setting the read_data[] pointers in PRG space
* to pointers to the desired bank in PRG ROM.
* For example, to switch the 16KB bank at $C000 to the 3rd 16KB bank in PRG ROM (offset 16384 * 3 in PRG ROM), 
* set read_data[0xC0] = &prg[0x4000*3], read_data[0xC1] = &prg[0x4000*3 + 256],
* read_data[0xC2] = &prg[0x4000*3 + 256*2], ..., read_data[0xFF] = &prg[0x4000*3 + 256*63].
*/
typedef struct {
    CPUReadFn read_handlers[0x100];
    void* read_data[0x100];
    CPUWriteFn write_handlers[0x100];
    void* write_data[0x100];
} CPUMemoryMap;

typedef struct {
    PPUReadFn read_handlers[0x40];
    void* read_data[0x40];
    PPUWriteFn write_handlers[0x40];
    void* write_data[0x40];
} PPUMemoryMap;

#endif