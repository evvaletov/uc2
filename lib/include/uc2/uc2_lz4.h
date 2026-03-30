/* LZ4-compatible ultra-fast compression.
 *
 * Minimal LZ4-like compressor optimized for speed over ratio.
 * Uses a single-probe hash table (no chains) for O(1) match finding.
 * Suitable for real-time or low-resource scenarios where decompression
 * speed is critical and compression ratio is secondary.
 *
 * Format: sequence of literal/match tokens:
 *   [token] [literal_length_ext?] [literals] [offset:16LE] [match_length_ext?]
 *   token = (literal_len:4 << 4) | match_len:4
 *   If literal_len == 15: read additional bytes until < 255
 *   If match_len == 15: read additional bytes until < 255
 *   Match lengths are +4 (minimum match = 4)
 *
 * Usage:
 *   size_t bound = uc2_lz4_bound(src_len);
 *   uint8_t *dst = malloc(bound);
 *   size_t clen = uc2_lz4_compress(src, src_len, dst, bound);
 *   size_t dlen = uc2_lz4_decompress(dst, clen, out, out_cap);
 */

#ifndef UC2_LZ4_H
#define UC2_LZ4_H

#include <stdint.h>
#include <stddef.h>

/* Maximum compressed size for a given input length. */
static inline size_t uc2_lz4_bound(size_t src_len)
{
	return src_len + src_len / 255 + 16;
}

/* Compress src into dst.  Returns compressed size, or 0 on error.
 * dst must be at least uc2_lz4_bound(src_len) bytes. */
size_t uc2_lz4_compress(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap);

/* Decompress src into dst.  Returns decompressed size, or 0 on error.
 * dst must be large enough for the original data. */
size_t uc2_lz4_decompress(const uint8_t *src, size_t src_len,
                          uint8_t *dst, size_t dst_cap);

#endif
