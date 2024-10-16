#include "cpu.h"
#include <string.h>

// Addressing modes
typedef enum { 
    AD_IMP, AD_ACC, AD_IMM, AD_ZPG, AD_ZPX, AD_ZPY, AD_REL, AD_ABS, AD_ABX, AD_ABY, AD_IND, AD_IDX, AD_IDY
} AddrMode;

// Interrupt types
typedef enum {
    IR_BRK, IR_IRQ, IR_NMI, IR_RESET
} InterruptType;


/* PRIVATE FUNCTIONS */

uint8_t Read(CPU* cpu, uint16_t addr) {
    cpu->state.cycles++;
    return cpu->readfn(cpu->fndata, addr);
}

void Write(CPU* cpu, uint16_t addr, uint8_t data) {
    cpu->state.cycles++;
    return cpu->writefn(cpu->fndata, addr, data);
}

//Does not handle page boundary crossing (the high byte will always be fetched from the same page as the low byte)
uint16_t ReadWord(CPU* cpu, uint16_t addr) {
    return Read(cpu, addr) | Read(cpu, ((addr + 1) & 0xFF) | (addr & 0xFF00)) << 8;
}

uint8_t FetchByte(CPU* cpu) {
    return Read(cpu, cpu->state.pc++);
}

uint16_t FetchWord(CPU* cpu) {
    return FetchByte(cpu) | FetchByte(cpu) << 8;
}

uint16_t StackAddr(CPU* cpu) {
    return 0x0100 + cpu->state.s;
}

uint8_t ReadStack(CPU* cpu) {
    return Read(cpu, StackAddr(cpu));
}

void Push(CPU* cpu, uint8_t val) {
    Write(cpu, StackAddr(cpu), val);
    cpu->state.s--;
}

uint8_t Pop(CPU* cpu) {
    cpu->state.s++;
    ReadStack(cpu);
}

void DummyPush(CPU* cpu) {
    Read(cpu, StackAddr(cpu));
    cpu->state.s--;
}

void PushPC(CPU* cpu) {
    Push(cpu, cpu->state.pc >> 8);
    Push(cpu, cpu->state.pc);
}

void PopPC(CPU* cpu) {
    cpu->state.pc = Pop(cpu) | Pop(cpu) << 8;
}

//Use this to keep the unused flag (bit 5) set and the B flag (bit 4) clear when recovering P from the stack
void PopP(CPU* cpu) {
    cpu->state.p = Pop(cpu) & ~CPU_FLAG_B | CPU_FLAG_UNUSED;
}

//Fetch/calculate address by addressing mode for all modes except implied, accumulator and relative.
uint16_t FetchAddr(CPU* cpu, AddrMode mode, int forWrite);
uint8_t ReadByMode(CPU* cpu, AddrMode mode) {
    return Read(cpu, FetchAddr(cpu, mode, 0));
}
//Read value by addressing mode, then dummy write back value.
uint8_t RMWReadByMode(CPU* cpu, AddrMode mode, uint16_t* outAddr) {
    *outAddr = FetchAddr(cpu, mode, 1);
    uint8_t val = Read(cpu, *outAddr);
    Write(cpu, *outAddr, val);
    return val;
}
void WriteByMode(CPU* cpu, AddrMode mode, uint8_t val) {
    Write(cpu, FetchAddr(cpu, mode, 1), val);
}

//Add index to address for absolute X/Y and indexed indirect addressing. Does dummy read if there is a page crossing or a write/read-modify-write instruction is being executed.
uint16_t AddrAddIndex(CPU* cpu, uint16_t base, uint8_t index, int forWrite) {
    uint16_t addr = base + index;
    if (forWrite || (addr & 0xFF00) != (base & 0xFF00))
        Read(cpu, addr - ((addr & 0xFF00) - (base & 0xFF00)));
    return addr;
}

void UpdateNZ(CPU* cpu, uint8_t val);

void OpADC(CPU* cpu, uint8_t val);
void OpCMP(CPU* cpu, uint8_t a, uint8_t b);
uint8_t OpASL(CPU* cpu, uint8_t val);
uint8_t OpLSR(CPU* cpu, uint8_t val);
uint8_t OpROL(CPU* cpu, uint8_t val);
uint8_t OpROR(CPU* cpu, uint8_t val);

