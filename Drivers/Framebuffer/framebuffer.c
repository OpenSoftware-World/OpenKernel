/* OpenKernel Framebuffer Driver: mode attachment and the pixel engine. */

#include "framebuffer.h"
#include <OpenKernel/SystemLib/Memory/mem.h>

/* Sanity limits: rejected before any of them is used for pointer maths. */
#define FB_MAX_WIDTH  8192u
#define FB_MAX_HEIGHT 8192u
#define FB_MAX_PITCH  65535u
#define FB_MAX_RADIUS 65536u

#define FB_MB_FRAMEBUFFER_FLAG (1u << 12)   /* multiboot1: framebuffer valid */
#define FB_MB_TYPE_TEXT        2u           /* multiboot framebuffer_type: text */

static fb_info_t fb_mode;
static volatile uint8_t *fb_mem = (volatile uint8_t *)0;
static bool_t    fb_ready = false;
static uint8_t   fb_px_bytes;               /* storage bytes per pixel */
static uint32_t  fb_row_bytes;              /* width * fb_px_bytes */
static uint8_t   fb_rpos, fb_gpos, fb_bpos;
static uint8_t   fb_rsize, fb_gsize, fb_bsize;

/* Multiboot1 information structure, read only up to the colour block.
   Packed so the u64 framebuffer address sits at offset 88 and the colour
   block at 110-115, exactly as the specification lays them out. */
typedef struct PACKED {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t  framebuffer_bpp;
    uint8_t  framebuffer_type;
    /* The colour block at 110 means two different things: an external
       palette for indexed modes, a channel layout for RGB modes. */
    union PACKED {
        struct PACKED {
            uint32_t palette_addr;      /* physical address of descriptors */
            uint16_t num_colors;
        } indexed;
        struct PACKED {
            uint8_t rpos;
            uint8_t rsize;
            uint8_t gpos;
            uint8_t gsize;
            uint8_t bpos;
            uint8_t bsize;
        } rgb;
    } color;
} mb1_info_t;

/* Multiboot2 tag header and the framebuffer tag (type 8). */
typedef struct PACKED {
    uint32_t type;
    uint32_t size;
} mb2_tag_t;

typedef struct PACKED {
    uint32_t type;
    uint32_t size;
    uint64_t addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
    uint8_t  bpp;
    uint8_t  fb_type;
    uint8_t  reserved[6];
} mb2_fb_tag_t;

/* __builtin_offsetof (unlike the project OFFSET_OF cast form) is an integer
   constant expression, which file-scope assertions require. */
STATIC_ASSERT(__builtin_offsetof(mb1_info_t, framebuffer_addr) == 88, mb1_fb_addr_offset);
STATIC_ASSERT(__builtin_offsetof(mb1_info_t, framebuffer_bpp) == 108, mb1_fb_bpp_offset);
STATIC_ASSERT(__builtin_offsetof(mb1_info_t, color) == 110, mb1_color_offset);
STATIC_ASSERT(sizeof(((mb1_info_t *)0)->color) == 6, mb1_color_size);
STATIC_ASSERT(__builtin_offsetof(mb2_fb_tag_t, addr) == 8, mb2_fb_addr_offset);

/* Storage bytes for a pixel of the given depth (15 bpp still takes 2). */
static uint8_t fb_bytes_for_bpp(uint8_t bpp) {
    return (uint8_t)((bpp + 7u) / 8u);
}

/* Fill in the standard x86 colour layout when the caller did not give one. */
static void fb_layout_defaults(fb_info_t *info) {
    if (info->type != FB_TYPE_RGB || info->rsize != 0) return;

    switch (info->bpp) {
    case 32:
    case 24:
        info->rpos = 16; info->gpos = 8;  info->bpos = 0;
        info->rsize = 8; info->gsize = 8; info->bsize = 8;
        break;
    case 16:
        info->rpos = 11; info->gpos = 5;  info->bpos = 0;
        info->rsize = 5; info->gsize = 6; info->bsize = 5;
        break;
    case 15:
        info->rpos = 10; info->gpos = 5;  info->bpos = 0;
        info->rsize = 5; info->gsize = 5; info->bsize = 5;
        break;
    default:
        break;   /* 8 bpp RGB has no usable layout, rejected below */
    }
}

