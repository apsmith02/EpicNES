#ifndef MMC1_H
#define MMC1_H

#include <stdint.h>
#include "rom.h"

typedef struct Mapper Mapper;

typedef struct {
    uint8_t shift;
    /*
    * Control ($8000-$9FFF) bits: xxxCPPMM
    * MM: Nametable arrangement (0: one-screen A; 1: one-screen B; 2: vertical mirror; 3: horizontal mirror)
    * PP: PRG-ROM bank mode (0, 1: switch 32KB at $8000, ignore low bit of bank num;
    *                        2: fix first bank at $8000, switch 16KB at $C000;
    *                        3: fix last bank at $C000, switch 16KB at $8000)
    * C: CHR bank mode (0: switch 8KB at a time; 1: switch two separate 4KB banks)
    */
    uint8_t control;
    //5-bit CHR bank 0 select ($A000-$BFFF)
    uint8_t chr_bank0;
    //5-bit CHR bank 1 select ($C000-$DFFF)
    uint8_t chr_bank1;
    //4-bit PRG bank select ($E000-$FFFF)
    uint8_t prg_bank;
} MMC1;

int MMC1_Init(Mapper* mapper, const INESHeader* ines);
void MMC1_WriteRegisters(Mapper* mapper, uint16_t addr, uint8_t data);

void MMC1_UpdateBanks(Mapper* mapper);

#endif