void Branch(CPU* cpu, int takeBranch);

/* Execute cycles 2-7 of an interrupt (BRK, IRQ, NMI or reset) sequence.
*  TODO: Implement interrupt hijacking.
*/
void HandleInterrupt(CPU* cpu, InterruptType interrupt);

/* OPCODES */

void ADC(CPU* cpu, AddrMode mode);
void AND(CPU* cpu, AddrMode mode);
void ASL(CPU* cpu, AddrMode mode);
void ASLA(CPU* cpu, AddrMode mode);
void BCC(CPU* cpu, AddrMode mode);
void BCS(CPU* cpu, AddrMode mode);
void BEQ(CPU* cpu, AddrMode mode);
void BIT(CPU* cpu, AddrMode mode);
void BMI(CPU* cpu, AddrMode mode);
void BNE(CPU* cpu, AddrMode mode);
void BPL(CPU* cpu, AddrMode mode);
void BRK(CPU* cpu, AddrMode mode);
void BVC(CPU* cpu, AddrMode mode);
void BVS(CPU* cpu, AddrMode mode);
void CLC(CPU* cpu, AddrMode mode);
void CLD(CPU* cpu, AddrMode mode);
void CLI(CPU* cpu, AddrMode mode);
void CLV(CPU* cpu, AddrMode mode);
void CMP(CPU* cpu, AddrMode mode);
void CPX(CPU* cpu, AddrMode mode);
void CPY(CPU* cpu, AddrMode mode);
void DEC(CPU* cpu, AddrMode mode);
void DEX(CPU* cpu, AddrMode mode);
void DEY(CPU* cpu, AddrMode mode);
void EOR(CPU* cpu, AddrMode mode);
void INC(CPU* cpu, AddrMode mode);
void INX(CPU* cpu, AddrMode mode);
void INY(CPU* cpu, AddrMode mode);
void JMP(CPU* cpu, AddrMode mode);
void JSR(CPU* cpu, AddrMode mode);
void LDA(CPU* cpu, AddrMode mode);
void LDX(CPU* cpu, AddrMode mode);
void LDY(CPU* cpu, AddrMode mode);
void LSR(CPU* cpu, AddrMode mode);
void LSRA(CPU* cpu, AddrMode mode);
void NOP(CPU* cpu, AddrMode mode);
void ORA(CPU* cpu, AddrMode mode);
void PHA(CPU* cpu, AddrMode mode);
void PHP(CPU* cpu, AddrMode mode);
void PLA(CPU* cpu, AddrMode mode);
void PLP(CPU* cpu, AddrMode mode);
void ROL(CPU* cpu, AddrMode mode);
void ROLA(CPU* cpu, AddrMode mode);
void ROR(CPU* cpu, AddrMode mode);
void RORA(CPU* cpu, AddrMode mode);
void RTI(CPU* cpu, AddrMode mode);
void RTS(CPU* cpu, AddrMode mode);
void SBC(CPU* cpu, AddrMode mode);
void SEC(CPU* cpu, AddrMode mode);
void SED(CPU* cpu, AddrMode mode);
void SEI(CPU* cpu, AddrMode mode);
void STA(CPU* cpu, AddrMode mode);
void STX(CPU* cpu, AddrMode mode);
void STY(CPU* cpu, AddrMode mode);
void TAX(CPU* cpu, AddrMode mode);
void TAY(CPU* cpu, AddrMode mode);
void TSX(CPU* cpu, AddrMode mode);
void TXA(CPU* cpu, AddrMode mode);
void TXS(CPU* cpu, AddrMode mode);
void TYA(CPU* cpu, AddrMode mode);

