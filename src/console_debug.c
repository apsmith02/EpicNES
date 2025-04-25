#define _CRT_SECURE_NO_WARNINGS
#include "console_debug.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define INPUT_BUFFER_MAX 1024
#define CMD_MAX_ARGS 5

DebugStepType ConsoleDebugCallback(void *emulator)
{
    Emulator* emu = (Emulator*)emulator;
    static int step_cycles = 0;
    if (step_cycles > 0) {
        step_cycles--;
        return DEBUG_STEP_CPUCYCLE;
    }

    PrintDisassembly(emu, emu->cpu.instr_addr, 4, 4);
    printf("\nCPU:\n");
    PrintCPURegisters(emu);

    //Read commands

    static char input_buffer[INPUT_BUFFER_MAX];
    char* args[CMD_MAX_ARGS];
    int argc;
    do {
        fgets(input_buffer, INPUT_BUFFER_MAX, stdin);

        //Trim newline
        for (int i = 0; i < INPUT_BUFFER_MAX; i++) {
            if (input_buffer[i] == '\n') {
                input_buffer[i] = '\0';
                break;
            }
        }

        //Tokenize args
        argc = 0;
        args[0] = strtok(input_buffer, " ");
        if (args[0] == NULL) {
            printf("No command entered.\n");
            continue;
        }
        argc++;
        while (argc < CMD_MAX_ARGS && (args[argc] = strtok(NULL, " ")) != NULL) {
            argc++;
        }
        
        //Run command
        if (!strcmp(args[0], "help")) {
            printf("apu - Print APU registers\n");
            printf("b $<instrAddr> - Set instruction breakpoint\n");
            printf("b -rwx {cpu|ram|prgrom} $<startAddr> $<endAddr> - Set read/write/execute breakpoint in address range\n");
            printf("blist - List breakpoints\n");
            printf("brst {on|off} - Enable/disable break on reset\n");
            printf("c - Continue\n");
            printf("cpu - Print CPU registers\n");
            printf("d <listNumber> - Delete breakpoint list entry listNumber\n");
            printf("d clear - Clear breakpoint list\n");
            printf("dsm [pc | $<addr>] [<linesBefore=4>] [<linesAfter=4>] - Print disassembly\n");
            printf("mem {cpu|ppu|ram|vram|prgrom|chrrom} $<startAddr> $<endAddr> - Print memory\n");
            printf("ppu - Print PPU registers\n");
            printf("pwr - Power cycle console\n");
            printf("sc - Step CPU cycle\n");
            printf("sc <n> - Step n CPU cycles\n");
            printf("si - Step instruction\n");
        }
        else if (!strcmp(args[0], "apu"))       PrintAPU(emu);
        else if (!strcmp(args[0], "b"))         Cmd_b(emu, argc, args);
        else if (!strcmp(args[0], "blist"))     PrintBreakpointList(emu);
        else if (!strcmp(args[0], "brst"))      Cmd_brst(emu, argc, args);
        else if (!strcmp(args[0], "c"))         return DEBUG_STEP_NONE;
        else if (!strcmp(args[0], "cpu"))       PrintCPURegisters(emu);
        else if (!strcmp(args[0], "d"))         Cmd_d(emu, argc, args);
        else if (!strcmp(args[0], "dsm"))       Cmd_dsm(emu, argc, args);
        else if (!strcmp(args[0], "mem"))       Cmd_mem(emu, argc, args);
        else if (!strcmp(args[0], "ppu"))       PrintPPURegisters(emu);
        else if (!strcmp(args[0], "pwr")) {     Emu_PowerOn(emu); return DEBUG_STEP_NONE; }
        else if (!strcmp(args[0], "sc"))        {
            if (argc >= 2)
                step_cycles = atoi(args[1]) - 1;
            return DEBUG_STEP_CPUCYCLE;
        }
        else if (!strcmp(args[0], "si"))        return DEBUG_STEP_INTO;
        else {
            printf("Invalid command. Type 'help' for a command list.\n");
        }
    } while (true);
}

