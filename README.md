# PAS + OS (UNLICENCE)

Single-header C libraries in stb style: no malloc, user buffers, OS-only dependencies.

- **pas_unicode.h** — UTF-8/16/32 encode/decode, conversions, length, C-strings; optional C11 `char16_t`/`char32_t`.
- **pas_http1.h** — HTTP/1.1 client: GET/POST, URL parsing, timeouts, response parsing; uses OS sockets only (Winsock2 / BSD).
- **pas_gfx.h** — 2D framebuffer graphics: pixel, line, rect, circle, bitmap (alpha mask); optional stb_truetype text; window frame and button primitives; 32-bit RGBA, no malloc.

---

# pas_unicode.h

Single-header, stb-style, dependency-free Unicode helpers for C. Suitable for bare-metal, OS development, and embedded use: **no malloc**, no locale dependency, no C runtime beyond `stddef.h` / `stdint.h` (or minimal fallbacks).

## Design

- **One header**: include in one TU with `PAS_UNICODE_IMPLEMENTATION`, in others without.
- **No dynamic allocation**: all APIs accept caller-provided buffers and capacities.
- **Error reporting**: `pasu_status` (e.g. `PASU_OK`, `PASU_E_INVALID`, `PASU_E_NOSPACE`) via optional `status` pointer.
- **Null-terminated C-strings**: `_cstr` functions guarantee a trailing NUL in the output when `dst_capacity > 0`, even on error.
- **C11 optional**: when `__STDC_UTF_16__` and `__STDC_UTF_32__` are defined, `PASU_USE_C11_TYPES` is set and `_c11` variants using `char16_t*` / `char32_t*` are available.

## Usage

In **one** translation unit:

```c
#define PAS_UNICODE_IMPLEMENTATION
#include "pas_unicode.h"
```

In all others:

```c
#include "pas_unicode.h"
```

Optional: define `PAS_UNICODE_STATIC` before the include to make implementation symbols `static` (single-TU only).

---

## API overview

### Types

| Type | Description |
|------|-------------|
| `pasu_uint8`, `pasu_uint16`, `pasu_uint32`, `pasu_int32`, `pasu_size` | Integer types (from stdint/stddef or fallbacks). |
| `pasu_codepoint` | Unicode scalar value (0..0x10FFFF, excluding surrogates). |
| `pasu_status` | Result code: `PASU_OK`, `PASU_E_INVALID`, `PASU_E_RANGE`, `PASU_E_SURROG`, `PASU_E_TRUNC`, `PASU_E_NOSPACE`. |

### Low-level encode/decode

- **UTF-8**: `pasu_utf8_decode(s, len, &cp, &used)`, `pasu_utf8_encode(cp, out[4], &used)`, `pasu_utf8_next(s, len, &pos, &cp)`.
- **UTF-16**: `pasu_utf16_decode(s, len, &cp, &used)`, `pasu_utf16_encode(cp, out[2], &used)`, `pasu_utf16_next(s, len, &pos, &cp)`.

### Query helpers

- `pasu_is_valid_scalar(cp)` — valid Unicode scalar (not surrogate, in range).
- ASCII: `pasu_is_ascii`, `pasu_is_ascii_alpha`, `pasu_is_ascii_digit`, `pasu_is_ascii_alnum`, `pasu_is_ascii_space`, `pasu_is_ascii_upper` / `pasu_is_ascii_lower`.

### Buffer conversions (no NUL added)

All take `(src, src_len, dst, dst_capacity, status)`. Return number of units written; on error set `*status` and return partial count.

| From   | To    | Function |
|--------|-------|----------|
| UTF-8  | UTF-16| `pasu_utf8_to_utf16` |
| UTF-16 | UTF-8 | `pasu_utf16_to_utf8` |
| UTF-8  | UTF-32| `pasu_utf8_to_utf32` |
| UTF-32 | UTF-8 | `pasu_utf32_to_utf8` |
| UTF-16 | UTF-32| `pasu_utf16_to_utf32` |
| UTF-32 | UTF-16| `pasu_utf32_to_utf16` |

### Length (code points)

