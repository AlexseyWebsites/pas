/*
    pas_zip.h - single-header ZIP reader/writer (stb-style)

    - No malloc: all APIs use user-provided buffers
    - Read ZIP from memory: Central Directory parsing
    - Extract: Store (always), Deflate (optional via PAS_ZIP_USE_MINIZ or PAS_ZIP_USE_ZLIB)
    - Write ZIP: Store only (optional)
    - UTF-8 filenames: optional via pas_unicode.h

    Usage:
        In ONE translation unit:
            #define PAS_ZIP_IMPLEMENTATION
            #include "pas_zip.h"

        In others:
            #include "pas_zip.h"

    Deflate support (optional):
        #define PAS_ZIP_USE_MINIZ
        #include "miniz.h"   // miniz.c or miniz.h single-header

        Or:
        #define PAS_ZIP_USE_ZLIB
        #include <zlib.h>
*/

#ifndef PAS_ZIP_H
#define PAS_ZIP_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAS_ZIP_OK          0
#define PAS_ZIP_E_INVALID  -1
#define PAS_ZIP_E_NOT_FOUND -2
#define PAS_ZIP_E_COMPRESSED -3  /* deflate needed */
#define PAS_ZIP_E_NOSPACE   -4
#define PAS_ZIP_E_ZLIB      -5

#define PAS_ZIP_METHOD_STORE  0
#define PAS_ZIP_METHOD_DEFLATE 8

typedef int pas_zip_status;

typedef struct pas_zip pas_zip_t;
typedef struct pas_zip_file pas_zip_file_t;

struct pas_zip {
    const uint8_t *data;
    size_t         size;
    uint32_t       cd_offset;
    uint16_t       num_entries;
};

struct pas_zip_file {
    const char *name;
    size_t      compressed_size;
    size_t      uncompressed_size;
    uint16_t    compression_method;
    uint32_t    local_header_offset;
};

/* Open ZIP from memory. data/size must remain valid. */
pas_zip_t *pas_zip_open(const void *data, size_t size, pas_zip_status *status);

/* Find file by name (case-sensitive). Returns NULL if not found. */
pas_zip_file_t *pas_zip_find(pas_zip_t *zip, const char *name);

/* File info */
const char *pas_zip_name(pas_zip_file_t *file);
size_t      pas_zip_size(pas_zip_file_t *file);
int         pas_zip_is_compressed(pas_zip_file_t *file);

/* Extract file to buffer. Returns bytes written or 0 on error. */
size_t pas_zip_extract(pas_zip_file_t *file, void *buffer, size_t buffer_size, pas_zip_status *status);

/* List all files. callback(name, uncompressed_size, user) */
int pas_zip_list(pas_zip_t *zip, void (*callback)(const char *name, size_t size, void *user), void *user);

/* Create ZIP (Store only). Returns bytes written or 0 on error. */
size_t pas_zip_create(const char **filenames, const void **datas, const size_t *sizes,
                      int file_count, void *buffer, size_t buffer_size, pas_zip_status *status);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_ZIP_IMPLEMENTATION

#include <string.h>

#ifdef PAS_ZIP_USE_MINIZ
#include "miniz.h"
#endif
#ifdef PAS_ZIP_USE_ZLIB
#include <zlib.h>
#endif

#define PAS_ZIP_EOCD_SIG  0x06054b50u
#define PAS_ZIP_CDH_SIG   0x02014b50u
#define PAS_ZIP_LFH_SIG   0x04034b50u

static pas_zip_t pas_zip__handle;
static pas_zip_file_t pas_zip__current_file;
static char pas_zip__name_buf[512];

static uint32_t read_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int find_eocd(const uint8_t *data, size_t size, uint32_t *cd_offset, uint16_t *num_entries) {
    size_t i;
    size_t search_end;

    if (size < 22) return 0;
    search_end = (size > 65557) ? size - 65557 : 0;

    for (i = size - 22; ; i--) {
        if (i + 22 > size) continue;
        if (read_u32_le(data + i) == PAS_ZIP_EOCD_SIG) {
            *num_entries = read_u16_le(data + i + 8);
            *cd_offset = read_u32_le(data + i + 16);
            if (*cd_offset >= size) return 0;
            return 1;
        }
        if (i <= search_end || i == 0) break;
    }
    return 0;
}

pas_zip_t *pas_zip_open(const void *data, size_t size, pas_zip_status *status) {
    uint32_t cd_offset;
    uint16_t num_entries;

    if (status) *status = PAS_ZIP_E_INVALID;
    if (!data || size < 22) return NULL;

    if (!find_eocd((const uint8_t *)data, size, &cd_offset, &num_entries)) {
        if (status) *status = PAS_ZIP_E_INVALID;
        return NULL;
    }

    pas_zip__handle.data = (const uint8_t *)data;
    pas_zip__handle.size = size;
    pas_zip__handle.cd_offset = cd_offset;
    pas_zip__handle.num_entries = num_entries;

    if (status) *status = PAS_ZIP_OK;
    return &pas_zip__handle;
}

