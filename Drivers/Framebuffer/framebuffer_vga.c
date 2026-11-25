/* OpenKernel Framebuffer Driver: VGA backend.
 *
 * Programs graphics mode 13h (320x200, 256 colours, chain-4 at 0xA0000)
 * and restores the BIOS 80x25 text mode, using only VGA register ports.
 * No BIOS call is made, so it is safe from protected mode. */

#include "framebuffer.h"
#include <OpenKernel/SystemLib/SystemIO/io.h>

/* VGA register ports. */
#define VGA_MISC_WRITE   0x3C2
#define VGA_SEQ_INDEX    0x3C4
#define VGA_SEQ_DATA     0x3C5
#define VGA_CRTC_INDEX   0x3D4
#define VGA_CRTC_DATA    0x3D5
#define VGA_GC_INDEX     0x3CE
#define VGA_GC_DATA      0x3CF
#define VGA_AC_INDEX     0x3C0
#define VGA_AC_DATA      0x3C1
#define VGA_STAT1_READ   0x3DA
#define VGA_DAC_WRITE_IDX 0x3C8
#define VGA_DAC_DATA     0x3C9

#define VGA_LFB_ADDR     FB_VGA_LFB_ADDR
#define VGA_MODE13_WIDTH  320u
#define VGA_MODE13_HEIGHT 200u
#define VGA_MODE13_PITCH  320u

/* True while this driver owns the video mode and must restore it. */
static bool_t vga_mode_active = false;

/* Mode 13h stores pixels across all four display planes, so the text-mode
   character generator living in plane 2 is destroyed on the way in. This
   buffer keeps a copy of that plane so fb_vga_restore_text() can put the
   font back; vga_font_valid records whether a text-mode font was captured. */
#define VGA_FONT_BYTES 8192u
static uint8_t vga_font[VGA_FONT_BYTES];
static bool_t  vga_font_valid = false;

/* Register dumps for mode 13h (index 0..N of each register block). */
static const uint8_t mode13_misc = 0x63;
static const uint8_t mode13_seq[5]  = { 0x03, 0x01, 0x0F, 0x00, 0x0E };
static const uint8_t mode13_crtc[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
    0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
    0xFF
};
static const uint8_t mode13_gc[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05 };
static const uint8_t mode13_ac[21] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A,
    0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x41, 0x00, 0x0F, 0x00, 0x00
};

/* Register dumps for the BIOS 80x25 colour text mode (mode 3). */
static const uint8_t mode3_misc = 0x67;
static const uint8_t mode3_seq[5]  = { 0x03, 0x00, 0x03, 0x00, 0x02 };
static const uint8_t mode3_crtc[25] = {
    0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
    0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x00,
    0x9C, 0x8E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
    0xFF
};
static const uint8_t mode3_gc[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E };
static const uint8_t mode3_ac[21] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3A,
    0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x0C, 0x00, 0x0F, 0x08, 0x00
};

/*
 * The default 64-entry VGA text-mode palette in 6-bit DAC values, read
 * back from SeaBIOS's own mode 3 state (QEMU, probe-verified): entries
 * 0-7 are the dark colours, entry 20 holds brown because attribute
 * register 6 points at it (0x14), and entries 56-63 are the bright
 * colours that attribute registers 8-15 (0x38-0x3F) point at. Entries
 * 64-255 are left alone: the attribute palette select is 6 bits wide, so
 * text mode can never reach them.
 */
static const uint8_t text_dac[64][3] = {
    { 0x00, 0x00, 0x00 }, { 0x00, 0x00, 0x2A }, { 0x00, 0x2A, 0x00 }, { 0x00, 0x2A, 0x2A },
    { 0x2A, 0x00, 0x00 }, { 0x2A, 0x00, 0x2A }, { 0x2A, 0x2A, 0x00 }, { 0x2A, 0x2A, 0x2A },
    { 0x00, 0x00, 0x15 }, { 0x00, 0x00, 0x3F }, { 0x00, 0x2A, 0x15 }, { 0x00, 0x2A, 0x3F },
    { 0x2A, 0x00, 0x15 }, { 0x2A, 0x00, 0x3F }, { 0x2A, 0x2A, 0x15 }, { 0x2A, 0x2A, 0x3F },
    { 0x00, 0x15, 0x00 }, { 0x00, 0x15, 0x2A }, { 0x00, 0x3F, 0x00 }, { 0x00, 0x3F, 0x2A },
    { 0x2A, 0x15, 0x00 }, { 0x2A, 0x15, 0x2A }, { 0x2A, 0x3F, 0x00 }, { 0x2A, 0x3F, 0x2A },
    { 0x00, 0x15, 0x15 }, { 0x00, 0x15, 0x3F }, { 0x00, 0x3F, 0x15 }, { 0x00, 0x3F, 0x3F },
    { 0x2A, 0x15, 0x15 }, { 0x2A, 0x15, 0x3F }, { 0x2A, 0x3F, 0x15 }, { 0x2A, 0x3F, 0x3F },
    { 0x15, 0x00, 0x00 }, { 0x15, 0x00, 0x2A }, { 0x15, 0x2A, 0x00 }, { 0x15, 0x2A, 0x2A },
    { 0x3F, 0x00, 0x00 }, { 0x3F, 0x00, 0x2A }, { 0x3F, 0x2A, 0x00 }, { 0x3F, 0x2A, 0x2A },
    { 0x15, 0x00, 0x15 }, { 0x15, 0x00, 0x3F }, { 0x15, 0x2A, 0x15 }, { 0x15, 0x2A, 0x3F },
    { 0x3F, 0x00, 0x15 }, { 0x3F, 0x00, 0x3F }, { 0x3F, 0x2A, 0x15 }, { 0x3F, 0x2A, 0x3F },
    { 0x15, 0x15, 0x00 }, { 0x15, 0x15, 0x2A }, { 0x15, 0x3F, 0x00 }, { 0x15, 0x3F, 0x2A },
    { 0x3F, 0x15, 0x00 }, { 0x3F, 0x15, 0x2A }, { 0x3F, 0x3F, 0x00 }, { 0x3F, 0x3F, 0x2A },
    { 0x15, 0x15, 0x15 }, { 0x15, 0x15, 0x3F }, { 0x15, 0x3F, 0x15 }, { 0x15, 0x3F, 0x3F },
    { 0x3F, 0x15, 0x15 }, { 0x3F, 0x15, 0x3F }, { 0x3F, 0x3F, 0x15 }, { 0x3F, 0x3F, 0x3F }
};

