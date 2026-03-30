/* BLAKE3 cryptographic hashing — simplified single-threaded implementation.
 *
 * Based on the BLAKE3 specification (github.com/BLAKE3-team/BLAKE3).
 * Uses the BLAKE2s round function with Bao tree structure.
 *
 * This implementation handles the common case (single chunk, sequential
 * hashing) and supports the tree structure for inputs > 1024 bytes. */

#include "uc2/uc2_blake3.h"
#include <string.h>

/* BLAKE3 IV (same as BLAKE2s) */
static const uint32_t IV[8] = {
	0x6A09E667, 0xBB67AE85, 0x3C6EF372, 0xA54FF53A,
	0x510E527F, 0x9B05688C, 0x1F83D9AB, 0x5BE0CD19,
};

/* Flags */
enum {
	CHUNK_START         = 1 << 0,
	CHUNK_END           = 1 << 1,
	PARENT              = 1 << 2,
	ROOT                = 1 << 3,
};

/* Message schedule (BLAKE3 permutation) */
static const uint8_t MSG_SCHEDULE[7][16] = {
	{0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},
	{2,6,3,10,7,0,4,13,1,11,12,5,9,14,15,8},
	{3,4,10,12,13,2,7,14,6,5,9,0,11,15,8,1},
	{10,7,12,9,14,3,13,15,4,0,11,2,5,8,1,6},
	{12,13,9,11,15,10,14,8,7,2,5,3,0,1,6,4},
	{9,14,11,5,8,12,15,1,13,3,0,10,2,6,4,7},
	{11,15,5,0,1,9,8,6,14,10,2,12,3,4,7,13},
};

static uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static void g(uint32_t *s, int a, int b, int c, int d, uint32_t mx, uint32_t my)
{
	s[a] = s[a] + s[b] + mx; s[d] = rotr(s[d] ^ s[a], 16);
	s[c] = s[c] + s[d];      s[b] = rotr(s[b] ^ s[c], 12);
	s[a] = s[a] + s[b] + my; s[d] = rotr(s[d] ^ s[a], 8);
	s[c] = s[c] + s[d];      s[b] = rotr(s[b] ^ s[c], 7);
}

static void round_fn(uint32_t *s, const uint32_t *m)
{
	g(s,0,4, 8,12,m[0],m[1]); g(s,1,5, 9,13,m[2],m[3]);
	g(s,2,6,10,14,m[4],m[5]); g(s,3,7,11,15,m[6],m[7]);
	g(s,0,5,10,15,m[8],m[9]); g(s,1,6,11,12,m[10],m[11]);
	g(s,2,7, 8,13,m[12],m[13]); g(s,3,4,9,14,m[14],m[15]);
}

static void compress(const uint32_t cv[8], const uint8_t block[64],
                     uint8_t block_len, uint64_t counter, uint8_t flags,
                     uint32_t out[16])
{
	uint32_t m[16];
	for (int i = 0; i < 16; i++)
		m[i] = (uint32_t)block[i*4] | ((uint32_t)block[i*4+1]<<8) |
		       ((uint32_t)block[i*4+2]<<16) | ((uint32_t)block[i*4+3]<<24);

	uint32_t s[16] = {
		cv[0],cv[1],cv[2],cv[3],cv[4],cv[5],cv[6],cv[7],
		IV[0],IV[1],IV[2],IV[3],
		(uint32_t)counter, (uint32_t)(counter>>32),
		block_len, flags
	};

	for (int r = 0; r < 7; r++) {
		uint32_t pm[16];
		for (int i = 0; i < 16; i++) pm[i] = m[MSG_SCHEDULE[r][i]];
		round_fn(s, pm);
	}

	for (int i = 0; i < 8; i++) out[i] = s[i] ^ s[i+8];
	for (int i = 8; i < 16; i++) out[i] = s[i] ^ cv[i-8];
}