/* Reject a mode description that would break pointer arithmetic. */
static bool_t fb_validate(const fb_info_t *info) {
    uint32_t min_pitch;

    if (info->addr == 0) return false;
    if (info->width == 0 || info->width > FB_MAX_WIDTH) return false;
    if (info->height == 0 || info->height > FB_MAX_HEIGHT) return false;
    if (info->type != FB_TYPE_INDEXED && info->type != FB_TYPE_RGB) return false;
    if (info->bpp != 8 && info->bpp != 15 && info->bpp != 16 &&
        info->bpp != 24 && info->bpp != 32) return false;
    if (info->pitch > FB_MAX_PITCH) return false;

    min_pitch = info->width * fb_bytes_for_bpp(info->bpp);
    if (info->pitch < min_pitch) return false;

    if (info->type == FB_TYPE_RGB) {
        /* Every channel must fit inside the pixel. */
        if (info->rsize == 0 || info->gsize == 0 || info->bsize == 0) return false;
        if ((uint32_t)info->rpos + info->rsize > info->bpp) return false;
        if ((uint32_t)info->gpos + info->gsize > info->bpp) return false;
        if ((uint32_t)info->bpos + info->bsize > info->bpp) return false;
    }
    return true;
}

bool_t framebuffer_init(const fb_info_t *info) {
    fb_info_t local;

    if (info == NULL) return false;
    local = *info;
    fb_layout_defaults(&local);
    if (!fb_validate(&local)) return false;

    fb_mode = local;
    fb_mem = (volatile uint8_t *)(uintptr_t)local.addr;
    fb_px_bytes = fb_bytes_for_bpp(local.bpp);
    fb_row_bytes = local.width * fb_px_bytes;

    fb_rpos = local.rpos; fb_gpos = local.gpos; fb_bpos = local.bpos;
    fb_rsize = local.rsize; fb_gsize = local.gsize; fb_bsize = local.bsize;

    fb_ready = true;

    /* Attaching anything but the VGA aperture hands mode ownership away
       from the VGA backend, so close() will not restore text mode over a
       bootloader-provided framebuffer. */
    if (local.addr != FB_VGA_LFB_ADDR) fb_vga_set_active(false);

    /* Only a mode this driver programmed gets its palette rebuilt; a
       bootloader-supplied indexed mode keeps the palette it came with. */
    if (local.type == FB_TYPE_INDEXED && fb_vga_is_active()) {
        fb_vga_program_palette();
    }

    framebuffer_clear(framebuffer_rgb(0, 0, 0));
    return true;
}

bool_t framebuffer_init_from_multiboot1(const void *mb_info) {
    const mb1_info_t *mb = (const mb1_info_t *)mb_info;
    fb_info_t info;
    bool_t attached;

    if (mb == NULL) return false;
    if (!(mb->flags & FB_MB_FRAMEBUFFER_FLAG)) return false;
    if (mb->framebuffer_type == FB_MB_TYPE_TEXT) return false;
    if ((mb->framebuffer_addr >> 32) != 0) return false;   /* not addressable here */

    info.addr = (uint32_t)mb->framebuffer_addr;
    info.pitch = mb->framebuffer_pitch;
    info.width = mb->framebuffer_width;
    info.height = mb->framebuffer_height;
    info.bpp = mb->framebuffer_bpp;
    info.type = mb->framebuffer_type;

    /* Channel layout is only defined for RGB modes; zeroed RGB bytes fall
       back to the standard x86 layout inside framebuffer_init(). */
    info.rpos = info.gpos = info.bpos = 0;
    info.rsize = info.gsize = info.bsize = 0;
    if (info.type == FB_TYPE_RGB) {
        info.rpos = mb->color.rgb.rpos;
        info.rsize = mb->color.rgb.rsize;
        info.gpos = mb->color.rgb.gpos;
        info.gsize = mb->color.rgb.gsize;
        info.bpos = mb->color.rgb.bpos;
        info.bsize = mb->color.rgb.bsize;
    }

    attached = framebuffer_init(&info);
    if (!attached) return false;

    /* Indexed modes may come with an external palette; without it the
       colour indices the bootloader chose would mean nothing. */
    if (info.type == FB_TYPE_INDEXED) {
        uint32_t palette = (uint32_t)mb->color.indexed.palette_addr;
        uint32_t count = mb->color.indexed.num_colors;

        if (palette != 0 && count != 0) {
            const uint8_t *desc = (const uint8_t *)(uintptr_t)palette;
            uint32_t i;

            if (count > 256u) count = 256u;      /* the DAC has 256 entries */
            for (i = 0; i < count; i++) {
                framebuffer_set_palette_entry((uint8_t)i, desc[i * 3],
                                              desc[i * 3 + 1], desc[i * 3 + 2]);
            }
        }
    }
    return true;
}

