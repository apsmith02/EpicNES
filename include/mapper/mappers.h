#ifndef MAPPERS_H
#define MAPPERS_H

#include "mapper_base.h"
#include "nrom.h"
#include "mmc1.h"
#include "uxrom.h"

/* Returns a pointer to a new mapper of the given mapper number, or NULL if the mapper is not supported by the emulator.
* Mapper must be initialized with Mapper_Init() after creating.
*/
MapperBase* Mapper_New(unsigned mapper_number) {
    //Add mapper implementations' new functions here. Position in array must correspond to mapper number.
    static MapperBase*(*const NEW_FNS[])(void)= {
        NROM_New,
        MMC1_New,
        UxROM_New
    };
    
    if (mapper_number < sizeof(NEW_FNS) / sizeof(NEW_FNS[0]))
        return NEW_FNS[mapper_number]();
    return NULL;
}

#endif