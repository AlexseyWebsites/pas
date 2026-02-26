/*
    pas_gfx.h - single-header 2D framebuffer graphics (stb-style)

    - No malloc: works with user-supplied framebuffer memory
    - No OS/window dependencies: pure software rasterizer into 32-bit RGBA
    - Optional stb_truetype integration (define PAS_GFX_USE_STB_TRUETYPE and have stb_truetype.h available)

    Usage:
        In ONE translation unit:
            #define PAS_GFX_IMPLEMENTATION
            #include "pas_gfx.h"

        In others:
            #include "pas_gfx.h"
*/

#ifndef PAS_GFX_H
#define PAS_GFX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 32-bit RGBA color helpers (0xAARRGGBB) */
#define PAS_GFX_RGBA(a,r,g,b)  ((uint32_t)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))

#define PAS_GFX_BLACK   PAS_GFX_RGBA(0xFF,0x00,0x00,0x00)
#define PAS_GFX_WHITE   PAS_GFX_RGBA(0xFF,0xFF,0xFF,0xFF)
#define PAS_GFX_RED     PAS_GFX_RGBA(0xFF,0xFF,0x00,0x00)
#define PAS_GFX_GREEN   PAS_GFX_RGBA(0xFF,0x00,0xFF,0x00)
#define PAS_GFX_BLUE    PAS_GFX_RGBA(0xFF,0x00,0x00,0xFF)
#define PAS_GFX_YELLOW  PAS_GFX_RGBA(0xFF,0xFF,0xFF,0x00)
#define PAS_GFX_CYAN    PAS_GFX_RGBA(0xFF,0x00,0xFF,0xFF)
#define PAS_GFX_MAGENTA PAS_GFX_RGBA(0xFF,0xFF,0x00,0xFF)
#define PAS_GFX_GRAY    PAS_GFX_RGBA(0xFF,0x80,0x80,0x80)

typedef struct pas_gfx_fb {
    uint32_t *pixels;  /* pointer to first pixel (0,0) */
    int       width;   /* width in pixels */
    int       height;  /* height in pixels */
    int       pitch;   /* pixels per row (>= width) */
} pas_gfx_fb_t;

/* Global singleton framebuffer. Library currently supports one active fb. */
pas_gfx_fb_t *pas_gfx_init(uint32_t *pixels, int width, int height, int pitch);

/* Primitives */
void pas_gfx_pixel(pas_gfx_fb_t *fb, int x, int y, uint32_t color);
void pas_gfx_line(pas_gfx_fb_t *fb, int x1, int y1, int x2, int y2, uint32_t color);
void pas_gfx_rect(pas_gfx_fb_t *fb, int x, int y, int w, int h, uint32_t color);   /* filled rectangle */
void pas_gfx_circle(pas_gfx_fb_t *fb, int cx, int cy, int r, uint32_t color);      /* outline circle */
void pas_gfx_bitmap(pas_gfx_fb_t *fb, int x, int y,
                    const uint8_t *bitmap, int w, int h, uint32_t color);          /* 8-bit alpha mask */

/* stb_truetype integration (optional) */
#ifdef PAS_GFX_USE_STB_TRUETYPE
#include "stb_truetype.h"

typedef struct pas_font {
    stbtt_fontinfo info;
    float          size;
    float          scale;
    int            ascent;
    int            descent;
    int            line_gap;
} pas_font_t;

pas_font_t *pas_gfx_font_open(const uint8_t *ttf_data, float size);
void pas_gfx_text(pas_gfx_fb_t *fb, pas_font_t *font,
                  int x, int y, const char *text, uint32_t color);
#endif /* PAS_GFX_USE_STB_TRUETYPE */

/* Window primitives (use internal monospace bitmap font for title/label) */
void pas_gfx_window_frame(pas_gfx_fb_t *fb, int x, int y, int w, int h,
                          const char *title, uint32_t bg_color);
void pas_gfx_button(pas_gfx_fb_t *fb, int x, int y, int w, int h,
                    const char *label, int pressed);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_GFX_IMPLEMENTATION

/* Internal helpers */

