/* SPDX-License-Identifier: GPL-3.0-or-later */

/* LZ4 block compression with a single-probe match table.
 * End-of-block constraints follow the upstream block format specification:
 * https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md */

#include "uc2/uc2_lz4.h"
#include <string.h>

#define HASH_BITS 16
#define HASH_SIZE (1 << HASH_BITS)
#define MIN_MATCH 4
#define ML_BITS   4
#define ML_MASK   ((1 << ML_BITS) - 1)
#define RUN_BITS  4
#define RUN_MASK  ((1 << RUN_BITS) - 1)

static uint32_t lz4_hash(const uint8_t *p)
{
	uint32_t v = p[0] | ((uint32_t)p[1] << 8) |
	             ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
	return (v * 2654435761U) >> (32 - HASH_BITS);
}

static void write_len(uint8_t **dst, size_t len)
{
	while (len >= 255) {
		*(*dst)++ = 255;
		len -= 255;
	}
	*(*dst)++ = (uint8_t)len;
}

size_t uc2_lz4_compress(const uint8_t *src, size_t src_len,
                        uint8_t *dst, size_t dst_cap)
{
	if (!dst || dst_cap < 1 || (src_len && !src) || src_len > UINT32_MAX) return 0;
	if (src_len == 0) { dst[0] = 0; return 1; }

	uint32_t htab[HASH_SIZE];
	memset(htab, 0, sizeof htab);

	const uint8_t *ip = src;
	const uint8_t *const iend = src + src_len;
	const uint8_t *const mflimit = src_len >= 12 ? iend - 12 : src;
	const uint8_t *anchor = ip;
	uint8_t *op = dst;
	uint8_t *const oend = dst + dst_cap;

	if (src_len < 13) goto emit_last;

	ip++;  /* first byte can't be a match ref */

	for (;;) {
		/* Find a match */
		const uint8_t *ref;
		uint32_t h;

		for (;;) {
			if (ip > mflimit) goto emit_last;
			h = lz4_hash(ip);
			ref = src + htab[h];
			htab[h] = (uint32_t)(ip - src);
			if (ref >= src && ip - ref <= 65535 && ip - ref > 0 &&
			    memcmp(ref, ip, MIN_MATCH) == 0)
				break;
			ip++;
		}

		/* Encode literal run before match */
		size_t lit_len = (size_t)(ip - anchor);
		size_t match_len = MIN_MATCH;

		/* Extend match forward */
		while (match_len < (size_t)(iend - ip) - 5 && ref[match_len] == ip[match_len])
			match_len++;

		/* Emit token */
		size_t overhead = 3 + (lit_len > 14 ? lit_len/255 + 1 : 0) +
		    (match_len - MIN_MATCH > 14 ? (match_len - MIN_MATCH)/255 + 1 : 0);
		if (lit_len > (size_t)(oend - op) || overhead > (size_t)(oend - op) - lit_len)
			return 0;  /* output overflow */

		uint8_t *token = op++;
		size_t ll = lit_len < 15 ? lit_len : 15;
		size_t ml = (match_len - MIN_MATCH) < 15 ? (match_len - MIN_MATCH) : 15;
		*token = (uint8_t)((ll << 4) | ml);

		if (lit_len >= 15)
			write_len(&op, lit_len - 15);
		memcpy(op, anchor, lit_len);
		op += lit_len;

		/* Offset (16-bit LE) */
		uint16_t offset = (uint16_t)(ip - ref);
		*op++ = (uint8_t)(offset & 0xFF);
		*op++ = (uint8_t)(offset >> 8);

		if (match_len - MIN_MATCH >= 15)
			write_len(&op, match_len - MIN_MATCH - 15);

		ip += match_len;
		anchor = ip;

		if (ip > mflimit) goto emit_last;

		/* Hash the positions we skipped */
		htab[lz4_hash(ip - 2)] = (uint32_t)(ip - 2 - src);
	}

emit_last:;
	/* Emit final literal run */
	size_t last_lit = (size_t)(iend - anchor);
	if (last_lit > (size_t)(oend - op) ||
	    1 + (last_lit > 14 ? last_lit/255 + 1 : 0) > (size_t)(oend - op) - last_lit)
		return 0;

	uint8_t *token = op++;
	size_t ll = last_lit < 15 ? last_lit : 15;
	*token = (uint8_t)(ll << 4);  /* match_len = 0 (no match) */
	if (last_lit >= 15)
		write_len(&op, last_lit - 15);
	memcpy(op, anchor, last_lit);
	op += last_lit;

	return (size_t)(op - dst);
}

size_t uc2_lz4_decompress(const uint8_t *src, size_t src_len,
                          uint8_t *dst, size_t dst_cap)
{
	if (!src || !dst || !src_len) return 0;
	const uint8_t *ip = src;
	const uint8_t *const iend = src + src_len;
	uint8_t *op = dst;
	uint8_t *const oend = dst + dst_cap;

	while (ip < iend) {
		uint8_t token = *ip++;

		/* Literal length */
		size_t lit_len = token >> 4;
		if (lit_len == 15) {
			uint8_t b;
			do {
				if (ip >= iend) return 0;
				b = *ip++;
				if (lit_len > SIZE_MAX - b) return 0;
				lit_len += b;
			} while (b == 255);
		}

		/* Copy literals */
		if (lit_len > (size_t)(iend - ip) || lit_len > (size_t)(oend - op)) return 0;
		memcpy(op, ip, lit_len);
		ip += lit_len;
		op += lit_len;

		if (ip >= iend) break;  /* end of stream (last token has no match) */

		/* Match offset */
		if ((size_t)(iend - ip) < 2) return 0;
		uint16_t offset = ip[0] | ((uint16_t)ip[1] << 8);
		ip += 2;
		if (offset == 0 || op - dst < offset) return 0;

		/* Match length */
		size_t match_len = (token & ML_MASK) + MIN_MATCH;
		if ((token & ML_MASK) == ML_MASK) {
			uint8_t b;
			do {
				if (ip >= iend) return 0;
				b = *ip++;
				if (match_len > SIZE_MAX - b) return 0;
				match_len += b;
			} while (b == 255);
		}

		/* Copy match */
		if (match_len > (size_t)(oend - op)) return 0;
		const uint8_t *ref = op - offset;
		for (size_t i = 0; i < match_len; i++)
			op[i] = ref[i];  /* byte-by-byte for overlapping matches */
		op += match_len;
	}

	return (size_t)(op - dst);
}
