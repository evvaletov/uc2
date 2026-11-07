/* SPDX-License-Identifier: GPL-3.0-or-later
 * Mutate real method-11 archives, repairing their recipe digest so mutations
 * reach descriptor and codec validation instead of stopping at the table hash.
 * The central directory and all payload hashes remain independently checked. */
#define LLVMFuzzerTestOneInput extract_archive
#include "fuzz_extract.c"
#undef LLVMFuzzerTestOneInput
#include <uc2/uc2_sha256.h>
#include <uc2/uc2_chunks.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > (1u << 20)) return 0;
    uint8_t *copy = malloc(size ? size : 1);
    if (!copy) return 0;
    memcpy(copy, data, size);
    for (size_t i = 29; i + UC2_CHUNK_HEADER <= size; ++i) {
        if (memcmp(copy + i, UC2_CHUNK_MAGIC, 8)) continue;
        const uint8_t *p = copy + i + 8;
        uint32_t count = p[0] | (uint32_t)p[1] << 8 |
                         (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
        if (count > (size - i - UC2_CHUNK_HEADER) / UC2_CHUNK_RECORD) continue;
        struct uc2_sha256 hash;
        uc2_sha256_init(&hash);
        uc2_sha256_update(&hash, copy + i, 56);
        uc2_sha256_update(&hash, copy + i + UC2_CHUNK_HEADER, count * UC2_CHUNK_RECORD);
        uc2_sha256_final(&hash, copy + i + 56);
    }
    extract_archive(copy, size);
    free(copy);
    return 0;
}
