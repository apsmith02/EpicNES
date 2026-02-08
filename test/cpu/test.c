#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t RAM64k[0x10000];
uint8_t ram_read(void* ram, uint16_t addr) { return ((uint8_t*)ram)[addr]; }
void ram_write(void* ram, uint16_t addr, uint8_t data) { ((uint8_t*)ram)[addr] = data; }

void assert_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p);

void test_CPU_PowerOn();
void test_LDAimm();

int main() {
    test_CPU_PowerOn();
    test_LDAimm();

    return 0;
}

void assert_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p) {
    if (pc != state->pc || a != state->a || x != state->x || y != state->y || s != state->s || p != state->p) {
        printf("FAIL: CPU registers do not have expected values.\n");
        printf("Expected - PC: $%04X, A: $%02X, X: $%02X, Y: $%02X, S: $%02X, P: $%02X\n", pc, a, x, y, s, p);
        printf("Actual   - PC: $%04X, A: $%02X, X: $%02X, Y: $%02X, S: $%02X, P: $%02X\n", state->pc, state->a, state->x, state->y, state->s, state->p);
        exit(1);
    }
}

void test_CPU_PowerOn() {
    CPU cpu;
    RAM64k ram;

    //Set reset vector to $ABCD. On power-on, the CPU should jump to $ABCD.
    ram[0xFFFC] = 0xCD;
    ram[0xFFFD] = 0xAB;

    CPU_Init(&cpu, (CPUCallbacks){
        .context = &ram,
        .onread = ram_read,
        .onwrite = ram_write
    });
    CPU_PowerOn(&cpu);
    assert_cpu_regs(&cpu.state, 0xABCD, 0, 0, 0, 0xFD, 0x24);
}

void test_LDAimm()
{
    CPU cpu;
    RAM64k ram = {0xA9, 0xCD};

    CPU_Init(&cpu, (CPUCallbacks){
        .context = &ram,
        .onread = ram_read,
        .onwrite = ram_write
    });
    cpu.state = (CPUState){
        .pc = 0x0000,
        .a = 0x00,
        .x = 0x00,
        .y = 0x00,
        .s = 0xFD,
        .p = 0x04
    };
    CPU_Exec(&cpu);
    assert_cpu_regs(&cpu.state, 0x0002, 0xCD, 0x00, 0x00, 0xFD, 0x84);
}
