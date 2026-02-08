#include "nrom.h"
#include <stdlib.h>
#include <string.h>

MapperBase *NROM_New()
{
    MapperBase* mapper = calloc(1, sizeof(NROM));
    mapper->vtable = &NROM_VTBL;
    return mapper;
}

void NROM_Init(NROM *mapper, const INESHeader *romHeader, FILE *romFile)
{
    MapperBase* base = &mapper->base;
    
    Mapper_InitVRAM(base, 0x800);

    Mapper_MapPRGROMPages(base, 0x80, 0xBF, 0x0);
    Mapper_MapPRGROMPages(base, 0xC0, 0xFF, 0x4000 % base->prg_rom_size);
    Mapper_MapNTMirroring(base, romHeader->nt_mirroring ? NT_MIRROR_VERTICAL : NT_MIRROR_HORIZONTAL);
}