static pas_gfx_fb_t pas_gfx__global_fb;

pas_gfx_fb_t *pas_gfx_init(uint32_t *pixels, int width, int height, int pitch)
{
    pas_gfx__global_fb.pixels = pixels;
    pas_gfx__global_fb.width  = width;
    pas_gfx__global_fb.height = height;
    pas_gfx__global_fb.pitch  = pitch;
    return &pas_gfx__global_fb;
}

static void pas_gfx__put_pixel_clipped(pas_gfx_fb_t *fb, int x, int y, uint32_t color)
{
    if (!fb || !fb->pixels) return;
    if ((unsigned)x >= (unsigned)fb->width) return;
    if ((unsigned)y >= (unsigned)fb->height) return;
    fb->pixels[y * fb->pitch + x] = color;
}

void pas_gfx_pixel(pas_gfx_fb_t *fb, int x, int y, uint32_t color)
{
    pas_gfx__put_pixel_clipped(fb, x, y, color);
}

void pas_gfx_line(pas_gfx_fb_t *fb, int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = (x2 > x1) ? (x2 - x1) : (x1 - x2);
    int sx = (x1 < x2) ? 1 : -1;
    int dy = (y2 > y1) ? (y1 - y2) : (y2 - y1);
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        pas_gfx__put_pixel_clipped(fb, x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }
}

void pas_gfx_rect(pas_gfx_fb_t *fb, int x, int y, int w, int h, uint32_t color)
{
    int j;
    if (!fb || !fb->pixels) return;
    if (w <= 0 || h <= 0) return;
    for (j = 0; j < h; ++j) {
        int yy = y + j;
        int i;
        if ((unsigned)yy >= (unsigned)fb->height) continue;
        for (i = 0; i < w; ++i) {
            int xx = x + i;
            if ((unsigned)xx >= (unsigned)fb->width) continue;
            fb->pixels[yy * fb->pitch + xx] = color;
        }
    }
}

void pas_gfx_circle(pas_gfx_fb_t *fb, int cx, int cy, int r, uint32_t color)
{
    int x = r;
    int y = 0;
    int err = 1 - r;

    if (r <= 0 || !fb || !fb->pixels) return;

    while (x >= y) {
        pas_gfx__put_pixel_clipped(fb, cx + x, cy + y, color);
        pas_gfx__put_pixel_clipped(fb, cx + y, cy + x, color);
        pas_gfx__put_pixel_clipped(fb, cx - y, cy + x, color);
        pas_gfx__put_pixel_clipped(fb, cx - x, cy + y, color);
        pas_gfx__put_pixel_clipped(fb, cx - x, cy - y, color);
        pas_gfx__put_pixel_clipped(fb, cx - y, cy - x, color);
        pas_gfx__put_pixel_clipped(fb, cx + y, cy - x, color);
        pas_gfx__put_pixel_clipped(fb, cx + x, cy - y, color);

        ++y;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            --x;
            err += 2 * (y - x + 1);
        }
    }
}

static uint32_t pas_gfx__blend_rgba(uint32_t dst, uint32_t src, uint8_t alpha)
{
    /* alpha: 0..255, src in 0xAARRGGBB (we combine src.A with alpha) */
    uint8_t src_a = (uint8_t)((src >> 24) & 0xFF);
    uint8_t a = (uint8_t)((src_a * (uint32_t)alpha + 127) / 255);
    uint8_t inv_a = (uint8_t)(255 - a);

    uint8_t dst_r = (uint8_t)((dst >> 16) & 0xFF);
    uint8_t dst_g = (uint8_t)((dst >> 8) & 0xFF);
    uint8_t dst_b = (uint8_t)(dst & 0xFF);

    uint8_t src_r = (uint8_t)((src >> 16) & 0xFF);
    uint8_t src_g = (uint8_t)((src >> 8) & 0xFF);
    uint8_t src_b = (uint8_t)(src & 0xFF);

    uint8_t out_r = (uint8_t)((src_r * a + dst_r * inv_a + 127) / 255);
    uint8_t out_g = (uint8_t)((src_g * a + dst_g * inv_a + 127) / 255);
    uint8_t out_b = (uint8_t)((src_b * a + dst_b * inv_a + 127) / 255);

    /* resulting alpha: just max of both for simplicity */
    uint8_t out_a = (uint8_t)(((dst >> 24) & 0xFF) | a);

    return PAS_GFX_RGBA(out_a, out_r, out_g, out_b);
}