bool_t framebuffer_init_from_multiboot2(const void *mb_info) {
    const uint8_t *base = (const uint8_t *)mb_info;
    const uint8_t *tag, *end;
    uint32_t total;

    if (base == NULL) return false;

    total = *(const uint32_t *)base;
    if (total < 16u) return false;

    tag = base + 8;                 /* skip total_size and reserved */
    end = base + total;

    while (tag + sizeof(mb2_tag_t) <= end) {
        const mb2_tag_t *hdr = (const mb2_tag_t *)tag;
        uint32_t next;

        if (hdr->size < sizeof(mb2_tag_t)) break;   /* corrupt */
        if (hdr->type == 0u) break;                 /* end tag */

        if (hdr->type == 8u && hdr->size >= 30u) {  /* framebuffer tag */
            const mb2_fb_tag_t *fb = (const mb2_fb_tag_t *)tag;
            fb_info_t info;

            if ((fb->addr >> 32) != 0) return false;
            if (fb->fb_type == FB_MB_TYPE_TEXT) return false;

            info.addr = (uint32_t)fb->addr;
            info.pitch = fb->pitch;
            info.width = fb->width;
            info.height = fb->height;
            info.bpp = fb->bpp;
            info.type = fb->fb_type;
            info.rpos = info.gpos = info.bpos = 0;
            info.rsize = info.gsize = info.bsize = 0;   /* GOP has none: use defaults */

            return framebuffer_init(&info);
        }

        next = (hdr->size + 7u) & ~7u;               /* tags are 8-byte aligned */
        if (next < sizeof(mb2_tag_t)) break;
        tag += next;
    }
    return false;
}

bool_t framebuffer_init_from_boot(uint32_t magic, const void *boot_info) {
    if (magic == FB_MULTIBOOT1_MAGIC) return framebuffer_init_from_multiboot1(boot_info);
    if (magic == FB_MULTIBOOT2_MAGIC) return framebuffer_init_from_multiboot2(boot_info);
    return false;
}

void framebuffer_close(void) {
    if (!fb_ready) return;

    if (fb_vga_is_active()) fb_vga_restore_text();

    fb_ready = false;
    fb_mem = (volatile uint8_t *)0;
    fb_px_bytes = 0;
    fb_row_bytes = 0;
    memset(&fb_mode, 0, sizeof(fb_mode));
}

bool_t framebuffer_is_ready(void) {
    return fb_ready;
}

fb_info_t framebuffer_get_info(void) {
    return fb_mode;
}

uint32_t framebuffer_get_width(void)  { return fb_mode.width; }
uint32_t framebuffer_get_height(void) { return fb_mode.height; }
uint32_t framebuffer_get_pitch(void)  { return fb_mode.pitch; }
uint8_t  framebuffer_get_bpp(void)    { return fb_mode.bpp; }

void *framebuffer_get_buffer(void) {
    return (void *)(uintptr_t)fb_mode.addr;
}

/* --- Colour ------------------------------------------------------------- */

uint32_t framebuffer_rgb(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t rv, gv, bv;

    if (!fb_ready) return 0;

    if (fb_mode.type == FB_TYPE_INDEXED) {
        /* 3-3-2: red in bits 7-5, green in 4-2, blue in 1-0. */
        return ((uint32_t)(r >> 5) << 5) | ((uint32_t)(g >> 5) << 2) | (uint32_t)(b >> 6);
    }

    rv = ((uint32_t)r * (((uint32_t)1u << fb_rsize) - 1u) + 127u) / 255u;
    gv = ((uint32_t)g * (((uint32_t)1u << fb_gsize) - 1u) + 127u) / 255u;
    bv = ((uint32_t)b * (((uint32_t)1u << fb_bsize) - 1u) + 127u) / 255u;

    return (rv << fb_rpos) | (gv << fb_gpos) | (bv << fb_bpos);
}

void framebuffer_set_palette_entry(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    if (!fb_ready || fb_mode.type != FB_TYPE_INDEXED) return;
    fb_vga_dac_write(index, r, g, b);
}

