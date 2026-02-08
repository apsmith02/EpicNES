#include "ppu.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>


/* PRIVATE FUNCTIONS */

//Read PPU memory (Does not read palette RAM)
uint8_t Read(PPU* ppu, uint16_t addr) {
    uint8_t val = ppu->readfn(ppu->fndata, addr % 0x4000);
    return val;
}

//Write PPU memory (Does not write palette RAM)
void Write(PPU* ppu, uint16_t addr, uint8_t data) {
    ppu->writefn(ppu->fndata, addr % 0x4000, data);
}

//Fetch NT, AT, and BG and sprite patterns. Updates v according to visible frame timings. Shifts and reloads pixel shift registers.
void VRAMFetch(PPU* ppu);
//Shift pixel shift registers
void ShiftPixels(PPU* ppu);
//Reload pixel shift registers
void ReloadPixels(PPU* ppu);
//Render a pixel from the pixel shift registers at (x,y) on the pixel buffer.
void RenderPixel(PPU* ppu, int x, int y);

//Get the 2-bit pattern pixel of a sprite from secondary OAM at a given x position on screen
int GetSprPatternPixel(PPU* ppu, int sprite, int x);

void FetchNT(PPU* ppu);
void FetchAT(PPU* ppu);
void FetchBGlsb(PPU* ppu);
void FetchBGmsb(PPU* ppu);
void FetchSprLsb(PPU* ppu);
void FetchSprMsb(PPU* ppu);

//Do secondary OAM clear and sprite evaluation all at once and without setting the sprite overflow flag because I'm lazy
void QuickSpriteEval(PPU* ppu);

//Increment coarse X scroll of v register
void IncHoriV(PPU* ppu) {
    //Based on pseudocode found on https://www.nesdev.org/wiki/PPU_scrolling#Coarse_X_increment
    PPUState* state = &ppu->state;
    if ((state->v & 0x001F) == 31){
        state->v &= ~0x001F;
        state->v ^= 0x0400;
    } else
        state->v++;
}

//Increment Y scroll of v register
void IncVertV(PPU* ppu) {
    //Based on pseudocode found on https://www.nesdev.org/wiki/PPU_scrolling#Y_increment
    PPUState* state = &ppu->state;
    if ((state->v & 0x7000) != 0x7000)
        state->v += 0x1000;
    else {
        state->v &= ~0x7000;
        int y = (state->v & 0x03E0) >> 5;
        if (y == 29) {
            y = 0;
            state->v ^= 0x0800;
        } else if (y == 31)
            y = 0;
        else
            y++;
        state->v = (state->v & ~0x03E0) | (y << 5);
    }
}

//Copy horizontal components from t to v
void HoriVCopyT(PPU* ppu) {
    PPUState* state = &ppu->state;
    state->v &= ~0x41F;
    state->v |= (state->t & 0x41F);
}

//Copy vertical components from t to v
void VertVCopyT(PPU* ppu) {
    PPUState* state = &ppu->state;
    state->v &= ~0x7BE0;
    state->v |= (state->t & 0x7BE0);
}


/* FUNCTION DEFINITIONS */

void PPU_Init(PPU* ppu, PPUReadFn readfn, PPUWriteFn writefn, void* fndata)
{
    memset(ppu, 0, sizeof(PPU));
    ppu->readfn = readfn;
    ppu->writefn = writefn;
    ppu->fndata = fndata;
}

void PPU_PowerOn(PPU* ppu) {
    PPUState* state = &ppu->state;

    state->ppuctrl = state->ppumask = state->ppustatus = state->oamaddr =
    state->v = state->t = state->w = state->x = state->readBuffer = 0;
    state->frames = 1;
    state->cycle = state->scanline = 0;
}

void PPU_Reset(PPU* ppu) {
    PPUState* state = &ppu->state;

    state->ppuctrl = state->ppumask = state->w = state->x = state->t =
    state->readBuffer = 0;
}

uint8_t PPU_RegRead(PPU *ppu, uint16_t addr)
{
    PPUState* state = &ppu->state;
    switch (addr % 8) {
        //PPUSTATUS
        case 2:
        {
            uint8_t status = state->ppustatus & 0xE0;
            state->ppustatus &= ~PPUSTATUS_VBLANK;
            state->w = 0;
            return status;
        }
        //PPUDATA
        case 7:
        {
            uint8_t ppuData = (state->v < 0x3F00) ? state->readBuffer : state->paletteRam[state->v % 32];
            state->readBuffer = Read(ppu, state->v);
            state->v += (state->ppuctrl & PPUCTRL_INC) ? 32 : 1;
            return ppuData;
        }
        default:
            return 0;
    }
}

