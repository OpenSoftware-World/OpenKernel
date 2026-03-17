#include "keyboard.h"
#include "../../SystemLib/SystemIO/io.h"

#define KBD_DATA 0x60
#define KBD_STAT 0x64

static char kmap[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',0,
    '*',0,' ',0
};

static char kbf[KEY_BF_SIZE];
static uint8_t buf_head = 0;
static uint8_t buf_tail = 0;

void kbd_init() {
    buf_head = 0;
    buf_tail = 0;
}

uint8_t kb_check() {
    return buf_head != buf_tail;
}

char get_char() {
    uint8_t scancode;
    
    while (1) {
        while (!(inb(KBD_STAT) & 1));
        scancode = inb(KBD_DATA);

        if (scancode & 0x80)
            continue;

        if (scancode < 128)
            return kmap[scancode];
    }
}

void kbd_hndlr() {
    uint8_t status = inb(KBD_STAT);
    if (status & 0x01) {
        uint8_t scancode = inb(KBD_DATA);
        char c = 0;
        if (scancode < 128) c = kmap[scancode];
        if (c) {
            kbf[buf_head] = c;
            buf_head = (buf_head + 1) % KEY_BF_SIZE;
        }
    }
}