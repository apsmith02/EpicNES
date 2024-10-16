#include "rom.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int ROM_Load(ROM *rom, const char *filename)
{
    memset(rom, 0, sizeof(ROM));

    FILE* file = fopen(filename, "rb");
    if (!file)
        return -1;
    
    char* header = rom->header;
    fread(header, 1, sizeof(rom->header), file);

    //Bytes 0-3: Constant ASCII "NES" followed by MS-DOS EOF (0x1a)
    static const char hdrConstant[4] = {'N', 'E', 'S', '\x1a'};
    if (memcmp(&header[0], hdrConstant, 4))
        return -1;

    //Bytes 4-5: PRG and CHR size
    rom->prg_units = rom->header[4];
    rom->chr_units = rom->header[5];
    rom->prg_bytes = rom->prg_units * 16384;
    rom->chr_bytes = rom->chr_units * 8192;

    //Byte 6: Mapper, mirroring, battery, trainer
    rom->nt_mirroring = rom->header[6] & 1;         //Bit 0: NT Mirroring
    rom->trainer =      rom->header[6] >> 2 & 1;    //Bit 2: 512-byte trainer before PRG data
    rom->nt_alt =       rom->header[6] >> 3 & 1;    //Bit 3: Alternative NT layout   
    rom->mapper =       rom->header[6] >> 4;        //Bits 4-7: Lower nibble of mapper number

    //Byte 7: Mapper, VS/Playchoice, NES 2.0
    rom->mapper |=      rom->header[7] & 0xF0;      //Bits 4-7: Upper nibble of mapper number

    fseek(file, 16 + (rom->trainer ? 512 : 0), SEEK_SET);

    //Load PRG and CHR ROM data
    rom->prg_rom = malloc(rom->prg_bytes);
    rom->chr_rom = malloc(rom->chr_bytes);
    fread(rom->prg_rom, 1, rom->prg_bytes, file);
    fread(rom->chr_rom, 1, rom->chr_bytes, file);

    fclose(file);

    return 0;
}

void ROM_Destroy(ROM *rom)
{
    if (rom->prg_rom) free(rom->prg_rom);
    if (rom->chr_rom) free(rom->chr_rom);
}
