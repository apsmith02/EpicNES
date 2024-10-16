#ifndef ROM_H
#define ROM_H

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

    char* prg_rom;
    char* chr_rom;
} ROM;

//Returns 0 on success, -1 on error.
int ROM_Load(ROM* rom, const char* filename);

void ROM_Destroy(ROM* rom);

#endif