- `pasu_utf8_length(str, len, status)` — code points in UTF-8 buffer.
- `pasu_utf16_length(str, len, status)` — code points in UTF-16 buffer.
- `pasu_utf32_length(str, len, status)` — valid scalars in UTF-32 buffer (stops on first invalid).

### C-string helpers (null-terminated)

Input: null-terminated source. Output: always null-terminated when `dst_capacity > 0`. Return value: units written (excluding NUL).

**UTF-8 / UTF-16**

- `pasu_utf8_to_utf16_cstr(src, dst, dst_capacity, status)`
- `pasu_utf16_to_utf8_cstr(src, dst, dst_capacity, status)`
- `pasu_utf8_length_cstr(src, status)`

**UTF-32**

- `pasu_utf8_to_utf32_cstr(src, dst, dst_capacity, status)`
- `pasu_utf32_to_utf8_cstr(src, dst, dst_capacity, status)`
- `pasu_utf16_to_utf32_cstr(src, dst, dst_capacity, status)`
- `pasu_utf32_to_utf16_cstr(src, dst, dst_capacity, status)`
- `pasu_utf32_length_cstr(src, status)` — code points before the first 0 (validates each scalar).

### C11 variants (`PASU_USE_C11_TYPES`)

When `__STDC_UTF_16__` and `__STDC_UTF_32__` are defined, the following use `char16_t*` / `char32_t*`:

- `pasu_utf8_to_utf16_cstr_c11`, `pasu_utf16_to_utf8_cstr_c11`
- `pasu_utf8_to_utf32_cstr_c11`, `pasu_utf32_to_utf8_cstr_c11`
- `pasu_utf16_to_utf32_cstr_c11`, `pasu_utf32_to_utf16_cstr_c11`
- `pasu_utf8_length_cstr_c11`, `pasu_utf32_length_cstr_c11`

Same semantics: no malloc, errors via `status`, output null-terminated.

---

## Error codes

| Code | Meaning |
|------|---------|
| `PASU_OK` | Success. |
| `PASU_E_INVALID` | Ill-formed sequence (e.g. invalid UTF-8). |
| `PASU_E_RANGE` | Code point out of Unicode range. |
| `PASU_E_SURROG` | Surrogate where not allowed. |
| `PASU_E_TRUNC` | Truncated input. |
| `PASU_E_NOSPACE` | Output buffer too small. |

---

# pas_http1.h

HTTP/1.1 client: one header, no malloc, OS sockets only (Windows: Winsock2, Unix: BSD). User supplies the response buffer; parsed `status_code`, `headers`, and `body` point into it.

**Usage:** In one TU define `PAS_HTTP1_IMPLEMENTATION` then `#include "pas_http1.h"`.

**API**
- `pas_http_get(url, response_buffer, buffer_size, timeout_ms, &out_response, &status)` — GET request.
- `pas_http_post(url, body, body_len, response_buffer, buffer_size, timeout_ms, &out_response, &status)` — POST with body.

**Structure:** `pas_http_response_t` has `status_code`, `headers` / `headers_len`, `body` / `body_len` (all pointing into your buffer).

**Errors:** `PAS_HTTP_OK`, `PAS_HTTP_E_INVALID_URL`, `PAS_HTTP_E_CONNECTION`, `PAS_HTTP_E_TIMEOUT`, `PAS_HTTP_E_NOSPACE` (buffer too small; response is still parsed up to buffer size).

---

# pas_gfx.h

2D software rasterizer for a single framebuffer (32-bit RGBA, 0xAARRGGBB). No malloc: you pass `uint32_t *pixels`, width, height, pitch. Optional **stb_truetype** for TTF text (define `PAS_GFX_USE_STB_TRUETYPE` and provide `stb_truetype.h`).

**Usage:** In one TU define `PAS_GFX_IMPLEMENTATION` then `#include "pas_gfx.h"`.

**Init:** `pas_gfx_fb_t *pas_gfx_init(uint32_t *pixels, int width, int height, int pitch);` — returns pointer to the single global fb.

**Primitives:** `pas_gfx_pixel`, `pas_gfx_line`, `pas_gfx_rect` (filled), `pas_gfx_circle` (outline), `pas_gfx_bitmap` (8-bit alpha mask, blended).

