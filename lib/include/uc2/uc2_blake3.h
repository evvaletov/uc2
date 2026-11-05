/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Unkeyed BLAKE3 with a fixed 32-byte digest. This standalone module is
 * not the archive format's checksum. Keyed hashing, KDF and XOF are unsupported.
 * The public context layout is retained for source/ABI compatibility. */

#ifndef UC2_BLAKE3_H
#define UC2_BLAKE3_H

#include <stdint.h>
#include <stddef.h>

#define UC2_BLAKE3_OUT_LEN  32
#define UC2_BLAKE3_BLOCK_LEN 64
#define UC2_BLAKE3_CHUNK_LEN 1024

struct uc2_blake3 {
	uint32_t cv[8];        /* chaining value */
	uint8_t buf[UC2_BLAKE3_BLOCK_LEN];
	uint8_t buf_len;
	uint64_t counter;      /* reserved; unused */
	uint8_t flags;
	/* Stack for tree hashing */
	uint32_t cv_stack[8 * 54]; /* max tree depth */
	uint8_t cv_stack_len;
	uint64_t chunk_counter;
	uint8_t blocks_compressed;
};

/* Initialize hasher. */
void uc2_blake3_init(struct uc2_blake3 *ctx);

/* Feed data to the hasher. */
void uc2_blake3_update(struct uc2_blake3 *ctx, const void *data, size_t len);

/* Produce the digest without changing ctx; updates may continue afterward. */
void uc2_blake3_final(const struct uc2_blake3 *ctx, uint8_t out[UC2_BLAKE3_OUT_LEN]);

/* One-shot hash. */
void uc2_blake3_hash(const void *data, size_t len, uint8_t out[UC2_BLAKE3_OUT_LEN]);

/* Compare all 32 bytes without early exit. Returns 1 if equal. */
int uc2_blake3_equal(const uint8_t a[UC2_BLAKE3_OUT_LEN],
                     const uint8_t b[UC2_BLAKE3_OUT_LEN]);

#endif