void PrintDisassembly(Emulator *emu, uint16_t addr, int linesBefore, int linesAfter)
{
    static char asm_buffer[64];

    if (!Emu_DebugIsOpcode(emu, addr) && 
    Emu_DebugIterLastInstruction(emu, &addr) == -1 && Emu_DebugIterNextInstruction(emu, &addr) == -1) {
        return;
    }

    for (int i = 0; i < linesBefore; i++) {
        if (Emu_DebugIterLastInstruction(emu, &addr) == -1)
            break;
    }

    printf("  |ADDR |\tOPC|OP1|OP2|\tINST|OPERAND\n");
    for (int i = 0; i < linesBefore + 1 + linesAfter; i++) {
        Emu_DebugDisassemble(emu, addr, asm_buffer, sizeof(asm_buffer));
        printf("%c%c %s", 
            (addr == emu->cpu.instr_addr) ? '>' : ' ',                 //Display an arrow pointing to current instruction
            (Emu_DebugAddrHasCodeBreakpoint(emu, addr) ? '*' : ' '), //Display an asterisk next to instructions with a code breakpoint
            asm_buffer
        );
        if (addr == emu->cpu.instr_addr) {
            //Display cycle number and access type next to current instruction
            printf("  %d ", emu->cpu.instr_cycle);
            switch (emu->cpu.access_type) {
               case ACCESS_READ: printf("R"); break;
                case ACCESS_WRITE: printf("W"); break;
                case ACCESS_EXECUTE: printf("X"); break;
                case ACCESS_DUMMY_READ: printf("DR"); break;
                case ACCESS_DUMMY_WRITE: printf("DW"); break;
                default: break;
            }
        }
        printf("\n");

        if (Emu_DebugIterNextInstruction(emu, &addr) == -1)
            break;
    }
}

void PrintBreakpointList(Emulator *emu)
{
    for (size_t i = 0; i < Emu_DebugGetBreakpointCount(emu); i++) {
        Breakpoint bp = Emu_DebugGetBreakpoint(emu, i);
        const char* typeStr;
        switch (bp.type) {
            case MEM_CPU:       typeStr = "CPU"; break;
            case MEM_PPU:       typeStr = "PPU"; break;
            case MEM_RAM:       typeStr = "RAM"; break;
            case MEM_VRAM:      typeStr = "VRAM"; break;
            case MEM_PRG_ROM:   typeStr = "PRG ROM"; break;
            case MEM_CHR_ROM:   typeStr = "CHR ROM"; break;
            default:            typeStr = "NONE"; break;
        }

        printf("%u \t%c%c%c \t%-8s \t",
            (unsigned)i+1,
            (bp.access & MEMDEBUG_BREAK_R) ? 'r' : '-',
            (bp.access & MEMDEBUG_BREAK_W) ? 'w' : '-',
            (bp.access & MEMDEBUG_BREAK_X) ? 'x' : '-',
            typeStr
        );
        if (bp.start == bp.end) {
            printf("$%X", bp.start);
            //For single addresses, print CPU mirrors of memory
            int cpuAddr = -1;
            if (Emu_DebugIterPRGMirrors(emu, bp.type, bp.start, &cpuAddr) != -1) {
                printf(" (CPU: $%04X", cpuAddr);
                while (Emu_DebugIterPRGMirrors(emu, bp.type, bp.start, &cpuAddr) != -1) {
                    printf(",$%04X", cpuAddr);
                }
                printf(")");
            }
            printf("\n");
        }
        else
            printf("[$%X,$%X]\n", bp.start, bp.end);
    }
}

MemoryType ParseMemoryType(const char *str)
{
    MemoryType type = MEM_NONE;
    if      (!strcmp(str, "cpu"))   type = MEM_CPU;
    else if (!strcmp(str, "ppu"))   type = MEM_PPU;
    else if (!strcmp(str, "ram"))   type = MEM_RAM;
    else if (!strcmp(str, "vram"))  type = MEM_VRAM;
    else if (!strcmp(str, "prgrom"))type = MEM_PRG_ROM;
    else if (!strcmp(str, "chrrom"))type = MEM_CHR_ROM;
    return type;
}

