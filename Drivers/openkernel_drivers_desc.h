#ifndef OPENKERNEL_DRIVERS_DESC_H
#define OPENKERNEL_DRIVERS_DESC_H

#include "../SystemLib/Std/types.h"

typedef struct {
    uint8_t driver_name;
    uint8_t driver_desc;
    uint8_t driver_ver;
    uint8_t driver_krnl_ver;
} openkernel_drivers_information;

#endif