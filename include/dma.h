#ifndef DMA_H
#define DMA_H

#include "cpu.h"
#include "apu.h"

typedef struct {
    bool oamdma, dmcdma;
    uint8_t oamdma_page;
    uint16_t dmcdma_addr;
} DMAController;

void DMA_Process(DMAController *dma, CPU *cpu, APU *apu, uint16_t dummyReadAddr);

void DMA_ScheduleOAMDMA(DMAController *dma, CPU *cpu, uint8_t oamdma_page);
void DMA_ScheduleDMCDMA(DMAController *dma, CPU *cpu, uint16_t addr);

#endif