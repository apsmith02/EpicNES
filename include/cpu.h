#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

//Opcode names for disassembly
static const char* OPCODE_NAMES[256] = {
    "BRK","ORA",NULL,NULL, NULL,"ORA","ASL",NULL, "PHP","ORA","ASL",NULL,NULL,"ORA","ASL",NULL,
    "BPL","ORA",NULL,NULL, NULL,"ORA","ASL",NULL, "CLC","ORA",NULL,NULL, NULL,"ORA","ASL",NULL,
    "JSR","AND",NULL,NULL, "BIT","AND","ROL",NULL, "PLP","AND","ROL",NULL,"BIT","AND","ROL",NULL,
    "BMI","AND",NULL,NULL, NULL,"AND","ROL",NULL, "SEC","AND",NULL,NULL, NULL,"AND","ROL",NULL,
    "RTI","EOR",NULL,NULL, NULL,"EOR","LSR",NULL, "PHA","EOR","LSR",NULL,"JMP","EOR","LSR",NULL,
    "BVC","EOR",NULL,NULL, NULL,"EOR","LSR",NULL, "CLI","EOR",NULL,NULL, NULL,"EOR","LSR",NULL,
    "RTS","ADC",NULL,NULL, NULL,"ADC","ROR",NULL, "PLA","ADC","ROR",NULL,"JMP","ADC","ROR",NULL,
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


/*Memory access type bit flags for debug. Each CPU cycle, the CPU sets its access_type member to
* the type of memory access it is doing that cycle. The RWX flags can be ANDed with a breakpoint's
* access flags to determine if the CPU access type matches the access type a breakpoint is set
* to break on.
*
* The ACCESS_DUMMY flag is used to indicate a dummy read/write. This flag can be used in a breakpoint's
* access flags to include or exclude dummy reads/writes in the breakpoint condition,
* i.e. only break if the CPU's ACCESS_DUMMY flag is clear or the breakpoint's ACCESS_DUMMY flag is set.
*
* Use the ACCESS_DMA flag to indicate that a read/write is part of a DMA memory access.
*/
typedef enum {
    ACCESS_READ         = 1,
    ACCESS_WRITE        = 1 << 1,
    ACCESS_EXECUTE      = 1 << 2,
    ACCESS_MASK_RWX     = ACCESS_READ | ACCESS_WRITE | ACCESS_EXECUTE,

    ACCESS_DUMMY        = 1 << 3,
    ACCESS_DUMMY_READ   = ACCESS_DUMMY | ACCESS_READ,
    ACCESS_DUMMY_WRITE  = ACCESS_DUMMY | ACCESS_WRITE,

    ACCESS_DMA          = 1 << 4,

    ACCESS_MASK         = ACCESS_READ | ACCESS_WRITE | ACCESS_EXECUTE | ACCESS_DUMMY | ACCESS_DMA,
    /*The bit to the left of the most significant AccessType bit. Use this to determine where to add
    * additional bit flags to store with the AccessType flags in a debug flag storage scheme.
    */
    ACCESS_FLAGS_END
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
} CPUState;

typedef struct CPU CPU;

typedef uint8_t(*CPUReadFn)(void* context, uint16_t addr);
typedef void(*CPUWriteFn)(void* context, uint16_t addr, uint8_t data);
typedef CPUReadFn CPUPeekFn;
/*
* @param nextAddr The address the CPU was going to read before it halted. Use this address to perform DMA dummy read cycles.
*/
typedef void(*CPUHaltFn)(void* context, CPU *cpu, uint16_t nextAddr);

typedef struct {
    void *context; //Context pointer that is passed to every callback
    CPUReadFn onread; //Called on CPU memory read cycles. This function should read from memory at the address passed to it, and return the read value.
    CPUWriteFn onwrite; //Called on CPU memory write cycles. This function should write to memory at the address passed to it.
    CPUPeekFn onpeek; //Peek at values in memory without side-effects. Not required for normal execution, but used by functions like CPU_Disassemble().
    CPUHaltFn onhalt; //Called when the CPU is halted. Use this to implement DMAs.
} CPUCallbacks;

struct CPU {
    //Callback functions
    CPUCallbacks callbacks;

    //CPU state
    CPUState state;
    bool nmi; //NMI signal
    bool irq; //IRQ signal
    bool nmi_detected; //Set when NMI signal goes from low to high
    bool halt;

    //Log file
    FILE* log;

    //Debug info
    uint16_t instr_addr; //Address of current instruction being executed
    int instr_cycle; //Cycle in current instruction being executed (starting from 1)
    AccessType access_type; //Access type of current cycle
};


/*
* Initialize a CPU struct.
* 
* @param callbacks A struct containing the callback context and callback functions.
*/
void CPU_Init(CPU *cpu, CPUCallbacks callbacks);

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
* Set the CPU's NMI signal.
* NMI is edge-triggered; A transition from low to high (inverted on real hardware) will trigger an NMI.
*/
void CPU_SetNMISignal(CPU *cpu, bool nmi);

/*
* Set the CPU's IRQ signal.
*/
void CPU_SetIRQSignal(CPU *cpu, bool irq);

/*
* Schedule a halt to occur before the next read cycle.
*/
void CPU_ScheduleHalt(CPU *cpu);

/*
* Perform a read to CPU memory.
* This function should be used to read from the CPU's memory while the CPU is halted
* in order to update the CPU cycle counter and debug information.
*
* @param addr The memory address to read from
* @param access Indicates the memory access type for the debugger
*
* @return The value in memory at the given address.
*/
uint8_t CPU_Read(CPU *cpu, uint16_t addr, AccessType access);

/*
* Perform a write to CPU memory.
* This function should be used to write to the CPU's memory while the CPU is halted
* in order to update the CPU cycle counter and debug information.
*
* @param addr The memory address to write to
* @param data The value to write to memory at the given address
* @param access Indicates the memory access type for the debugger
*/
void CPU_Write(CPU *cpu, uint16_t addr, uint8_t data, AccessType access);


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