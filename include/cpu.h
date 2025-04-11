#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

//Opcode names for disassembly
static const char* OPCODE_NAMES[256] = {
    "BRK","ORA",NULL,NULL, NULL,"ORA","ASL",NULL, "PHP","ORA","ASLA",NULL,NULL,"ORA","ASL",NULL,
    "BPL","ORA",NULL,NULL, NULL,"ORA","ASL",NULL, "CLC","ORA",NULL,NULL, NULL,"ORA","ASL",NULL,
    "JSR","AND",NULL,NULL, "BIT","AND","ROL",NULL, "PLP","AND","ROLA",NULL,"BIT","AND","ROL",NULL,
    "BMI","AND",NULL,NULL, NULL,"AND","ROL",NULL, "SEC","AND",NULL,NULL, NULL,"AND","ROL",NULL,
    "RTI","EOR",NULL,NULL, NULL,"EOR","LSR",NULL, "PHA","EOR","LSRA",NULL,"JMP","EOR","LSR",NULL,
    "BVC","EOR",NULL,NULL, NULL,"EOR","LSR",NULL, "CLI","EOR",NULL,NULL, NULL,"EOR","LSR",NULL,
    "RTS","ADC",NULL,NULL, NULL,"ADC","ROR",NULL, "PLA","ADC","RORA",NULL,"JMP","ADC","ROR",NULL,
    "BVS","ADC",NULL,NULL, NULL,"ADC","ROR",NULL, "SEI","ADC",NULL,NULL, NULL,"ADC","ROR",NULL,
    NULL,"STA",NULL,NULL, "STY","STA","STX",NULL, "DEY",NULL,"TXA",NULL, "STY","STA","STX",NULL,
    "BCC","STA",NULL,NULL, "STY","STA","STX",NULL, "TYA","STA","TXS",NULL, NULL,"STA",NULL,NULL,
    "LDY","LDA","LDX",NULL, "LDY","LDA","LDX",NULL, "TAY","LDA","TAX",NULL, "LDY","LDA","LDX",NULL,
    "BCS","LDA",NULL,NULL, "LDY","LDA","LDX",NULL, "CLV","LDA","TSX",NULL, "LDY","LDA","LDX",NULL,
    "CPY","CMP",NULL,NULL, "CPY","CMP","DEC",NULL, "INY","CMP","DEX",NULL, "CPY","CMP","DEC",NULL,
    "BNE","CMP",NULL,NULL, NULL,"CMP","DEC",NULL, "CLD","CMP",NULL,NULL, NULL,"CMP","DEC",NULL,
    "CPX","SBC",NULL,NULL, "CPX","SBC","INC",NULL, "INX","SBC","NOP",NULL, "CPX","SBC","INC",NULL,
    "BEQ","SBC",NULL,NULL, NULL,"SBC","INC",NULL, "SED","SBC",NULL,NULL, NULL,"SBC","INC",NULL
};

//Memory access type bit flags
typedef enum {
    ACCESS_READ         = 1,
    ACCESS_WRITE        = 1 << 1,
    ACCESS_EXECUTE      = 1 << 2,
    ACCESS_DUMMY        = 1 << 3,

    ACCESS_DUMMY_READ   = ACCESS_DUMMY | ACCESS_READ,
    ACCESS_DUMMY_WRITE  = ACCESS_DUMMY | ACCESS_WRITE
} AccessType;


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
typedef CPUReadFn CPUPeekFn;

typedef struct {
    //Callback functions

    CPUReadFn readfn;
    CPUWriteFn writefn;
    CPUPeekFn peekfn;
    void* fndata;

    //CPU state
    CPUState state;
    bool oamdma; //Set when OAMDMA ($4014) is written, causes CPU_Exec to begin OAM DMA after the current instruction.
    uint8_t oamdma_page;

    //Log file
    FILE* log;

    //
} CPU;



/*
* CPU_Init - Initialize a CPU struct.
* 
* @param readfn CPU Read callback. Called on every read cycle, i.e. when the CPU reads memory from an address.
* @param writefn CPU Write callback. Called on every write cycle, i.e. when the CPU writes to a memory address.
* @param fndata Pointer to data that is passed to the read/write callbacks, such as the CPU's memory.
*/
void CPU_Init(CPU* cpu, CPUReadFn readfn, CPUWriteFn writefn, void* fndata);

/*
* CPU_SetPeekFn - Set the CPU Peek callback. This is not required for CPU execution, but is used by
* functions like CPU_Disassemble() to peek at values in memory without any side effects.
* The peek function is passed the same data pointer that is passed to the read/write callbacks.
*/
void CPU_SetPeekFn(CPU* cpu, CPUPeekFn peekfn);



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
void CPU_NMI(CPU* cpu);

/*
* Print the disassembly of an instruction at an address to a string buffer of size n.
* Requires the CPU Peek callback to be set with CPU_SetPeekFn().
* String format: [address] \t[opcode byte] [operand byte 1] [operand byte 2] \t[instruction name] [operand]
*
* @return Instruction length
*/
int CPU_Disassemble(CPU* cpu, uint16_t instr_addr, char* buffer, size_t n);

/*
* Set log file stream.
*
* @param log Log file stream
*/
void CPU_SetLogFile(CPU* cpu, FILE* logfile);

#endif