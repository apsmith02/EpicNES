#include "cpu.h"
#include <string.h>
#include <assert.h>

// Addressing modes
typedef enum { 
    AD_IMP, AD_ACC, AD_IMM, AD_ZPG, AD_ZPX, AD_ZPY, AD_REL, AD_ABS, AD_ABX, AD_ABY, AD_IND, AD_IDX, AD_IDY
} AddrMode;

// Interrupt types
typedef enum {
    IR_BRK, IR_IRQ, IR_NMI, IR_RESET
} InterruptType;


/* PRIVATE FUNCTIONS */

static void ProcessHalt(CPU *cpu, uint16_t nextAddr) {
    if (cpu->halt) {
        cpu->halt = false;
        cpu->callbacks.onhalt(cpu->callbacks.context, cpu, nextAddr);
    }
}

//Read, and process pending halt before reading
static uint8_t Read(CPU* cpu, uint16_t addr) {
    ProcessHalt(cpu, addr);
    return CPU_Read(cpu, addr, ACCESS_READ);
}

static void Write(CPU* cpu, uint16_t addr, uint8_t data) {
    CPU_Write(cpu, addr, data, ACCESS_WRITE);
}

static uint8_t DummyRead(CPU* cpu, uint16_t addr) {
    ProcessHalt(cpu, addr);
    return CPU_Read(cpu, addr, ACCESS_DUMMY_READ);
}

static void DummyWrite(CPU* cpu, uint16_t addr, uint8_t data) {
    CPU_Write(cpu, addr, data, ACCESS_DUMMY_WRITE);
}

static uint8_t Peek(CPU* cpu, uint16_t addr) {
    assert(cpu->callbacks.onpeek != NULL);
    return cpu->callbacks.onpeek(cpu->callbacks.context, addr);
}

//Does not handle page boundary crossing (the high byte will always be fetched from the same page as the low byte)
static uint16_t ReadWord(CPU* cpu, uint16_t addr) {
    return Read(cpu, addr) | Read(cpu, ((addr + 1) & 0xFF) | (addr & 0xFF00)) << 8;
}

//Fetch opcode, increment pc. Use to fetch with execute access type.
static uint8_t FetchOpcode(CPU* cpu) {
    uint8_t val = CPU_Read(cpu, cpu->state.pc, ACCESS_EXECUTE);
    cpu->state.pc++;
    return val;
}

static uint8_t FetchByte(CPU* cpu) {
    uint8_t val = Read(cpu, cpu->state.pc);
    cpu->state.pc++;
    return val;
}

static uint8_t DummyFetchByte(CPU* cpu) {
    uint8_t val = Read(cpu, cpu->state.pc);
    cpu->state.pc++;
    return val;
}

static uint16_t FetchWord(CPU* cpu) {
    return FetchByte(cpu) | FetchByte(cpu) << 8;
}

static uint16_t StackAddr(CPU* cpu) {
    return 0x0100 + cpu->state.s;
}

static uint8_t ReadStack(CPU* cpu) {
    return Read(cpu, StackAddr(cpu));
}

static uint8_t DummyReadStack(CPU* cpu) {
    return DummyRead(cpu, StackAddr(cpu));
}

static void Push(CPU* cpu, uint8_t val) {
    Write(cpu, StackAddr(cpu), val);
    cpu->state.s--;
}

static uint8_t Pop(CPU* cpu) {
    cpu->state.s++;
    return ReadStack(cpu);
}

static void DummyPush(CPU* cpu) {
    DummyRead(cpu, StackAddr(cpu));
    cpu->state.s--;
}

static void PushPC(CPU* cpu) {
    Push(cpu, cpu->state.pc >> 8);
    Push(cpu, cpu->state.pc);
}

static void PopPC(CPU* cpu) {
    cpu->state.pc = Pop(cpu) | Pop(cpu) << 8;
}

//Use this to keep the unused flag (bit 5) set and the B flag (bit 4) clear when recovering P from the stack
static void PopP(CPU* cpu) {
    cpu->state.p = Pop(cpu) & ~CPU_FLAG_B | CPU_FLAG_UNUSED;
}

