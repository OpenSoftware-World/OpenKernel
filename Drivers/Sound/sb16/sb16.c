#include "sb16.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

static sb16_t sb16;
#define BUFFER_SIZE 8000
uint8_t audio_buffer[BUFFER_SIZE];

void sb16_write_dsp(uint8_t value) {
    sb16_wait_ready();
    outb(DSP_WRITE, value);
}

uint8_t sb16_read_dsp() {
    return inb(DSP_READ);
}

int sb16_wait_ready() {
    int timeout = 100000;
    while (inb(DSP_STATUS) & 0x80) {
        if (--timeout <= 0) {
            return -1;
        }
    }
    return 0;
}

int sb16_reset() {
    outb(DSP_RESET, 1);
    for (volatile int i = 0; i < 10000; i++);
    outb(DSP_RESET, 0);
    for (int i = 0; i < 100000; i++) {
        if (inb(DSP_READ) == 0xAA) {
            return 0;
        }
    }
    return -1;
}

void sb16_set_sample_rate(uint32_t hz) {
    sb16_wait_ready();
    sb16_write_dsp(0x40);
    sb16_write_dsp((hz >> 8) & 0xFF);
    sb16_write_dsp(hz & 0xFF);
    sb16.sample_rate = hz;
}

void sb16_play_buffer(uint8_t *data, uint32_t size) {
    sb16_write_dsp(0x14);
    uint32_t length = size - 1;
    sb16_write_dsp(length & 0xFF);
    sb16_write_dsp((length >> 8) & 0xFF);

    for (uint32_t i = 0; i < size; i++) {
        sb16_wait_ready();
        outb(DSP_WRITE, data[i]);
    }
}

void sb16_stop() {
    sb16_write_dsp(0xD0);
}

void sb16_init() {
    sb16.initialized = 0;
    if (sb16_reset() != 0) {
        return;
    }
    sb16_set_sample_rate(44100);
    sb16.initialized = 1;
}

void sb16_gen_flat_sound(uint8_t level) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        audio_buffer[i] = level;
    }
}

void sb16_test() {
    sb16_gen_flat_sound(200);
    sb16_play_buffer(audio_buffer, BUFFER_SIZE);
}