/* SPDX-License-Identifier: GPL-3.0-or-later */

/* LZ4 block codec. The caller supplies compressed and uncompressed lengths;
 * this API does not read or write LZ4 frame headers. */

#ifndef UC2_LZ4_H
#define UC2_LZ4_H

#include <stdint.h>
#include <stddef.h>

/* Maximum compressed size; 0 if length is unsupported or would overflow. */
static inline size_t uc2_lz4_bound(size_t src_len)
{
	if (src_len > UINT32_MAX || src_len > SIZE_MAX - 16 - src_len / 255) return 0;
	return src_len + src_len / 255 + 16;
}

/* Compress src into dst.  Returns compressed size, or 0 on error.
 * Inputs over UINT32_MAX bytes are rejected. Empty input encodes as one byte.
 * dst must be at least uc2_lz4_bound(src_len) bytes. */
size_t uc2_lz4_compress(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap);

/* Decompress src into dst.  Returns decompressed size, or 0 on error.
 * dst must be large enough for the original data. */
size_t uc2_lz4_decompress(const uint8_t *src, size_t src_len,
                          uint8_t *dst, size_t dst_cap);

#endif