//Fetch/calculate address by addressing mode for all modes except implied, accumulator and relative.
static uint16_t FetchAddr(CPU* cpu, AddrMode mode, int forWrite);
static uint8_t ReadByMode(CPU* cpu, AddrMode mode) {
    return Read(cpu, FetchAddr(cpu, mode, 0));
}
//Read value by addressing mode, then dummy write back value.
static uint8_t RMWReadByMode(CPU* cpu, AddrMode mode, uint16_t* outAddr) {
    *outAddr = FetchAddr(cpu, mode, 1);
    uint8_t val = Read(cpu, *outAddr);
    DummyWrite(cpu, *outAddr, val);
    return val;
}
static void WriteByMode(CPU* cpu, AddrMode mode, uint8_t val) {
    Write(cpu, FetchAddr(cpu, mode, 1), val);
}

//Add index to address for absolute X/Y and indexed indirect addressing. Does dummy read if there is a page crossing or a write/read-modify-write instruction is being executed.
static uint16_t AddrAddIndex(CPU* cpu, uint16_t base, uint8_t index, int forWrite) {
    uint16_t addr = base + index;
    if (forWrite || (addr & 0xFF00) != (base & 0xFF00))
        DummyRead(cpu, addr - ((addr & 0xFF00) - (base & 0xFF00)));
    return addr;
}

static void UpdateNZ(CPU* cpu, uint8_t val);

static void OpADC(CPU* cpu, uint8_t val);
static void OpCMP(CPU* cpu, uint8_t a, uint8_t b);
static uint8_t OpASL(CPU* cpu, uint8_t val);
static uint8_t OpLSR(CPU* cpu, uint8_t val);
static uint8_t OpROL(CPU* cpu, uint8_t val);
static uint8_t OpROR(CPU* cpu, uint8_t val);

static void Branch(CPU* cpu, int takeBranch);

/* Execute cycles 2-7 of an interrupt (BRK, IRQ, NMI or reset) sequence.
*  TODO: Implement interrupt hijacking.
*/
static void HandleInterrupt(CPU* cpu, InterruptType interrupt);

/* OPCODES */

static void ADC(CPU* cpu, AddrMode mode);
static void AND(CPU* cpu, AddrMode mode);
static void ASL(CPU* cpu, AddrMode mode);
static void ASLA(CPU* cpu, AddrMode mode);
static void BCC(CPU* cpu, AddrMode mode);
static void BCS(CPU* cpu, AddrMode mode);
static void BEQ(CPU* cpu, AddrMode mode);
static void BIT(CPU* cpu, AddrMode mode);
static void BMI(CPU* cpu, AddrMode mode);
static void BNE(CPU* cpu, AddrMode mode);
static void BPL(CPU* cpu, AddrMode mode);
static void BRK(CPU* cpu, AddrMode mode);
static void BVC(CPU* cpu, AddrMode mode);
static void BVS(CPU* cpu, AddrMode mode);
static void CLC(CPU* cpu, AddrMode mode);
static void CLD(CPU* cpu, AddrMode mode);
static void CLI(CPU* cpu, AddrMode mode);
static void CLV(CPU* cpu, AddrMode mode);
static void CMP(CPU* cpu, AddrMode mode);
static void CPX(CPU* cpu, AddrMode mode);
static void CPY(CPU* cpu, AddrMode mode);
static void DEC(CPU* cpu, AddrMode mode);
static void DEX(CPU* cpu, AddrMode mode);
static void DEY(CPU* cpu, AddrMode mode);
static void EOR(CPU* cpu, AddrMode mode);
static void INC(CPU* cpu, AddrMode mode);
static void INX(CPU* cpu, AddrMode mode);
static void INY(CPU* cpu, AddrMode mode);
static void JMP(CPU* cpu, AddrMode mode);
static void JSR(CPU* cpu, AddrMode mode);
static void LDA(CPU* cpu, AddrMode mode);
static void LDX(CPU* cpu, AddrMode mode);
static void LDY(CPU* cpu, AddrMode mode);
static void LSR(CPU* cpu, AddrMode mode);
static void LSRA(CPU* cpu, AddrMode mode);
static void NOP(CPU* cpu, AddrMode mode);
static void ORA(CPU* cpu, AddrMode mode);
static void PHA(CPU* cpu, AddrMode mode);
static void PHP(CPU* cpu, AddrMode mode);
static void PLA(CPU* cpu, AddrMode mode);
static void PLP(CPU* cpu, AddrMode mode);
static void ROL(CPU* cpu, AddrMode mode);
static void ROLA(CPU* cpu, AddrMode mode);
static void ROR(CPU* cpu, AddrMode mode);
static void RORA(CPU* cpu, AddrMode mode);
static void RTI(CPU* cpu, AddrMode mode);
static void RTS(CPU* cpu, AddrMode mode);
static void SBC(CPU* cpu, AddrMode mode);
static void SEC(CPU* cpu, AddrMode mode);
static void SED(CPU* cpu, AddrMode mode);
static void SEI(CPU* cpu, AddrMode mode);
static void STA(CPU* cpu, AddrMode mode);
static void STX(CPU* cpu, AddrMode mode);
static void STY(CPU* cpu, AddrMode mode);
static void TAX(CPU* cpu, AddrMode mode);
static void TAY(CPU* cpu, AddrMode mode);
static void TSX(CPU* cpu, AddrMode mode);
static void TXA(CPU* cpu, AddrMode mode);
static void TXS(CPU* cpu, AddrMode mode);
static void TYA(CPU* cpu, AddrMode mode);

