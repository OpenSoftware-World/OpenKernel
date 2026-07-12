#include "nonuniversal_api_defs.h"


void openkernel_nonuniversal_api_vga_clear_screen() {
    syscall(1, NULL);
}

void openkernel_nonuniversal_api_vga_print_hex(uint32_t n) {
    syscall(13, &n);
}

void openkernel_nonuniversal_api_vga_print_dec(int num) {
    syscall(14, &num);
}

void openkernel_nonuniversal_api_vga_print_bin(unsigned int num) {
    syscall(15, &num);
}

void openkernel_nonuniversal_api_sb16_gen_flat_sound(uint8_t level) {
    syscall(16, &level);
}

void openkernel_nonuniversal_api_sb16_test() {
    syscall(17, NULL);
}

void openkernel_nonuniversal_api_get_sys_cpuinfo(const char *mode) {
    syscall(18, &mode);
}

int openkernel_nonuniversal_api_fat12_read_file(const char* file_name, void* buf, uint32_t size) {
    fat12_read_file_t args = {
        .file_name = file_name,
        .buf = buf,
        .size = size
    };
    return syscall(19, &args);
}

void openkernel_nonuniversal_api_fat16_list_root() {
    syscall(20, NULL);
}

int openkernel_nonuniversal_api_fat16_read_file(const char* filename, void* buffer, uint32_t size) {
    fat16_read_file_t args = {
        .filename = filename,
        .buffer = buffer,
        .size = size
    };
    return syscall(21, &args);
}

int openkernel_nonuniversal_api_fat32_read_file(const char* filename, void* buffer, uint32_t size) {
    fat32_read_file_t args = {
        .filename = filename,
        .buffer = buffer,
        .size = size
    };
    return syscall(22, &args);
}

void openkernel_nonuniversal_api_fat32_list_dir(uint32_t cluster) {
    syscall(23, &cluster);
}

void openkernel_nonuniversal_api_log_message(klog_level_t level, const char *message) {
    log_message_t args = {
        .level = level,
        .message = message
    };
    syscall(24, &args);
}

void openkernel_nonuniversal_api_exit_program(const char *mode, const char *message) {
    exit_program_t args = {
        .mode = mode,
        .message = message
    };
    syscall(25, &args);
}

void openkernel_nonuniversal_api_ofs_create_file(const char* name, uint32_t size) {
    ofs_create_file_t args = {
        .name = name,
        .size = size
    };
    syscall(26, &args);
}

void openkernel_nonuniversal_api_ofs_read_file(const char* name, uint8_t* buffer) {
    ofs_read_file_t args = {
        .name = name,
        .buffer = buffer
    };
    syscall(27, &args);
}

void openkernel_nonuniversal_api_ofs_write_file(const char* name, const uint8_t* data, uint32_t size) {
    ofs_write_file_t args = {
        .name = name,
        .data = data,
        .size = size
    };
    syscall(28, &args);
}

void openkernel_nonuniversal_api_ofs_delete_file(const char* name) {
    syscall(29, &name);
}

void openkernel_nonuniversal_api_ofs_list_files() {
    syscall(30, NULL);
}

void openkernel_nonuniversal_api_vfs_list() {
    syscall(31, NULL);
}

void* openkernel_nonuniversal_api_memset(void* dst, uint8_t val, uint32_t size) {
    memset_t args = {
        .dst = dst,
        .val = val,
        .size = size
    };
    return (void*) syscall(32, &args);
}

void* openkernel_nonuniversal_api_memcpy(void* dst, const void* src, uint32_t size) {
    memcpy_t args = {
        .dst = dst,
        .src = src,
        .size = size
    };
    return (void*) syscall(33, &args);
}

int openkernel_nonuniversal_api_memcmp(const void* a, const void* b, uint32_t size) {
    memcmp_t args = {
        .a = a,
        .b = b,
        .size = size
    };
    return syscall(34, &args);
}

void* openkernel_nonuniversal_api_kmalloc(uint32_t size) {
    return (void*) syscall(35, &size);
}

void openkernel_nonuniversal_api_kfree(void* ptr) {
    syscall(36, &ptr);
}

int openkernel_nonuniversal_api_strcmp(const char *s1, const char *s2) {
    strcmp_t args = {
        .s1 = s1,
        .s2 = s2
    };
    return syscall(37, &args);
}

int openkernel_nonuniversal_api_strncmp(const char *s1, const char *s2, unsigned int n) {
    strncmp_t args = {
        .s1 = s1,
        .s2 = s2,
        .n = n
    };
    return syscall(38, &args);
}

int openkernel_nonuniversal_api_strlen(const char *str) {
    return syscall(39, &str);
}

void openkernel_nonuniversal_api_strcpy(char *dst, const char *src) {
    strcpy_t args = {
        .dst = dst,
        .src = src
    };
    syscall(40, &args);
}


void openkernel_nonuniversal_api_strncpy(char *dst, const char *src, unsigned int n) {
    strncpy_t args = {
        .dst = dst,
        .src = src,
        .n = n
    };
    syscall(41, &args);
}

void openkernel_nonuniversal_api_print_time() {
    syscall(42, NULL);
}