void pas_gfx_bitmap(pas_gfx_fb_t *fb, int x, int y,
                    const uint8_t *bitmap, int w, int h, uint32_t color)
{
    int j, i;
    if (!fb || !fb->pixels || !bitmap) return;
    if (w <= 0 || h <= 0) return;

    for (j = 0; j < h; ++j) {
        int yy = y + j;
        if ((unsigned)yy >= (unsigned)fb->height) continue;
        for (i = 0; i < w; ++i) {
            int xx = x + i;
            uint8_t cov;
            uint32_t *dst;
            if ((unsigned)xx >= (unsigned)fb->width) continue;
            cov = bitmap[j * w + i];
            if (cov == 0) continue;
            dst = &fb->pixels[yy * fb->pitch + xx];
            *dst = pas_gfx__blend_rgba(*dst, color, cov);
        }
    }
}

/* --- Simple 6x8 monospace ASCII font for window/title/button text --- */

static const uint8_t pas_gfx__font6x8[96][8] = {
    /* minimal subset: space and some printable ASCII; others are blanks */
    /* ' ' (32) */ { 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 },
    /* '!' (33) */ { 0x10,0x10,0x10,0x10,0x10,0x00,0x10,0x00 },
    /* '\"'(34) */ { 0x24,0x24,0x24,0x00,0x00,0x00,0x00,0x00 },
    /* '#' (35) */ { 0x24,0x24,0x7E,0x24,0x7E,0x24,0x24,0x00 },
    /* '$' (36) */ { 0x10,0x3C,0x50,0x38,0x14,0x78,0x10,0x00 },
    /* '%' (37) */ { 0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00 },
    /* '&' (38) */ { 0x30,0x48,0x30,0x4A,0x44,0x3A,0x00,0x00 },
    /* ''' (39)*/ { 0x10,0x10,0x20,0x00,0x00,0x00,0x00,0x00 },
    /* '(' (40) */ { 0x08,0x10,0x20,0x20,0x20,0x10,0x08,0x00 },
    /* ')' (41) */ { 0x20,0x10,0x08,0x08,0x08,0x10,0x20,0x00 },
    /* '*' (42) */ { 0x00,0x10,0x54,0x38,0x54,0x10,0x00,0x00 },
    /* '+' (43) */ { 0x00,0x10,0x10,0x7C,0x10,0x10,0x00,0x00 },
    /* ',' (44) */ { 0x00,0x00,0x00,0x00,0x10,0x10,0x20,0x00 },
    /* '-' (45) */ { 0x00,0x00,0x00,0x7C,0x00,0x00,0x00,0x00 },
    /* '.' (46) */ { 0x00,0x00,0x00,0x00,0x00,0x30,0x30,0x00 },
    /* '/' (47) */ { 0x04,0x08,0x10,0x20,0x40,0x00,0x00,0x00 },
    /* '0' (48) */ { 0x38,0x44,0x4C,0x54,0x64,0x44,0x38,0x00 },
    /* '1' (49) */ { 0x10,0x30,0x10,0x10,0x10,0x10,0x38,0x00 },
    /* '2' (50) */ { 0x38,0x44,0x04,0x18,0x20,0x40,0x7C,0x00 },
    /* '3' (51) */ { 0x38,0x44,0x04,0x18,0x04,0x44,0x38,0x00 },
    /* '4' (52) */ { 0x08,0x18,0x28,0x48,0x7C,0x08,0x08,0x00 },
    /* '5' (53) */ { 0x7C,0x40,0x78,0x04,0x04,0x44,0x38,0x00 },
    /* '6' (54) */ { 0x38,0x44,0x40,0x78,0x44,0x44,0x38,0x00 },
    /* '7' (55) */ { 0x7C,0x04,0x08,0x10,0x20,0x20,0x20,0x00 },
    /* '8' (56) */ { 0x38,0x44,0x44,0x38,0x44,0x44,0x38,0x00 },
    /* '9' (57) */ { 0x38,0x44,0x44,0x3C,0x04,0x44,0x38,0x00 },
    /* ':' (58) */ { 0x00,0x30,0x30,0x00,0x30,0x30,0x00,0x00 },
    /* ';' (59) */ { 0x00,0x30,0x30,0x00,0x30,0x30,0x20,0x00 },
    /* '<' (60) */ { 0x08,0x10,0x20,0x40,0x20,0x10,0x08,0x00 },
    /* '=' (61) */ { 0x00,0x00,0x7C,0x00,0x7C,0x00,0x00,0x00 },
    /* '>' (62) */ { 0x20,0x10,0x08,0x04,0x08,0x10,0x20,0x00 },
    /* '?' (63) */ { 0x38,0x44,0x04,0x08,0x10,0x00,0x10,0x00 },
    /* '@' (64) */ { 0x38,0x44,0x5C,0x54,0x5C,0x40,0x3C,0x00 },
    /* 'A' (65) */ { 0x38,0x44,0x44,0x7C,0x44,0x44,0x44,0x00 },
    /* 'B' (66) */ { 0x78,0x44,0x44,0x78,0x44,0x44,0x78,0x00 },
    /* 'C' (67) */ { 0x38,0x44,0x40,0x40,0x40,0x44,0x38,0x00 },
    /* 'D' (68) */ { 0x78,0x44,0x44,0x44,0x44,0x44,0x78,0x00 },
    /* 'E' (69) */ { 0x7C,0x40,0x40,0x78,0x40,0x40,0x7C,0x00 },
    /* 'F' (70) */ { 0x7C,0x40,0x40,0x78,0x40,0x40,0x40,0x00 },
    /* 'G' (71) */ { 0x38,0x44,0x40,0x40,0x4C,0x44,0x38,0x00 },
    /* 'H' (72) */ { 0x44,0x44,0x44,0x7C,0x44,0x44,0x44,0x00 },
    /* 'I' (73) */ { 0x38,0x10,0x10,0x10,0x10,0x10,0x38,0x00 },
    /* 'J' (74) */ { 0x1C,0x08,0x08,0x08,0x08,0x48,0x30,0x00 },
    /* 'K' (75) */ { 0x44,0x48,0x50,0x60,0x50,0x48,0x44,0x00 },
    /* 'L' (76) */ { 0x40,0x40,0x40,0x40,0x40,0x40,0x7C,0x00 },
    /* 'M' (77) */ { 0x44,0x6C,0x54,0x54,0x44,0x44,0x44,0x00 },
    /* 'N' (78) */ { 0x44,0x64,0x54,0x4C,0x44,0x44,0x44,0x00 },
    /* 'O' (79) */ { 0x38,0x44,0x44,0x44,0x44,0x44,0x38,0x00 },
    /* 'P' (80) */ { 0x78,0x44,0x44,0x78,0x40,0x40,0x40,0x00 },
    /* 'Q' (81) */ { 0x38,0x44,0x44,0x44,0x54,0x48,0x34,0x00 },
    /* 'R' (82) */ { 0x78,0x44,0x44,0x78,0x50,0x48,0x44,0x00 },
    /* 'S' (83) */ { 0x38,0x44,0x40,0x38,0x04,0x44,0x38,0x00 },
    /* 'T' (84) */ { 0x7C,0x10,0x10,0x10,0x10,0x10,0x10,0x00 },
    /* 'U' (85) */ { 0x44,0x44,0x44,0x44,0x44,0x44,0x38,0x00 },
    /* 'V' (86) */ { 0x44,0x44,0x44,0x44,0x44,0x28,0x10,0x00 },
    /* 'W' (87) */ { 0x44,0x44,0x44,0x54,0x54,0x6C,0x44,0x00 },
    /* 'X' (88) */ { 0x44,0x44,0x28,0x10,0x28,0x44,0x44,0x00 },
    /* 'Y' (89) */ { 0x44,0x44,0x44,0x28,0x10,0x10,0x10,0x00 },
    /* 'Z' (90) */ { 0x7C,0x04,0x08,0x10,0x20,0x40,0x7C,0x00 },
    /* '[' (91) */ { 0x38,0x20,0x20,0x20,0x20,0x20,0x38,0x00 },
    /* '\\'(92) */ { 0x40,0x20,0x10,0x08,0x04,0x00,0x00,0x00 },
    /* ']' (93) */ { 0x38,0x08,0x08,0x08,0x08,0x08,0x38,0x00 },
    /* '^' (94) */ { 0x10,0x28,0x44,0x00,0x00,0x00,0x00,0x00 },
    /* '_' (95) */ { 0x00,0x00,0x00,0x00,0x00,0x00,0x7C,0x00 },
    /* '`' (96) */ { 0x10,0x10,0x08,0x00,0x00,0x00,0x00,0x00 },
    /* 'a' (97) */ { 0x00,0x00,0x38,0x04,0x3C,0x44,0x3C,0x00 },
    /* 'b' (98) */ { 0x40,0x40,0x78,0x44,0x44,0x44,0x78,0x00 },
    /* 'c' (99) */ { 0x00,0x00,0x38,0x44,0x40,0x44,0x38,0x00 },
    /* 'd' (100)*/ { 0x04,0x04,0x3C,0x44,0x44,0x44,0x3C,0x00 },
    /* 'e' (101)*/ { 0x00,0x00,0x38,0x44,0x7C,0x40,0x38,0x00 },
    /* 'f' (102)*/ { 0x18,0x24,0x20,0x70,0x20,0x20,0x20,0x00 },
    /* 'g' (103)*/ { 0x00,0x00,0x3C,0x44,0x44,0x3C,0x04,0x38 },
    /* 'h' (104)*/ { 0x40,0x40,0x78,0x44,0x44,0x44,0x44,0x00 },
    /* 'i' (105)*/ { 0x10,0x00,0x30,0x10,0x10,0x10,0x38,0x00 },
    /* 'j' (106)*/ { 0x08,0x00,0x18,0x08,0x08,0x48,0x30,0x00 },
    /* 'k' (107)*/ { 0x40,0x40,0x44,0x48,0x70,0x48,0x44,0x00 },
    /* 'l' (108)*/ { 0x30,0x10,0x10,0x10,0x10,0x10,0x38,0x00 },
    /* 'm' (109)*/ { 0x00,0x00,0x68,0x54,0x54,0x54,0x54,0x00 },
    /* 'n' (110)*/ { 0x00,0x00,0x78,0x44,0x44,0x44,0x44,0x00 },
    /* 'o' (111)*/ { 0x00,0x00,0x38,0x44,0x44,0x44,0x38,0x00 },
    /* 'p' (112)*/ { 0x00,0x00,0x78,0x44,0x44,0x78,0x40,0x40 },
    /* 'q' (113)*/ { 0x00,0x00,0x3C,0x44,0x44,0x3C,0x04,0x04 },
    /* 'r' (114)*/ { 0x00,0x00,0x58,0x64,0x40,0x40,0x40,0x00 },
    /* 's' (115)*/ { 0x00,0x00,0x3C,0x40,0x38,0x04,0x78,0x00 },
    /* 't' (116)*/ { 0x20,0x20,0x70,0x20,0x20,0x24,0x18,0x00 },
    /* 'u' (117)*/ { 0x00,0x00,0x44,0x44,0x44,0x4C,0x34,0x00 },
    /* 'v' (118)*/ { 0x00,0x00,0x44,0x44,0x44,0x28,0x10,0x00 },
    /* 'w' (119)*/ { 0x00,0x00,0x44,0x44,0x54,0x54,0x28,0x00 },
    /* 'x' (120)*/ { 0x00,0x00,0x44,0x28,0x10,0x28,0x44,0x00 },
    /* 'y' (121)*/ { 0x00,0x00,0x44,0x44,0x44,0x3C,0x04,0x38 },
    /* 'z' (122)*/ { 0x00,0x00,0x7C,0x08,0x10,0x20,0x7C,0x00 },
    /* '{' (123)*/ { 0x0C,0x10,0x10,0x60,0x10,0x10,0x0C,0x00 },
    /* '|' (124)*/ { 0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x00 },
    /* '}' (125)*/ { 0x60,0x10,0x10,0x0C,0x10,0x10,0x60,0x00 },
    /* '~' (126)*/ { 0x28,0x54,0x00,0x00,0x00,0x00,0x00,0x00 }
};

