#ifndef MMC1_H
#define MMC1_H

#include "mapper_base.h"

typedef struct {
    MapperBase base;
    uint8_t shift;
    uint8_t control;
    uint8_t prg_bank; //4-bit PRG bank select
    uint8_t chr_bank0; //5-bit CHR bank select (PPU $0000)
    uint8_t chr_bank1; //5-bit CHR bank select (PPU $1000)
} MMC1;

MapperBase* MMC1_New();
void MMC1_Init(MMC1* mapper, const INESHeader* romHeader, FILE* romFile);
void MMC1_RegWrite(MMC1* mapper, uint16_t addr, uint8_t data);

static const MapperVtable MMC1_VTBL = {
    .Init = (void(*)(MapperBase*, const INESHeader*, FILE*))MMC1_Init,
    .RegWrite = (void(*)(MapperBase*, uint16_t, uint8_t))MMC1_RegWrite
};

#endif