static void vga_write_seq(uint8_t index, uint8_t value) {
    outb(VGA_SEQ_INDEX, index);
    outb(VGA_SEQ_DATA, value);
}

static void vga_write_crtc(uint8_t index, uint8_t value) {
    outb(VGA_CRTC_INDEX, index);
    outb(VGA_CRTC_DATA, value);
}

static void vga_write_gc(uint8_t index, uint8_t value) {
    outb(VGA_GC_INDEX, index);
    outb(VGA_GC_DATA, value);
}

static void vga_write_ac(uint8_t index, uint8_t value) {
    inb(VGA_STAT1_READ);        /* reset the attribute-controller flip-flop */
    outb(VGA_AC_INDEX, index);
    outb(VGA_AC_DATA, value);
}

/* The character generator is reached through display memory plane 2 at
   0xA0000, which is not directly addressable while text mode is active.
   These helpers remap that window onto plane 2 and put every register they
   touch back afterwards, so a save/restore round-trip leaves the mode
   exactly as they found it. QEMU probe-verified: reading and writing all
   8192 bytes round-trips with zero differences. */
typedef struct {
    uint8_t seq2, seq4, gc4, gc5, gc6;
} vga_plane_regs_t;

static void vga_plane2_enter(vga_plane_regs_t *saved) {
    outb(VGA_SEQ_INDEX, 0x02); saved->seq2 = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 0x04); saved->seq4 = inb(VGA_SEQ_DATA);
    outb(VGA_GC_INDEX, 0x04);  saved->gc4  = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 0x05);  saved->gc5  = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 0x06);  saved->gc6  = inb(VGA_GC_DATA);

    vga_write_seq(0x02, 0x04);   /* map mask: enable plane 2 only */
    vga_write_seq(0x04, 0x06);   /* extended memory, odd/even and chain-4 off */
    vga_write_gc(0x04, 0x02);    /* read map select: plane 2 */
    vga_write_gc(0x05, 0x00);    /* read mode 0, write mode 0 */
    vga_write_gc(0x06, 0x01);    /* graphics, memory at 0xA0000 */
}

static void vga_plane2_leave(const vga_plane_regs_t *saved) {
    vga_write_seq(0x02, saved->seq2);
    vga_write_seq(0x04, saved->seq4);
    vga_write_gc(0x04, saved->gc4);
    vga_write_gc(0x05, saved->gc5);
    vga_write_gc(0x06, saved->gc6);
}

/* Copy the character generator out of plane 2. Refuses unless the VGA is in
   text mode: in a graphics mode plane 2 holds pixels, and there would be no
   font there worth saving. */
static bool_t vga_font_capture(void) {
    vga_plane_regs_t regs;
    volatile uint8_t *plane = (volatile uint8_t *)VGA_LFB_ADDR;
    uint32_t i;

    outb(VGA_GC_INDEX, 0x06);
    if ((inb(VGA_GC_DATA) & 0x01u) != 0u) return false;

    vga_plane2_enter(&regs);
    for (i = 0; i < VGA_FONT_BYTES; i++) vga_font[i] = plane[i];
    vga_plane2_leave(&regs);
    return true;
}

/* Put the captured character generator back into plane 2. No-op until a
   text-mode font has actually been captured. */
static void vga_font_restore(void) {
    vga_plane_regs_t regs;
    volatile uint8_t *plane = (volatile uint8_t *)VGA_LFB_ADDR;
    uint32_t i;

    if (!vga_font_valid) return;

    vga_plane2_enter(&regs);
    for (i = 0; i < VGA_FONT_BYTES; i++) plane[i] = vga_font[i];
    vga_plane2_leave(&regs);
}

