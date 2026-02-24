#include "mmc1.h"
#include "mapper.h"

int MMC1_Init(Mapper *mapper, const INESHeader *ines)
{
    MMC1* mmc1 = &mapper->mmc1;

    mapper->f.WriteRegisters = &MMC1_WriteRegisters;
    
    mmc1->shift = 0b10000; //Initialize shift register with a 1 to detect when it is full
    mmc1->control |= 0x0C; //Power on in the last PRG ROM bank

    MMC1_UpdateBanks(mapper);

    Mapper_ResizePRGRAM(mapper, 0x2000);
    MapPRGPages(mapper, 0x60, 0x7F, 0x0, PRGTYPE_PRG_RAM);

    return 0;
}

void MMC1_WriteRegisters(Mapper *mapper, uint16_t addr, uint8_t data)
{
    MMC1* mmc1 = &mapper->mmc1;

    if (addr >= 0x8000) {
        bool srFull = mmc1->shift & 1;
        mmc1->shift >>= 1;
        mmc1->shift |= (data & 1) << 4;
        if (data & 0x80) {
            //Bit 7 set: Clear SR, fix PRG ROM at $C000-$FFFF to last bank
            mmc1->shift = 0b10000;
            mmc1->control |= 0x0C;
        } else if (srFull) {
            //SR full: Write to bank register
            if (addr <= 0x9FFF)         mmc1->control = mmc1->shift;    //$8000-$9FFF
            else if (addr <= 0xBFFF)    mmc1->chr_bank0 = mmc1->shift;  //$A000-$BFFF
            else if (addr <= 0xDFFF)    mmc1->chr_bank1 = mmc1->shift;  //$C000-$DFFF
            else                        mmc1->prg_bank = mmc1->shift;   //$E000-$FFFF

            mmc1->shift = 0b10000;
            MMC1_UpdateBanks(mapper);
        }
    }
}

void MMC1_UpdateBanks(Mapper *mapper)
{
    MMC1* mmc1 = &mapper->mmc1;

    switch (mmc1->control & 0x3) {
        case 0: MapNametable(mapper, NT_MIRROR_ONESCREEN_A); break;
        case 1: MapNametable(mapper, NT_MIRROR_ONESCREEN_B); break;
        case 2: MapNametable(mapper, NT_MIRROR_VERTICAL); break;
        case 3: MapNametable(mapper, NT_MIRROR_HORIZONTAL); break;
    }

    switch ((mmc1->control >> 2) & 0x3) {
        case 0:
        case 1: MapPRGPages(mapper, 0x80, 0xFF, (mmc1->prg_bank & 0x0E) * 0x4000 / 0x100, PRGTYPE_PRG_ROM);
                break;
        case 2: MapPRGPages(mapper, 0x80, 0xBF, 0x0, PRGTYPE_PRG_ROM);
                MapPRGPages(mapper, 0xC0, 0xFF, (mmc1->prg_bank & 0x0F) * 0x4000 / 0x100, PRGTYPE_PRG_ROM);
                break;
        case 3: MapPRGPages(mapper, 0x80, 0xBF, (mmc1->prg_bank & 0x0F) * 0x4000 / 0x100, PRGTYPE_PRG_ROM);
                MapPRGPages(mapper, 0xC0, 0xFF, -0x4000 / 0x100, PRGTYPE_PRG_ROM);
                break;
    }

    if (!(mmc1->control & 0x10)) {
        MapCHRPages(mapper, 0x00, 0x1F, (mmc1->chr_bank0 & 0x1E) * 0x1000 / 0x100, CHRTYPE_DEFAULT);
    } else {
        MapCHRPages(mapper, 0x00, 0x0F, (mmc1->chr_bank0 & 0x1F) * 0x1000 / 0x100, CHRTYPE_DEFAULT);
        MapCHRPages(mapper, 0x10, 0x1F, (mmc1->chr_bank1 & 0x1F) * 0x1000 / 0x100, CHRTYPE_DEFAULT);
    }
}