static void cv_from_out(const uint32_t out[16], uint32_t cv[8])
{
	for (int i = 0; i < 8; i++) cv[i] = out[i];
}

/* Process one block within a chunk */
static void chunk_block(struct uc2_blake3 *ctx, const uint8_t block[64],
                        uint8_t block_len, uint8_t extra_flags)
{
	uint8_t flags = ctx->flags | extra_flags;
	if (ctx->blocks_compressed == 0) flags |= CHUNK_START;

	uint32_t out[16];
	compress(ctx->cv, block, block_len, ctx->chunk_counter, flags, out);
	cv_from_out(out, ctx->cv);
	ctx->blocks_compressed++;
}

/* Finalize a chunk: compress the last block with CHUNK_END */
static void chunk_finalize(struct uc2_blake3 *ctx, uint32_t cv_out[8])
{
	uint8_t flags = ctx->flags | CHUNK_END;
	if (ctx->blocks_compressed == 0) flags |= CHUNK_START;

	uint8_t block[64];
	memset(block, 0, 64);
	memcpy(block, ctx->buf, ctx->buf_len);

	uint32_t out[16];
	compress(ctx->cv, block, ctx->buf_len, ctx->chunk_counter, flags, out);
	cv_from_out(out, cv_out);
}

/* Merge two chaining values as a parent node */
static void parent_cv(const uint32_t left[8], const uint32_t right[8],
                      uint32_t out_cv[8])
{
	uint8_t block[64];
	for (int i = 0; i < 8; i++) {
		block[i*4]   = (uint8_t)(left[i]);
		block[i*4+1] = (uint8_t)(left[i]>>8);
		block[i*4+2] = (uint8_t)(left[i]>>16);
		block[i*4+3] = (uint8_t)(left[i]>>24);
	}
	for (int i = 0; i < 8; i++) {
		block[32+i*4]   = (uint8_t)(right[i]);
		block[32+i*4+1] = (uint8_t)(right[i]>>8);
		block[32+i*4+2] = (uint8_t)(right[i]>>16);
		block[32+i*4+3] = (uint8_t)(right[i]>>24);
	}
	uint32_t out[16];
	compress(IV, block, 64, 0, PARENT, out);
	cv_from_out(out, out_cv);
}

static void push_cv(struct uc2_blake3 *ctx, const uint32_t cv[8])
{
	/* Merge with stack entries that have matching tree levels */
	uint32_t new_cv[8];
	memcpy(new_cv, cv, 32);
	uint64_t total = ctx->chunk_counter;
	while (total & 1) {
		ctx->cv_stack_len--;
		parent_cv(&ctx->cv_stack[ctx->cv_stack_len * 8], new_cv, new_cv);
		total >>= 1;
	}
	memcpy(&ctx->cv_stack[ctx->cv_stack_len * 8], new_cv, 32);
	ctx->cv_stack_len++;
}

void uc2_blake3_init(struct uc2_blake3 *ctx)
{
	memset(ctx, 0, sizeof *ctx);
	memcpy(ctx->cv, IV, 32);
}

void uc2_blake3_update(struct uc2_blake3 *ctx, const void *data, size_t len)
{
	const uint8_t *p = data;
	while (len > 0) {
		/* If buffer has a full block, process it */
		if (ctx->buf_len == 64) {
			chunk_block(ctx, ctx->buf, 64, 0);
			ctx->buf_len = 0;

			/* If we've filled a full chunk (1024 bytes = 16 blocks),
			   finalize this chunk and start a new one */
			if (ctx->blocks_compressed == 16) {
				/* This was the 16th block; we need to finalize with the
				   PREVIOUS block as the last, and this leftover starts a
				   new chunk.  Actually, we process blocks as they come
				   and finalize when the chunk is complete. */
			}
		}

		/* Check if we're at a chunk boundary */
		size_t chunk_bytes = (size_t)ctx->blocks_compressed * 64 + ctx->buf_len;
		if (chunk_bytes >= UC2_BLAKE3_CHUNK_LEN && ctx->buf_len == 0 &&
		    ctx->blocks_compressed > 0) {
			/* Finalize current chunk — but we've already processed all
			   blocks.  The last block was a full block, so re-compress
			   it with CHUNK_END. */
			/* Start new chunk */
			uint32_t chunk_cv[8];
			/* Recompute final block with CHUNK_END */
			chunk_finalize(ctx, chunk_cv);
			push_cv(ctx, chunk_cv);

			ctx->chunk_counter++;
			memcpy(ctx->cv, IV, 32);
			ctx->blocks_compressed = 0;
			ctx->flags = 0;
		}

		size_t take = 64 - ctx->buf_len;
		if (take > len) take = len;
		memcpy(ctx->buf + ctx->buf_len, p, take);
		ctx->buf_len += (uint8_t)take;
		p += take;
		len -= take;
	}
}