void PPU_RegWrite(PPU *ppu, uint16_t addr, uint8_t data)
{
    PPUState* state = &ppu->state;
    switch (addr % 8) {
        //PPUCTRL
        case 0:
            state->ppuctrl = data;
            state->t &= ~0xC00;
            state->t |= ((uint16_t)data & 0x3) << 10;
            break;
        //PPUMASK
        case 1:
            state->ppumask = data;
            break;
        //OAMADDR
        case 3:
            state->oamaddr = data;
            break;
        //OAMDATA
        case 4:
            state->oam[state->oamaddr++] = data;
            break;
        //PPUSCROLL
        case 5:
            if (state->w == 0) {
                //1st write: set x scroll
                state->t &= ~0x1F;
                state->t |= data >> 3;
                state->x = data & 0x7;
                state->w = 1;
            } else {
                //2nd write: set y scroll
                state->t &= ~0x73E0;
                state->t |= ((uint16_t)data & 0x7) << 12;
                state->t |= ((uint16_t)data & 0xF8) << 2;
                state->w = 0;
            }
            break;
        //PPUADDR
        case 6:
            if (state->w == 0) {
                //high byte
                state->t &= 0x7F;
                state->t |= (uint16_t)data << 8;
                state->w = 1;
            } else {
                //low byte
                state->t &= 0xFF00;
                state->t |= data;
                state->v = state->t;
                state->w = 0;
            }
            break;
        //PPUDATA
        case 7:
            if (state->v < 0x3F00)
                Write(ppu, state->v, data); //Write VRAM ($0000-$3EFF)
            else {
                state->paletteRam[state->v % 32] = data; //Write palette ($3F00-$3F1F, mirrors up to $3FFF)
                if (state->v % 4 == 0) //mirror $3Fx0, $3Fx4, $3Fx8, $3FxC
                    state->paletteRam[(state->v + 16) % 32] = data;
            }
            state->v += (state->ppuctrl & PPUCTRL_INC) ? 32 : 1;
            break;
        default:
            break;
    }
}

void PPU_Cycle(PPU *ppu)
{
    PPUState* state = &ppu->state;

    //Render pixel
    if (state->scanline < NES_SCREEN_H && 1 <= state->cycle && state->cycle <= NES_SCREEN_W) {
        RenderPixel(ppu, state->cycle - 1, state->scanline);
    }

    //Do frame rendering operations (render fetches, sprite evalution, flag updates) according
    //to the NTSC PPU frame timing diagram: https://www.nesdev.org/w/images/default/4/4f/Ppu.svg
    if (state->scanline <= 239) { //Visible scanlines (0-239)
        if (state->ppumask & PPUMASK_RENDER) {
            VRAMFetch(ppu);
            if (state->cycle == 256) //Cycle 65-256: Sprite evaluation (not cycle accurate; currently does sprite evaluation all at once on cycle 256)
                QuickSpriteEval(ppu);
        }
    } else if (state->scanline == 241) {
        if (state->cycle == 1) { //Post-render scanline 241, cycle 1: Set VBlank flag
            state->ppustatus |= PPUSTATUS_VBLANK;
        }
    } else if (state->scanline == 261) { //Pre-render scanline (261)
        if (state->ppumask & PPUMASK_RENDER) {
            VRAMFetch(ppu);
            if (280 <= state->cycle && state->cycle <= 304) { //Cycles 280-304: vert(v)=vert(t) each tick
                VertVCopyT(ppu);
            }
        }
        if (state->cycle == 1) { //Scanline 261, Cycle 1: Clear VBlank, Sprite 0, Overflow
            state->ppustatus = 0;
        }
    }
    
    //Increment cycle, scanline and frame counters. Skip (0,0) on odd frames when rendering is enabled.
    if (++state->cycle > 340) {
        state->cycle = 0;
        if (++state->scanline > 261) {
            state->scanline = 0;
            if ((state->ppumask & PPUMASK_RENDER) && state->frames % 2 == 1) { //Skip first cycle on rendering + odd
                state->cycle++;
            }
        } else if (state->scanline == 240) {
            state->frames++;
        }
    }
}

bool PPU_NMISignal(PPU *ppu)
{
    PPUState* state = &ppu->state;
    return 
        (state->ppustatus & PPUSTATUS_VBLANK) == PPUSTATUS_VBLANK &&
        (state->ppuctrl & PPUCTRL_NMI) == PPUCTRL_NMI;
}

