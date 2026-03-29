/* Content-defined chunking (CDC) for UC2 deduplication.
 *
 * Uses the Gear rolling hash for fast, content-aware chunk boundary
 * detection.  Gear hash is a simple multiplicative hash that XORs each
 * byte with a pre-computed random table, giving O(1) per-byte updates.
 *
 * Typical usage:
 *   struct uc2_chunker c;
 *   uc2_chunker_init(&c, 13);  // avg chunk ~8KB (2^13)
 *   while (uc2_chunker_next(&c, data, len, &chunk_off, &chunk_len))
 *       process(data + chunk_off, chunk_len);
 */

#ifndef UC2_CDC_H
#define UC2_CDC_H

#include <stdint.h>
#include <stddef.h>

/* Gear hash: fast rolling hash with O(1) per-byte update. */
uint32_t uc2_gear_hash(const uint8_t *data, size_t len);

/* CDC chunker state. */
struct uc2_chunker {
	uint32_t mask;       /* boundary mask: (1 << bits) - 1 */
	size_t   min_chunk;  /* minimum chunk size */
	size_t   max_chunk;  /* maximum chunk size */
	size_t   pos;        /* current position in data */
};

/* Initialize chunker.
 *   bits:      target chunk size exponent (avg chunk = 2^bits bytes).
 *              Recommended: 13 (8KB), 14 (16KB), or 15 (32KB).
 *   min_chunk: minimum chunk size (0 = bits-2 default)
 *   max_chunk: maximum chunk size (0 = bits+2 default) */
void uc2_chunker_init(struct uc2_chunker *c, int bits,
                      size_t min_chunk, size_t max_chunk);

/* Find the next chunk boundary in [data, data+len).
 * Returns 1 and sets *chunk_len if a chunk was found.
 * Returns 0 when all data has been consumed (final chunk).
 * Call repeatedly until it returns 0. */
int uc2_chunker_next(struct uc2_chunker *c,
                     const uint8_t *data, size_t len,
                     size_t *chunk_off, size_t *chunk_len);

/* Reset chunker for a new data stream. */
void uc2_chunker_reset(struct uc2_chunker *c);

/* FNV-1a hash for chunk content addressing. */
uint32_t uc2_fnv1a(const uint8_t *data, size_t len);

#endif
