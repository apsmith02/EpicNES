#ifndef UXROM_H
#define UXROM_H

#include "mapper_base.h"

typedef struct {
    MapperBase base;
} UxROM;

MapperBase* UxROM_New();
void UxROM_Init(UxROM* mapper, const INESHeader* romHeader, FILE* romFile);
void UxROM_RegWrite(UxROM* mapper, uint16_t addr, uint8_t data);

static const MapperVtable UXROM_VTBL = {
    .Init = (void(*)(MapperBase*, const INESHeader*, FILE*))UxROM_Init,
    .RegWrite = (void(*)(MapperBase*, uint16_t, uint8_t))UxROM_RegWrite
};

#endif