#ifndef ROM_H
#define ROM_H

#include <stdio.h>

/*
* iNES ROM file format header. Contains information on PRG/CHR ROM sizes, mapper, nametable mirroring, etc.
*
* To load a ROM, open a ROM file, use INES_ReadHeader() to read the header from the file,
* then use INES_ReadPRG() and INES_ReadCHR() to read the PRG ROM and CHR ROM data from the file
* using the ROM size information from the header.
*/
typedef struct {
    char header[16];

    unsigned prg_units; //Size of PRG ROM in 16KB units
    unsigned chr_units; //Size of CHR ROM in 8KB units
    unsigned prg_bytes; //Size of PRG ROM in bytes
    unsigned chr_bytes; //Size of CHR ROM in bytes

    int nt_mirroring;   //Nametable mirroring. 0: horizontal, 1: vertical
    int nt_alt;         //1: Alternative nametable layout
    int trainer;        //1: 512-byte trainer before PRG data
    unsigned mapper;    //Mapper number

    int has_battery_saves; //1: Cartridge contains battery-backed PRG RAM ($6000-7FFF) or other persistent memory
} INESHeader;

/**
* Read INES header from ROM file.
* @return 0 on success, -1 if the format is invalid.
*/
int INES_ReadHeader(INESHeader* ines, FILE* rom_file);
/**
* Read PRG ROM data from INES ROM file. Must initialize header using INES_ReadHeader() first to locate PRG ROM.
*
* @return Memory the size of header->prg_bytes allocated with malloc() containing PRG ROM data.
* The caller is responsible for freeing the memory with free().
*/

char* INES_ReadPRG(const INESHeader* ines, FILE* rom_file);
/**
* Read CHR ROM data from INES ROM file. Must initialize header using INES_ReadHeader() first to locate CHR ROM.
*
* @return Memory the size of header->chr_bytes allocated with malloc() containing CHR ROM data.
* The caller is responsible for freeing the memory with free().
*/
char* INES_ReadCHR(const INESHeader* ines, FILE* rom_file);


#endif