void VRAMFetch(PPU *ppu)
{
    PPUState* state = &ppu->state;

    //Cycles 1-256 and 321-336: Fetch NT/AT/BG patterns, inc hori(v), shift pixel shift registers and reload them every 8th cycle
    if ((1 <= state->cycle && state->cycle <= 256) || (321 <= state->cycle && state->cycle <= 336)) {
        ShiftPixels(ppu);
        switch (state->cycle % 8) {
                break;
            case 2:
                FetchNT(ppu);
                break;
            case 4:
                FetchAT(ppu);
                break;
            case 6:
                FetchBGlsb(ppu);
                break;
            case 0:
                FetchBGmsb(ppu);
                ReloadPixels(ppu);
                IncHoriV(ppu);
                if (state->cycle == 256) {
                    IncVertV(ppu);
                }
                break;
            default: break;
        }

    }
    else if (257 <= state->cycle && state->cycle <= 320) { //Cycles 257-320: Hori(v)=hori(t) on 257, then fetch sprite patterns, do garbage NT fetches between sprite fetches
        if (state->cycle == 257) {
            HoriVCopyT(ppu);
        }
        switch (state->cycle % 8) {
            case 2:
            case 4:
                FetchNT(ppu); //Garbage NT fetches
                break;
            case 6:
                FetchSprLsb(ppu);
                break;
            case 0:
                FetchSprMsb(ppu);
                break;
            default: break;
        }
    }
    else if (state->cycle >= 337) { //Cycles 337-340: Unused NT fetches
        if (state->cycle % 2 == 0)
            FetchNT(ppu);
    }
}

void RenderPixel(PPU *ppu, int x, int y)
{
    assert(0 <= x && x < NES_SCREEN_W);
    assert(0 <= y && y < NES_SCREEN_H);

    PPUState* state = &ppu->state;
    int pixel = 0;

    //Select background pixel
    if (state->ppumask & PPUMASK_BG) {
        //fine x selects bg shift register bits
        pixel =
            state->bgShift0  >> (15 - state->x) & 0x1 |
            state->bgShift1  >> (15 - state->x) << 1 & 0x2 |
            state->attrShift0 >> (7 - state->x) << 2 & 0x4 |
            state->attrShift1 >> (7 - state->x) << 3 & 0x8;
        if ((pixel & 0x03) == 0)
            pixel = 0;
    }

    if (state->ppumask & PPUMASK_SPR) {
        //Find first opaque sprite at x
        int sprite = 0;
        int sprPixel = 0;
        OAMSprite* oamSprite = NULL;
        while (sprite < state->secondaryOamCount && sprPixel == 0) {
            sprPixel = GetSprPatternPixel(ppu, sprite, x);
            oamSprite = &state->secondaryOam[sprite];
            sprite++;
        }
        sprite--;
        //Sprite pixel at x?
        if (sprPixel > 0) {
            //Sprite 0 hit
            if (state->scanlineHasSpr0 && sprite == 0 && pixel > 0) {
                state->ppustatus |= PPUSTATUS_SPR0HIT;
            }

            //If the background pixel is transparent or the sprite has foreground priority, render it instead of the background pixel
            if (pixel == 0 || (oamSprite->attributes & OAMATTR_PRIORITY) == 0) {
                sprPixel |= (oamSprite->attributes & OAMATTR_PALETTE) << 2;
                sprPixel |= 0x10; //Select sprite palettes
                pixel = sprPixel;
            }
        }
    }

    ppu->pixelBuffer[y][x] = PPUCOLORS[state->paletteRam[pixel]];
}

void ShiftPixels(PPU *ppu)
{
    PPUState* state = &ppu->state;

    state->bgShift0 <<= 1;
    state->bgShift1 <<= 1;
    state->attrShift0 <<= 1;
    state->attrShift0 |= state->attrLatch0;
    state->attrShift1 <<= 1;
    state->attrShift1 |= state->attrLatch1;
}

void ReloadPixels(PPU *ppu)
{
    PPUState* state = &ppu->state;

    state->bgShift0 &= 0xFF00;
    state->bgShift0 |= state->bgPattern0;
    state->bgShift1 &= 0xFF00;
    state->bgShift1 |= state->bgPattern1;

    int attrPos = 
        ((state->v & 0x02) ? 2 : 0) +
        ((state->v & 0x40) ? 4 : 0);
    state->attrLatch0 = state->atByte >> attrPos & 1;
    state->attrLatch1 = state->atByte >> (attrPos+1) & 1;
}