typedef void(*OpcodeFn)(CPU*, AddrMode);
const OpcodeFn OPCODE_TABLE[256] = {
    &BRK,&ORA,NULL,NULL, NULL,&ORA,&ASL,NULL, &PHP,&ORA,&ASLA,NULL,NULL,&ORA,&ASL,NULL,
    &BPL,&ORA,NULL,NULL, NULL,&ORA,&ASL,NULL, &CLC,&ORA,NULL,NULL, NULL,&ORA,&ASL,NULL,
    &JSR,&AND,NULL,NULL, &BIT,&AND,&ROL,NULL, &PLP,&AND,&ROLA,NULL,&BIT,&AND,&ROL,NULL,
    &BMI,&AND,NULL,NULL, NULL,&AND,&ROL,NULL, &SEC,&AND,NULL,NULL, NULL,&AND,&ROL,NULL,
    &RTI,&EOR,NULL,NULL, NULL,&EOR,&LSR,NULL, &PHA,&EOR,&LSRA,NULL,&JMP,&EOR,&LSR,NULL,
    &BVC,&EOR,NULL,NULL, NULL,&EOR,&LSR,NULL, &CLI,&EOR,NULL,NULL, NULL,&EOR,&LSR,NULL,
    &RTS,&ADC,NULL,NULL, NULL,&ADC,&ROR,NULL, &PLA,&ADC,&RORA,NULL,&JMP,&ADC,&ROR,NULL,
    &BVS,&ADC,NULL,NULL, NULL,&ADC,&ROR,NULL, &SEI,&ADC,NULL,NULL, NULL,&ADC,&ROR,NULL,
    NULL,&STA,NULL,NULL, &STY,&STA,&STX,NULL, &DEY,NULL,&TXA,NULL, &STY,&STA,&STX,NULL,
    &BCC,&STA,NULL,NULL, &STY,&STA,&STX,NULL, &TYA,&STA,&TXS,NULL, NULL,&STA,NULL,NULL,
    &LDY,&LDA,&LDX,NULL, &LDY,&LDA,&LDX,NULL, &TAY,&LDA,&TAX,NULL, &LDY,&LDA,&LDX,NULL,
    &BCS,&LDA,NULL,NULL, &LDY,&LDA,&LDX,NULL, &CLV,&LDA,&TSX,NULL, &LDY,&LDA,&LDX,NULL,
    &CPY,&CMP,NULL,NULL, &CPY,&CMP,&DEC,NULL, &INY,&CMP,&DEX,NULL, &CPY,&CMP,&DEC,NULL,
    &BNE,&CMP,NULL,NULL, NULL,&CMP,&DEC,NULL, &CLD,&CMP,NULL,NULL, NULL,&CMP,&DEC,NULL,
    &CPX,&SBC,NULL,NULL, &CPX,&SBC,&INC,NULL, &INX,&SBC,&NOP,NULL, &CPX,&SBC,&INC,NULL,
    &BEQ,&SBC,NULL,NULL, NULL,&SBC,&INC,NULL, &SED,&SBC,NULL,NULL, NULL,&SBC,&INC,NULL,
};
const AddrMode MODE_TABLE[256] = {
    AD_IMP,AD_IDX,AD_IMP,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_ACC,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX,
    AD_ABS,AD_IDX,AD_IMP,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_ACC,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX,
    AD_IMP,AD_IDX,AD_IMP,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_ACC,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX,
    AD_IMP,AD_IDX,AD_IMP,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_ACC,AD_IMM,AD_IND,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX,
    AD_IMM,AD_IDX,AD_IMM,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_IMP,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPY,AD_ZPY,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABY,AD_ABY,
    AD_IMM,AD_IDX,AD_IMM,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_IMP,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPY,AD_ZPY,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABY,AD_ABY,
    AD_IMM,AD_IDX,AD_IMM,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_IMP,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX,
    AD_IMM,AD_IDX,AD_IMM,AD_IDX,AD_ZPG,AD_ZPG,AD_ZPG,AD_ZPG,AD_IMP,AD_IMM,AD_IMP,AD_IMM,AD_ABS,AD_ABS,AD_ABS,AD_ABS,
    AD_REL,AD_IDY,AD_IMP,AD_IDY,AD_ZPX,AD_ZPX,AD_ZPX,AD_ZPX,AD_IMP,AD_ABY,AD_IMP,AD_ABY,AD_ABX,AD_ABX,AD_ABX,AD_ABX
};


/* PUBLIC FUNCTION DEFINITIONS */

void CPU_Init(CPU* cpu, CPUReadFn readfn, CPUWriteFn writefn, void* fndata) {
    memset(&cpu->state, 0, sizeof(CPUState));
    cpu->readfn = readfn;
    cpu->writefn = writefn;
    cpu->fndata = fndata;
}

