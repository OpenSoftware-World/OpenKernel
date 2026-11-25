#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <OpenKernel/SystemLib/Std/types.h>

/*
 * Framebuffer driver
 *
 * A pixel-level display driver with two backends:
 *   1. A linear framebuffer handed over by a bootloader (multiboot1,
 *      multiboot2 or a raw fb_info_t description).
 *   2. VGA mode 13h (320x200x256) programmed through VGA registers alone,
 *      so the driver works even when the boot path provides no framebuffer.
 *
 * Colour values passed to the drawing functions are "native" pixels for the
 * active mode. Use framebuffer_rgb() to build them from 8-bit R/G/B, or
 * FB_COLOR_TRANSPARENT to leave a pixel untouched (text overlays).
 *
 * Rules:
 *   - Call one of the framebuffer_init*() functions first; every other
 *     function is a no-op until the driver is ready.
 *   - The driver allocates nothing: it writes straight into video memory.
 */

/* Pixel layout of the active mode (multiboot framebuffer_type values). */
#define FB_TYPE_INDEXED 0
#define FB_TYPE_RGB     1

/*
 * Sentinel colour. A pixel drawn with this value is skipped, so a glyph
 * background (or any shape) can be see-through. Native colours produced by
 * framebuffer_rgb() never have bit 24 set, so the sentinel cannot collide
 * with a real colour in any supported mode.
 */
#define FB_COLOR_TRANSPARENT 0x01000000u

/* Boot magic values recognised by framebuffer_init_from_boot(). */
#define FB_MULTIBOOT1_MAGIC 0x2BADB002u
#define FB_MULTIBOOT2_MAGIC 0x36D76289u

/*
 * Description of a linear framebuffer. Leave rsize/gsize/bsize at 0 for
 * RGB modes and the driver assumes the standard x86 layouts (32/24 bpp =
 * XRGB8888, 16 bpp = RGB565, 15 bpp = RGB555).
 */
typedef struct {
    uint32_t addr;                  /* physical address of the pixel memory */
    uint32_t width;                 /* pixels per row, max 8192 */
    uint32_t height;                /* rows, max 8192 */
    uint32_t pitch;                 /* bytes per row, >= width * bpp / 8 */
    uint8_t  bpp;                   /* 8, 15, 16, 24 or 32 bits per pixel */
    uint8_t  type;                  /* FB_TYPE_INDEXED or FB_TYPE_RGB */
    uint8_t  rpos, gpos, bpos;      /* channel bit positions (RGB modes) */
    uint8_t  rsize, gsize, bsize;   /* channel widths in bits (RGB modes) */
} fb_info_t;

/* --- Lifecycle ---------------------------------------------------------- */

/* Attach to a framebuffer described by `info` and clear the screen. */
bool_t framebuffer_init(const fb_info_t *info);

/* Attach using a bootloader-provided multiboot information structure.
   Return false when the structure carries no usable graphics framebuffer
   (no flag, text mode, address above 4 GiB or invalid geometry). */
bool_t framebuffer_init_from_multiboot1(const void *mb_info);
bool_t framebuffer_init_from_multiboot2(const void *mb_info);

/* Convenience dispatcher: pass the boot magic value (EAX at entry) and the
   information pointer (EBX at entry) and the right parser runs. */
bool_t framebuffer_init_from_boot(uint32_t magic, const void *boot_info);

/* Fallback backend: program VGA mode 13h (320x200x256 at 0xA0000) with
   port I/O only. Needs no bootloader cooperation. */
bool_t framebuffer_init_vga(void);

/* Stop using the framebuffer. When this driver programmed the VGA mode it
   also restores the BIOS 80x25 text mode, so the VGA text driver can take
   over again. Font and text scale are left as they are. */
void framebuffer_close(void);

bool_t framebuffer_is_ready(void);
fb_info_t framebuffer_get_info(void);

/* --- Queries ------------------------------------------------------------ */

uint32_t framebuffer_get_width(void);
uint32_t framebuffer_get_height(void);
uint32_t framebuffer_get_pitch(void);
uint8_t  framebuffer_get_bpp(void);
void    *framebuffer_get_buffer(void);

