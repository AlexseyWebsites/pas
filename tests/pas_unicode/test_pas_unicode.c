/*
    test_pas_unicode.c - Tests for pas_unicode.h
    From repo root: gcc -o tests/pas_unicode/test_pas_unicode tests/pas_unicode/test_pas_unicode.c -I.
*/

#define PAS_UNICODE_IMPLEMENTATION
#include "pas_unicode.h"
#include <stdio.h>
#include <string.h>

#if defined(PASU_USE_C11_TYPES)
#include <uchar.h>
#endif

static int g_failed;
static int g_assertions;

#define ASSERT(cond) do { \
    ++g_assertions; \
    if (!(cond)) { \
        (void)fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        ++g_failed; \
    } \
} while (0)

#define ASSERT_OK(st) ASSERT((st) == PASU_OK)
#define ASSERT_EQ(a, b) ASSERT((a) == (b))

static void test_utf8_utf32_buffer(void)
{
    const pasu_uint8 utf8[] = { 'A', 0xC3, 0xA9, 0xF0, 0x9F, 0x98, 0x80 };
    pasu_codepoint utf32_buf[8];
    pasu_uint8 utf8_back[32];
    pasu_status st;
    pasu_size n;

    n = pasu_utf8_to_utf32(utf8, sizeof(utf8), utf32_buf, 8, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 3);
    ASSERT_EQ(utf32_buf[0], (pasu_codepoint)'A');
    ASSERT_EQ(utf32_buf[1], (pasu_codepoint)0x00E9);
    ASSERT_EQ(utf32_buf[2], (pasu_codepoint)0x1F600);

    n = pasu_utf32_to_utf8(utf32_buf, 3, utf8_back, 32, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, sizeof(utf8));
    ASSERT(memcmp(utf8_back, utf8, sizeof(utf8)) == 0);
}

static void test_utf32_cstr(void)
{
    const pasu_uint8 *utf8_cstr = (const pasu_uint8 *)"Hi";
    pasu_codepoint utf32_buf[16];
    pasu_uint8 utf8_back[16];
    pasu_status st;
    pasu_size n;

    n = pasu_utf8_to_utf32_cstr(utf8_cstr, utf32_buf, 16, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(utf32_buf[0], (pasu_codepoint)'H');
    ASSERT_EQ(utf32_buf[1], (pasu_codepoint)'i');
    ASSERT_EQ(utf32_buf[2], (pasu_codepoint)0);

    n = pasu_utf32_to_utf8_cstr(utf32_buf, utf8_back, 16, &st);
    ASSERT_OK(st);
    ASSERT(strcmp((const char *)utf8_back, "Hi") == 0);
    ASSERT_EQ(utf8_back[2], 0);
}

static void test_utf16_utf32_cstr(void)
{
    pasu_uint16 utf16_hi[] = { 'H', 'i', 0 };
    pasu_codepoint utf32_buf[16];
    pasu_uint16 utf16_back[16];
    pasu_status st;
    pasu_size n;

    n = pasu_utf16_to_utf32_cstr(utf16_hi, utf32_buf, 16, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 2);
    ASSERT_EQ(utf32_buf[0], (pasu_codepoint)'H');
    ASSERT_EQ(utf32_buf[1], (pasu_codepoint)'i');
    ASSERT_EQ(utf32_buf[2], (pasu_codepoint)0);

    n = pasu_utf32_to_utf16_cstr(utf32_buf, utf16_back, 16, &st);
    ASSERT_OK(st);
    ASSERT_EQ(utf16_back[0], (pasu_uint16)'H');
    ASSERT_EQ(utf16_back[1], (pasu_uint16)'i');
    ASSERT_EQ(utf16_back[2], (pasu_uint16)0);
}

static void test_utf32_length_cstr(void)
{
    pasu_codepoint s[] = { 'a', 'b', 'c', 0 };
    pasu_status st;
    pasu_size n;

    n = pasu_utf32_length_cstr(s, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 3);
}

static void test_nospace(void)
{
    const pasu_uint8 *utf8 = (const pasu_uint8 *)"Hello";
    pasu_codepoint utf32_buf[2];
    pasu_status st;
    pasu_size n;

    n = pasu_utf8_to_utf32_cstr(utf8, utf32_buf, 2, &st);
    ASSERT_EQ(st, PASU_E_NOSPACE);
    ASSERT(n <= 2);
    ASSERT_EQ(utf32_buf[1], (pasu_codepoint)0);
}

static void test_null_src(void)
{
    pasu_uint16 dst16[8];
    pasu_codepoint dst32[8];
    pasu_status st;
    pasu_size n;

    n = pasu_utf8_to_utf16_cstr(NULL, dst16, 8, &st);
    ASSERT_EQ(st, PASU_E_INVALID);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(dst16[0], (pasu_uint16)0);

    n = pasu_utf8_to_utf32_cstr(NULL, dst32, 8, &st);
    ASSERT_EQ(st, PASU_E_INVALID);
    ASSERT_EQ(n, 0);
    ASSERT_EQ(dst32[0], (pasu_codepoint)0);

    n = pasu_utf8_length_cstr(NULL, &st);
    ASSERT_EQ(st, PASU_E_INVALID);
    ASSERT_EQ(n, 0);

    n = pasu_utf32_length_cstr(NULL, &st);
    ASSERT_EQ(st, PASU_E_INVALID);
    ASSERT_EQ(n, 0);
}

#if defined(PASU_USE_C11_TYPES)
static void test_c11_cstr(void)
{
    const pasu_uint8 *utf8 = (const pasu_uint8 *)"A";
    char16_t u16[8];
    char32_t u32[8];
    pasu_uint8 u8_back[8];
    pasu_status st;
    pasu_size n;

    n = pasu_utf8_to_utf16_cstr_c11(utf8, u16, 8, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(u16[0], (char16_t)'A');
    ASSERT_EQ(u16[1], (char16_t)0);

    n = pasu_utf16_to_utf32_cstr_c11(u16, u32, 8, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 1);
    ASSERT_EQ(u32[0], (char32_t)'A');
    ASSERT_EQ(u32[1], (char32_t)0);

    n = pasu_utf32_to_utf8_cstr_c11(u32, u8_back, 8, &st);
    ASSERT_OK(st);
    ASSERT_EQ(u8_back[0], (pasu_uint8)'A');
    ASSERT_EQ(u8_back[1], (pasu_uint8)0);

    n = pasu_utf8_length_cstr_c11(utf8, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 1);

    n = pasu_utf32_length_cstr_c11(u32, &st);
    ASSERT_OK(st);
    ASSERT_EQ(n, 1);
}
#endif

int main(void)
{
    g_failed = 0;
    g_assertions = 0;

    test_utf8_utf32_buffer();
    test_utf32_cstr();
    test_utf16_utf32_cstr();
    test_utf32_length_cstr();
    test_nospace();
    test_null_src();
#if defined(PASU_USE_C11_TYPES)
    test_c11_cstr();
#endif

    if (g_failed) {
        (void)fprintf(stderr, "Total: %d assertions, %d failed\n", g_assertions, g_failed);
        return 1;
    }
    (void)printf("All %d assertions passed.\n", g_assertions);
    return 0;
}