void CPU_PowerOn(CPU *cpu)
{
    CPUState* state = &cpu->state;
    state->pc = state->a = state->x = state->y = state->s = state->cycles = 0;
    state->p = CPU_FLAG_UNUSED;
    CPU_SoftReset(cpu);
}

void CPU_SoftReset(CPU *cpu)
{
    Read(cpu, cpu->state.pc);
    HandleInterrupt(cpu, IR_RESET);
}

int CPU_Exec(CPU *cpu)
{
    //Fetch
    uint8_t opcode = FetchByte(cpu);
    OpcodeFn opcodeFn = OPCODE_TABLE[opcode];

    //Some opcodes crash the CPU (or at this stage of development, aren't implemented yet). Return error if any of these opcodes are fetched.
    if (opcodeFn == NULL)
        return -1;

    //Execute
    opcodeFn(cpu, MODE_TABLE[opcode]);

    //Handle interrupts
    if (cpu->state.nmi) {
        HandleInterrupt(cpu, IR_NMI);
        cpu->state.nmi = 0;
    }
    
    return 0;
}

void NMI(CPU *cpu) { cpu->state.nmi = 1; }

/* PRIVATE FUNCTION DEFINITIONS */

uint16_t FetchAddr(CPU *cpu, AddrMode mode, int forWrite)
{
    switch (mode) {
        case AD_IMM: return cpu->state.pc++;
        case AD_ZPG: return FetchByte(cpu);
        case AD_ZPX: {
            uint8_t addr = FetchByte(cpu);
            Read(cpu, addr);
            return addr + cpu->state.x & 0xFF;
        }
        case AD_ZPY: {
            uint8_t addr = FetchByte(cpu);
            Read(cpu, addr);
            return addr + cpu->state.y & 0xFF;
        }
        case AD_ABS: return FetchWord(cpu);
        case AD_ABX: return AddrAddIndex(cpu, FetchWord(cpu), cpu->state.x, forWrite);
        case AD_ABY: return AddrAddIndex(cpu, FetchWord(cpu), cpu->state.y, forWrite);
        case AD_IND: return ReadWord(cpu, FetchWord(cpu));
        case AD_IDX: {
            uint8_t ptr = FetchByte(cpu);
            Read(cpu, ptr);
            return ReadWord(cpu, ptr + cpu->state.x & 0xFF);
        }
        case AD_IDY: return AddrAddIndex(cpu, ReadWord(cpu, FetchByte(cpu)), cpu->state.y, forWrite);
        default: return 0;
    }
}

void UpdateNZ(CPU *cpu, uint8_t val)
{
    cpu->state.p &= ~(CPU_FLAG_N | CPU_FLAG_Z);
    cpu->state.p |= val & 0x80;
    cpu->state.p |= (val == 0) << 1;
}

void OpADC(CPU *cpu, uint8_t val)
{
    uint16_t sum = cpu->state.a + val + (cpu->state.p & CPU_FLAG_C);

    cpu->state.p &= ~(CPU_FLAG_V | CPU_FLAG_C);
    cpu->state.p |= (sum > 0xFF); //Set Carry flag
    cpu->state.p |= (0x80 & (cpu->state.a ^ sum) & (val ^ sum)) >> 1; //Set oVerflow flag
    UpdateNZ(cpu, sum);

    cpu->state.a = sum;
}

void OpCMP(CPU *cpu, uint8_t a, uint8_t b)
{
    cpu->state.p &= ~(CPU_FLAG_N | CPU_FLAG_Z | CPU_FLAG_C);
    cpu->state.p |= (a >= b) | ((a == b) << 1) | ((a - b) & 0x80);
}

uint8_t OpASL(CPU *cpu, uint8_t val)
{
    cpu->state.p &= ~CPU_FLAG_C;
    cpu->state.p |= val >> 7; //Shift bit 7 into Carry
    val <<= 1;
    UpdateNZ(cpu, val);
    return val;
}

uint8_t OpLSR(CPU *cpu, uint8_t val)
{
    cpu->state.p &= ~CPU_FLAG_C;
    cpu->state.p |= val & 0x01; //Shift bit 0 into Carry
    val >>= 1;
    UpdateNZ(cpu, val);
    return val;
}

