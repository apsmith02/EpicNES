#include "cpu.h"
#include "cputest.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    // Command line args
    bool no_illegal = false; //command line argument: --no-illegal flag
    if (argc > 1) {
        if (!strcmp(argv[1], "--no-illegal")) {
            no_illegal = true;
        } else {
            printf("Invalid argument %s\n", argv[1]);
            printf("Usage:\nnestest <args...>\nValid arguments:\n--no-illegal : Do not test illegal opcodes\n");
            return 1;
        }
    }

    //Initialize CPU and RAM
    CPU cpu;
    RAMLog ram;
    ramlog_init(&ram, 10);
    CPU_Init(&cpu, (CPUCallbacks){
        .context = &ram,
        .onread = &ramlog_read,
        .onwrite = &ramlog_write
    });

    //Load ROM
    FILE* romFile;
    if ((romFile = fopen("nestest.nes", "rb")) == NULL) {
        perror("Error opening file nestest.nes");
    }
    INESHeader ines;
    if (INES_ReadHeader(&ines, romFile)) {
        fprintf(stderr, "Error reading nestest.nes: Invalid iNES ROM file format.\n");
    }
    char *prg = INES_ReadPRG(&ines, romFile);
    memcpy(ram.ram + 0xC000, prg, ines.prg_bytes);

    //The reset vector for nestest.nes on "automation" is $C000.
    ram.ram[0xFFFC] = 0x00;
    ram.ram[0xFFFD] = 0xC0;
    
    //Load log file
    FILE* logfile = fopen("nestest.log", "r");
    if (!logfile) {
        printf("Error loading nestest.log.\n");
        return 1;
    }
    char line[256];
    char lastline[256];

    strcpy(lastline, "Power-on reset (CYC: 0)");

    //Run tests! Run each instruction and compare results with log.

    CPU_PowerOn(&cpu);
    while (fgets(line, sizeof(line), logfile)) {
        uint16_t pc;
        uint8_t a, x, y, s, p;
        unsigned long long cycles;
        char buf[256];
        
        strncpy(buf, line, 256);

        //Extract expected state from log line
        char* col = strtok(buf, " ");
        pc = strtol(col, NULL, 16);
        do {
            /*If not testing illegal opcodes and
            * the next logged instruction is an illegal opcode (which starts with an asterisk),
            * end testing early and consider the official opcode tests passed.
            */
            if (no_illegal && col[0] == '*') {
                printf("All tests before the illegal opcode tests pass.\nTo test illegal opcodes, remove the --no-illegal flag.\n");
                exit(0);
            }

            //Parse register values
            if          (!strncmp(col, "A:", 2)) {
                a = strtol(col + 2, NULL, 16);
            } else if   (!strncmp(col, "X:", 2)) {
                x = strtol(col + 2, NULL, 16);
            } else if   (!strncmp(col, "Y:", 2)) {
                y = strtol(col + 2, NULL, 16);
            } else if   (!strncmp(col, "P:", 2)) {
                p = strtol(col + 2, NULL, 16);
            } else if   (!strncmp(col, "SP:", 2)) {
                s = strtol(col + 3, NULL, 16);
            } else if   (!strncmp(col, "CYC:", 2)) {
                cycles = strtol(col + 4, NULL, 10);
            }
        } while (col = strtok(NULL, " "));

        if (test_cpu_regs(&cpu.state, pc, a, x, y, s, p) != 0) {
            printf("Log line of last instruction executed: %s\n", lastline);
            printf("Last %u cycles:\n", ram.log_max);
            ramlog_print(&ram);
            exit(1);
        }
        if (cpu.state.cycles != cycles) {
            printf("FAIL: CPU did not execute expected number of cycles.\n");
            printf("Expected CYC after last instruction - %d\n", cycles);
            printf("Actual CYC after last instruction   - %d\n", cpu.state.cycles);
            printf("Log line of last instruction executed: %s\n", lastline);
            printf("Last %u cycles:\n", ram.log_max);
            ramlog_print(&ram);
            exit(1);
        }

        strcpy(lastline, line);
        
        if (CPU_Exec(&cpu) != 0) {
            printf("FAIL: CPU crashed.\n");
            printf("Log line of last instruction executed: %s\n", lastline);
            printf("Last %u cycles:\n", ram.log_max);
            ramlog_print(&ram);
            exit(1);
        }
    }

    return 0;
}