static void pas_gfx__text_mono(pas_gfx_fb_t *fb, int x, int y,
                               const char *text, uint32_t color)
{
    int pen_x = x;
    int pen_y = y;
    const unsigned char *s = (const unsigned char *)text;
    if (!fb || !fb->pixels || !text) return;

    while (*s) {
        unsigned char c = *s++;
        int row;
        if (c == '\n') {
            pen_x = x;
            pen_y += 9;
            continue;
        }
        if (c < 32 || c > 126) {
            pen_x += 6;
            continue;
        }
        for (row = 0; row < 8; ++row) {
            uint8_t bits = pas_gfx__font6x8[c - 32][row];
            int col;
            for (col = 0; col < 6; ++col) {
                if (bits & (1 << (5 - col))) {
                    pas_gfx__put_pixel_clipped(fb, pen_x + col, pen_y + row, color);
                }
            }
        }
        pen_x += 6;
    }
}

void pas_gfx_window_frame(pas_gfx_fb_t *fb, int x, int y, int w, int h,
                          const char *title, uint32_t bg_color)
{
    int title_bar_h = 14;
    uint32_t border_color = PAS_GFX_WHITE;
    uint32_t title_color  = PAS_GFX_BLUE;

    if (!fb || !fb->pixels) return;
    if (w <= 2 || h <= 2) return;

    /* background */
    pas_gfx_rect(fb, x + 1, y + 1, w - 2, h - 2, bg_color);

    /* border */
    pas_gfx_line(fb, x, y, x + w - 1, y, border_color);
    pas_gfx_line(fb, x, y + h - 1, x + w - 1, y + h - 1, border_color);
    pas_gfx_line(fb, x, y, x, y + h - 1, border_color);
    pas_gfx_line(fb, x + w - 1, y, x + w - 1, y + h - 1, border_color);

    /* title bar */
    if (title_bar_h > h - 2) title_bar_h = h - 2;
    pas_gfx_rect(fb, x + 1, y + 1, w - 2, title_bar_h, title_color);

    if (title && *title) {
        pas_gfx__text_mono(fb, x + 4, y + 4, title, PAS_GFX_WHITE);
    }
}