typedef void(*OpcodeFn)(CPU*, AddrMode);
static const OpcodeFn OPCODE_TABLE[256] = {
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
    &BEQ,&SBC,NULL,NULL, NULL,&SBC,&INC,NULL, &SED,&SBC,NULL,NULL, NULL,&SBC,&INC,NULL
};

static const AddrMode ADDRMODE_TABLE[256] = {
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

void CPU_Init(CPU *cpu, CPUCallbacks callbacks) {
    memset(cpu, 0, sizeof(CPU));
    cpu->callbacks = callbacks;
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
    DummyRead(cpu, cpu->state.pc);
    HandleInterrupt(cpu, IR_RESET);
}

int CPU_Exec(CPU *cpu)
{
    CPUState* state = &cpu->state;

    cpu->instr_addr = state->pc;
    cpu->instr_cycle = 0;
    
    //Fetch opcode
    uint8_t opcode = FetchOpcode(cpu);
    OpcodeFn opcodeFn = OPCODE_TABLE[opcode];

    //Log opcode
    if (cpu->log) {
        uint16_t pc = cpu->state.pc - 1;
        fprintf(cpu->log, "%04x ", pc);
        if (OPCODE_NAMES[opcode]) {
            fprintf(cpu->log, OPCODE_NAMES[opcode]);
        } else {
            fprintf(cpu->log, "Null");
        }
        fprintf(cpu->log, " A:%02x X:%02x Y:%02x S:%02x P:%02x CYC:%llu\n", state->a, state->x, state->y, state->s, state->p, state->cycles);
    }

    //Some illegal opcodes crash the CPU (or at this stage of development, aren't implemented yet). Return error if any of these opcodes are fetched.
    if (opcodeFn == NULL)
        return -1;

    //Execute instruction
    opcodeFn(cpu, ADDRMODE_TABLE[opcode]);

    //Handle interrupts
    if (cpu->nmi_detected) {
        if (cpu->log)
            fprintf(cpu->log, "NMI\n");
        DummyRead(cpu, state->pc);
        HandleInterrupt(cpu, IR_NMI);
        cpu->nmi_detected = false;
    } else if (cpu->irq && !(cpu->state.p & CPU_FLAG_I)) {
        if (cpu->log)
            fprintf(cpu->log, "IRQ\n");
        DummyRead(cpu, state->pc);
        HandleInterrupt(cpu, IR_IRQ);
    }

    //Process pending halt before next instruction
    ProcessHalt(cpu, state->pc);
    
    cpu->instr_cycle = 0;
    return 0;
}

void CPU_SetNMISignal(CPU *cpu, bool nmi)
{
    if (nmi == true && cpu->nmi == false)
        cpu->nmi_detected = true;
    cpu->nmi = nmi;
}

void CPU_SetIRQSignal(CPU *cpu, bool irq) { cpu->irq = irq; }

void CPU_ScheduleHalt(CPU *cpu) { cpu->halt = true; }

uint8_t CPU_Read(CPU *cpu, uint16_t addr, AccessType access)
{
    cpu->state.cycles++;
    cpu->instr_cycle++;
    cpu->access_type = access;

    return cpu->callbacks.onread(cpu->callbacks.context, addr);
}

void CPU_Write(CPU *cpu, uint16_t addr, uint8_t data, AccessType access)
{
    cpu->state.cycles++;
    cpu->instr_cycle++;
    cpu->access_type = access;

    cpu->callbacks.onwrite(cpu->callbacks.context, addr, data);
}

int CPU_Disassemble(CPU *cpu, uint16_t instr_addr, char *buffer, size_t n)
{
    

    uint8_t opcode = Peek(cpu, instr_addr);
    uint8_t op1 = Peek(cpu, instr_addr + 1);
    uint8_t op2 = Peek(cpu, instr_addr + 2);
    const char* opcode_name = OPCODE_NAMES[opcode];

    switch (ADDRMODE_TABLE[opcode]) {
        case AD_IMP:
            snprintf(buffer, n, "$%04X \t$%02X         \t%-4s", instr_addr, opcode, opcode_name);
            return 1;
        case AD_ACC:
            snprintf(buffer, n, "$%04X \t$%02X         \t%-4s A", instr_addr, opcode, opcode_name);
            return 1;
        case AD_IMM:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s #$%02X", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        case AD_ZPG:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s $%02X", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        case AD_ZPX:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s $%02X,X", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        case AD_ZPY:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s $%02X,Y", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        case AD_REL:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s $%02X", instr_addr, opcode, op1, opcode_name, instr_addr + 2 + (int8_t)op1);
            return 2;
        case AD_ABS:
            snprintf(buffer, n, "$%04X \t$%02X $%02X $%02X \t%-4s $%04X", instr_addr, opcode, op1, op2, opcode_name, (uint16_t)op2 << 8 | op1);
            return 3;
        case AD_ABX:
            snprintf(buffer, n, "$%04X \t$%02X $%02X $%02X \t%-4s $%04X,X", instr_addr, opcode, op1, op2, opcode_name, (uint16_t)op2 << 8 | op1);
            return 3;
        case AD_ABY:
            snprintf(buffer, n, "$%04X \t$%02X $%02X $%02X \t%-4s $%04X,Y", instr_addr, opcode, op1, op2, opcode_name, (uint16_t)op2 << 8 | op1);
            return 3;
        case AD_IND:
            snprintf(buffer, n, "$%04X \t$%02X $%02X $%02X \t%-4s ($%04X)", instr_addr, opcode, op1, op2, opcode_name, (uint16_t)op2 << 8 | op1);
            return 3;
        case AD_IDX:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s ($%02X,X)", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        case AD_IDY:
            snprintf(buffer, n, "$%04X \t$%02X $%02X     \t%-4s ($%02X),Y", instr_addr, opcode, op1, opcode_name, op1);
            return 2;
        default:
            return 0;
    }
}

void CPU_SetLogFile(CPU *cpu, FILE *logfile) { cpu->log = logfile; }

/* PRIVATE FUNCTION DEFINITIONS */

uint16_t FetchAddr(CPU *cpu, AddrMode mode, int forWrite)
{
    switch (mode) {
        case AD_IMM: return cpu->state.pc++;
        case AD_ZPG: return FetchByte(cpu);
        case AD_ZPX: {
            uint8_t addr = FetchByte(cpu);
            DummyRead(cpu, addr);
            return addr + cpu->state.x & 0xFF;
        }
        case AD_ZPY: {
            uint8_t addr = FetchByte(cpu);
            DummyRead(cpu, addr);
            return addr + cpu->state.y & 0xFF;
        }
        case AD_ABS: return FetchWord(cpu);
        case AD_ABX: return AddrAddIndex(cpu, FetchWord(cpu), cpu->state.x, forWrite);
        case AD_ABY: return AddrAddIndex(cpu, FetchWord(cpu), cpu->state.y, forWrite);
        case AD_IND: return ReadWord(cpu, FetchWord(cpu));
        case AD_IDX: {
            uint8_t ptr = FetchByte(cpu);
            DummyRead(cpu, ptr);
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
       DummyRead(cpu, cpu->state.pc);
       uint16_t branch = cpu->state.pc + disp;
       uint16_t pchDiff = (branch & 0xFF00) - (cpu->state.pc & 0xFF00);
       if (pchDiff != 0) {
            DummyRead(cpu, branch - pchDiff);
       }
       cpu->state.pc = branch;
    }
}

void HandleInterrupt(CPU *cpu, InterruptType interrupt)
{
    DummyRead(cpu, cpu->state.pc);

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
    DummyRead(cpu, cpu->state.pc);
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
    DummyRead(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_C;
}

void CLD(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_D;
}

void CLI(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.p &= ~CPU_FLAG_I;
}

void CLV(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
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
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, --cpu->state.x);
}

void DEY(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
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
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, ++cpu->state.x);
}

void INY(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, ++cpu->state.y);
}

