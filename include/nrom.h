#ifndef NROM_H
#define NROM_H
#include "mapper_base.h"

typedef struct {
    Mapper_Base base;
} NROM;


void NROM_Init(NROM* mapper, Emulator* emu);


static const Mapper_Vtable NROM_VTABLE = {
    .size_of = sizeof(NROM),
    .init = NROM_Init,
    .free = MapperBase_Free,
    .cpuRead = MapperBase_CPURead,
    .cpuWrite = MapperBase_CPUWrite,
    .ppuRead = MapperBase_CPURead,
    .ppuWrite = MapperBase_CPUWrite,
    .irq = MapperBase_IRQ
};

#endif