void framebuffer_set_palette(const uint8_t *rgb, uint8_t first, uint16_t count) {
    uint32_t i;

    if (!fb_ready || rgb == NULL || fb_mode.type != FB_TYPE_INDEXED) return;
    if ((uint32_t)first + count > 256u) {
        count = (uint16_t)(256u - first);   /* clamp, keep the valid prefix */
    }

    for (i = 0; i < count; i++) {
        fb_vga_dac_write((uint8_t)(first + i), rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }
}

/* --- Pixel access ------------------------------------------------------- */

/* One native pixel written to `p`; byte stores keep the code free of
   alignment and strict-aliasing assumptions (pixels are little-endian). */
static void fb_store(volatile uint8_t *p, uint32_t color) {
    switch (fb_mode.bpp) {
    case 8:
        p[0] = (uint8_t)color;
        break;
    case 15:
    case 16:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        break;
    case 24:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        break;
    default:   /* 32 */
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        p[3] = (uint8_t)((color >> 24) & 0xFFu);
        break;
    }
}

static uint32_t fb_load(const volatile uint8_t *p) {
    switch (fb_mode.bpp) {
    case 8:
        return p[0];
    case 15:
    case 16:
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    case 24:
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
    default:   /* 32 */
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
}

void framebuffer_put_pixel(int32_t x, int32_t y, uint32_t color) {
    if (!fb_ready) return;
    if (x < 0 || y < 0) return;
    if ((uint32_t)x >= fb_mode.width || (uint32_t)y >= fb_mode.height) return;

    fb_store(fb_mem + (uint32_t)y * fb_mode.pitch + (uint32_t)x * fb_px_bytes, color);
}

uint32_t framebuffer_get_pixel(int32_t x, int32_t y) {
    if (!fb_ready) return 0;
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= fb_mode.width || (uint32_t)y >= fb_mode.height) return 0;

    return fb_load(fb_mem + (uint32_t)y * fb_mode.pitch + (uint32_t)x * fb_px_bytes);
}

/* Fill [x0, x1) x [y0, y1) of an already-clipped rectangle. */
static void fb_fill_clipped(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1, uint32_t color) {
    uint8_t bytes[4];
    uint32_t y, x, b;
    uint32_t span = (x1 - x0) * fb_px_bytes;

    for (b = 0; b < fb_px_bytes; b++) {
        bytes[b] = (uint8_t)((color >> (b * 8)) & 0xFFu);
    }

    for (y = y0; y < y1; y++) {
        volatile uint8_t *row = fb_mem + y * fb_mode.pitch + x0 * fb_px_bytes;
        for (x = 0; x < span; x += fb_px_bytes) {
            for (b = 0; b < fb_px_bytes; b++) {
                row[x + b] = bytes[b];
            }
        }
    }
}

void framebuffer_fill_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    int64_t rx, ry;
    int64_t x0, y0, x1, y1;

    if (!fb_ready || w == 0 || h == 0) return;

    /* int64 edges: x + w cannot overflow and clipping stays exact even for
       rectangles that start far outside the screen. */
    x0 = x; y0 = y;
    rx = (int64_t)x + (int64_t)w;
    ry = (int64_t)y + (int64_t)h;

    if (rx <= 0 || ry <= 0) return;
    if (x0 >= (int64_t)fb_mode.width || y0 >= (int64_t)fb_mode.height) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    x1 = rx > (int64_t)fb_mode.width ? (int64_t)fb_mode.width : rx;
    y1 = ry > (int64_t)fb_mode.height ? (int64_t)fb_mode.height : ry;
    if (x1 <= x0 || y1 <= y0) return;

    fb_fill_clipped((uint32_t)x0, (uint32_t)y0, (uint32_t)x1, (uint32_t)y1, color);
}

void framebuffer_clear(uint32_t color) {
    if (!fb_ready) return;
    fb_fill_clipped(0, 0, fb_mode.width, fb_mode.height, color);
}

void framebuffer_draw_rect(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color) {
    int32_t w32, h32;

    if (!fb_ready || w == 0 || h == 0) return;

    /* Bounding-box early-out; after it, x/y sit near the screen so the
       edge coordinates below stay inside the int32 range. */
    if ((int64_t)x >= (int64_t)fb_mode.width || (int64_t)y >= (int64_t)fb_mode.height) return;
    if ((int64_t)x + (int64_t)w <= 0 || (int64_t)y + (int64_t)h <= 0) return;

    if (w > FB_MAX_WIDTH) w = FB_MAX_WIDTH;
    if (h > FB_MAX_HEIGHT) h = FB_MAX_HEIGHT;
    w32 = (int32_t)w;
    h32 = (int32_t)h;

    framebuffer_fill_rect(x, y, w, 1, color);                                  /* top */
    framebuffer_fill_rect(x, y + h32 - 1, w, 1, color);                        /* bottom */
    framebuffer_fill_rect(x, y, 1, h, color);                                  /* left */
    framebuffer_fill_rect(x + w32 - 1, y, 1, h, color);                        /* right */
}

