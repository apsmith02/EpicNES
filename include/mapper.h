//MAPPER IMPLEMENTATIONS
#include "nrom.h"

#define NUM_MAPPERS 1

static const Mapper_Vtable* const MAPPER_VTABLES[NUM_MAPPERS] = {
    &NROM_VTABLE
};
