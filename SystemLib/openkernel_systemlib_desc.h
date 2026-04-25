#ifndef OPENKERNEL_SYSTEMLIB_DESC_H
#define OPENKERNEL_SYSTEMLIB_DESC_H

#include <OpenKernel/SystemLib/Std/types.h>
#include <OpenKernel/SystemLib/KernelFunc/kernel_func.h>
#include <OpenKernel/SystemLib/Memory/mem.h>
#include <OpenKernel/SystemLib/Std/std.h>
#include <OpenKernel/SystemLib/Syscalls/syscall.h>
#include <OpenKernel/SystemLib/SystemIO/io.h>
#include <OpenKernel/SystemLib/TaskMng/task.h>
#include <OpenKernel/SystemLib/TimeMng/time.h>
#include <OpenKernel/SystemLib/TypeConv/type_conv.h>

#define OPENKERNEL_SYSTEMLIB_PKG_NAME "OpenKernel System Libraries"
#define OPENKERNEL_SYSTEMLIB_PKG_VER "1.0"

typedef struct {
    const char *module_name;
    const char *module_desc;
    const char *module_ver;
    const char *module_author;
    const char *module_file_name;
    const char *module_krnl_ver;
} openkernel_systemlib_module_information_t;

openkernel_systemlib_module_information_t kernel_func_lib;
openkernel_systemlib_module_information_t memory_lib;
openkernel_systemlib_module_information_t std_lib;
openkernel_systemlib_module_information_t types_lib;
openkernel_systemlib_module_information_t syscalls_lib;
openkernel_systemlib_module_information_t systemio_lib;
openkernel_systemlib_module_information_t taskmng_lib;
openkernel_systemlib_module_information_t timemng_lib;
openkernel_systemlib_module_information_t type_conv_lib;

kernel_func_lib = (openkernel_systemlib_module_information_t) {
    .module_name = KERNEL_FUNC_MODULE_NAME,
    .module_desc = KERNEL_FUNC_MODULE_DESC,
    .module_ver = KERNEL_FUNC_MODULE_VER,
    .module_author = KERNEL_FUNC_MODULE_AUTHOR,
    .module_file_name = KERNEL_FUNC_MODULE_FILE_NAME,
    .module_krnl_ver = KERNEL_FUNC_MODULE_KRNL_VER,
};

memory_lib = (openkernel_systemlib_module_information_t) {
    .module_name = MEM_MODULE_NAME,
    .module_desc = MEM_MODULE_DESC,
    .module_ver = MEM_MODULE_VER,
    .module_author = MEM_MODULE_AUTHOR,
    .module_file_name = MEM_MODULE_FILE_NAME,
    .module_krnl_ver = MEM_MODULE_KRNL_VER,
};

std_lib = (openkernel_systemlib_module_information_t) {
    .module_name = STD_MODULE_NAME,
    .module_desc = STD_MODULE_DESC,
    .module_ver = STD_MODULE_VER,
    .module_author = STD_MODULE_AUTHOR,
    .module_file_name = STD_MODULE_FILE_NAME,
    .module_krnl_ver = STD_MODULE_KRNL_VER,
};

types_lib = (openkernel_systemlib_module_information_t) {
    .module_name = TYPES_MODULE_NAME,
    .module_desc = TYPES_MODULE_DESC,
    .module_ver = TYPES_MODULE_VER,
    .module_author = TYPES_MODULE_AUTHOR,
    .module_file_name = TYPES_MODULE_FILE_NAME,
    .module_krnl_ver = TYPES_MODULE_KRNL_VER,
};

syscalls_lib = (openkernel_systemlib_module_information_t) {
    .module_name = SYSCALL_MODULE_NAME,
    .module_desc = SYSCALL_MODULE_DESC,
    .module_ver = SYSCALL_MODULE_VER,
    .module_author = SYSCALL_MODULE_AUTHOR,
    .module_file_name = SYSCALL_MODULE_FILE_NAME,
    .module_krnl_ver = SYSCALL_MODULE_KRNL_VER,
};

systemio_lib = (openkernel_systemlib_module_information_t) {
    .module_name = IO_MODULE_NAME,
    .module_desc = IO_MODULE_DESC,
    .module_ver = IO_MODULE_VER,
    .module_author = IO_MODULE_AUTHOR,
    .module_file_name = IO_MODULE_FILE_NAME,
    .module_krnl_ver = IO_MODULE_KRNL_VER,
};

taskmng_lib = (openkernel_systemlib_module_information_t) {
    .module_name = TASK_MNG_MODULE_NAME,
    .module_desc = TASK_MNG_MODULE_DESC,
    .module_ver = TASK_MNG_MODULE_VER,
    .module_author = TASK_MNG_MODULE_AUTHOR,
    .module_file_name = TASK_MNG_MODULE_FILE_NAME,
    .module_krnl_ver = TASK_MNG_MODULE_KRNL_VER,
};

timemng_lib = (openkernel_systemlib_module_information_t) {
    .module_name = TIME_MODULE_NAME,
    .module_desc = TIME_MODULE_DESC,
    .module_ver = TIME_MODULE_VER,
    .module_author = TIME_MODULE_AUTHOR,
    .module_file_name = TIME_MODULE_FILE_NAME,
    .module_krnl_ver = TIME_MODULE_KRNL_VER,
};

type_conv_lib = (openkernel_systemlib_module_information_t) {
    .module_name = TYPE_CONV_MODULE_NAME,
    .module_desc = TYPE_CONV_MODULE_DESC,
    .module_ver = TYPE_CONV_MODULE_VER,
    .module_author = TYPE_CONV_MODULE_AUTHOR,
    .module_file_name = TYPE_CONV_MODULE_FILE_NAME,
    .module_krnl_ver = TYPE_CONV_MODULE_KRNL_VER,
};


#endif
