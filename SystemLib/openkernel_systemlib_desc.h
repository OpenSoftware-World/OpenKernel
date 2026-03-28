#ifndef OPENKERNEL_SYSTEMLIB_DESC_H
#define OPENKERNEL_SYSTEMLIB_DESC_H

#include "Std/types.h"

#define OPENKERNEL_SYSTEMLIB_PKG_NAME "OpenKernel System Libraries"
#define OPENKERNEL_SYSTEMLIB_PKG_VER "1.0"

typedef struct {
    uint8_t module_name;
    uint8_t module_desc;
    uint8_t module_ver;
    uint8_t module_author;
    uint8_t module_file_name;
    uint8_t module_krnl_ver;
} openkernel_systemlib_module_information;

#endif