void PrintCPURegisters(Emulator* emu) {
    CPUState* state = &emu->cpu.state;
    printf("Cycle: %llu  PC: $%04X  A: $%02X  X: $%02X  Y: $%02X  S: $%02X  P: $%02X (%c%c-%c%c%c%c%c)\n", 
        state->cycles, state->pc, state->a, state->x, state->y, state->s, state->p,
    (state->p & CPU_FLAG_N) ? 'N' : 'n',
    (state->p & CPU_FLAG_V) ? 'V' : 'v',
    (state->p & CPU_FLAG_B) ? 'B' : 'b',
    (state->p & CPU_FLAG_D) ? 'D' : 'd',
    (state->p & CPU_FLAG_I) ? 'I' : 'i',
    (state->p & CPU_FLAG_Z) ? 'Z' : 'z',
    (state->p & CPU_FLAG_C) ? 'C' : 'c');
}

void PrintPPURegisters(Emulator* emu) {
    PPUState* state = &emu->ppu.state;
    printf("Cycle: %d  Scanline: %d  Frame: %llu  VRAM Addr: $%04X  T: $%04X  Fine X Scroll:  %d  Write Toggle: [%c]  OAM Addr: $%02X\n",
        state->cycle, state->scanline, state->frames, state->v, state->t, (int)state->x, state->w ? 'X' : ' ', state->oamaddr
    );
    printf("VRAM Increment: +%d  Sprite Table: $%04X  BG Table: $%04X  8x16 Sprite Mode: [%c]  NMI Enable: [%c]\n",
        (state->ppuctrl & PPUCTRL_INC) ? 32 : 1,
        (state->ppuctrl & PPUCTRL_SPRTABLE) ? 0x1000 : 0, (state->ppuctrl & PPUCTRL_BGTABLE) ? 0x1000 : 0,
        (state->ppuctrl & PPUCTRL_SPRSIZE) ? 'X' : ' ', (state->ppuctrl & PPUCTRL_NMI) ? 'X' : ' '
    );
    printf("Render BG: [%c]  Render Sprites: [%c]  Sprite Overflow: [%c]  Sprite 0 Hit: [%c]  VBlank: [%c]\n",
        (state->ppumask & PPUMASK_BG) ? 'X' : ' ', (state->ppumask & PPUMASK_SPR) ? 'X' : ' ',
        (state->ppustatus & PPUSTATUS_SPROVERFLOW) ? 'X' : ' ', (state->ppustatus & PPUSTATUS_SPR0HIT) ? 'X' : ' ', (state->ppustatus & PPUSTATUS_VBLANK) ? 'X' : ' '
    );
}

