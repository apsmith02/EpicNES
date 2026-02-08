#ifndef NROM_H
#define NROM_H

#include "mapper_base.h"

typedef struct {
    MapperBase base;
} NROM;

MapperBase* NROM_New();
void NROM_Init(NROM* mapper, const INESHeader* romHeader, FILE* romFile);

static const MapperVtable NROM_VTBL = {
    .Init = (void(*)(MapperBase*, const INESHeader*, FILE*))NROM_Init
};

#endif