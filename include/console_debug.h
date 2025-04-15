#ifndef CONSOLE_DEBUG_H
#define CONSOLE_DEBUG_H
#include "emulator.h"

/**
 * EpicNES console debugger functions
*/

//Debug callback for emulator
DebugStepType ConsoleDebugCallback(void* emulator);

void PrintDisassembly(Emulator* emu, uint16_t addr, int linesBefore, int linesAfter);
void PrintCPURegisters(Emulator* emu);
void PrintPPURegisters(Emulator* emu);
void PrintAPU(Emulator* emu);

void PrintBreakpointList(Emulator* emu);

/**
 * Breakpoint set
 * Usage:
 * - b $<instrAddr>
 * - b -rwx {cpu|ppu|ram|vram|prgrom|chrrom} $<startAddr> $<endAddr>
*/
void Cmd_b(Emulator* emu, int argc, char* args[]);

/**
 * Break on reset
 * Usage:
 * - brst {on|off}
*/
void Cmd_brst(Emulator* emu, int argc, char* args[]);

/**
 * Breakpoint delete
 * Usage:
 * - d <listNumber>
 * - d clear
 */
void Cmd_d(Emulator* emu, int argc, char* args[]);

/**
  * Print disassembly
  * Usage:
  * - dsm [pc | $<addr>] [<linesBefore=4>] [<linesAfter=4>]
 */
void Cmd_dsm(Emulator* emu, int argc, char* args[]);

/**
 * Print memory
 * Usage:
 * - mem {cpu|ppu|ram|vram|prgrom|chrrom} $<startAddr> $<endAddr>
*/
//void Cmd_mem(Emulator* emu, int argc, char* args[]);

#endif