**Window UI:** `pas_gfx_window_frame(fb, x, y, w, h, title, bg_color)` — frame, title bar, built-in 6×8 font for title. `pas_gfx_button(fb, x, y, w, h, label, pressed)` — bevel, label with built-in font.

**Optional TTF:** If `PAS_GFX_USE_STB_TRUETYPE` is defined: `pas_gfx_font_open(ttf_data, size)`, `pas_gfx_text(fb, font, x, y, text, color)`.

**Colors:** `PAS_GFX_BLACK`, `PAS_GFX_WHITE`, `PAS_GFX_RED`, `PAS_GFX_GREEN`, `PAS_GFX_BLUE`, `PAS_GFX_YELLOW`, `PAS_GFX_CYAN`, `PAS_GFX_MAGENTA`, `PAS_GFX_GRAY`, and `PAS_GFX_RGBA(a,r,g,b)`.

---

## Examples and tests

Layout: **examples/** and **tests/** are split by library: **pas_unicode/**, **pas_http1/**, **pas_gfx/**.

**pas_unicode**
- **examples/pas_unicode/example_minimal.c** — buffer conversions: UTF-8 → UTF-16 → UTF-32 → UTF-8, length.
- **examples/pas_unicode/example_cstrings.c** — C-string conversions and length_cstr.
- **examples/pas_unicode/example_c11.c** — C11 `_c11` APIs (no-op if `PASU_USE_C11_TYPES` is not defined).
- **tests/pas_unicode/test_pas_unicode.c** — tests for buffer, cstr, UTF-32 cstr, NOSPACE, NULL, and (if C11) _c11.

**pas_http1**
- **examples/pas_http1/example_get.c** — GET request.
- **tests/pas_http1/test_pas_http1.c** — invalid URL, null buffer, optional live GET.

**pas_gfx**
- **examples/pas_gfx/example_primitives.c** — 1024×768 in-memory fb, lines/rects/circles, save to PPM (raw RGB).
- **examples/pas_gfx/example_window.c** — window frame with title, button (pressed/unpressed), built-in font, save to PPM.
- **examples/pas_gfx/example_text.c** — if `PAS_GFX_USE_STB_TRUETYPE`: load `data/font.ttf`, multiline text, kerning demo, save to PPM.
- **tests/pas_gfx/test_pas_gfx.c** — pixel (color/clip), line (all octants), rect (bounds/fill), circle (symmetry), bitmap (alpha), window_frame (frame pixels), button (pressed/unpressed).

Build from repo root (`-I.`):

```bash
gcc -o examples/pas_unicode/example_minimal   examples/pas_unicode/example_minimal.c   -I.
gcc -o examples/pas_unicode/example_cstrings  examples/pas_unicode/example_cstrings.c  -I.
gcc -std=c11 -o examples/pas_unicode/example_c11 examples/pas_unicode/example_c11.c -I.
gcc -o tests/pas_unicode/test_pas_unicode tests/pas_unicode/test_pas_unicode.c -I.

gcc -o examples/pas_http1/example_get examples/pas_http1/example_get.c -I.
gcc -o tests/pas_http1/test_pas_http1 tests/pas_http1/test_pas_http1.c -I.

gcc -o examples/pas_gfx/example_primitives examples/pas_gfx/example_primitives.c -I.
gcc -o examples/pas_gfx/example_window     examples/pas_gfx/example_window.c -I.
gcc -o examples/pas_gfx/example_text       examples/pas_gfx/example_text.c -I. -DPAS_GFX_USE_STB_TRUETYPE
gcc -o tests/pas_gfx/test_pas_gfx          tests/pas_gfx/test_pas_gfx.c -I.
```

On Windows with MinGW, link pas_http1 with `-lws2_32` if the header’s pragma doesn’t pull it in.

Run tests:

```bash
./tests/pas_unicode/test_pas_unicode
./tests/pas_http1/test_pas_http1
./tests/pas_gfx/test_pas_gfx
```

---

## License

Use as you like (e.g. UNLICENSE / public domain). See project terms.