void PrintAPU(Emulator* emu) {
    APUState* state = &emu->apu.state;

    APUPulse* pulse1 = &state->ch_pulse1;
    APUEnvelope* p1_env = &pulse1->envelope;

    APUPulse* pulse2 = &state->ch_pulse2;
    APUEnvelope* p2_env = &pulse2->envelope;

    APUTriangle* triangle = &state->ch_triangle;
    
    APUNoise* noise = &state->ch_noise;
    APUEnvelope* n_env = &state->ch_noise.envelope;

    printf("Frame Counter -  5-step: [%c]  IRQ inhibit: [%c]  FC Cycle Counter: %.1f APU cycles\n",
        (state->fc_ctrl & FC_5STEP) ? 'X' : ' ',
        (state->fc_ctrl & FC_IRQ_INHIBIT) ? 'X' : ' ',
        (float)(state->fc_cycles / 2.0)
    );
    printf("Channel Enable -  DMC: [%c]  Noise: [%c]  Triangle: [%c]  Pulse 2: [%c]  Pulse 1: [%c]\n",
        ' ',
        noise->length.enabled ? 'X' : ' ',
        triangle->length.enabled ? 'X' : ' ',
        pulse2->length.enabled ? 'X' : ' ',
        pulse1->length.enabled ? 'X' : ' '
    );
    printf("Pulse 1 -  Period: %d  Timer: %d  Duty: %d  Duty Pos: %d\n",
        (int)pulse1->period, (int)pulse1->timer, (int)pulse1->duty, (int)pulse1->duty_pos);
    printf(" Env Start: [%c]  Env Loop / Length Halt: [%c]  Env Const Volume: [%c]  Env Period/Volume: %d  Env Divider: %d  Env Decay: %d  Length Counter: %d\n",
        p1_env->start ? 'X' : ' ',
        pulse1->length.halt ? 'X' : ' ',
        p1_env->constant_volume ? 'X' : ' ',
        (int)p1_env->period,
        (int)p1_env->divider,
        (int)p1_env->decay,
        (int)pulse1->length.counter
    );
    printf("Pulse 2 -  Period: %d  Timer: %d  Duty: %d  Duty Pos: %d\n",
        (int)pulse2->period, (int)pulse2->timer, (int)pulse2->duty, (int)pulse2->duty_pos);
    printf(" Env Start: [%c]  Env Loop / Length Halt: [%c]  Env Const Volume: [%c]  Env Period/Volume: %d  Env Divider: %d  Env Decay: %d  Length Counter: %d\n",
        p2_env->start ? 'X' : ' ',
        pulse2->length.halt ? 'X' : ' ',
        p2_env->constant_volume ? 'X' : ' ',
        (int)p2_env->period,
        (int)p2_env->divider,
        (int)p2_env->decay,
        (int)pulse2->length.counter
    );
    printf("Triangle - Period: %d  Timer: %d  Wave Pos: %d\n", 
        (int)triangle->period, (int)triangle->timer, (int)triangle->wave_pos);
    printf(" Length Counter: %d  Linear Counter: %d  Linear Ctr Reload Value: %d  Linear Ctr Reload: [%c]\n",
        (int)triangle->length.counter, (int)triangle->linear_counter, (int)triangle->linear_reload_value, triangle->linear_reload ? 'X' : ' ');
    printf("Noise - Period: %d  Timer: %d  LFSR: %d%d%d%d%d%d%d%d%d%d%d%d%d%d%d",
        (int)noise->period, (int)noise->timer,
        (int)(noise->lfsr >> 14 & 1), (int)(noise->lfsr >> 13 & 1), (int)(noise->lfsr >> 12 & 1),
        (int)(noise->lfsr >> 11 & 1), (int)(noise->lfsr >> 10 & 1), (int)(noise->lfsr >> 9 & 1), (int)(noise->lfsr >> 8 & 1),
        (int)(noise->lfsr >> 7 & 1), (int)(noise->lfsr >> 6 & 1), (int)(noise->lfsr >> 5 & 1), (int)(noise->lfsr >> 4 & 1),
        (int)(noise->lfsr >> 3 & 1), (int)(noise->lfsr >> 2 & 1), (int)(noise->lfsr >> 1 & 1), (int)(noise->lfsr & 1)
    );
    printf(" Env Start: [%c]  Env Loop / Length Halt: [%c]  Env Const Volume: [%c]  Env Period/Volume: %d  Env Divider: %d  Env Decay: %d  Length Counter: %d\n",
        n_env->start ? 'X' : ' ',
        noise->length.halt ? 'X' : ' ',
        n_env->constant_volume ? 'X' : ' ',
        (int)n_env->period,
        (int)n_env->divider,
        (int)n_env->decay,
        (int)noise->length.counter
    );
}


void Cmd_b(Emulator *emu, int argc, char *args[])
{
    if (argc == 2) {
        if (!strncmp(args[1], "$", 1)) {
            Emu_DebugSetCodeBreakpoint(emu, strtoul(&args[1][1], NULL, 16));
            return;
        }
    } else if (argc == 5 && 
        !strncmp(args[1], "-", 1) &&//-access flag
        !strncmp(args[3], "$", 1) &&//$<startAddr>
        !strncmp(args[4], "$", 1)   //$<endAddr>
    ) {
        Breakpoint bp;
        //Parse -access flags
        bp.access = 0;
        for (int i = 1; args[1][i] != '\0'; i++) {
            switch (args[1][i]) {
                case 'r': bp.access |= MEMDEBUG_BREAK_R; break;
                case 'w': bp.access |= MEMDEBUG_BREAK_W; break;
                case 'x': bp.access |= MEMDEBUG_BREAK_X; break;
                default: break;
            }
        }
        if (bp.access > 0) {
            //Parse memory type
            bp.type = ParseMemoryType(args[2]);
            if (bp.type != MEM_NONE) {
                bp.start = strtoul(&args[3][1], NULL, 16);
                bp.end = strtoul(&args[4][1], NULL, 16);
                Emu_DebugSetBreakpointRange(emu, bp.type, bp.access, bp.start, bp.end);
                return;
            }
        }
    }
    printf("Usage:\n");
    printf(" b $<instrAddr>\n");
    printf(" b -rwx {cpu|ram|prgrom} $<startAddr> $<endAddr>\n");
}

