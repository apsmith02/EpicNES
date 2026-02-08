#ifndef NES_DEFS_H
#define NES_DEFS_H

#define NTSC_MASTER_CLOCK 21.477272 //NTSC master clock rate in MHz
#define PAL_MASTER_CLOCK 21.477272 //PAL master clock rate in MHz

#define NTSC_CPU_CLOCK (NTSC_MASTER_CLOCK / 12) //NTSC CPU clock rate in MHz
#define PAL_CPU_CLOCK (PAL_MASTER_CLOCK / 12) //PAL CPU clock rate in MHz

#define NTSC_PPU_CYCLES_PER_CPU_CYCLE 3
#define PAL_PPU_CYCLES_PER_CPU_CYCLE 3.2

#endif