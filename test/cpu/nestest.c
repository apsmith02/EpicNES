#include "cpu.h"
#include "cputest.h"
#include "rom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    CPU cpu;
    RAMLog ram;
    ramlog_init(&ram, 10);
    CPU_Init(&cpu, &ramlog_read, &ramlog_write, &ram);

    //Load ROM
    ROM rom;
    if (ROM_Load(&rom, "nestest.nes") != 0) {
        printf("Error loading nestest.nes.\n");
        return 1;
    }

    memcpy(ram.ram + 0xC000, rom.prg_rom, 16384);

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