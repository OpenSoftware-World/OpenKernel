#include "vga.h"
#include "../../SystemLib/SystemIO/io.h"

static uint16_t *vgabuffer = (uint16_t *)VBUFFER;
static uint8_t txt_color = VGA_COLOR(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
static uint8_t cx = 0;
static uint8_t cy = 0;

void vga_screen_clear() {
    for (uint16_t y=0; y < VHEIGHT; y++) {
        for (uint16_t x=0; x < VWIDTH; x++) {
            vgabuffer[y * VWIDTH + x] = VGA_ENTRY(' ', txt_color);
        }
    }
    cx = 0;
    cy = 0;
}

void vga_set_bg_color(uint8_t color) {
    uint8_t fg = txt_color & 0x0F;
    txt_color = VGA_COLOR(color, fg);
    vga_screen_clear();
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
        vga_screen_clear();
    }
    vga_set_cursor(cx, cy);
}

void vga_print_scr(const char *str) {
    while (*str) {
        ptchar(*str++);
    }
}

void vga_print_scr_nw(const char *str) {
    vga_print_scr(str);
    vga_newline();
}

void vga_newline() {
    cx = 0;
    cy++;

    if (cy >= VHEIGHT) {
        vga_screen_clear();
    }
    vga_set_cursor(cx, cy);
}

void vga_init() {
    vga_screen_clear();
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