int GetSprPatternPixel(PPU *ppu, int sprite, int x)
{
    assert(0 <= sprite && sprite < 8);
    assert(0 <= x < 256);
    PPUState* state = &ppu->state;
    OAMSprite* oamSprite = &state->secondaryOam[sprite];
    
    int pixel = 0;
    if (oamSprite->x <= x && x < oamSprite->x + 8) {
        int pxSelect = x - oamSprite->x;
        if ((oamSprite->attributes & OAMATTR_FLIP_H) == 0)
            pxSelect = 7 - pxSelect;
        uint8_t pattern0 = state->sprPattern0[sprite] >> pxSelect;
        uint8_t pattern1 = state->sprPattern1[sprite] >> pxSelect;
        pixel = pattern0 & 0x1 | (pattern1 << 1) & 0x2;
    }

    return pixel;
}

void FetchNT(PPU *ppu)
{
    PPUState* state = &ppu->state;
    state->ntByte = Read(ppu, 0x2000 | (state->v & 0x0FFF));
}

void FetchAT(PPU *ppu)
{
    PPUState* state = &ppu->state;
    state->atByte = Read(ppu, 0x23C0 | (state->v & 0x0C00) | ((state->v >> 4) & 0x38) | ((state->v >> 2) & 0x07));
}

void FetchBGlsb(PPU *ppu)
{
    PPUState* state = &ppu->state;
    uint16_t addr = 
        state->v >> 12  |                                 //fine y offset
        (uint16_t)state->ntByte << 4 |                    //tile number from nt
        ((state->ppuctrl & PPUCTRL_BGTABLE) ? 0x1000 : 0);//half of pattern table
    ppu->state.bgPattern0 = Read(ppu, addr);
}

void FetchBGmsb(PPU *ppu)
{
    PPUState* state = &ppu->state;
    uint16_t addr = 
        state->v >> 12  |                                 //fine y offset
        0x8 |                                             //msb bit plane
        (uint16_t)state->ntByte << 4 |                    //tile number from nt
        ((state->ppuctrl & PPUCTRL_BGTABLE) ? 0x1000 : 0);//half of pattern table
    ppu->state.bgPattern1 = Read(ppu, addr);
}

void FetchSprPattern(PPU *ppu, bool msbp) {
    PPUState* state = &ppu->state;

    int sprIndex = (state->cycle - 257) / 8;
    
    if (sprIndex < state->secondaryOamCount) {
        OAMSprite* sprite = &state->secondaryOam[sprIndex];
        
        int yOffset = state->scanline - sprite->y;
        if (sprite->attributes & OAMATTR_FLIP_V)
            yOffset = ((state->ppuctrl & PPUCTRL_SPRSIZE) ? 15 : 7) - yOffset;
        
        uint16_t addr;
        if (!(state->ppuctrl & PPUCTRL_SPRSIZE)) {
            //8x8 sprite
            addr =
                yOffset & 0x7 |                                     //fine y offset
                (msbp ? 0x8 : 0) |                                  //bit plane
                (uint16_t)sprite->tile << 4 |                       //tile number
                ((state->ppuctrl & PPUCTRL_SPRTABLE) ? 0x1000 : 0); //half of pattern table
        } else {
            //8x16 sprite
            uint8_t tile = sprite->tile & 0xFE;
            if (yOffset >= 8)
            {
                tile++;
            }
            addr =
                yOffset & 0x7 |                         //fine y offset
                (msbp ? 0x8 : 0) |                      //bit plane
                (uint16_t)tile << 4 |                   //tile number
                ((sprite->tile & 0x1) ? 0x1000 : 0);    //half of pattern table
        }
        if (!msbp)
            state->sprPattern0[sprIndex] = Read(ppu, addr);
        else
            state->sprPattern1[sprIndex] = Read(ppu, addr);
    }
}

void FetchSprLsb(PPU *ppu)
{
    FetchSprPattern(ppu, 0);
}

void FetchSprMsb(PPU *ppu)
{
    FetchSprPattern(ppu, 1);
}

void QuickSpriteEval(PPU *ppu)
{
    PPUState* state = &ppu->state;

    //Clear secondary OAM
    for (int i = 0; i < 64; i++) {
        state->secondaryOamBytes[i] = 0xFF;
    }
    state->secondaryOamCount = 0;

    state->scanlineHasSpr0 = false;

    //If scanline 239, don't do sprite evaluation for next scanline or those sprites will be mistakenly drawn to scanline 0 next frame
    if (state->scanline < 239) {
        //Iterate over OAM, add first 8 sprites in range of scanline to secondary OAM
        for (int i = 0; i < 64 && state->secondaryOamCount < 8; i++) {
            if (state->oamSprites[i].y <= state->scanline && state->scanline < state->oamSprites[i].y + ((state->ppuctrl & PPUCTRL_SPRSIZE) ? 16 : 8)) {
                state->secondaryOam[state->secondaryOamCount++] = state->oamSprites[i];
                if (i == 0)
                    state->scanlineHasSpr0 = true;
            }
        }
    }
}
