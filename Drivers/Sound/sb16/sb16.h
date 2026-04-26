#ifndef SB16_H
#define SB16_H

#include <OpenKernel/SystemLib/Std/types.h>

#define SB16_BASE 0x220

#define DSP_RESET (SB16_BASE + 0x6)
#define DSP_READ (SB16_BASE + 0xA)
#define DSP_WRITE (SB16_BASE + 0xC)
#define DSP_STATUS (SB16_BASE + 0xE)

typedef struct {
    uint32_t sample_rate;
    uint8_t initialized;
} sb16_t;

extern uint8_t audio_buffer[8000];

void sb16_init();
int sb16_reset();
void sb16_set_sample_rate(uint32_t hz);
void sb16_play_buffer(uint8_t *data, uint32_t size);
void sb16_stop();

void sb16_write_dsp(uint8_t value);
uint8_t sb16_read_dsp();
int sb16_wait_ready();
void sb16_gen_flat_sound(uint8_t level);
void sb16_test();

#endif