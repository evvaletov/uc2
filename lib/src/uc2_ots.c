/* SPDX-License-Identifier: GPL-3.0-or-later */

/* OpenTimestamps proof parser, serializer, walker, and UC2 trailer.
 *
 * The walker supports the calendar-path subset of opcodes (APPEND,
 * PREPEND, SHA256) directly.  Other unary crypto ops (SHA1, RIPEMD160,
 * KECCAK256) are accepted as structurally valid but flagged as not
 * locally cryptographically verified; for full validation, extract
 * the proof and run the standard `ots verify` tool. */

#include "uc2/uc2_ots.h"
#include "uc2/uc2_sha256.h"
#include <string.h>

static uint32_t r32le(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void w32le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

int uc2_ots_varint_decode(const uint8_t *in, size_t in_len,
                          uint64_t *out_value, size_t *consumed)
{
	uint64_t v = 0;
	int shift = 0;
	size_t i = 0;
	for (;;) {
		if (i >= in_len) return UC2_OTS_ERR_TRUNCATED;
		if (shift >= 64) return UC2_OTS_ERR_OVERFLOW;
		uint8_t b = in[i++];
		uint8_t group = b & 0x7f;
		/* At shift == 63 only payloads of 0 or 1 fit in 64 bits;
		 * anything larger would silently lose its high bits. */
		if (shift == 63 && group > 1)
			return UC2_OTS_ERR_OVERFLOW;
		v |= (uint64_t)group << shift;
		if (!(b & 0x80)) {
			/* Canonical: a multi-byte encoding must not have a zero
			 * high group, i.e. the last byte cannot be 0x00 unless
			 * the value is zero in a single byte. */
			if (i > 1 && b == 0)
				return UC2_OTS_ERR_NONCANONICAL;
			*out_value = v;
			*consumed = i;
			return UC2_OTS_OK;
		}
		shift += 7;
	}
}

size_t uc2_ots_varint_encode(uint64_t value, uint8_t out[10])
{
	size_t i = 0;
	while (value >= 0x80) {
		out[i++] = (uint8_t)(value | 0x80);
		value >>= 7;
	}
	out[i++] = (uint8_t)value;
	return i;
}

/* Read a "varbytes" field: varint length + that many bytes. */
static int read_varbytes(const uint8_t *p, size_t len,
                         const uint8_t **out_data, size_t *out_data_len,
                         size_t *consumed)
{
	uint64_t n;
	size_t lc;
	int rc = uc2_ots_varint_decode(p, len, &n, &lc);
	if (rc < 0) return rc;
	if (n > UC2_OTS_MAX_VARBYTES) return UC2_OTS_ERR_TOO_LARGE;
	if (n > len - lc) return UC2_OTS_ERR_TRUNCATED;
	*out_data = p + lc;
	*out_data_len = (size_t)n;
	*consumed = lc + (size_t)n;
	return UC2_OTS_OK;
}

int uc2_ots_parse_file(const uint8_t *file, size_t file_len,
                       uint8_t *out_hash_op,
                       const uint8_t **out_leaf_digest,
                       size_t *out_leaf_digest_len,
                       const uint8_t **out_body,
                       size_t *out_body_len)
{
	if (file_len < UC2_OTS_HEADER_MAGIC_LEN + 1 + 1 + 32)
		return UC2_OTS_ERR_TRUNCATED;
	if (memcmp(file, UC2_OTS_HEADER_MAGIC, UC2_OTS_HEADER_MAGIC_LEN) != 0)
		return UC2_OTS_ERR_BAD_MAGIC;
	size_t off = UC2_OTS_HEADER_MAGIC_LEN;
	if (file[off++] != UC2_OTS_VERSION)
		return UC2_OTS_ERR_BAD_VERSION;
	uint8_t hash_op = file[off++];
	size_t digest_len;
	switch (hash_op) {
	case UC2_OTS_OP_SHA1:      digest_len = 20; break;
	case UC2_OTS_OP_RIPEMD160: digest_len = 20; break;
	case UC2_OTS_OP_SHA256:    digest_len = 32; break;
	case UC2_OTS_OP_KECCAK256: digest_len = 32; break;
	default: return UC2_OTS_ERR_BAD_HASH_OP;
	}
	if (file_len - off < digest_len)
		return UC2_OTS_ERR_TRUNCATED;
	*out_hash_op = hash_op;
	*out_leaf_digest = file + off;
	*out_leaf_digest_len = digest_len;
	off += digest_len;
	*out_body = file + off;
	*out_body_len = file_len - off;
	return UC2_OTS_OK;
}

int uc2_ots_serialize_file(uint8_t hash_op,
                           const uint8_t *leaf_digest, size_t leaf_digest_len,
                           const uint8_t *body, size_t body_len,
                           uint8_t *out, size_t out_cap)
{
	size_t want_len;
	switch (hash_op) {
	case UC2_OTS_OP_SHA1:      want_len = 20; break;
	case UC2_OTS_OP_RIPEMD160: want_len = 20; break;
	case UC2_OTS_OP_SHA256:    want_len = 32; break;
	case UC2_OTS_OP_KECCAK256: want_len = 32; break;
	default: return UC2_OTS_ERR_BAD_HASH_OP;
	}
	if (leaf_digest_len != want_len) return UC2_OTS_ERR_BAD_HASH_OP;
	size_t need = UC2_OTS_HEADER_MAGIC_LEN + 1 + 1 + leaf_digest_len + body_len;
	if (need > out_cap) return UC2_OTS_ERR_TRUNCATED;
	uint8_t *p = out;
	memcpy(p, UC2_OTS_HEADER_MAGIC, UC2_OTS_HEADER_MAGIC_LEN);
	p += UC2_OTS_HEADER_MAGIC_LEN;
	*p++ = UC2_OTS_VERSION;
	*p++ = hash_op;
	memcpy(p, leaf_digest, leaf_digest_len);
	p += leaf_digest_len;
	memcpy(p, body, body_len);
	p += body_len;
	return (int)(p - out);
}

/* A serialized timestamp is a sequence of "items"; each item is either
 *   (attestation)  0x00 + tag(8) + varbytes(payload)
 *   (op)           op-byte + (varbytes operand for binary ops) + child-timestamp
 *
 * Within one timestamp node, items are separated by 0xff: every item
 * except the LAST is preceded by 0xff.  Children timestamps recurse
 * the same structure with the digest produced by their parent op. */

struct walker {
	const uint8_t *p, *end;
	uc2_ots_attest_cb cb;
	void *ctx;
	int has_unsupported_op;
};

/* Apply an op to `digest`, consuming a varbytes operand for binary ops.
 * Supported ops update the digest in place; unsupported unary ops set
 * has_unsupported_op and leave the digest unchanged so the structural
 * walk can continue. */
static int apply_op(struct walker *w, uint8_t op,
                    uint8_t *digest, size_t *digest_len)
{
	switch (op) {
	case UC2_OTS_OP_APPEND:
	case UC2_OTS_OP_PREPEND: {
		const uint8_t *operand;
		size_t operand_len, consumed;
		int rc = read_varbytes(w->p, (size_t)(w->end - w->p),
		                       &operand, &operand_len, &consumed);
		if (rc < 0) return rc;
		w->p += consumed;
		if (*digest_len + operand_len > UC2_OTS_MAX_DIGEST_LEN)
			return UC2_OTS_ERR_TOO_LARGE;
		if (op == UC2_OTS_OP_APPEND) {
			memcpy(digest + *digest_len, operand, operand_len);
		} else {
			memmove(digest + operand_len, digest, *digest_len);
			memcpy(digest, operand, operand_len);
		}
		*digest_len += operand_len;
		return UC2_OTS_OK;
	}
	case UC2_OTS_OP_SHA256: {
		uint8_t out[UC2_SHA256_OUT_LEN];
		uc2_sha256_hash(digest, *digest_len, out);
		memcpy(digest, out, UC2_SHA256_OUT_LEN);
		*digest_len = UC2_SHA256_OUT_LEN;
		return UC2_OTS_OK;
	}
	case UC2_OTS_OP_SHA1:
	case UC2_OTS_OP_RIPEMD160:
	case UC2_OTS_OP_KECCAK256:
	case UC2_OTS_OP_REVERSE:
	case UC2_OTS_OP_HEXLIFY:
		w->has_unsupported_op = 1;
		return UC2_OTS_OK;
	default:
		return UC2_OTS_ERR_BAD_OP;
	}
}

static int walk_attestation(struct walker *w,
                            const uint8_t *digest, size_t digest_len)
{
	if (w->end - w->p < UC2_OTS_TAG_LEN) return UC2_OTS_ERR_TRUNCATED;
	const uint8_t *tag = w->p;
	w->p += UC2_OTS_TAG_LEN;
	const uint8_t *payload;
	size_t payload_len, consumed;
	int rc = read_varbytes(w->p, (size_t)(w->end - w->p),
	                       &payload, &payload_len, &consumed);
	if (rc < 0) return rc;
	w->p += consumed;
	if (w->cb && w->cb(w->ctx, tag, payload, payload_len, digest, digest_len))
		return UC2_OTS_ERR_OVERFLOW;
	return UC2_OTS_OK;
}

static int walk_node(struct walker *w,
                     const uint8_t *digest_in, size_t digest_in_len,
                     int depth)
{
	if (depth >= UC2_OTS_MAX_DEPTH) return UC2_OTS_ERR_DEPTH;

	for (;;) {
		if (w->p >= w->end) return UC2_OTS_ERR_TRUNCATED;
		uint8_t b = *w->p++;
		int is_last = (b != UC2_OTS_BRANCH);
		if (!is_last) {
			if (w->p >= w->end) return UC2_OTS_ERR_TRUNCATED;
			b = *w->p++;
		}

		if (b == UC2_OTS_ATTESTATION) {
			int rc = walk_attestation(w, digest_in, digest_in_len);
			if (rc < 0) return rc;
		} else {
			/* Op item: snapshot digest into a local buffer (siblings
			 * within the same node share the parent digest), apply
			 * the op, recurse into the sub-timestamp. */
			uint8_t mut[UC2_OTS_MAX_DIGEST_LEN];
			size_t mut_len = digest_in_len;
			memcpy(mut, digest_in, digest_in_len);
			int rc = apply_op(w, b, mut, &mut_len);
			if (rc < 0) return rc;
			rc = walk_node(w, mut, mut_len, depth + 1);
			if (rc < 0) return rc;
		}

		if (is_last) return UC2_OTS_OK;
	}
}

int uc2_ots_walk(const uint8_t *body, size_t body_len,
                 const uint8_t *leaf_digest, size_t leaf_digest_len,
                 uc2_ots_attest_cb cb, void *ctx)
{
	if (leaf_digest_len > UC2_OTS_MAX_DIGEST_LEN)
		return UC2_OTS_ERR_TOO_LARGE;

	struct walker w = { body, body + body_len, cb, ctx, 0 };
	int rc = walk_node(&w, leaf_digest, leaf_digest_len, 0);
	if (rc < 0) return rc;
	if (w.p != w.end) return UC2_OTS_ERR_OVERFLOW;
	return w.has_unsupported_op ? UC2_OTS_RESULT_STRUCTURAL
	                            : UC2_OTS_RESULT_VERIFIED;
}

const char *uc2_ots_attest_name(const uint8_t tag[UC2_OTS_TAG_LEN])
{
	if (memcmp(tag, UC2_OTS_TAG_PENDING, UC2_OTS_TAG_LEN) == 0)
		return "pending";
	if (memcmp(tag, UC2_OTS_TAG_BITCOIN, UC2_OTS_TAG_LEN) == 0)
		return "Bitcoin";
	if (memcmp(tag, UC2_OTS_TAG_LITECOIN, UC2_OTS_TAG_LEN) == 0)
		return "Litecoin";
	return 0;
}

int uc2_ots_trailer_build(uint32_t archive_len,
                          const uint8_t *proof, size_t proof_len,
                          uint8_t *out, size_t out_cap)
{
	if (proof_len > UC2_OTS_TRAILER_MAX_PROOF)
		return UC2_OTS_ERR_TOO_LARGE;
	size_t total = UC2_OTS_TRAILER_OVERHEAD + proof_len;
	if (total > out_cap) return UC2_OTS_ERR_TRUNCATED;
	uint8_t *p = out;
	memcpy(p, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN);
	p += UC2_OTS_TRAILER_MAGIC_LEN;
	w32le(p, UC2_OTS_TRAILER_VERSION); p += 4;
	w32le(p, archive_len);             p += 4;
	w32le(p, (uint32_t)proof_len);     p += 4;
	memcpy(p, proof, proof_len);       p += proof_len;
	w32le(p, (uint32_t)proof_len);     p += 4;
	memcpy(p, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN);
	p += UC2_OTS_TRAILER_MAGIC_LEN;
	return (int)(p - out);
}

int uc2_ots_trailer_parse(const uint8_t *file, size_t file_len,
                          uint32_t *out_archive_len,
                          const uint8_t **out_proof, size_t *out_proof_len)
{
	if (file_len < UC2_OTS_TRAILER_TAIL_LEN) return 1;
	const uint8_t *back = file + file_len - UC2_OTS_TRAILER_MAGIC_LEN;
	if (memcmp(back, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN) != 0)
		return 1;

	/* Back magic present: from here on, every check is hard-failed. */
	uint32_t back_proof_len = r32le(file + file_len - UC2_OTS_TRAILER_TAIL_LEN);
	if (back_proof_len > UC2_OTS_TRAILER_MAX_PROOF)
		return UC2_OTS_ERR_TOO_LARGE;

	size_t total = UC2_OTS_TRAILER_OVERHEAD + back_proof_len;
	if (total > file_len) return UC2_OTS_ERR_TRUNCATED;
	const uint8_t *trailer_start = file + file_len - total;

	if (memcmp(trailer_start, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN) != 0)
		return UC2_OTS_ERR_BAD_MAGIC;

	uint32_t version    = r32le(trailer_start + UC2_OTS_TRAILER_MAGIC_LEN);
	uint32_t archive_ln = r32le(trailer_start + UC2_OTS_TRAILER_MAGIC_LEN + 4);
	uint32_t front_pl   = r32le(trailer_start + UC2_OTS_TRAILER_MAGIC_LEN + 8);

	if (version != UC2_OTS_TRAILER_VERSION) return UC2_OTS_ERR_BAD_VERSION;
	if (front_pl != back_proof_len) return UC2_OTS_ERR_NONCANONICAL;
	if ((size_t)archive_ln != (size_t)(trailer_start - file))
		return UC2_OTS_ERR_OVERFLOW;

	*out_archive_len = archive_ln;
	*out_proof = trailer_start + UC2_OTS_TRAILER_HEAD_LEN;
	*out_proof_len = back_proof_len;
	return UC2_OTS_OK;
}
