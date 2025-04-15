#ifndef EMULATOR_H
#define EMULATOR_H

#include "nes_defs.h"
#include "rom.h"
#include "memory.h"
#include "cpu.h"
#include "ppu.h"
#include "apu.h"
#include "standard_controller.h"


typedef struct {
    unsigned start;     //Start address
    unsigned end;       //End address
    MemoryType type;    //Memory type
    uint8_t rwx;        //DebugBreakOn flags
} Breakpoint;

typedef struct {
    Breakpoint* arr;
    size_t size;
    size_t capacity;
} Vec_Breakpoint;

void Vec_BP_Init(Vec_Breakpoint* vec);
void Vec_BP_Free(Vec_Breakpoint* vec);

void Vec_BP_Add(Vec_Breakpoint* vec, Breakpoint bp);
void Vec_BP_Remove(Vec_Breakpoint* vec, size_t index);
void Vec_BP_Clear(Vec_Breakpoint* vec);

Breakpoint* Vec_BP_At(Vec_Breakpoint* vec, size_t index);


typedef enum {
    DEBUG_STEP_NONE = 0,
    DEBUG_STEP_INTO,
    DEBUG_STEP_CPUCYCLE
} DebugStepType;

typedef DebugStepType(*DebugPauseCallback)(void*);


typedef struct {
    INESHeader rom_ines;
    //ROM rom;
    int is_rom_loaded;

    CPU cpu;
    PPU ppu;
    APU apu;
    StandardController controller;

    Memory memory;

    bool power_on_queued;

    // Debugging

    bool debug_enable;
    bool in_cpu_exec;
    bool debug_break_on_reset;

    DebugPauseCallback debug_pause_callback;
    void* debug_pause_userdata; //User data pointer passed to debug_pause_callback()
    DebugStepType debug_step;

    Vec_Breakpoint breakpoints;
} Emulator;

Emulator* Emu_Create();

void Emu_Free(Emulator* emu);

/**
* Load a ROM from a file and power on the console.
*
* @return 0 on success, -1 on error.
*/
int Emu_LoadROM(Emulator* emu, const char* filename);

void Emu_CloseROM(Emulator* emu);

int Emu_IsROMLoaded(Emulator* emu);

/**
 * Power on the console.
*/
void Emu_PowerOn(Emulator* emu);

/**
* Run one frame.
*
* @return 0 on success, -1 on error.
*/
int Emu_RunFrame(Emulator* emu);

/**
* Press a button on the standard controller connected to port 1.
*/
void Emu_PressButton(Emulator* emu, ControllerButton button);
/**
* Release a button on the standard controller connected to port 1.
*/
void Emu_ReleaseButton(Emulator* emu, ControllerButton button);

RGBAPixel* Emu_GetPixelBuffer(Emulator* emu, int* width, int* height);

void* Emu_GetAudioBuffer(Emulator* emu, size_t* len);
void Emu_ClearAudioBuffer(Emulator* emu);

/**
 * Enable debugging. This will enable debug pauses and breakpoints, which call a callback set via Emu_SetDebugPauseCallback() when hit.
*/
void Emu_DebugEnable(Emulator* emu, bool enabled);

/** 
 * Set a callback to be called when the debugger is paused. Emulator execution will not resume until returned.
 * The callback must take a void* containing user data, and return a DebugStepType. The return allows execution to step by
 * an instruction, CPU cycle, etc and then pause again.
*/
void Emu_SetDebugPauseCallback(Emulator* emu, DebugPauseCallback callback, void* userdata);

/**
 * @param enabled If true, the debugger will break on reset.
*/
void Emu_DebugSetBreakOnReset(Emulator* emu, bool enabled);

/**
 * @return True if debugger break on reset is enabled.
*/
bool Emu_DebugGetBreakOnReset(Emulator* emu);

/**
 * Pause for debugging before executing the next instruction.
*/
void Emu_DebugPause(Emulator* emu);

/**
 * Set an execute breakpoint at an instruction by CPU address.
 * This will set the breakpoint at the physical memory currently mapped to the address.
 */
void Emu_DebugSetCodeBreakpoint(Emulator* emu, unsigned addr);

/**
 * Set a breakpoint at an address.
 * 
 * @param type The type of memory to set the breakpoint for.
 * @param break_on_flags The type of memory access to trigger the breakpoint on. Use DebugBreakOn bit flags.
 * @param addr The address to set the breakpoint for.
 */
void Emu_DebugSetBreakpoint(Emulator* emu, MemoryType type, int break_on_flags, unsigned addr);

/**
 * Set a breakpoint at a range of addresses.
 * 
 * @param type The type of memory to set the breakpoint for.
 * @param break_on_flags The type of memory access to trigger the breakpoint on. Use DebugBreakOn bit flags.
 * @param start_addr The start of the address range to set the breakpoint for.
 * @param end_addr The end of the address range to set the breakpoint for (inclusive).
*/
void Emu_DebugSetBreakpointRange(Emulator* emu, MemoryType type, int break_on_flags, unsigned start_addr, unsigned end_addr);

/**
 * Delete a breakpoint by list index.
 */
void Emu_DebugDeleteBreakpoint(Emulator* emu, size_t index);

/**
 * Clear breakpoint list.
 */
void Emu_DebugClearBreakpoints(Emulator* emu);

/**
 * @return Number of breakpoints in the breakpoint list.
*/
size_t Emu_DebugGetBreakpointCount(Emulator* emu);

/**
 * @return Breakpoint at index in the breakpoint list.
*/
Breakpoint Emu_DebugGetBreakpoint(Emulator* emu, size_t index);

/**
 * @return True if the code mapped to CPU address addr has an execute breakpoint, else false.
 */
bool Emu_DebugAddrHasCodeBreakpoint(Emulator* emu, uint16_t addr);

/**
 * Print the disassembly of an instruction at a CPU address to a string buffer of size n.
 * String format: [address] \t[opcode byte] [operand byte 1] [operand byte 2] \t[instruction name] [operand]
 * 
 * @return Instruction length, or -1 if there is no detected opcode at the address.
 */
int Emu_DebugDisassemble(Emulator* emu, uint16_t address, char* buffer, size_t n);

/**
 * Iterate backwards to the address of the previous instruction from the current address.
 * 
 * @param address Current address. Set to the address of the previous instruction, if any.
 * @return 0 on success, -1 if there is no previous instruction.
 */
int Emu_DebugIterLastInstruction(Emulator* emu, uint16_t* address);

/**
 * Iterate to the address of the next instruction from the current address.
 * 
 * @param address Current address. Set to the address of the next instruction, if any.
 * @return 0 on success, -1 if there is no next instruction.
 */
int Emu_DebugIterNextInstruction(Emulator* emu, uint16_t* address);

/**
 * @return True if the memory mapped to the CPU address is an opcode, else false
 */
bool Emu_DebugIsOpcode(Emulator* emu, uint16_t address);

/**
 * Iterate all CPU addresses that a byte of physical memory is mapped to. Set cpu_addr to -1 to start.
 * @return 0 on success, -1 at the end.
 */
int Emu_DebugIterPRGMirrors(Emulator* emu, MemoryType type, int physical_addr, int* cpu_addr);

#endif