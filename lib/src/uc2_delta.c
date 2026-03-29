/* Delta compression for file versioning.
 *
 * Uses a hash-based matching approach: hash all 4-byte windows in the
 * source, then scan the target looking for matching regions.  Matched
 * regions become COPY instructions, unmatched regions become INSERT.
 *
 * This is a simplified version of the vcdiff/xdelta algorithm. */

#include "uc2/uc2_delta.h"
#include <stdlib.h>
#include <string.h>

#define HASH_SIZE 65536
#define WINDOW 4
#define MIN_MATCH 8

static uint32_t roll_hash(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t *p, uint32_t v)
{
	p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

static uint32_t get32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Growable output buffer */
struct obuf {
	uint8_t *data;
	size_t len, cap;
};

static int obuf_append(struct obuf *o, const void *data, size_t len)
{
	if (o->len + len > o->cap) {
		size_t newcap = o->cap ? o->cap * 2 : 256;
		while (newcap < o->len + len) newcap *= 2;
		uint8_t *p = realloc(o->data, newcap);
		if (!p) return -1;
		o->data = p;
		o->cap = newcap;
	}
	memcpy(o->data + o->len, data, len);
	o->len += len;
	return 0;
}

static int emit_copy(struct obuf *o, uint32_t offset, uint32_t length)
{
	uint8_t buf[9];
	buf[0] = 0x01;
	put32(buf + 1, offset);
	put32(buf + 5, length);
	return obuf_append(o, buf, 9);
}

static int emit_insert(struct obuf *o, const uint8_t *data, uint32_t length)
{
	uint8_t hdr[5];
	hdr[0] = 0x02;
	put32(hdr + 1, length);
	if (obuf_append(o, hdr, 5) < 0) return -1;
	return obuf_append(o, data, length);
}

int uc2_delta_encode(const uint8_t *src, size_t src_len,
                     const uint8_t *tgt, size_t tgt_len,
                     uint8_t **out_delta, size_t *out_delta_len)
{
	*out_delta = NULL;
	*out_delta_len = 0;

	/* Build hash table of source positions */
	int32_t *htab = calloc(HASH_SIZE, sizeof(int32_t));
	if (!htab) return -1;
	for (size_t i = 0; i < HASH_SIZE; i++) htab[i] = -1;

	if (src_len >= WINDOW) {
		for (size_t i = 0; i <= src_len - WINDOW; i++) {
			uint32_t h = roll_hash(src + i) & (HASH_SIZE - 1);
			htab[h] = (int32_t)i;  /* last occurrence wins */
		}
	}

	struct obuf out = {0};

	/* Header */
	uint8_t hdr[8] = {'U','C','2','D', 0,0,0,0};
	put32(hdr + 4, (uint32_t)tgt_len);
	obuf_append(&out, hdr, 8);

	/* Scan target, emit COPY or INSERT */
	size_t tpos = 0;
	size_t insert_start = 0;
	int in_insert = 0;

	while (tpos + WINDOW <= tgt_len) {
		uint32_t h = roll_hash(tgt + tpos) & (HASH_SIZE - 1);
		int32_t spos = htab[h];

		if (spos >= 0 && (size_t)spos + WINDOW <= src_len &&
		    memcmp(src + spos, tgt + tpos, WINDOW) == 0) {
			/* Extend match forward */
			size_t match_len = WINDOW;
			while (tpos + match_len < tgt_len &&
			       (size_t)spos + match_len < src_len &&
			       src[spos + match_len] == tgt[tpos + match_len])
				match_len++;

			if (match_len >= MIN_MATCH) {
				/* Flush pending insert */
				if (in_insert && tpos > insert_start)
					emit_insert(&out, tgt + insert_start,
					            (uint32_t)(tpos - insert_start));
				in_insert = 0;

				emit_copy(&out, (uint32_t)spos, (uint32_t)match_len);
				tpos += match_len;
				insert_start = tpos;
				continue;
			}
		}

		if (!in_insert) {
			insert_start = tpos;
			in_insert = 1;
		}
		tpos++;
	}

	/* Trailing bytes */
	size_t trailing = tgt_len - tpos;
	if (in_insert || trailing > 0) {
		size_t ins_start = in_insert ? insert_start : tpos;
		size_t ins_len = tgt_len - ins_start;
		if (ins_len > 0)
			emit_insert(&out, tgt + ins_start, (uint32_t)ins_len);
	}

	/* End marker */
	uint8_t end = 0x00;
	obuf_append(&out, &end, 1);

	free(htab);
	*out_delta = out.data;
	*out_delta_len = out.len;
	return 0;
}

int uc2_delta_apply(const uint8_t *src, size_t src_len,
                    const uint8_t *delta, size_t delta_len,
                    uint8_t **out_tgt, size_t *out_tgt_len)
{
	*out_tgt = NULL;
	*out_tgt_len = 0;

	if (delta_len < 9 || memcmp(delta, "UC2D", 4) != 0)
		return -1;

	uint32_t tgt_len = get32(delta + 4);
	uint8_t *tgt = malloc(tgt_len);
	if (!tgt) return -1;

	size_t dpos = 8;  /* after header */
	size_t tpos = 0;

	while (dpos < delta_len) {
		uint8_t op = delta[dpos++];
		if (op == 0x00) break;  /* END */

		if (op == 0x01) {  /* COPY */
			if (dpos + 8 > delta_len) goto err;
			uint32_t offset = get32(delta + dpos); dpos += 4;
			uint32_t length = get32(delta + dpos); dpos += 4;
			if ((size_t)offset + length > src_len) goto err;
			if (tpos + length > tgt_len) goto err;
			memcpy(tgt + tpos, src + offset, length);
			tpos += length;
		} else if (op == 0x02) {  /* INSERT */
			if (dpos + 4 > delta_len) goto err;
			uint32_t length = get32(delta + dpos); dpos += 4;
			if (dpos + length > delta_len) goto err;
			if (tpos + length > tgt_len) goto err;
			memcpy(tgt + tpos, delta + dpos, length);
			dpos += length;
			tpos += length;
		} else goto err;
	}

	*out_tgt = tgt;
	*out_tgt_len = tpos;
	return 0;

err:
	free(tgt);
	return -1;
}
