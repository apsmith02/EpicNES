#ifndef MAPPER_BASE_H
#define MAPPER_BASE_H
#include <stdint.h>
#include <stdbool.h>

typedef struct Emulator Emulator;
typedef struct Mapper_Base Mapper_Base;

typedef struct {
    size_t size_of; //Derived mapper type sizeof()
    void(*init)(Mapper_Base* mapper, Emulator* emu);
    void(*free)(Mapper_Base* mapper, Emulator* emu);
    uint8_t(*cpuRead)(Mapper_Base* mapper, Emulator* emu, uint16_t addr);
    void(*cpuWrite)(Mapper_Base* mapper, Emulator* emu, uint16_t addr, uint8_t data);
    uint8_t(*ppuRead)(Mapper_Base* mapper, Emulator* emu, uint16_t addr);
    void(*ppuWrite)(Mapper_Base* mapper, Emulator* emu, uint16_t addr, uint8_t data);
    bool(*irq)(Mapper_Base* mapper, Emulator* emu);
} Mapper_Vtable;

//Make this the first member of all derived structs
struct Mapper_Base {
    const Mapper_Vtable* vtable;
};


//Create mapper from vtable, call its init() method
Mapper_Base* Mapper_CreateFromVtable(const Mapper_Vtable* vtable, Emulator* emu);
//Free mapper from memory
void Mapper_Destroy(Mapper_Base* mapper, Emulator* emu);

//Mapper base init method (Does nothing)
void MapperBase_Init(Mapper_Base* mapper, Emulator* emu);
//Mapper base free method (Does nothing)
void MapperBase_Free(Mapper_Base* mapper, Emulator* emu);
//Mapper base CPU read method. Reads from console memory.
uint8_t MapperBase_CPURead(Mapper_Base* mapper, Emulator* emu, uint16_t addr);
//Mapper base CPU write method. Writes to console memory.
void MapperBase_CPUWrite(Mapper_Base* mapper, Emulator* emu, uint16_t addr, uint8_t data);
//Mapper base PPU read method. Reads from console memory.
uint8_t MapperBase_PPURead(Mapper_Base* mapper, Emulator* emu, uint16_t addr);
//Mapper base PPU write method. Writes to console memory.
void MapperBase_PPUWrite(Mapper_Base* mapper, Emulator* emu, uint16_t addr, uint8_t data);
//Mapper base IRQ signal method (Returns false)
bool MapperBase_IRQ(Mapper_Base* mapper, Emulator* emu);

#endif