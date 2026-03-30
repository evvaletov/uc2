/* BLAKE3 cryptographic hashing for archive integrity.
 *
 * BLAKE3 is a fast cryptographic hash based on the Bao tree hashing
 * mode and the BLAKE2s compression function.  It produces 256-bit
 * (32-byte) digests suitable for content verification, integrity
 * checking, and content-addressable storage.
 *
 * This is a simplified single-threaded implementation (~300 lines).
 * For full BLAKE3 features (keyed hashing, KDF, XOF), see the
 * reference implementation at github.com/BLAKE3-team/BLAKE3.
 *
 * Usage:
 *   struct uc2_blake3 ctx;
 *   uc2_blake3_init(&ctx);
 *   uc2_blake3_update(&ctx, data, len);
 *   uint8_t hash[32];
 *   uc2_blake3_final(&ctx, hash);
 *
 *   // Or one-shot:
 *   uc2_blake3_hash(data, len, hash);
 */

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
	uint64_t counter;
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

/* Finalize and produce hash. */
void uc2_blake3_final(const struct uc2_blake3 *ctx, uint8_t out[UC2_BLAKE3_OUT_LEN]);

/* One-shot hash. */
void uc2_blake3_hash(const void *data, size_t len, uint8_t out[UC2_BLAKE3_OUT_LEN]);

/* Compare two hashes (constant-time). Returns 1 if equal. */
int uc2_blake3_equal(const uint8_t a[UC2_BLAKE3_OUT_LEN],
                     const uint8_t b[UC2_BLAKE3_OUT_LEN]);

#endif