static int parse_cd_entry(const uint8_t *p, size_t end, pas_zip_file_t *out) {
    uint16_t fn_len, extra_len, comment_len;
    size_t need;

    if (p + 46 > (const uint8_t *)end) return 0;
    if (read_u32_le(p) != PAS_ZIP_CDH_SIG) return 0;

    fn_len = read_u16_le(p + 28);
    extra_len = read_u16_le(p + 30);
    comment_len = read_u16_le(p + 32);
    need = 46 + fn_len + extra_len + comment_len;
    if (p + need > (const uint8_t *)end) return 0;

    out->compression_method = read_u16_le(p + 10);
    out->compressed_size = (size_t)read_u32_le(p + 20);
    out->uncompressed_size = (size_t)read_u32_le(p + 24);
    out->local_header_offset = read_u32_le(p + 42);

    if (fn_len >= sizeof(pas_zip__name_buf)) fn_len = (uint16_t)(sizeof(pas_zip__name_buf) - 1);
    memcpy(pas_zip__name_buf, p + 46, fn_len);
    pas_zip__name_buf[fn_len] = '\0';
    out->name = pas_zip__name_buf;

    return 1;
}

static int cd_iterate(pas_zip_t *zip, const char *find_name,
                      pas_zip_file_t *out, void (*cb)(const char *, size_t, void *), void *user) {
    const uint8_t *p = zip->data + zip->cd_offset;
    const uint8_t *end = zip->data + zip->size;
    uint16_t n = zip->num_entries;

    while (n-- && p < end) {
        pas_zip_file_t entry;
        if (!parse_cd_entry(p, end, &entry)) return 0;

        if (cb) {
            cb(entry.name, entry.uncompressed_size, user);
        }
        if (find_name && strcmp(entry.name, find_name) == 0) {
            *out = entry;
            return 1;
        }

        p += 46 + read_u16_le(p + 28) + read_u16_le(p + 30) + read_u16_le(p + 32);
    }
    return find_name ? 0 : 1;
}

pas_zip_file_t *pas_zip_find(pas_zip_t *zip, const char *name) {
    pas_zip_file_t ent;

    if (!zip || !name) return NULL;
    if (!cd_iterate(zip, name, &ent, NULL, NULL)) return NULL;
    pas_zip__current_file = ent;
    return &pas_zip__current_file;
}

const char *pas_zip_name(pas_zip_file_t *file) { return file ? file->name : NULL; }
size_t pas_zip_size(pas_zip_file_t *file) { return file ? file->uncompressed_size : 0; }
int pas_zip_is_compressed(pas_zip_file_t *file) { return file && file->compression_method != PAS_ZIP_METHOD_STORE; }

static size_t skip_local_header(const uint8_t *data, size_t size, uint32_t offset) {
    const uint8_t *p;
    uint16_t fn_len, extra_len;

    if (offset + 30 > size) return 0;
    p = data + offset;
    if (read_u32_le(p) != PAS_ZIP_LFH_SIG) return 0;
    fn_len = read_u16_le(p + 26);
    extra_len = read_u16_le(p + 28);
    return 30 + fn_len + extra_len;
}

size_t pas_zip_extract(pas_zip_file_t *file, void *buffer, size_t buffer_size, pas_zip_status *status) {
    const uint8_t *data;
    size_t data_size;
    size_t payload_offset;

    if (status) *status = PAS_ZIP_E_INVALID;
    if (!file || !buffer) return 0;

    data = pas_zip__handle.data;
    data_size = pas_zip__handle.size;

    payload_offset = skip_local_header(data, data_size, file->local_header_offset);
    if (payload_offset == 0) return 0;
    if (payload_offset + file->compressed_size > data_size) return 0;

    if (buffer_size < file->uncompressed_size) {
        if (status) *status = PAS_ZIP_E_NOSPACE;
        return 0;
    }

    if (file->compression_method == PAS_ZIP_METHOD_STORE) {
        memcpy(buffer, data + payload_offset, file->compressed_size);
        if (status) *status = PAS_ZIP_OK;
        return file->uncompressed_size;
    }

    if (file->compression_method == PAS_ZIP_METHOD_DEFLATE) {
#if defined(PAS_ZIP_USE_MINIZ) || defined(PAS_ZIP_USE_ZLIB)
        unsigned long dest_len = (unsigned long)file->uncompressed_size;
        int r;
#if defined(PAS_ZIP_USE_MINIZ)
        r = mz_uncompress((unsigned char *)buffer, &dest_len,
                          data + payload_offset, (mz_ulong)file->compressed_size);
#else
        r = uncompress((Bytef *)buffer, &dest_len,
                       data + payload_offset, (uLong)file->compressed_size);
#endif
        if (r != 0) {
            if (status) *status = PAS_ZIP_E_ZLIB;
            return 0;
        }
        if (status) *status = PAS_ZIP_OK;
        return (size_t)dest_len;
#else
        if (status) *status = PAS_ZIP_E_COMPRESSED;
        return 0;
#endif
    }

    if (status) *status = PAS_ZIP_E_INVALID;
    return 0;
}

