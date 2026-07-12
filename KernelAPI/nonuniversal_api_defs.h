#ifndef NONUNIVERSAL_API_DEFS_H
#define NONUNIVERSAL_API_DEFS_H

#include <OpenKernel/KernelAPI/OpenKernelEnvironment/SysCalls/syscalls.h>
#include <OpenKernel/KernelAPI/OpenKernelEnvironment/types.h>
#include <OpenKernel/SystemLib/SysCalls/Structs/structs.h>

void openkernel_nonuniversal_api_vga_clear_screen();
void openkernel_nonuniversal_api_vga_print_hex(uint32_t n);
void openkernel_nonuniversal_api_vga_print_dec(int num);
void openkernel_nonuniversal_api_vga_print_bin(unsigned int num);
void openkernel_nonuniversal_api_sb16_gen_flat_sound(uint8_t level);
void openkernel_nonuniversal_api_sb16_test();
void openkernel_nonuniversal_api_get_sys_cpuinfo(const char *mode);
int openkernel_nonuniversal_api_fat12_read_file(const char* file_name, void* buf, uint32_t size);
void openkernel_nonuniversal_api_fat16_list_root();
int openkernel_nonuniversal_api_fat16_read_file(const char* filename, void* buffer, uint32_t size);
int openkernel_nonuniversal_api_fat32_read_file(const char* filename, void* buffer, uint32_t size);
void openkernel_nonuniversal_api_fat32_list_dir(uint32_t cluster);
void openkernel_nonuniversal_api_log_message(klog_level_t level, const char *message);
void openkernel_nonuniversal_api_sys_next_status(const char *mode, uint32_t seconds);
void openkernel_nonuniversal_api_exit_program(const char *mode, const char *message);
void openkernel_nonuniversal_api_ofs_create_file(const char* name, uint32_t size);
void openkernel_nonuniversal_api_ofs_read_file(const char* name, uint8_t* buffer);
void openkernel_nonuniversal_api_ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
void openkernel_nonuniversal_api_ofs_delete_file(const char* name);
void openkernel_nonuniversal_api_ofs_list_files();
void openkernel_nonuniversal_api_vfs_list();
void* openkernel_nonuniversal_api_memset(void* dst, uint8_t val, uint32_t size);
void* openkernel_nonuniversal_api_memcpy(void* dst, const void* src, uint32_t size);
int openkernel_nonuniversal_api_memcmp(const void* a, const void* b, uint32_t size);
void* openkernel_nonuniversal_api_kmalloc(uint32_t size);
void openkernel_nonuniversal_api_kfree(void* ptr);
int openkernel_nonuniversal_api_strcmp(const char *s1, const char *s2);
int openkernel_nonuniversal_api_strncmp(const char *s1, const char *s2, unsigned int n);
int openkernel_nonuniversal_api_strlen(const char *str);
void openkernel_nonuniversal_api_strcpy(char *dst, const char *src);
void openkernel_nonuniversal_api_strncpy(char *dst, const char *src, unsigned int n);
void openkernel_nonuniversal_api_print_time();

#endif