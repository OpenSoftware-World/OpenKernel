#include "vga.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

static uint16_t *vgabuffer = (uint16_t *)VBUFFER;
static uint8_t txt_color = VGA_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
static uint8_t cx = 0; // cursor x
static uint8_t cy = 0; // cursor y

void vga_clear_screen(const char *mode) {
    if (mode[0] == 'F') {
        for (uint16_t y=0; y < VHEIGHT; y++) {
            for (uint16_t x=0; x < VWIDTH; x++) {
                vgabuffer[y * VWIDTH + x] = VGA_ENTRY(' ', txt_color);
            }
        }
        cx = 0;
        cy = 0;
    } else if (mode[0] == 'C') {
        for (int y = 1; y < VHEIGHT - 1; y++) {
        for (int x = 0; x < VWIDTH; x++) {
            vgabuffer[y * VWIDTH + x] = VGA_ENTRY(' ', txt_color);
        }
    }
    cx = 0;
    cy = 1;
    }
}

void vga_set_bg_color(uint8_t color) {
    uint8_t fg = txt_color & 0x0F;
    txt_color = VGA_COLOR(color, fg);
    vga_clear_screen("F");
}

void vga_set_text_color(uint8_t color) {
    uint8_t bg = (txt_color >> 4) & 0x0F;
    txt_color = VGA_COLOR(bg, color);
}

void vga_set_color_scheme(uint8_t bg_color, uint8_t text_color) {
    vga_set_bg_color(bg_color);
    vga_set_text_color(text_color);
}

void ptchar(char c) {
    if (c == '\n') {
        cx = 0;
        cy++;
    } else {
        vgabuffer[cy * VWIDTH + cx] = VGA_ENTRY(c, txt_color);
        cx++;
        if (cx >= VWIDTH) {
            cx = 0;
            cy++;
        }
    }
    if (cy >= VHEIGHT) {
        vga_clear_screen("C");
    }
    vga_set_cursor(cx, cy);
}

void vga_print_scr(const char *str) {
    while (*str) {
        ptchar(*str++);
    }
}

void vga_print_hex(uint32_t n) {
    char hex[9];
    const char *digits = "0123456789ABCDEF";

    for (int i = 0; i < 8; i++) {
        hex[7 - i] = digits[n & 0xF];
        n >>= 4;
    }
    hex[8] = '\0';

    for(int i = 0; i < 8; i++) {
        ptchar(hex[i]);
    }
}

void vga_print_dec(int num) {
    char buf[12];
    int i = 0;

    if (num == 0) {
        ptchar('0');
        return;
    }
    if (num < 0) {
        ptchar('-');
        num = -num;
    }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    while (i--) {
        ptchar(buf[i]);
    }
}

void vga_print_bin(unsigned int num) {
    int started = 0;
    for (int i = 31; i >= 0; i--) {
        if (num & (1u << i)) {
            ptchar('1');
            started = 1;
        } else if (started) {
            ptchar('0');
        }
    }
    if (!started) {
        ptchar('0');
    }
}

void vga_cursor_mode(const char *mode, uint8_t start, uint8_t end) {
    if (mode[0] == 'E') {
        outb(0x3D4, 0x0A);
        outb(0x3D5, start & 0x1F);
        outb(0x3D4, 0x0B);
        outb(0x3D5, end & 0x1F);
    }
    else if (mode[0] == 'D') {
        outb(0x3D4, 0x0A);
        outb(0x3D5, 0x20);
    }
}

void vga_print_scr_nw(const char *str) {
    vga_print_scr(str);
    vga_newline();
}

void vga_newline() {
    cx = 0;
    cy++;

    if (cy >= VHEIGHT - 1) {
        vga_clear_screen("C");
    }
    vga_set_cursor(cx, cy);
}

void vga_init() {
    vga_clear_screen("F");
}

void vga_set_cursor(uint8_t x, uint8_t y) {
    uint16_t pos = y * 80 + x;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void vga_bckspc() {
    if (cx == 0 && cy == 0) return;

    if (cx > 0) {
        cx--;
    } else {
        cy--;
        cx = VWIDTH - 1;
    }
    uint16_t *video = (uint16_t*)VBUFFER;
    video[cy * VWIDTH + cx] = VGA_ENTRY(' ', txt_color);
    vga_set_cursor(cx, cy);
}