void Cmd_brst(Emulator *emu, int argc, char *args[])
{
    if (argc >= 2) {
        if (!strcmp(args[1], "on")) {
            Emu_DebugSetBreakOnReset(emu, true);
            return;
        }
        if (!strcmp(args[1], "off")) {
            Emu_DebugSetBreakOnReset(emu, false);
            return;
        }
    }

    printf("Usage:\n");
    printf("brst {on|off}");
}

void Cmd_d(Emulator *emu, int argc, char *args[])
{
    if (argc == 2) {
        if (!strcmp(args[1], "clear")) {
            Emu_DebugClearBreakpoints(emu);
            return;
        } else {
            size_t i = atoi(args[1]) - 1;
            if (i < Emu_DebugGetBreakpointCount(emu)) {
                Emu_DebugDeleteBreakpoint(emu, i);
            } else {
                printf("Invalid breakpoint number.\n");
            }
            return;
        }
    }
    printf("Usage:\n");
    printf(" d <listNumber>\n");
    printf(" d clear\n");
}

void Cmd_dsm(Emulator *emu, int argc, char *args[])
{
    uint16_t addr = emu->cpu.instr_addr;
    int linesBefore = 4;
    int linesAfter = 4;

    if (argc >= 2 && args[1][0] == '$') {
        addr = strtoul(&args[1][1], NULL, 16);
    }
    if (argc >= 3) {
        linesBefore = atoi(args[2]);
    } if (argc >= 4) {
        linesAfter = atoi(args[3]);
    }
    PrintDisassembly(emu, addr, linesBefore, linesAfter);
}

void Cmd_mem(Emulator *emu, int argc, char *args[])
{
    if (argc >= 4) {
        MemoryType type = ParseMemoryType(args[1]);
        if (type != MEM_NONE && args[2][0] == '$' && args[3][0] == '$') {
            unsigned start = strtoul(&args[2][1], NULL, 16);
            unsigned end = strtoul(&args[3][1], NULL, 16);
            
            Memory* memory = &emu->memory;

            //Display hex dump divided into 16 byte rows

            start &= ~0xF; //Round down start address to 16-byte alignment
            end &= ~0xF; //Round up end address to 16-byte alignment
            end += 0x10;
            
            unsigned size;
            if (type == MEM_CPU)        size = CPU_ADDR_SPACE;
            else if (type == MEM_PPU)   size = PPU_ADDR_SPACE;
            else                        size = Mem_Size(memory, type);

            end = min(end, size);
            
            printf("         | $0  $1  $2  $3  $4  $5  $6  $7  $8  $9  $A  $B  $C  $D  $E  $F\n\n");
            while (start < end) {
                printf("$%08X|", start);
                for (unsigned i = start; i < start + 0x10 && i < end; i++) {
                    uint8_t byte;
                    if (type == MEM_CPU)        byte = Mem_CPUPeek(memory, i);
                    else if (type == MEM_PPU)   byte = Mem_PPUPeek(memory, i);
                    else                        byte = Mem_Get(memory, type, i);
                    printf(" $%02X", byte);
                }
                printf("\n");
                start += 0x10;
            }
            return;
        }
    }
    printf("Usage:\n");
    printf(" mem {cpu|ppu|ram|vram|prgrom|chrrom} $<startAddr> $<endAddr>\n");
}
