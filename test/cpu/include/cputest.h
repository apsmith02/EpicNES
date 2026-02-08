#ifndef CPUTEST_H
#define CPUTEST_H

#include "cpu.h"
#include <stdio.h>
#include <stdlib.h>

typedef uint8_t RAM64k[0x10000];
//Read RAM. $0800-$0FFF, $1000-$17FF, and $1800-$1FFF are mirrors of $0000-$07FF.
uint8_t ram_read(void* ram, uint16_t addr);

//Write RAM. $0800-$0FFF, $1000-$17FF, and $1800-$1FFF are mirrors of $0000-$07FF.
void ram_write(void* ram, uint16_t addr, uint8_t data);

typedef struct {
    char rw;
    uint16_t addr;
    uint8_t data;
} Cycle;

typedef struct CycleNode_s CycleNode;
struct CycleNode_s {
    Cycle data;
    CycleNode* next;
};

CycleNode* cyclenode_add(CycleNode* head, Cycle data, size_t max_size);

//RAM test structure with a log of the last log_max cycles.
typedef struct {
    RAM64k ram;
    CycleNode* cycle_log;
    size_t log_max;
} RAMLog;

void ramlog_init(RAMLog* ram, size_t log_max);

void ramlog_free(RAMLog* ram);

void ramlog_print(RAMLog* ram);

uint8_t ramlog_read(void* ramlog, uint16_t addr);

void ramlog_write(void* ramlog, uint16_t addr, uint8_t data);

//Returns -1 on fail
int test_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p);

void assert_cpu_regs(CPUState* state, uint16_t pc, uint8_t a, uint8_t x, uint8_t y, uint8_t s, uint8_t p);

#endif