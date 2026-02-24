#include "nrom.h"
#include "mapper.h"

int NROM_Init(Mapper* mapper, const INESHeader* ines) {
    MapPRGPages(mapper, 0x80, 0xFF, 0x0, PRGTYPE_PRG_ROM);
    MapCHRPages(mapper, 0x00, 0x1F, 0x0, CHRTYPE_DEFAULT);
    MapNametable(mapper, ines->nt_mirroring ? NT_MIRROR_VERTICAL : NT_MIRROR_HORIZONTAL);
    
    return 0;
}