uint8_t OpROL(CPU *cpu, uint8_t val)
{
    int carry = cpu->state.p & 0x01;
    cpu->state.p &= ~CPU_FLAG_C;
    cpu->state.p |= val >> 7; //Shift bit 7 into Carry
    val <<= 1;
    val |= carry; //Shift original Carry into bit 0
    UpdateNZ(cpu, val);
    return val;
}

uint8_t OpROR(CPU *cpu, uint8_t val)
{
    int carry = cpu->state.p & 0x01;
    cpu->state.p &= ~CPU_FLAG_C;
    cpu->state.p |= val & 0x01; //Shift bit 0 into Carry
    val >>= 1;
    val |= carry << 7; //Shift original Carry into bit 7
    UpdateNZ(cpu, val);
    return val;
}

void Branch(CPU *cpu, int takeBranch)
{
    int8_t disp = FetchByte(cpu);
    if (takeBranch) {
       Read(cpu, cpu->state.pc);
       uint16_t branch = cpu->state.pc + disp;
       uint16_t pchDiff = (branch & 0xFF00) - (cpu->state.pc & 0xFF00);
       if (pchDiff != 0) {
            Read(cpu, branch - pchDiff);
       }
       cpu->state.pc = branch;
    }
}

void HandleInterrupt(CPU *cpu, InterruptType interrupt)
{
    FetchByte(cpu);

    //BRK, IRQ and NMI push PC and P to the stack. But for reset, the stack pointer is decremented, but writes are suppressed.
    if (interrupt != IR_RESET) {
        PushPC(cpu);
        Push(cpu, cpu->state.p | ((interrupt == IR_BRK) ? CPU_FLAG_B : 0));
    } else {
        DummyPush(cpu);
        DummyPush(cpu);
        DummyPush(cpu);
    }

    uint16_t vec;
    switch (interrupt) {
        case IR_BRK:
        case IR_IRQ: vec = 0xFFFE; break;
        case IR_NMI: vec = 0xFFFA; break;
        case IR_RESET: vec = 0xFFFC; break;
    }
    cpu->state.p |= CPU_FLAG_I;
    cpu->state.pc = ReadWord(cpu, vec);
}

void ADC(CPU *cpu, AddrMode mode)
{
    OpADC(cpu, ReadByMode(cpu, mode));
}

void AND(CPU *cpu, AddrMode mode)
{
    cpu->state.a &= ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.a);
}

void ASL(CPU* cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpASL(cpu, val));
}

void ASLA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.a = OpASL(cpu, cpu->state.a);
}

void BCC(CPU *cpu, AddrMode mode) {
    Branch(cpu, !(cpu->state.p & CPU_FLAG_C));
}

void BCS(CPU* cpu, AddrMode mode) {
    Branch(cpu, cpu->state.p & CPU_FLAG_C);
}

void BEQ(CPU* cpu, AddrMode mode) {
    Branch(cpu, cpu->state.p & CPU_FLAG_Z);
}

void BIT(CPU* cpu, AddrMode mode) {
    uint8_t val = ReadByMode(cpu, mode);
    cpu->state.p &= ~(CPU_FLAG_N | CPU_FLAG_V | CPU_FLAG_Z);
    cpu->state.p |= val & 0xC0; //N and V (bits 7 and 6)
    cpu->state.p |= ((val & cpu->state.a) == 0) << 1; //Z flag
}

void BMI(CPU* cpu, AddrMode mode) {
    Branch(cpu, cpu->state.p & CPU_FLAG_N);
}

void BNE(CPU* cpu, AddrMode mode) {
    Branch(cpu, !(cpu->state.p & CPU_FLAG_Z));
}

void BPL(CPU* cpu, AddrMode mode) {
    Branch(cpu, !(cpu->state.p & CPU_FLAG_N));
}

void BRK(CPU *cpu, AddrMode mode) {
    HandleInterrupt(cpu, IR_BRK);
}

void BVC(CPU* cpu, AddrMode mode) {
    Branch(cpu, !(cpu->state.p & CPU_FLAG_V));
}

