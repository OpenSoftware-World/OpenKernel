#ifndef SYSCALL_EXTERN_H
#define SYSCALL_EXTERN_H

#include <OpenKernel/SystemLib/Std/types.h>

extern void vga_print_scr(const char *str);
extern void vga_clear_screen(const char *mode);
extern void sys_next_status(const char *mode, uint32_t seconds);
extern void vga_set_bg_color(uint8_t color);
extern void vga_set_text_color(uint8_t color);
extern void vga_newline();
extern void vga_set_cursor(uint8_t x, uint8_t y);
extern void ptchar(char c);
extern void vga_print_scr_nw(const char *str);
extern void vga_set_color_scheme(uint8_t bg_color, uint8_t text_color);
extern char get_char();
extern void sleep(uint32_t seconds);
extern void vga_print_hex(uint32_t n);
extern void vga_print_dec(int num);
extern void vga_print_bin(unsigned int num);
extern void sb16_gen_flat_sound(uint8_t level);
extern void sb16_test();
extern void get_sys_cpuinfo(const char *mode);
extern int fat12_read_file(const char* file_name, void* buf, uint32_t size);
extern void fat16_list_root();
extern int fat16_read_file(const char* filename, void* buffer, uint32_t size);
extern int fat32_read_file(const char* filename, void* buffer, uint32_t size);
extern void fat32_list_dir(uint32_t cluster);
extern void log_message(klog_level_t level, const char *message);
extern void exit_program(const char *mode, const char *message);
extern void ofs_create_file(const char* name, uint32_t size);
extern void ofs_read_file(const char* name, uint8_t* buffer);
extern void ofs_write_file(const char* name, const uint8_t* data, uint32_t size);
extern void ofs_delete_file(const char* name);
extern void ofs_list_files();
extern void vfs_list();
extern void* memset(void* dst, uint8_t val, uint32_t size);
extern void* memcpy(void* dst, const void* src, uint32_t size);
extern int memcmp(const void* a, const void* b, uint32_t size);
extern void* kmalloc(uint32_t size);
extern void kfree(void* ptr);
extern int strcmp(const char *s1, const char *s2);
extern int strncmp(const char *s1, const char *s2, unsigned int n);
extern int strlen(const char *str);
extern void strcpy(char *dst, const char *src);
extern void strncpy(char *dst, const char *src, unsigned int n);
extern void print_time();

#endif