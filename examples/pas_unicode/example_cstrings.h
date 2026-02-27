/*
    example_cstrings.c - Null-terminated string conversions (cstr APIs)
    From repo root: gcc -o examples/pas_unicode/example_cstrings examples/pas_unicode/example_cstrings.c -I.
*/

#define PAS_UNICODE_IMPLEMENTATION
#include "pas_unicode.h"
#include <stdio.h>

int main(void)
{
    const pasu_uint8 *utf8_cstr = (const pasu_uint8 *)"Hello, world!";
    pasu_uint16 utf16_cstr[64];
    pasu_codepoint utf32_cstr[64];
    pasu_uint8 utf8_back[64];
    pasu_status st;
    pasu_size u16_len, u32_len, u8_len, cp8, cp32;

    u16_len = pasu_utf8_to_utf16_cstr(utf8_cstr, utf16_cstr, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf8_to_utf16_cstr error: %d\n", (int)st);
        return 1;
    }
    (void)printf("UTF-16 units (with NUL): %zu\n", (size_t)(u16_len + 1));

    u32_len = pasu_utf16_to_utf32_cstr(utf16_cstr, utf32_cstr, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf16_to_utf32_cstr error: %d\n", (int)st);
        return 1;
    }

    cp8 = pasu_utf8_length_cstr(utf8_cstr, &st);
    if (st != PASU_OK) return 1;
    cp32 = pasu_utf32_length_cstr(utf32_cstr, &st);
    if (st != PASU_OK) return 1;
    (void)printf("UTF-8 code points: %zu, UTF-32 code points: %zu\n", (size_t)cp8, (size_t)cp32);

    u8_len = pasu_utf32_to_utf8_cstr(utf32_cstr, utf8_back, 64, &st);
    if (st != PASU_OK) {
        (void)fprintf(stderr, "utf32_to_utf8_cstr error: %d\n", (int)st);
        return 1;
    }
    (void)printf("UTF-8 back: %s\n", (const char *)utf8_back);

    (void)u8_len;
    return 0;
}