void uc2_blake3_final(const struct uc2_blake3 *ctx, uint8_t out[UC2_BLAKE3_OUT_LEN])
{
	/* Finalize current chunk */
	uint32_t chunk_cv[8];
	struct uc2_blake3 tmp = *ctx;

	/* If this is the only chunk, it gets ROOT flag */
	if (tmp.chunk_counter == 0 && tmp.cv_stack_len == 0) {
		uint8_t flags = tmp.flags | CHUNK_START | CHUNK_END | ROOT;
		uint8_t block[64];
		memset(block, 0, 64);
		memcpy(block, tmp.buf, tmp.buf_len);
		uint32_t result[16];
		compress(tmp.cv, block, tmp.buf_len, 0, flags, result);
		for (int i = 0; i < 8; i++) {
			out[i*4]   = (uint8_t)(result[i]);
			out[i*4+1] = (uint8_t)(result[i]>>8);
			out[i*4+2] = (uint8_t)(result[i]>>16);
			out[i*4+3] = (uint8_t)(result[i]>>24);
		}
		return;
	}

	/* Multi-chunk: finalize current chunk */
	chunk_finalize(&tmp, chunk_cv);

	/* Merge with stack */
	uint32_t cv[8];
	memcpy(cv, chunk_cv, 32);
	for (int i = (int)tmp.cv_stack_len - 1; i >= 0; i--) {
		uint32_t merged[8];
		parent_cv(&tmp.cv_stack[i * 8], cv, merged);
		memcpy(cv, merged, 32);
	}

	/* Output with ROOT flag */
	uint8_t block[64];
	memset(block, 0, 64);
	for (int i = 0; i < 8; i++) {
		block[i*4]   = (uint8_t)(cv[i]);
		block[i*4+1] = (uint8_t)(cv[i]>>8);
		block[i*4+2] = (uint8_t)(cv[i]>>16);
		block[i*4+3] = (uint8_t)(cv[i]>>24);
	}
	uint32_t result[16];
	compress(IV, block, 32, 0, PARENT | ROOT, result);
	for (int i = 0; i < 8; i++) {
		out[i*4]   = (uint8_t)(result[i]);
		out[i*4+1] = (uint8_t)(result[i]>>8);
		out[i*4+2] = (uint8_t)(result[i]>>16);
		out[i*4+3] = (uint8_t)(result[i]>>24);
	}
}

void uc2_blake3_hash(const void *data, size_t len, uint8_t out[UC2_BLAKE3_OUT_LEN])
{
	struct uc2_blake3 ctx;
	uc2_blake3_init(&ctx);
	uc2_blake3_update(&ctx, data, len);
	uc2_blake3_final(&ctx, out);
}

int uc2_blake3_equal(const uint8_t a[UC2_BLAKE3_OUT_LEN],
                     const uint8_t b[UC2_BLAKE3_OUT_LEN])
{
	uint8_t diff = 0;
	for (int i = 0; i < UC2_BLAKE3_OUT_LEN; i++)
		diff |= a[i] ^ b[i];
	return diff == 0;
}
