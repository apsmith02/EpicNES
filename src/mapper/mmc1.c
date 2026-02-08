#include "mmc1.h"
#include <stdlib.h>

void MMC1_UpdateBanks(MMC1 *mapper) {
    MapperBase *base = (MapperBase*)mapper;
    //Nametable arrangement
    switch (mapper->control & 0x3) {
        case 0:
            Mapper_MapNTMirroring(base, NT_MIRROR_ONESCREEN_A);
            break;
        case 1:
            Mapper_MapNTMirroring(base, NT_MIRROR_ONESCREEN_B);
            break;
        case 2:
            Mapper_MapNTMirroring(base, NT_MIRROR_VERTICAL);
            break;
        case 3:
            Mapper_MapNTMirroring(base, NT_MIRROR_HORIZONTAL);
            break;
    }
    //PRG ROM bank
    switch ((mapper->control >> 2) & 0x3) {
        case 0:
        case 1:
            //Switch 32KB at $8000, ignore low bit of bank number
            Mapper_MapPRGROM_16k(base, 0, (mapper->prg_bank & 0xE) % Mapper_PRGROM16k_Size(base));
            Mapper_MapPRGROM_16k(base, 1, ((mapper->prg_bank & 0xE) + 1) % Mapper_PRGROM16k_Size(base));
            break;
        case 2:
            //Fix first bank at $8000 and switch 16KB bank at $C000
            Mapper_MapPRGROM_16k(base, 0, 0);
            Mapper_MapPRGROM_16k(base, 1, mapper->prg_bank % Mapper_PRGROM16k_Size(base));
            break;
        case 3:
            //Fix last bank at $C000 and switch 16KB bank at $8000
            Mapper_MapPRGROM_16k(base, 0, mapper->prg_bank % Mapper_PRGROM16k_Size(base));
            Mapper_MapPRGROM_16k(base, 1, Mapper_PRGROM16k_Size(base) - 1);
            break;
    }
    //CHR ROM bank
    if (mapper->control & 0x10) {
        //Switch two separate 4KB banks
        if (base->chr_rom_size == 0) {
            Mapper_MapCHRRAM_4k(base, 0, 0, mapper->chr_bank0 % Mapper_CHRRAM4k_Size(base));
            Mapper_MapCHRRAM_4k(base, 1, 1, mapper->chr_bank1 % Mapper_CHRRAM4k_Size(base));
        } else {
            Mapper_MapCHRROM_4k(base, 0, 0, mapper->chr_bank0 % Mapper_CHRROM4k_Size(base));
            Mapper_MapCHRROM_4k(base, 1, 1, mapper->chr_bank1 % Mapper_CHRROM4k_Size(base));
        }
    } else {
        //Switch 8KB at a time
        if (base->chr_rom_size == 0)
            Mapper_MapCHRRAM_4k(base, 0, 1, (mapper->chr_bank0 & 0xFE) % Mapper_CHRRAM4k_Size(base));
        else
            Mapper_MapCHRROM_4k(base, 0, 1, (mapper->chr_bank0 & 0xFE) % Mapper_CHRROM4k_Size(base));
    }
}

MapperBase *MMC1_New()
{
    MapperBase* mmc1 = calloc(1, sizeof(MMC1));
    mmc1->vtable = &MMC1_VTBL;
    return mmc1;
}

void MMC1_Init(MMC1 *mapper, const INESHeader *romHeader, FILE *romFile)
{
    MapperBase *base = (MapperBase*)mapper;

    Mapper_InitVRAM(base, 0x800);

    mapper->shift = 0b10000; //Initialize shift register with a 1 to detect when it is full
    mapper->control |= 0x0C; //Power on in the last PRG ROM bank
    MMC1_UpdateBanks(mapper);
}

void MMC1_RegWrite(MMC1 *mapper, uint16_t addr, uint8_t data)
{
    if (addr >= 0x8000) {
        bool srFull = mapper->shift & 1;
        mapper->shift >>= 1;
        mapper->shift |= (data & 1) << 4;
        if (data & 0x80) {
            //Bit 7 set: Clear shift register, set control to control OR $0C, fixing PRG ROM at $C000-$FFFF to the last bank
            mapper->shift = 0b10000;
            mapper->control |= 0x0C;
        } else if (srFull) {
            //SR full: Write to bank register
            if (addr <= 0x9FFF) {
                //$8000-$9FFF: Control
                mapper->control = mapper->shift;
            } else if (addr <= 0xBFFF) {
                //$A000-$BFFF: CHR bank 0
                mapper->chr_bank0 = mapper->shift;
            } else if (addr <= 0xDFFF) {
                //$C000-$DFFF: CHR bank 1
                mapper->chr_bank1 = mapper->shift;
            } else {
                //$E000-$FFFF: PRG bank
                mapper->prg_bank = mapper->shift;
            }
            mapper->shift = 0b10000;
        }
        MMC1_UpdateBanks(mapper);
    }
}
