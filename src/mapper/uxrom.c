#include "uxrom.h"
#include <stdlib.h>

MapperBase *UxROM_New()
{
    MapperBase* mapper = calloc(1, sizeof(UxROM));
    mapper->vtable = &UXROM_VTBL;
    return mapper;
}

void UxROM_Init(UxROM *mapper, const INESHeader *romHeader, FILE *romFile)
{
    MapperBase *base = (MapperBase*)mapper;

    Mapper_MapPRGROM_16k(base, false, 0);
    Mapper_MapPRGROM_16k(base, true, Mapper_PRGROM16k_Size(base) - 1);
    Mapper_InitVRAM(base, 0x800);
    Mapper_MapNTMirroring(base, romHeader->nt_mirroring ? NT_MIRROR_VERTICAL : NT_MIRROR_HORIZONTAL);
}

void UxROM_RegWrite(UxROM *mapper, uint16_t addr, uint8_t data)
{
    MapperBase *base = (MapperBase*)mapper;
    Mapper_MapPRGROM_16k(base, false, data % Mapper_PRGROM16k_Size(base));
}
