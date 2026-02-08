#include "rom.h"
#include <stdlib.h>
#include <string.h>

int INES_ReadHeader(INESHeader *ines, FILE *rom_file)
{
    fseek(rom_file, 0, SEEK_SET);
    fread(&ines->header[0], 1, sizeof(ines->header), rom_file);

    //Read header
    //Bytes 0-3: Constant ASCII "NES" followed by MS-DOS EOF (0x1a)
    static const char hdrConstant[4] = {'N', 'E', 'S', '\x1a'};
    if (memcmp(&ines->header[0], hdrConstant, 4))
        return -1;
    
    //Bytes 4-5: PRG and CHR size
    ines->prg_units = ines->header[4];
    ines->chr_units = ines->header[5];
    ines->prg_bytes = ines->prg_units * 16384;
    ines->chr_bytes = ines->chr_units * 8192;
    
    //Byte 6: Mapper, mirroring, battery, trainer
    ines->nt_mirroring  = ines->header[6] & 1;         //Bit 0: NT Mirroring
    ines->has_battery_saves = ines->header[6] & 2;      //Bit 1: Battery backed PRG RAM (usually at $6000-$7FFF) or other persistent memory
    ines->trainer       = ines->header[6] >> 2 & 1;    //Bit 2: 512-byte trainer before PRG data
    ines->nt_alt        = ines->header[6] >> 3 & 1;    //Bit 3: Alternative NT layout   
    ines->mapper        = ines->header[6] >> 4;        //Bits 4-7: Lower nibble of mapper number

    //If bytes 7-15 read "DiskDude!", then the iNES version is most likely archaic iNES, therefore bytes 7-15 are unused
    if (!strncmp(&ines->header[7], "DiskDude!", 9))
        return 0;
    
    //Byte 7: Mapper high nibble
    ines->mapper |= ines->header[7] & 0xF0;

    return 0;
}

char *INES_ReadPRG(const INESHeader *ines, FILE *rom_file)
{
    fseek(rom_file, 16 + (ines->trainer ? 512 : 0), SEEK_SET);
    char* prg_rom = malloc(ines->prg_bytes);
    fread(prg_rom, 1, ines->prg_bytes, rom_file);
    return prg_rom;
}

char *INES_ReadCHR(const INESHeader *ines, FILE *rom_file)
{
    fseek(rom_file, 16 + (ines->trainer ? 512 : 0) + ines->prg_bytes, SEEK_SET);
    char* chr_rom = malloc(ines->chr_bytes);
    fread(chr_rom, 1, ines->chr_bytes, rom_file);
    return chr_rom;
}
