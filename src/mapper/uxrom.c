#include "uxrom.h"
#include "mapper.h"

int UxROM_Init(Mapper *mapper, const INESHeader *ines)
{
    mapper->f.WriteRegisters = &UxROM_WriteRegisters;

    MapPRGPages(mapper, 0xC0, 0xFF, -0x4000 / 0x100, PRGTYPE_PRG_ROM);
    MapCHRPages(mapper, 0x00, 0x1F, 0x0, CHRTYPE_DEFAULT);
    MapNametable(mapper, ines->nt_mirroring ? NT_MIRROR_VERTICAL : NT_MIRROR_HORIZONTAL);

    UxROM_UpdateBanks(mapper);
    return 0;
}

void UxROM_WriteRegisters(Mapper *mapper, uint16_t addr, uint8_t data)
{
    mapper->uxrom.bank = data;
    UxROM_UpdateBanks(mapper);
}

void UxROM_UpdateBanks(Mapper *mapper)
{
    MapPRGPages(mapper, 0x80, 0xBF, mapper->uxrom.bank * 0x4000 / 0x100, PRGTYPE_PRG_ROM);
}
