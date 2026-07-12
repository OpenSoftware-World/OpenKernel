#ifndef SYSCALL_H
#define SYSCALL_H

#include "syscall_extern.h"

#define SYSCALL_PRINT           0
#define SYSCALL_CLEAR_SCREEN    1
#define SYSCALL_SHUTDOWN        2
#define SYSCALL_REBOOT          3
#define SYSCALL_SET_BG_COLOR    4
#define SYSCALL_SET_TXT_COLOR   5
#define SYSCALL_NEWLINE         6
#define SYSCALL_SET_CURSOR      7
#define SYSCALL_PTCHAR          8
#define SYSCALL_PRINT_NW        9
#define SYSCALL_SET_COLOR_SCH   10
#define SYSCALL_GET_CHR         11
#define SYSCALL_SLEEP           12
#define SYSCALL_PRINT_HEX       13
#define SYSCALL_PRINT_DEC       14
#define SYSCALL_PRINT_BIN       15
#define SYSCALL_SB16_FLAT       16
#define SYSCALL_SB16_TEST       17
#define SYSCALL_GET_CPU         18
#define SYSCALL_FAT12_RF        19
#define SYSCALL_FAT16_LIST_R    20
#define SYSCALL_FAT16_RF        21
#define SYSCALL_FAT32_RF        22
#define SYSCALL_FAT32_LIST_R    23
#define SYSCALL_LOG_MSG         24
#define SYSCALL_EXIT_PROG       25
#define SYSCALL_OFS_CREATE_F    26
#define SYSCALL_OFS_RF          27
#define SYSCALL_OFS_WF          28
#define SYSCALL_OFS_DF          29
#define SYSCALL_OFS_LF          30
#define SYSCALL_VFS_L           31
#define SYSCALL_MEMSET          32
#define SYSCALL_MEMCPY          33
#define SYSCALL_MEMCMP          34
#define SYSCALL_KMALLOC         35
#define SYSCALL_KFREE           36
#define SYSCALL_STRCMP          37
#define SYSCALL_STRNCMP         38
#define SYSCALL_STRLEN          39
#define SYSCALL_STRCPY          40
#define SYSCALL_STRNCPY         41
#define SYSCALL_PRINT_TIME      42

int syscall(int id, void* arg);
int syscall_handler(int id, void* arg);

#define SYSCALL_MODULE_NAME "Syscall Library"
#define SYSCALL_MODULE_DESC "Syscall Library for OpenKernel"
#define SYSCALL_MODULE_VER "2.0"
#define SYSCALL_MODULE_AUTHOR "OpenSoftware-World"
#define SYSCALL_MODULE_FILE_NAME "syscall.lib"
#define SYSCALL_MODULE_KRNL_VER "3.0"

#endif