/* Program a complete mode from the register dumps above.
   `keep_crtc_unlocked` leaves CRTC write-protect clear so the VGA text
   driver can still move the hardware cursor after a text-mode restore. */
static void vga_program_mode(uint8_t misc,
                             const uint8_t *seq, const uint8_t *crtc,
                             const uint8_t *gc, const uint8_t *ac,
                             bool_t keep_crtc_unlocked) {
    uint32_t i;

    vga_write_seq(0x00, 0x01);              /* sync reset while reprogramming */
    outb(VGA_MISC_WRITE, misc);

    for (i = 1; i < 5; i++) vga_write_seq((uint8_t)i, seq[i]);

    /* Unlock CRTC registers 0x00-0x18 (bit 7 of register 0x11). */
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, (uint8_t)(inb(VGA_CRTC_DATA) & 0x7F));

    for (i = 0; i < 25; i++) {
        uint8_t value = crtc[i];
        if (i == 0x11u && keep_crtc_unlocked) value &= 0x7Fu;
        vga_write_crtc((uint8_t)i, value);
    }

    for (i = 0; i < 7; i++) vga_write_gc((uint8_t)i, gc[i]);
    for (i = 0; i < 21; i++) vga_write_ac((uint8_t)i, ac[i]);

    vga_write_seq(0x00, 0x03);              /* release the reset */
    inb(VGA_STAT1_READ);                    /* flip-flop back to index */
    outb(VGA_AC_INDEX, 0x20);               /* bit 5: re-enable video */
}

/* --- Public backend entry points ---------------------------------------- */

bool_t framebuffer_init_vga(void) {
    fb_info_t info;

    /* Capture the text font before mode 13h reuses plane 2 for pixels. */
    if (!vga_font_valid) vga_font_valid = vga_font_capture();

    vga_program_mode(mode13_misc, mode13_seq, mode13_crtc,
                     mode13_gc, mode13_ac, false);
    vga_mode_active = true;

    info.addr = VGA_LFB_ADDR;
    info.width = VGA_MODE13_WIDTH;
    info.height = VGA_MODE13_HEIGHT;
    info.pitch = VGA_MODE13_PITCH;
    info.bpp = 8;
    info.type = FB_TYPE_INDEXED;
    info.rpos = info.gpos = info.bpos = 0;
    info.rsize = info.gsize = info.bsize = 0;

    if (!framebuffer_init(&info)) {
        vga_mode_active = false;
        return false;
    }
    return true;
}

bool_t fb_vga_is_active(void) {
    return vga_mode_active;
}

void fb_vga_set_active(bool_t active) {
    vga_mode_active = active;
}

void fb_vga_program_palette(void) {
    uint32_t i;

    outb(VGA_DAC_WRITE_IDX, 0);
    for (i = 0; i < 256u; i++) {
        /* 3-3-2: r = bits 7-5, g = bits 4-2, b = bits 1-0, each expanded
           to the 6-bit DAC (levels 0..63 land exactly on 0..255 steps). */
        uint8_t r6 = (uint8_t)(((i >> 5) & 7u) * 9u);
        uint8_t g6 = (uint8_t)(((i >> 2) & 7u) * 9u);
        uint8_t b6 = (uint8_t)((i & 3u) * 21u);

        outb(VGA_DAC_DATA, r6);
        outb(VGA_DAC_DATA, g6);
        outb(VGA_DAC_DATA, b6);
    }
}

void fb_vga_dac_write(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_IDX, index);
    outb(VGA_DAC_DATA, (uint8_t)(r >> 2));   /* 8-bit in, 6-bit DAC out */
    outb(VGA_DAC_DATA, (uint8_t)(g >> 2));
    outb(VGA_DAC_DATA, (uint8_t)(b >> 2));
}

void fb_vga_wait_retrace(void) {
    uint32_t polls;

    /* Wait for the current retrace to end, then for the next one to start.
       Bounded so a machine without a VGA cannot spin here forever. */
    polls = 0;
    while ((inb(VGA_STAT1_READ) & 0x08u) != 0) {
        if (++polls > 0x400000u) return;
    }
    polls = 0;
    while ((inb(VGA_STAT1_READ) & 0x08u) == 0) {
        if (++polls > 0x400000u) return;
    }
}

void fb_vga_restore_text(void) {
    uint32_t i;

    if (!vga_mode_active) return;

    vga_program_mode(mode3_misc, mode3_seq, mode3_crtc,
                     mode3_gc, mode3_ac, true);

    /* Mode 13h overwrote the character generator; put the captured font back
       now that text mode has mapped plane 2 for the display again. */
    vga_font_restore();

    /* The text driver picks colours up through the DAC, so put the default
       text palette back; framebuffer_close() left the 3-3-2 graphics one. */
    outb(VGA_DAC_WRITE_IDX, 0);
    for (i = 0; i < 64u; i++) {
        outb(VGA_DAC_DATA, text_dac[i][0]);
        outb(VGA_DAC_DATA, text_dac[i][1]);
        outb(VGA_DAC_DATA, text_dac[i][2]);
    }

    vga_mode_active = false;
}
