/* SHA-256 (FIPS 180-4) -- pure C implementation.
 *
 * Used by the OpenTimestamps integration; calendars accept SHA-256
 * digests as proof leaves. */

#ifndef UC2_SHA256_H
#define UC2_SHA256_H

#include <stdint.h>
#include <stddef.h>

#define UC2_SHA256_OUT_LEN 32
#define UC2_SHA256_BLOCK_LEN 64

struct uc2_sha256 {
	uint32_t state[8];
	uint64_t bitcount;
	uint8_t buf[UC2_SHA256_BLOCK_LEN];
	size_t buf_len;
};

void uc2_sha256_init(struct uc2_sha256 *ctx);
void uc2_sha256_update(struct uc2_sha256 *ctx, const void *data, size_t len);
void uc2_sha256_final(struct uc2_sha256 *ctx, uint8_t out[UC2_SHA256_OUT_LEN]);
void uc2_sha256_hash(const void *data, size_t len, uint8_t out[UC2_SHA256_OUT_LEN]);

#endif
