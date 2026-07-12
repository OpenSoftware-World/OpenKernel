#include "syscall.h"
#include "Structs/structs.h"

int syscall(int id, void* arg) {
    return syscall_handler(id, arg);
}

int syscall_handler(int id, void* arg) {
    switch (id) {
        case SYSCALL_PRINT:
            if (!arg) return -1;
            vga_print_scr((char*)arg);
            return 0;
        case SYSCALL_CLEAR_SCREEN:
            vga_clear_screen("C");
            return 0;
        case SYSCALL_SHUTDOWN:
            sys_next_status("S", 0);
            return 0;
        case SYSCALL_REBOOT:
            sys_next_status("R", 0);
            return 0;
        case SYSCALL_SET_BG_COLOR:
            if (!arg) return -1;
            vga_set_bg_color((uint8_t)arg);
            return 0;
        case SYSCALL_SET_TXT_COLOR:
            if (!arg) return -1;
            vga_set_text_color((uint8_t)arg);
            return 0;
        case SYSCALL_NEWLINE:
            vga_newline();
            return 0;
        case SYSCALL_SET_CURSOR:
            if (!arg) return -1;
            vga_set_cursor_t* cursor_args = (vga_set_cursor_t*)arg;
            vga_set_cursor(cursor_args->x, cursor_args->y);
            return 0;
        case SYSCALL_PTCHAR:
            if (!arg) return -1;
            ptchar((char)arg);
            return 0;
        case SYSCALL_PRINT_NW:
            if (!arg) return -1;
            vga_print_scr_nw((char*)arg);
            return 0;
        case SYSCALL_SET_COLOR_SCH:
            if (!arg) return -1;
            vga_set_color_scheme_t* color_scheme_args = (vga_set_color_scheme_t*)arg;
            vga_set_color_scheme(color_scheme_args->bg_color, color_scheme_args->text_color);
            return 0;
        case SYSCALL_GET_CHR:
            get_char();
            return 0;
        case SYSCALL_SLEEP:
            if (!arg) return -1;
            sleep((uint32_t)arg);
            return 0;
        case SYSCALL_PRINT_HEX:
            if (!arg) return -1;
            vga_print_hex((uint32_t)arg);
            return 0;
        case SYSCALL_PRINT_DEC:
            if (!arg) return -1;
            vga_print_dec((int)arg);
            return 0;
        case SYSCALL_PRINT_BIN:
            if (!arg) return -1;
            vga_print_bin((unsigned int)arg);
            return 0;
        case SYSCALL_SB16_FLAT:
            if (!arg) return -1;
            sb16_gen_flat_sound((uint8_t)arg);
            return 0;
        case SYSCALL_SB16_TEST:
            sb16_test();
            return 0;
        case SYSCALL_GET_CPU:
            if (!arg) return -1;
            get_sys_cpuinfo((char*)arg);
            return 0;
        case SYSCALL_FAT12_RF:
            if (!arg) return -1;
            fat12_read_file_t* fat12_rf_args = (fat12_read_file_t*)arg;
            fat12_read_file(fat12_rf_args->file_name, fat12_rf_args->buf, fat12_rf_args->size);
            return 0;
        case SYSCALL_FAT16_LIST_R:
            fat16_list_root();
            return 0;
        case SYSCALL_FAT16_RF:
            if (!arg) return -1;
            fat16_read_file_t* fat16_rf_args = (fat16_read_file_t*)arg;
            fat16_read_file(fat16_rf_args->filename, fat16_rf_args->buffer, fat16_rf_args->size);
            return 0;
        case SYSCALL_FAT32_RF:
            if (!arg) return -1;
            fat32_read_file_t* fat32_rf_args = (fat32_read_file_t*)arg;
            fat32_read_file(fat32_rf_args->filename, fat32_rf_args->buffer, fat32_rf_args->size);
            return 0;
        case SYSCALL_FAT32_LIST_R:
            if (!arg) return -1;
            fat32_list_dir((uint32_t)arg);
            return 0;
        case SYSCALL_LOG_MSG:
            if (!arg) return -1;
            log_message_t* log_message_args = (log_message_t*)arg;
            log_message(log_message_args->level, log_message_args->message);
            return 0;
        case SYSCALL_EXIT_PROG:
            if (!arg) return -1;
            exit_program_t* exit_program_args = (exit_program_t*)arg;
            exit_program(exit_program_args->mode, exit_program_args->message);
            return 0;
        case SYSCALL_OFS_CREATE_F:
            if (!arg) return -1;
            ofs_create_file_t* ofs_create_file_args = (ofs_create_file_t*)arg;
            ofs_create_file(ofs_create_file_args->name, ofs_create_file_args->size);
            return 0;
        case SYSCALL_OFS_RF:
            if (!arg) return -1;
            ofs_read_file_t* ofs_read_file_args = (ofs_read_file_t*)arg;
            ofs_read_file(ofs_read_file_args->name, ofs_read_file_args->buffer);
            return 0;
        case SYSCALL_OFS_WF:
            if (!arg) return -1;
            ofs_write_file_t* ofs_write_file_args = (ofs_write_file_t*)arg;
            ofs_write_file(ofs_write_file_args->name, ofs_write_file_args->data, ofs_write_file_args->size);
            return 0;
        case SYSCALL_OFS_DF:
            if (!arg) return -1;
            ofs_delete_file((char*)arg);
            return 0;
        case SYSCALL_OFS_LF:
            ofs_list_files();
            return 0;
        case SYSCALL_VFS_L:
            vfs_list();
            return 0;
        case SYSCALL_MEMSET:
            if (!arg) return -1;
            memset_t* memset_args = (memset_t*)arg;
            memset(memset_args->dst, memset_args->val, memset_args->size);
            return 0;
        case SYSCALL_MEMCPY:
            if (!arg) return -1;
            memcpy_t* memcpy_args = (memcpy_t*)arg;
            memcpy(memcpy_args->dst, memcpy_args->src, memcpy_args->size);
            return 0;
        case SYSCALL_MEMCMP:
            if (!arg) return -1;
            memcmp_t* memcmp_args = (memcmp_t*)arg;
            memcmp(memcmp_args->a, memcmp_args->b, memcmp_args->size);
            return 0;
        case SYSCALL_KMALLOC:
            if (!arg) return -1;
            kmalloc((uint32_t)arg);
            return 0;
        case SYSCALL_KFREE:
            if (!arg) return -1;
            kfree((void*)arg);
            return 0;
        case SYSCALL_STRCMP:
            if (!arg) return -1;
            strcmp_t* strcmp_args = (strcmp_t*)arg;
            strcmp(strcmp_args->s1, strcmp_args->s2);
            return 0;
        case SYSCALL_STRNCMP:
            if (!arg) return -1;
            strncmp_t* strncmp_args = (strncmp_t*)arg;
            strncmp(strncmp_args->s1, strncmp_args->s2, strncmp_args->n);
            return 0;
        case SYSCALL_STRLEN:
            if (!arg) return -1;
            strlen((char*)arg);
            return 0;
        case SYSCALL_STRCPY:
            if (!arg) return -1;
            strcpy_t* strcpy_args = (strcpy_t*)arg;
            strcpy(strcpy_args->dst, strcpy_args->src);
            return 0;
        case SYSCALL_STRNCPY:
            if (!arg) return -1;
            strncpy_t* strncpy_args = (strncpy_t*)arg;
            strncpy(strncpy_args->dst, strncpy_args->src, strncpy_args->n);
            return 0;
        case SYSCALL_PRINT_TIME:
            print_time();
            return 0;
        default:
            return -1;
    }
}