void framebuffer_draw_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    int64_t x = x0, y = y0;
    int64_t dx = (int64_t)x1 - (int64_t)x0;
    int64_t dy = (int64_t)y1 - (int64_t)y0;
    int64_t sx = dx < 0 ? -1 : 1;
    int64_t sy = dy < 0 ? -1 : 1;
    int64_t err;
    uint64_t guard = 0;
    uint64_t guard_limit;

    if (!fb_ready) return;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    err = dx - dy;

    /* Work bound: a line can never need more steps than the screen plus the
       distance of its off-screen ends; this only stops absurd coordinates
       from spinning forever. */
    guard_limit = (uint64_t)(dx > dy ? dx : dy) + 1u;
    if (guard_limit > 0x1000000u) guard_limit = 0x1000000u;

    for (;;) {
        framebuffer_put_pixel((int32_t)x, (int32_t)y, color);
        if (x == (int64_t)x1 && y == (int64_t)y1) break;
        if (++guard > guard_limit) break;

        if (2 * err > -dy) { err -= dy; x += sx; }
        if (2 * err < dx)  { err += dx; y += sy; }
    }
}

/* Midpoint circle: `fill` selects outline or disc. */
static void fb_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color, bool_t fill) {
    int64_t r = radius;
    int64_t x, y, d;

    if (!fb_ready || radius == 0) return;

    /* Radius cap: with it the bounding-box test below guarantees every
       cx/cy +/- r used further down stays inside the int32 range. */
    if (radius > FB_MAX_RADIUS) return;

    /* Bounding box must reach the screen, otherwise nothing would show. */
    if ((int64_t)cx + r <= 0 || (int64_t)cy + r <= 0) return;
    if ((int64_t)cx - r >= (int64_t)fb_mode.width) return;
    if ((int64_t)cy - r >= (int64_t)fb_mode.height) return;

    x = 0;
    y = r;
    d = 1 - r;

    while (x <= y) {
        if (fill) {
            int32_t row = (int32_t)(cy + y);
            int32_t row2 = (int32_t)(cy - y);
            int32_t row3 = (int32_t)(cy + x);
            int32_t row4 = (int32_t)(cy - x);
            uint32_t half = (uint32_t)(x * 2 + 1);
            uint32_t half2 = (uint32_t)(y * 2 + 1);

            framebuffer_fill_rect((int32_t)(cx - x), row, half, 1, color);
            framebuffer_fill_rect((int32_t)(cx - x), row2, half, 1, color);
            framebuffer_fill_rect((int32_t)(cx - y), row3, half2, 1, color);
            framebuffer_fill_rect((int32_t)(cx - y), row4, half2, 1, color);
        } else {
            framebuffer_put_pixel((int32_t)(cx + x), (int32_t)(cy + y), color);
            framebuffer_put_pixel((int32_t)(cx - x), (int32_t)(cy + y), color);
            framebuffer_put_pixel((int32_t)(cx + x), (int32_t)(cy - y), color);
            framebuffer_put_pixel((int32_t)(cx - x), (int32_t)(cy - y), color);
            framebuffer_put_pixel((int32_t)(cx + y), (int32_t)(cy + x), color);
            framebuffer_put_pixel((int32_t)(cx - y), (int32_t)(cy + x), color);
            framebuffer_put_pixel((int32_t)(cx + y), (int32_t)(cy - x), color);
            framebuffer_put_pixel((int32_t)(cx - y), (int32_t)(cy - x), color);
        }

        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

void framebuffer_draw_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color) {
    fb_circle(cx, cy, radius, color, false);
}

void framebuffer_fill_circle(int32_t cx, int32_t cy, uint32_t radius, uint32_t color) {
    fb_circle(cx, cy, radius, color, true);
}

void framebuffer_blit(int32_t x, int32_t y, uint32_t w, uint32_t h,
                      const void *src, uint32_t src_pitch) {
    const uint8_t *s = (const uint8_t *)src;
    int64_t x0, y0, x1, y1, rx, ry;
    uint32_t row, col;
    uint32_t copy_w, copy_h;
    uint32_t src_dx, src_dy;

    if (!fb_ready || s == NULL || w == 0 || h == 0) return;
    /* Larger than any screen: rejected so w * fb_px_bytes cannot overflow
       and the clipped source offsets below stay meaningful. */
    if (w > FB_MAX_WIDTH || h > FB_MAX_HEIGHT) return;
    if (src_pitch < w * fb_px_bytes) return;              /* malformed source */

    x0 = x; y0 = y;
    rx = (int64_t)x + (int64_t)w;
    ry = (int64_t)y + (int64_t)h;

    if (rx <= 0 || ry <= 0) return;
    if (x0 >= (int64_t)fb_mode.width || y0 >= (int64_t)fb_mode.height) return;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    x1 = rx > (int64_t)fb_mode.width ? (int64_t)fb_mode.width : rx;
    y1 = ry > (int64_t)fb_mode.height ? (int64_t)fb_mode.height : ry;
    if (x1 <= x0 || y1 <= y0) return;

    /* Source pixel that lands on the clipped top-left corner. */
    src_dx = (uint32_t)(x0 - (int64_t)x);
    src_dy = (uint32_t)(y0 - (int64_t)y);
    copy_w = (uint32_t)(x1 - x0);
    copy_h = (uint32_t)(y1 - y0);

    for (row = 0; row < copy_h; row++) {
        const uint8_t *sp = s + (src_dy + row) * src_pitch + src_dx * fb_px_bytes;
        volatile uint8_t *dp = fb_mem + ((uint32_t)y0 + row) * fb_mode.pitch +
                               (uint32_t)x0 * fb_px_bytes;
        for (col = 0; col < copy_w * fb_px_bytes; col++) {
            dp[col] = sp[col];
        }
    }
}

void framebuffer_scroll_up(uint32_t rows, uint32_t fill_color) {
    uint32_t y, i;

    if (!fb_ready || rows == 0) return;
    if (rows >= fb_mode.height) {
        framebuffer_clear(fill_color);
        return;
    }

    /* Destination rows always sit above their source, so a forward copy
       per row is overlap-safe. */
    for (y = 0; y + rows < fb_mode.height; y++) {
        volatile uint8_t *d = fb_mem + y * fb_mode.pitch;
        const volatile uint8_t *src = fb_mem + (y + rows) * fb_mode.pitch;
        for (i = 0; i < fb_row_bytes; i++) {
            d[i] = src[i];
        }
    }

    framebuffer_fill_rect(0, (int32_t)(fb_mode.height - rows), fb_mode.width, rows, fill_color);
}

void framebuffer_wait_vsync(void) {
    if (!fb_ready) return;
    if (fb_vga_is_active()) fb_vga_wait_retrace();
    /* External framebuffers expose no portable vsync: return at once. */
}

bool_t framebuffer_selftest(void) {
    uint32_t size, x0, y0, x, y;
    uint32_t c_on, c_off, probe;
    bool_t ok = true;

    if (!fb_ready) return false;

    size = 32;
    if (size > fb_mode.width) size = fb_mode.width;
    if (size > fb_mode.height) size = fb_mode.height;
    x0 = fb_mode.width - size;
    y0 = fb_mode.height - size;

    c_on = framebuffer_rgb(255, 255, 255);
    c_off = framebuffer_rgb(0, 0, 0);
    probe = framebuffer_rgb(128, 128, 128);

    /* Checkerboard: proves block writes and read-back agree. */
    for (y = 0; y < size; y++) {
        for (x = 0; x < size; x++) {
            uint32_t cell = ((x / 4) + (y / 4)) & 1u;
            framebuffer_put_pixel((int32_t)(x0 + x), (int32_t)(y0 + y), cell ? c_on : c_off);
        }
    }
    if (framebuffer_get_pixel((int32_t)x0, (int32_t)y0) != c_off) ok = false;
    if (framebuffer_get_pixel((int32_t)(x0 + 4), (int32_t)y0) != c_on) ok = false;

    /* Write/read round trip of an arbitrary colour. */
    framebuffer_put_pixel((int32_t)x0, (int32_t)y0, probe);
    if (framebuffer_get_pixel((int32_t)x0, (int32_t)y0) != probe) ok = false;
    framebuffer_put_pixel((int32_t)x0, (int32_t)y0, c_off);

    /* Off-screen writes must be dropped, not corrupt memory. */
    framebuffer_put_pixel(-1, -1, probe);
    framebuffer_put_pixel((int32_t)fb_mode.width, (int32_t)fb_mode.height, probe);
    if (framebuffer_get_pixel(-1, -1) != 0) ok = false;
    if (framebuffer_get_pixel((int32_t)fb_mode.width, (int32_t)fb_mode.height) != 0) ok = false;

    return ok;
}
