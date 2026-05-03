/* SHA-256 (FIPS 180-4). Reference textbook implementation. */

#include "uc2/uc2_sha256.h"
#include <string.h>

static const uint32_t K[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

static uint32_t r32be(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static void w32be(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)(v >> 24);
	p[1] = (uint8_t)(v >> 16);
	p[2] = (uint8_t)(v >> 8);
	p[3] = (uint8_t)v;
}

static void compress(uint32_t state[8], const uint8_t block[64])
{
	uint32_t w[64];
	for (int i = 0; i < 16; i++)
		w[i] = r32be(block + 4 * i);
	for (int i = 16; i < 64; i++) {
		uint32_t s0 = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
		uint32_t s1 = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}

	uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
	uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

	for (int i = 0; i < 64; i++) {
		uint32_t S1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
		uint32_t ch = (e & f) ^ (~e & g);
		uint32_t t1 = h + S1 + ch + K[i] + w[i];
		uint32_t S0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
		uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
		uint32_t t2 = S0 + mj;
		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	state[0] += a; state[1] += b; state[2] += c; state[3] += d;
	state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void uc2_sha256_init(struct uc2_sha256 *ctx)
{
	ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
	ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
	ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
	ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
	ctx->bitcount = 0;
	ctx->buf_len = 0;
}

void uc2_sha256_update(struct uc2_sha256 *ctx, const void *data, size_t len)
{
	const uint8_t *p = data;
	ctx->bitcount += (uint64_t)len * 8;
	if (ctx->buf_len) {
		size_t take = UC2_SHA256_BLOCK_LEN - ctx->buf_len;
		if (take > len) take = len;
		memcpy(ctx->buf + ctx->buf_len, p, take);
		ctx->buf_len += take;
		p += take;
		len -= take;
		if (ctx->buf_len == UC2_SHA256_BLOCK_LEN) {
			compress(ctx->state, ctx->buf);
			ctx->buf_len = 0;
		}
	}
	while (len >= UC2_SHA256_BLOCK_LEN) {
		compress(ctx->state, p);
		p += UC2_SHA256_BLOCK_LEN;
		len -= UC2_SHA256_BLOCK_LEN;
	}
	if (len) {
		memcpy(ctx->buf, p, len);
		ctx->buf_len = len;
	}
}

void uc2_sha256_final(struct uc2_sha256 *ctx, uint8_t out[UC2_SHA256_OUT_LEN])
{
	uint64_t bits = ctx->bitcount;
	ctx->buf[ctx->buf_len++] = 0x80;
	if (ctx->buf_len > 56) {
		memset(ctx->buf + ctx->buf_len, 0, UC2_SHA256_BLOCK_LEN - ctx->buf_len);
		compress(ctx->state, ctx->buf);
		ctx->buf_len = 0;
	}
	memset(ctx->buf + ctx->buf_len, 0, 56 - ctx->buf_len);
	for (int i = 0; i < 8; i++)
		ctx->buf[56 + i] = (uint8_t)(bits >> (56 - 8 * i));
	compress(ctx->state, ctx->buf);

	for (int i = 0; i < 8; i++)
		w32be(out + 4 * i, ctx->state[i]);
}

void uc2_sha256_hash(const void *data, size_t len, uint8_t out[UC2_SHA256_OUT_LEN])
{
	struct uc2_sha256 ctx;
	uc2_sha256_init(&ctx);
	uc2_sha256_update(&ctx, data, len);
	uc2_sha256_final(&ctx, out);
}
