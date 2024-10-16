#ifndef PPU_H
#define PPU_H

#define NES_SCREEN_W 256
#define NES_SCREEN_H 240

#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RGBAPixel;

typedef uint8_t(*PPUReadFn)(void*, uint16_t);
typedef void(*PPUWriteFn)(void*, uint16_t, uint8_t);
typedef void(*PPUNMIFn)(void*);

typedef struct {
    
} PPUState;

typedef struct {
    //PPU pixel output buffer
    RGBAPixel pixel_buffer[NES_SCREEN_H][NES_SCREEN_W];

    //Callbacks

    PPUReadFn read_fn;
    PPUWriteFn write_fn;
    void* rw_fndata;

    PPUNMIFn nmi_fn;
    void* nmi_fndata;

    //
} PPU;



#endif