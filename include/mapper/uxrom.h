#ifndef UXROM_H
#define UXROM_H

#include <stdint.h>
#include "rom.h"
typedef struct Mapper Mapper;

typedef struct {
    uint8_t bank; // ($8000-$FFFF): Select 16KB PRG ROM bank for CPU $8000-$BFFF
} UxROM;

int UxROM_Init(Mapper* mapper, const INESHeader* ines);
void UxROM_WriteRegisters(Mapper* mapper, uint16_t addr, uint8_t data);

void UxROM_UpdateBanks(Mapper* mapper);

#endif