#include "keyboard.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

#define KBD_DATA 0x60
#define KBD_STAT 0x64

static bool_t caps_lock = false;

static char kmap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

static char kbf[KEY_BF_SIZE]__attribute__((unused));
static uint8_t buf_head = 0;
static uint8_t buf_tail = 0;

static char to_upper(char c) {
    if (caps_lock && c >= 'a' && c <= 'z') {
        return c - 32;
    }
    return c;
}

void kbd_init() {
    buf_head = 0;
    buf_tail = 0;
    caps_lock = false;
}

uint8_t kb_check() {
    return buf_head != buf_tail;
}

char get_char() {
    uint8_t scancode;

    while (1) {
        while (!(inb(KBD_STAT) & 1));

        scancode = inb(KBD_DATA);

        if (scancode & 0x80) {
            continue;
        }

        kbd_hndlr(scancode);

        if (scancode < 128) {
            char c = kmap[scancode];

            if (c == 0) {
                continue;
            }
            
            c = to_upper(c);

            return c;
        }
    }
}

void kbd_hndlr(uint8_t scancode) {
    if (scancode == 0x3A) {
        caps_lock = !caps_lock;
    }
}