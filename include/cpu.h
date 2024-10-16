#ifndef CPU_H
#define CPU_H

#include <stdint.h>

//CPU status flags
typedef enum {
    CPU_FLAG_C          = 1 << 0,
    CPU_FLAG_Z          = 1 << 1,
    CPU_FLAG_I          = 1 << 2,
    CPU_FLAG_D          = 1 << 3,
    CPU_FLAG_B          = 1 << 4,
    CPU_FLAG_UNUSED     = 1 << 5,
    CPU_FLAG_V          = 1 << 6,
    CPU_FLAG_N          = 1 << 7
} CPUFlags;

//CPU state (registers)
typedef struct {
    uint16_t pc;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t s;
    uint8_t p;
    
    //Number of cycles run by the CPU since the last power cycle
    unsigned long long cycles;

    int nmi;
    int irq;
} CPUState;

typedef uint8_t(*CPUReadFn)(void*, uint16_t);
typedef void(*CPUWriteFn)(void*, uint16_t, uint8_t);

typedef struct {
    //Callback functions

    CPUReadFn readfn;
    CPUWriteFn writefn;
    void* fndata;

    //CPU state
    CPUState state;
} CPU;



/*
* CPU_Init - Initialize a CPU struct.
* 
* @param readfn CPU Read callback. Called on every read cycle, i.e. when the CPU reads memory from an address.
* @param writefn CPU Write callback. Called on every write cycle, i.e. when the CPU writes to a memory address.
* @param fndata Pointer to data that is passed to the read/write callbacks, such as the CPU's memory.
*/
void CPU_Init(CPU* cpu, CPUReadFn readfn, CPUWriteFn writefn, void* fndata);

//Reset the CPU from its power-on state. Runs the reset sequence, which does 7 read cycles.
void CPU_PowerOn(CPU* cpu);

//Soft reset the CPU. Reset sequence does 7 read cycles.
void CPU_SoftReset(CPU* cpu);

/*
* Execute the next instruction.
*
* @return 0 on success, -1 if CPU executes an unimplemented or "crash" opcode.
*/
int CPU_Exec(CPU* cpu);

/*
* Trigger an NMI.
*/
void NMI(CPU* cpu);

#endif