/* --- Colour ------------------------------------------------------------- */

/* Convert 8-bit R/G/B to a native pixel for the active mode.
   Indexed modes return a 3-3-2 palette index. */
uint32_t framebuffer_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Reprogram the VGA DAC. Meaningful for indexed modes only; no-op on RGB
   modes. `rgb` points at 3-byte R/G/B triplets, `first` is the starting
   entry and `count` entries are written (clamped to 256). */
void framebuffer_set_palette_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void framebuffer_set_palette(const uint8_t *rgb, uint8_t first, uint16_t count);

/* --- Drawing ------------------------------------------------------------ */

void     framebuffer_clear(uint32_t color);
void     framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color);
uint32_t framebuffer_get_pixel(int32_t x, int32_t y);   /* 0 outside the screen */
void     framebuffer_fill_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void     framebuffer_draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
void     framebuffer_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
void     framebuffer_draw_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color);
void     framebuffer_fill_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color);

/* Copy a w*h pixel block from `src` (bytes per source row = `src_pitch`)
   to (x, y). The source is clipped against the screen. */
void framebuffer_blit(int32_t x, int32_t y, uint32_t w, uint32_t h,
                      const void *src, uint32_t src_pitch);

/* Move the image up by `rows` pixel rows and fill the exposed band at the
   bottom with `fill_color`. */
void framebuffer_scroll_up(uint32_t rows, uint32_t fill_color);

/* Wait for the vertical retrace. Only the VGA backend can do this; on an
   external framebuffer it returns immediately. Bounded, never hangs. */
void framebuffer_wait_vsync(void);

/* Draw a checkerboard plus probe pixels into the bottom-right 32x32 corner
   and verify them by reading them back. The corner keeps the pattern so a
   successful test is visible; framebuffer_clear() wipes it. */
bool_t framebuffer_selftest(void);

/* --- Text --------------------------------------------------------------- */

/* Built-in font: 128 glyphs x 8 rows, row bit 7 = leftmost pixel
   (the classic VGA bit order, so BIOS font dumps can be used as-is). */
extern const uint8_t framebuffer_font_8x8[128][8];

/* Select a custom font with the same layout as framebuffer_font_8x8.
   NULL restores the built-in font. */
void        framebuffer_set_font(const uint8_t *font);
const uint8_t *framebuffer_get_font(void);

/* Glyph magnification for draw_char/draw_string, 1..8 (default 1). */
void     framebuffer_set_text_scale(uint32_t scale);
uint32_t framebuffer_get_text_scale(void);

/* Draw one glyph / a string. '\n' starts a new line below the start
   position; '\r' is ignored. Background FB_COLOR_TRANSPARENT = no box. */
void framebuffer_draw_char(int32_t x, int32_t y, char c, uint32_t fg, uint32_t bg);
void framebuffer_draw_string(int32_t x, int32_t y, const char *str, uint32_t fg, uint32_t bg);

/* Width in pixels of the widest line of `str` (for centring). */
uint32_t framebuffer_text_width(const char *str);

/* --- VGA backend internals (framebuffer_vga.c) -------------------------- */

#define FB_VGA_LFB_ADDR 0xA0000u   /* VGA aperture: mode 13h pixel memory */

bool_t fb_vga_is_active(void);      /* true while this driver owns the video mode */
void   fb_vga_set_active(bool_t active); /* transfer mode ownership */
void   fb_vga_wait_retrace(void);   /* poll the vertical retrace flag (port 0x3DA) */
void   fb_vga_restore_text(void);   /* restore BIOS 80x25 text mode (mode 3) */
void   fb_vga_program_palette(void);/* 3-3-2 palette matching framebuffer_rgb() */
void   fb_vga_dac_write(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

#define FRAMEBUFFER_DRIVER_NAME "OpenKernel Framebuffer Driver"
#define FRAMEBUFFER_DRIVER_VER "1.0"
#define FRAMEBUFFER_DRIVER_DESC "Linear framebuffer and VGA graphics mode driver for OpenKernel"
#define FRAMEBUFFER_DRIVER_AUTHOR "OpenSoftware-World"
#define FRAMEBUFFER_DRIVER_KRNL_VER "5.0"

#endif