void JMP(CPU *cpu, AddrMode mode) {
    cpu->state.pc = FetchAddr(cpu, mode, 0);
}

void JSR(CPU *cpu, AddrMode mode) {
    uint16_t addr = FetchByte(cpu);
    DummyReadStack(cpu);
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
    DummyRead(cpu, cpu->state.pc);
    cpu->state.a = OpLSR(cpu, cpu->state.a);
}

void NOP(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
}

void ORA(CPU* cpu, AddrMode mode) {
    cpu->state.a |= ReadByMode(cpu, mode);
    UpdateNZ(cpu, cpu->state.a);
}

void PHA(CPU *cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    Push(cpu, cpu->state.a);
}

void PHP(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    Push(cpu, cpu->state.p | CPU_FLAG_B);
}

void PLA(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    DummyReadStack(cpu);
    cpu->state.a = Pop(cpu);
    UpdateNZ(cpu, cpu->state.a);
}

void PLP(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    DummyReadStack(cpu);
    PopP(cpu);
}

void ROL(CPU *cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpROL(cpu, val));
}

void ROLA(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.a = OpROL(cpu, cpu->state.a);
}

void ROR(CPU *cpu, AddrMode mode) {
    uint16_t addr;
    uint8_t val = RMWReadByMode(cpu, mode, &addr);
    Write(cpu, addr, OpROR(cpu, val));
}

void RORA(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.a = OpROR(cpu, cpu->state.a);
}

void RTI(CPU *cpu, AddrMode mode)
{
    DummyRead(cpu, cpu->state.pc);
    DummyReadStack(cpu);
    PopP(cpu);
    PopPC(cpu);
}

void RTS(CPU *cpu, AddrMode mode)
{
    DummyRead(cpu, cpu->state.pc);
    DummyReadStack(cpu);
    PopPC(cpu);
    DummyFetchByte(cpu);
}

void SBC(CPU* cpu, AddrMode mode) {
    OpADC(cpu, ~ReadByMode(cpu, mode));
}

void SEC(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.p |= CPU_FLAG_C;
}

void SED(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.p |= CPU_FLAG_D;
}

void SEI(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
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
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.x = cpu->state.a);
}

void TAY(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.y = cpu->state.a);
}

void TSX(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.x = cpu->state.s);
}
void TXA(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.a = cpu->state.x);
}
void TXS(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    cpu->state.s = cpu->state.x;
}
void TYA(CPU* cpu, AddrMode mode) {
    DummyRead(cpu, cpu->state.pc);
    UpdateNZ(cpu, cpu->state.a = cpu->state.y);
}
