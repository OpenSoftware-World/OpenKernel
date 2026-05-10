#ifndef SYSCALL_EXTERN_H
#define SYSCALL_EXTERN_H

extern void vga_print_scr(const char *str);
extern void vga_clear_screen(const char *mode);
extern void sys_next_status(const char *mode, uint32_t seconds);

#endif