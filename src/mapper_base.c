#include <stdlib.h>
#include "mapper_base.h"
#include "emulator.h"

Mapper_Base *Mapper_CreateFromVtable(const Mapper_Vtable *vtable, Emulator* emu)
{
    Mapper_Base* mapper = malloc(vtable->size_of);
    mapper->vtable = vtable;
    mapper->vtable->init(mapper, emu);
    return mapper;
}

void Mapper_Destroy(Mapper_Base *mapper, Emulator* emu)
{
    mapper->vtable->free(mapper, emu);
    free(mapper);
}

void MapperBase_Init(Mapper_Base *mapper, Emulator *emu) {}
void MapperBase_Free(Mapper_Base *mapper, Emulator *emu) {}

uint8_t MapperBase_CPURead(Mapper_Base *mapper, Emulator *emu, uint16_t addr)
{
    return Mem_CPURead(&emu->memory, addr);
}

void MapperBase_CPUWrite(Mapper_Base *mapper, Emulator *emu, uint16_t addr, uint8_t data)
{
    Mem_CPUWrite(&emu->memory, addr, data);
}

uint8_t MapperBase_PPURead(Mapper_Base *mapper, Emulator *emu, uint16_t addr)
{
    return Mem_PPURead(&emu->memory, addr);
}

void MapperBase_PPUWrite(Mapper_Base *mapper, Emulator *emu, uint16_t addr, uint8_t data)
{
    Mem_PPUWrite(&emu->memory, addr, data);
}

bool MapperBase_IRQ(Mapper_Base *mapper, Emulator *emu) { return false; }