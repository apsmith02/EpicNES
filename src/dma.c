#include "dma.h"

void DMA_Process(DMAController *dma, CPU *cpu, APU *apu, uint16_t dummyReadAddr)
{
    CPU_Read(cpu, dummyReadAddr, ACCESS_DMA | ACCESS_DUMMY_READ); //DMA halt cycle
    if (dma->dmcdma)    CPU_Read(cpu, dummyReadAddr, ACCESS_DMA | ACCESS_DUMMY_READ); //DMC DMA: dummy cycle
    if (apu->state.fc_cycles % 2 == 1) CPU_Read(cpu, dummyReadAddr, ACCESS_DMA | ACCESS_DUMMY_READ); //Second half of an APU cycle (put): alignment cycle

    if (dma->oamdma) {
        uint16_t addr = (uint16_t)dma->oamdma_page << 8;
        for (int i = 0; i < 256; i++, addr++) {
            uint8_t data = CPU_Read(cpu, addr, ACCESS_DMA | ACCESS_READ); //(get) OAM DMA reads from address
            CPU_Write(cpu, 0x2004, data, ACCESS_DMA | ACCESS_WRITE); //(put) OAM DMA writes to $2004
        }
    } else if (dma->dmcdma) {
        APU_DMCLoadSample(apu, CPU_Read(cpu, dma->dmcdma_addr, ACCESS_DMA | ACCESS_READ));
    }

    dma->oamdma = dma->dmcdma = false;
}

void DMA_ScheduleOAMDMA(DMAController *dma, CPU *cpu, uint8_t oamdma_page)
{
    dma->oamdma = true;
    dma->oamdma_page = oamdma_page;
    CPU_ScheduleHalt(cpu);
}

void DMA_ScheduleDMCDMA(DMAController *dma, CPU *cpu, uint16_t addr)
{
    dma->dmcdma = true;
    dma->dmcdma_addr = addr;
    CPU_ScheduleHalt(cpu);
}