int pas_zip_list(pas_zip_t *zip, void (*callback)(const char *name, size_t size, void *user), void *user) {
    if (!zip || !callback) return -1;
    return cd_iterate(zip, NULL, NULL, callback, user) ? 0 : -1;
}

/* Create ZIP (Store only) */
size_t pas_zip_create(const char **filenames, const void **datas, const size_t *sizes,
                      int file_count, void *buffer, size_t buffer_size, pas_zip_status *status) {
    uint8_t *out = (uint8_t *)buffer;
    size_t written = 0;
    uint32_t cd_offset;
    uint32_t cd_size = 0;
    uint16_t n;
    int i;

    if (status) *status = PAS_ZIP_E_INVALID;
    if (!filenames || !datas || !sizes || !buffer || file_count <= 0) return 0;

    for (i = 0; i < file_count; i++) {
        uint16_t fn_len = (uint16_t)strlen(filenames[i]);
        uint32_t sz = (uint32_t)sizes[i];
        size_t need = 30 + fn_len + sz;
        if (fn_len != strlen(filenames[i])) return 0; /* overflow */
        if (written + need > buffer_size) {
            if (status) *status = PAS_ZIP_E_NOSPACE;
            return 0;
        }
        out[written++] = 0x50; out[written++] = 0x4b; out[written++] = 0x03; out[written++] = 0x04;
        out[written++] = 20; out[written++] = 0;  out[written++] = 0; out[written++] = 0;
        out[written++] = 0; out[written++] = 0;  /* store */
        for (n = 0; n < 12; n++) out[written++] = 0;
        out[written++] = (uint8_t)(sz); out[written++] = (uint8_t)(sz>>8);
        out[written++] = (uint8_t)(sz>>16); out[written++] = (uint8_t)(sz>>24);
        out[written++] = (uint8_t)(sz); out[written++] = (uint8_t)(sz>>8);
        out[written++] = (uint8_t)(sz>>16); out[written++] = (uint8_t)(sz>>24);
        out[written++] = (uint8_t)(fn_len); out[written++] = (uint8_t)(fn_len>>8);
        out[written++] = 0; out[written++] = 0;
        memcpy(out + written, filenames[i], fn_len); written += fn_len;
        memcpy(out + written, datas[i], sz); written += sz;
    }

    cd_offset = (uint32_t)written;
    for (i = 0; i < file_count; i++) {
        uint16_t fn_len = (uint16_t)strlen(filenames[i]);
        uint32_t local_off = 0;
        size_t j;
        for (j = 0; j < (size_t)i; j++)
            local_off += 30 + (uint32_t)strlen(filenames[j]) + (uint32_t)sizes[j];
        if (written + 46 + fn_len > buffer_size) {
            if (status) *status = PAS_ZIP_E_NOSPACE;
            return 0;
        }
        cd_size += 46 + fn_len;
        out[written++] = 0x50; out[written++] = 0x4b; out[written++] = 0x01; out[written++] = 0x02;
        out[written++] = 20; out[written++] = 0; out[written++] = 20; out[written++] = 0;
        out[written++] = 0; out[written++] = 0; out[written++] = 0; out[written++] = 0;
        for (n = 0; n < 12; n++) out[written++] = 0;
        out[written++] = (uint8_t)(fn_len); out[written++] = (uint8_t)(fn_len>>8);
        out[written++] = 0; out[written++] = 0; out[written++] = 0; out[written++] = 0;
        out[written++] = 0; out[written++] = 0; out[written++] = 0; out[written++] = 0;
        out[written++] = 0; out[written++] = 0;
        out[written++] = (uint8_t)(local_off); out[written++] = (uint8_t)(local_off>>8);
        out[written++] = (uint8_t)(local_off>>16); out[written++] = (uint8_t)(local_off>>24);
        memcpy(out + written, filenames[i], fn_len); written += fn_len;
    }

    if (written + 22 > buffer_size) {
        if (status) *status = PAS_ZIP_E_NOSPACE;
        return 0;
    }
    out[written++] = 0x50; out[written++] = 0x4b; out[written++] = 0x05; out[written++] = 0x06;
    for (n = 0; n < 4; n++) out[written++] = 0;
    out[written++] = (uint8_t)file_count; out[written++] = (uint8_t)(file_count>>8);
    out[written++] = (uint8_t)file_count; out[written++] = (uint8_t)(file_count>>8);
    out[written++] = (uint8_t)(cd_size); out[written++] = (uint8_t)(cd_size>>8);
    out[written++] = (uint8_t)(cd_size>>16); out[written++] = (uint8_t)(cd_size>>24);
    out[written++] = (uint8_t)(cd_offset); out[written++] = (uint8_t)(cd_offset>>8);
    out[written++] = (uint8_t)(cd_offset>>16); out[written++] = (uint8_t)(cd_offset>>24);
    out[written++] = 0; out[written++] = 0;

    if (status) *status = PAS_ZIP_OK;
    return written;
}

#endif /* PAS_ZIP_IMPLEMENTATION */

#endif /* PAS_ZIP_H */
