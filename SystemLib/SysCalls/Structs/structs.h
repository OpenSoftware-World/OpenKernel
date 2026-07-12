#ifndef STRUCTS_H
#define STRUCTS_H

#include <OpenKernel/SystemLib/Std/types.h>

enum kernel_log_level {
    LOG_SUCCESS,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
    LOG_UNKNOWN
} klog_level_t;

typedef enum kernel_log_level klog_level_t;

typedef struct {
    uint8_t x;
    uint8_t y;
} vga_set_cursor_t;

typedef struct {
    uint8_t bg_color;
    uint8_t text_color;
} vga_set_color_scheme_t;

typedef struct {
    const char* file_name;
    void* buf;
    uint32_t size;
} fat12_read_file_t;

typedef struct {
    const char* filename;
    void* buffer;
    uint32_t size;
} fat16_read_file_t;

typedef struct {
    const char* filename;
    void* buffer;
    uint32_t size;
} fat32_read_file_t;

typedef struct {
    klog_level_t level;
    const char *message;
} log_message_t;

typedef struct {
    const char *mode;
    const char *message;
} exit_program_t;

typedef struct {
    const char* name;
    uint32_t size;
} ofs_create_file_t;

typedef struct {
    const char* name;
    uint8_t* buffer;
} ofs_read_file_t;

typedef struct {
    const char* name;
    const uint8_t* data;
    uint32_t size;
} ofs_write_file_t;

typedef struct {
    void* dst;
    uint8_t val;
    uint32_t size;
} memset_t;

typedef struct {
    void* dst;
    const void* src;
    uint32_t size;
} memcpy_t;

typedef struct {
    const void* a;
    const void* b;
    uint32_t size;
} memcmp_t;

typedef struct {
    const char *s1;
    const char *s2;
} strcmp_t;

typedef struct {
    const char *s1;
    const char *s2;
    unsigned int n;
} strncmp_t;

typedef struct {
    char *dst;
    const char *src;
} strcpy_t;

typedef struct {
    char *dst;
    const char *src;
    unsigned int n;
} strncpy_t;

#endif