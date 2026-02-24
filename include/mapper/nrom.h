#ifndef NROM_H
#define NROM_H

#include "rom.h"
typedef struct Mapper Mapper;

int NROM_Init(Mapper* mapper, const INESHeader* ines);

#endif