void pas_gfx_button(pas_gfx_fb_t *fb, int x, int y, int w, int h,
                    const char *label, int pressed)
{
    uint32_t bg = pressed ? PAS_GFX_GRAY : PAS_GFX_WHITE;
    uint32_t border_light = PAS_GFX_WHITE;
    uint32_t border_dark  = PAS_GFX_BLACK;
    int text_x, text_y;
    int label_len = 0;
    const char *s;

    if (!fb || !fb->pixels) return;
    if (w <= 2 || h <= 2) return;

    /* fill */
    pas_gfx_rect(fb, x + 1, y + 1, w - 2, h - 2, bg);

    /* simple bevel border */
    if (!pressed) {
        pas_gfx_line(fb, x, y, x + w - 1, y, border_light);
        pas_gfx_line(fb, x, y, x, y + h - 1, border_light);
        pas_gfx_line(fb, x + w - 1, y, x + w - 1, y + h - 1, border_dark);
        pas_gfx_line(fb, x, y + h - 1, x + w - 1, y + h - 1, border_dark);
    } else {
        pas_gfx_line(fb, x, y, x + w - 1, y, border_dark);
        pas_gfx_line(fb, x, y, x, y + h - 1, border_dark);
        pas_gfx_line(fb, x + w - 1, y, x + w - 1, y + h - 1, border_light);
        pas_gfx_line(fb, x, y + h - 1, x + w - 1, y + h - 1, border_light);
    }

    if (!label) return;
    for (s = label; *s; ++s) ++label_len;

    /* approximate centering with 6x8 font */
    text_x = x + (w - label_len * 6) / 2;
    text_y = y + (h - 8) / 2;
    if (pressed) { text_x += 1; text_y += 1; }

    pas_gfx__text_mono(fb, text_x, text_y, label, PAS_GFX_BLACK);
}

