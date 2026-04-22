#ifndef OPENKERNEL_DRIVERS_DESC_H
#define OPENKERNEL_DRIVERS_DESC_H

#include <OpenKernel/SystemLib/Std/types.h>

typedef struct {
    const char *driver_name;
    const char *driver_desc;
    const char *driver_ver;
    const char *driver_author;
    const char *driver_krnl_ver;
} openkernel_drivers_information;

#endif