void BVS(CPU* cpu, AddrMode mode) {
    Branch(cpu, cpu->state.p & CPU_FLAG_V);
}

void CLC(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_C;
}

void CLD(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_D;
}

void CLI(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_I;
}

void CLV(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_V;
}

void CMP(CPU* cpu, AddrMode mode) {
    OpCMP(cpu, cpu->state.a, ReadByMode(cpu, mode));
}

void CPX(CPU* cpu, AddrMode mode) {
    OpCMP(cpu, cpu->state.x, ReadByMode(cpu, mode));
}

void CPY(CPU* cpu, AddrMode mode) {
    OpCMP(cpu, cpu->state.y, ReadByMode(cpu, mode));
}

void DEC(CPU* cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    UpdateNZ(cpu, --val);
    Write(cpu, addr, val);
}

void DEX(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, --cpu->state.x);
}

void DEY(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, --cpu->state.y);
}

void EOR(CPU* cpu, AddrMode mode) {
    cpu->state.a ^= ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.a);
}

void INC(CPU* cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    UpdateNZ(cpu, ++val);
    Write(cpu, addr, val);
}

void INX(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, ++cpu->state.x);
}

void INY(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, ++cpu->state.y);
}

void JMP(CPU *cpu, AddrMode mode) {
    cpu->state.pc = FetchAddr(cpu, mode, 0);
}

void JSR(CPU *cpu, AddrMode mode) {
    uint16_t addr = FetchByte(cpu);
    ReadStack(cpu);
    PushPC(cpu);
    cpu->state.pc = addr | FetchByte(cpu) << 8;
}

void LDA(CPU *cpu, AddrMode mode) {
    cpu->state.a = ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.a);
}

void LDX(CPU* cpu, AddrMode mode) {
    cpu->state.x = ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.x);
}

void LDY(CPU* cpu, AddrMode mode) {
    cpu->state.y = ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.y);
}

void LSR(CPU* cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpLSR(cpu, val));
}

void LSRA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.a = OpLSR(cpu, cpu->state.a);
}

void NOP(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
}

void ORA(CPU* cpu, AddrMode mode) {
    cpu->state.a |= ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.a);
}

void PHA(CPU *cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    Push(cpu, cpu->state.a);
}

void PHP(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    Push(cpu, cpu->state.p | CPU_FLAG_B);
}

void PLA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    ReadStack(cpu);
    cpu->state.a = Pop(cpu);
    UpdateNZ(cpu, cpu->state.a);
}

void PLP(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    ReadStack(cpu);
    PopP(cpu);
}

void ROL(CPU *cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpROL(cpu, val));
}

void ROLA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.a = OpROL(cpu, cpu->state.a);
}

void ROR(CPU *cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpROR(cpu, val));
}

void RORA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.a = OpROR(cpu, cpu->state.a);
}

void RTI(CPU *cpu, AddrMode mode)
{
    Read(cpu, cpu->state.pc);
    ReadStack(cpu);
    PopP(cpu);
    PopPC(cpu);
}

void RTS(CPU *cpu, AddrMode mode)
{
    Read(cpu, cpu->state.pc);
    ReadStack(cpu);
    PopPC(cpu);
    FetchByte(cpu);
}

void SBC(CPU* cpu, AddrMode mode) {
    OpADC(cpu, ~ReadByMode(cpu, mode));
}

void SEC(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p |= CPU_FLAG_C;
}

void SED(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p |= CPU_FLAG_D;
}

void SEI(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.p |= CPU_FLAG_I;
}

void STA(CPU* cpu, AddrMode mode) {
    WriteByMode(cpu, mode, cpu->state.a);
}

void STX(CPU* cpu, AddrMode mode) {
    WriteByMode(cpu, mode, cpu->state.x);
}

void STY(CPU* cpu, AddrMode mode) {
    WriteByMode(cpu, mode, cpu->state.y);
}

void TAX(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.x = cpu->state.a);
}

void TAY(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.y = cpu->state.a);
}

void TSX(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.x = cpu->state.s);
}
void TXA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.a = cpu->state.x);
}
void TXS(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    cpu->state.s = cpu->state.x;
}
void TYA(CPU* cpu, AddrMode mode) {
    Read(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.a = cpu->state.y);
}
