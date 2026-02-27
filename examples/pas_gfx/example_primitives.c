/*
    example_primitives.c - Lines, rectangles, circles; save to PPM.
    From repo root: gcc -o examples/pas_gfx/example_primitives examples/pas_gfx/example_primitives.c -I.
*/

#define PAS_GFX_IMPLEMENTATION
#include "pas_gfx.h"
#include <stdio.h>
#include <stdlib.h>

#define W 1024
#define H 768
#define PITCH W

static int save_ppm_raw(const char *path, const uint32_t *pixels, int width, int height, int pitch)
{
    FILE *f;
    int y, x;
    unsigned char rgb[3];

    f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", width, height);
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            uint32_t c = pixels[y * pitch + x];
            rgb[0] = (unsigned char)((c >> 16) & 0xFF);
            rgb[1] = (unsigned char)((c >> 8) & 0xFF);
            rgb[2] = (unsigned char)(c & 0xFF);
            if (fwrite(rgb, 1, 3, f) != 3) {
                fclose(f);
                return -1;
            }
        }
    }
    fclose(f);
    return 0;
}

int main(void)
{
    static uint32_t pixels[W * H];
    pas_gfx_fb_t *fb;
    int i;

    fb = pas_gfx_init(pixels, W, H, PITCH);
    if (!fb) {
        fprintf(stderr, "pas_gfx_init failed\n");
        return 1;
    }

    /* Clear to dark gray */
    pas_gfx_rect(fb, 0, 0, W, H, PAS_GFX_RGBA(0xFF, 0x30, 0x30, 0x30));

    /* Lines in all directions */
    pas_gfx_line(fb, 0, 0, W - 1, H - 1, PAS_GFX_RED);
    pas_gfx_line(fb, W - 1, 0, 0, H - 1, PAS_GFX_GREEN);
    pas_gfx_line(fb, W / 2, 0, W / 2, H - 1, PAS_GFX_BLUE);
    pas_gfx_line(fb, 0, H / 2, W - 1, H / 2, PAS_GFX_YELLOW);

    /* Rectangles */
    pas_gfx_rect(fb, 50, 50, 200, 150, PAS_GFX_CYAN);
    pas_gfx_rect(fb, 300, 200, 400, 100, PAS_GFX_MAGENTA);

    /* Circles */
    pas_gfx_circle(fb, 200, 400, 80, PAS_GFX_WHITE);
    pas_gfx_circle(fb, 600, 400, 120, PAS_GFX_YELLOW);
    for (i = 0; i < 8; i++) {
        int cx = 512 + (int)(200 * (i & 1 ? 1 : -1));
        int cy = 384 + (int)(150 * ((i >> 1) & 1 ? 1 : -1));
        pas_gfx_circle(fb, cx, cy, 40, PAS_GFX_RED);
    }

    if (save_ppm_raw("example_primitives.ppm", pixels, W, H, PITCH) != 0) {
        fprintf(stderr, "Failed to write example_primitives.ppm\n");
        return 1;
    }
    printf("Saved example_primitives.ppm (%dx%d)\n", W, H);
    return 0;
}
