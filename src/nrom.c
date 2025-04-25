#include "nrom.h"
#include "emulator.h"

void NROM_Init(NROM *mapper, Emulator* emu)
{
    INESHeader* ines = &emu->rom_ines;
    Memory* memory = &emu->memory;

    Mem_Create_VRAM(memory, 0x800);
    
    Mem_CPUMapPages(memory, 0x80, 0xBF, MEM_PRG_ROM, 0);
    Mem_CPUMapPages(memory, 0xC0, 0xFF, MEM_PRG_ROM, 0x40 % (ines->prg_bytes >> 8));
    Mem_PPUMapPages(memory, 0x00, 0x1F, MEM_CHR_ROM, 0);
    Mem_MapNTMirror(memory, ines->nt_mirroring == 0 ? NT_HORIZONTAL : NT_VERTICAL);
}
