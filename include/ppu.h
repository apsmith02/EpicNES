#ifndef PPU_H
#define PPU_H

#define NES_SCREEN_W 256
#define NES_SCREEN_H 240

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} RGBAPixel;

static const RGBAPixel PPUCOLORS[64] = {
	{84,84,84,255},   {0,30,116,255},   {8,16,144,255},   {48,0,136,255},   {68,0,100,255},   {92,0,48,255},    {84,4,0,255},     {60,24,0,255},    
    {32,42,0,255},    {8,58,0,255},     {0,64,0,255},     {0,60,0,255},     {0,50,60,255},    {0,0,0,255},      {0,0,0,255},      {0,0,0,255},
	{152,150,152,255},{8,76,196,255},   {48,50,236,255},  {92,30,228,255},  {136,20,176,255}, {160,20,100,255}, {152,34,32,255},  {120,60,0,255},   
    {84,90,0,255},    {40,114,0,255},   {8,124,0,255},    {0,118,40,255},   {0,102,120,255},  {0,0,0,255},      {0,0,0,255},      {0,0,0,255},
	{236,238,236,255},{76,154,236,255}, {120,124,236,255},{176,98,236,255}, {228,84,236,255}, {236,88,180,255}, {236,106,100,255},{212,136,32,255}, 
    {160,170,0,255},  {116,196,0,255},  {76,208,32,255},  {56,204,108,255}, {56,180,204,255}, {60,60,60,255},   {0,0,0,255},      {0,0,0,255},
	{236,238,236,255},{168,204,236,255},{188,188,236,255},{212,178,236,255},{236,174,236,255},{236,174,212,255},{236,180,176,255},{228,196,144,255},
    {204,210,120,255},{180,222,120,255},{168,226,144,255},{152,226,180,255},{160,214,228,255},{160,162,160,255},{0,0,0,255},      {0,0,0,255}
};


typedef uint8_t(*PPUReadFn)(void*, uint16_t);
typedef void(*PPUWriteFn)(void*, uint16_t, uint8_t);

typedef enum {
    //VRAM address increment per read/write of PPUDATA (0: add 1, 1: add 32)
    PPUCTRL_INC         = 1 << 2,
    //Sprite pattern table address for 8x8 sprites (0: $0000, 1: $1000)
    PPUCTRL_SPRTABLE    = 1 << 3,
    //Background pattern table address (0: $0000, 1: $1000)
    PPUCTRL_BGTABLE     = 1 << 4,
    //Sprite size (0: 8x8, 1: 8x16)
    PPUCTRL_SPRSIZE     = 1 << 5,
    //Vblank NMI enable
    PPUCTRL_NMI         = 1 << 7
} PPUCtrlFlags;

typedef enum {
    PPUMASK_BG          = 1 << 3,
    PPUMASK_SPR         = 1 << 4,

    PPUMASK_RENDER      = PPUMASK_BG | PPUMASK_SPR
} PPUMaskFlags;

typedef enum {
    PPUSTATUS_SPROVERFLOW   = 1 << 5,
    PPUSTATUS_SPR0HIT       = 1 << 6,
    PPUSTATUS_VBLANK        = 1 << 7
} PPUStatusFlags;

typedef enum {
    OAMATTR_PALETTE     = 0x03,
    OAMATTR_PRIORITY    = 1 << 5,
    OAMATTR_FLIP_H      = 1 << 6,
    OAMATTR_FLIP_V      = 1 << 7
} OAMAttributeFlags;


typedef struct {
    uint8_t y;
    uint8_t tile;
    uint8_t attributes;
    uint8_t x;
} OAMSprite;

typedef struct {
    uint8_t paletteRam[32];
    union {
        uint8_t oam[256];
        OAMSprite oamSprites[64];
    };

    uint8_t ppuctrl;
    uint8_t ppumask;
    uint8_t ppustatus;

    uint8_t readBuffer;
    uint16_t v;
    uint16_t t;
    uint8_t x;
    uint8_t w;

    uint8_t oamaddr;
    
    int cycle;
    int scanline;

    //Latches and shift registers used for rendering

    uint8_t ntByte;
    uint8_t atByte;
    //Next tile's pattern data, 2 bit planes. Transferred to bgShift registers every 8th render cycle
    uint8_t bgPattern0, bgPattern1;
    //Background shift registers, 2 bit planes. Shifted once per render cycle
    uint16_t bgShift0, bgShift1;
    //1-bit attribute latch, 2 bit planes. Shifted into attrShift registers
    uint8_t attrLatch0, attrLatch1;
    //Attribute shift registers, 2 bit planes. Shifted once per render cycle
    uint8_t attrShift0, attrShift1;

    //Secondary OAM and sprite patterns used for rendering one scanline

    union {
        uint8_t secondaryOamBytes[64];
        OAMSprite secondaryOam[8];
    };
    int secondaryOamCount;
    bool scanlineHasSpr0; //Used for sprite 0 hit detection
    uint8_t sprPattern0[8];
    uint8_t sprPattern1[8];

    //Frame count. Incremented when a full picture has been rendered for a PPU frame.
    unsigned long long frames;
} PPUState;

typedef struct {
    //PPU pixel output buffer
    RGBAPixel pixelBuffer[NES_SCREEN_H][NES_SCREEN_W];

    //Callbacks

    PPUReadFn readfn;
    PPUWriteFn writefn;
    void* fndata;

    //State
    PPUState state;
} PPU;


void PPU_Init(PPU* ppu, PPUReadFn readfn, PPUWriteFn writefn, void* fndata);

void PPU_PowerOn(PPU* ppu);

void PPU_Reset(PPU* ppu);

uint8_t PPU_RegRead(PPU* ppu, uint16_t addr);

void PPU_RegWrite(PPU* ppu, uint16_t addr, uint8_t data);

void PPU_Cycle(PPU* ppu);

bool PPU_NMISignal(PPU* ppu);

#endif