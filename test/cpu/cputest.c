#include "cputest.h"
#include <string.h>

uint8_t ram_read(void* ram, uint16_t addr) {
    if (addr < 0x2000)
        addr %= 0x800;
    return ((uint8_t*)ram)[addr];
}

void ram_write(void* ram, uint16_t addr, uint8_t data) {
    if (addr < 0x2000)
        addr %= 0x800;
    ((uint8_t*)ram)[addr] = data;
}

CycleNode* cyclenode_add(CycleNode* head, Cycle data, size_t max_size) {
    size_t size = 1;
    CycleNode** p = &head;
    
    while (*p != NULL) {
        p = &(*p)->next;
        size++;
    }

    *p = malloc(sizeof(CycleNode));
    (*p)->data = data;
    (*p)->next = NULL;

    while (size > max_size) {
        CycleNode* next = head->next;
        free(head);
        head = next;
        size--;
    }

    return head;
}

void ramlog_init(RAMLog* ram, size_t log_max) {
    memset(ram->ram, 0, sizeof(RAM64k));
    ram->cycle_log = NULL;
    ram->log_max = log_max;
}

void ramlog_free(RAMLog* ram) {
    CycleNode* c = ram->cycle_log;
    while (c != NULL) {
        CycleNode* next = c->next;
        free(c);
        c = next;
    }
}

void ramlog_print(RAMLog* ram) {
    for (CycleNode* c = ram->cycle_log; c != NULL; c = c->next) {
        Cycle* data = &c->data;
        printf("%c [$%04X] = $%02X\n", data->rw, data->addr, data->data);
    }
}

uint8_t ramlog_read(void* ramlog, uint16_t addr) {
    RAMLog* ram = ramlog;
    Cycle cycle;
    cycle.rw = 'R';
    cycle.addr = addr;

    cycle.data = ram_read(&ram->ram, addr);

    ram->cycle_log = cyclenode_add(ram->cycle_log, cycle, ram->log_max);

    return cycle.data;
}

void ramlog_write(void* ramlog, uint16_t addr, uint8_t data) {
    RAMLog* ram = ramlog;
    Cycle cycle;
    cycle.rw = 'W';
    cycle.addr = addr;
    cycle.data = data;

    ram_write(&ram->ram, addr, data);
    
    ram->cycle_log = cyclenode_add(ram->cycle_log, cycle, ram->log_max);
}

int test_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p) {
    if (pc != state->pc || a != state->a || x != state->x || y != state->y || s != state->s || p != state->p) {
        printf("FAIL: CPU registers do not have expected values.\n");
        printf("Expected - PC: $%04X, A: $%02X, X: $%02X, Y: $%02X, S: $%02X, P: $%02X\n", pc, a, x, y, s, p);
        printf("Actual   - PC: $%04X, A: $%02X, X: $%02X, Y: $%02X, S: $%02X, P: $%02X\n", state->pc, state->a, state->x, state->y, state->s, state->p);
        return -1;
    }
    return 0;
}

void assert_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p) {
    if (test_cpu_regs(state, pc, a, x, y, s, p) != 0)
        exit(1);
}