/* stb_truetype text rendering */

#ifdef PAS_GFX_USE_STB_TRUETYPE

static pas_font_t pas_gfx__global_font;

pas_font_t *pas_gfx_font_open(const uint8_t *ttf_data, float size)
{
    int offset;
    if (!ttf_data || size <= 0) return NULL;
    offset = stbtt_GetFontOffsetForIndex(ttf_data, 0);
    if (offset < 0) return NULL;
    if (!stbtt_InitFont(&pas_gfx__global_font.info, ttf_data, offset))
        return NULL;
    pas_gfx__global_font.size  = size;
    pas_gfx__global_font.scale = stbtt_ScaleForPixelHeight(&pas_gfx__global_font.info, size);
    stbtt_GetFontVMetrics(&pas_gfx__global_font.info,
                          &pas_gfx__global_font.ascent,
                          &pas_gfx__global_font.descent,
                          &pas_gfx__global_font.line_gap);
    return &pas_gfx__global_font;
}

void pas_gfx_text(pas_gfx_fb_t *fb, pas_font_t *font,
                  int x, int y, const char *text, uint32_t color)
{
    int pen_x = x;
    int pen_y;
    const unsigned char *s = (const unsigned char *)text;
    unsigned char glyph_bitmap[128 * 128];

    if (!fb || !fb->pixels || !font || !text) return;

    pen_y = y + (int)(font->ascent * font->scale);

    while (*s) {
        int ch = *s++;
        int gx0, gy0, gx1, gy1;
        int gw, gh;
        int x_off, y_off;
        int advance, kern = 0;
        int gy, gx;

        if (ch == '\n') {
            pen_x = x;
            pen_y += (int)((font->ascent - font->descent + font->line_gap) * font->scale);
            continue;
        }

        stbtt_GetCodepointHMetrics(&font->info, ch, &advance, NULL);

        stbtt_GetCodepointBitmapBox(&font->info, ch,
                                    font->scale, font->scale,
                                    &gx0, &gy0, &gx1, &gy1);
        gw = gx1 - gx0;
        gh = gy1 - gy0;
        if (gw <= 0 || gh <= 0) {
            pen_x += (int)(advance * font->scale);
            continue;
        }
        if (gw > 128 || gh > 128) {
            pen_x += (int)(advance * font->scale);
            continue;
        }

        stbtt_MakeCodepointBitmap(&font->info,
                                  glyph_bitmap, gw, gh, gw,
                                  font->scale, font->scale, ch);

        x_off = pen_x + gx0;
        y_off = pen_y + gy0;

        for (gy = 0; gy < gh; ++gy) {
            int yy = y_off + gy;
            if ((unsigned)yy >= (unsigned)fb->height) continue;
            for (gx = 0; gx < gw; ++gx) {
                int xx = x_off + gx;
                uint8_t cov;
                uint32_t *dst;
                if ((unsigned)xx >= (unsigned)fb->width) continue;
                cov = glyph_bitmap[gy * gw + gx];
                if (cov == 0) continue;
                dst = &fb->pixels[yy * fb->pitch + xx];
                *dst = pas_gfx__blend_rgba(*dst, color, cov);
            }
        }

        if (*s) {
            kern = stbtt_GetCodepointKernAdvance(&font->info, ch, *s);
        }
        pen_x += (int)((advance + kern) * font->scale);
    }
}

#endif /* PAS_GFX_USE_STB_TRUETYPE */

#endif /* PAS_GFX_IMPLEMENTATION */

#endif /* PAS_GFX_H */

