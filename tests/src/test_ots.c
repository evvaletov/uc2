/* Tests for the OpenTimestamps integration. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_ots.h>
#include <uc2/uc2_sha256.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_varint_roundtrip(void)
{
	uint64_t cases[] = { 0, 1, 0x7f, 0x80, 0xff, 0x3fff, 0x4000,
	                     0xffff, 0x1fffff, 0xfffffffful, 0x123456789aULL };
	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		uint8_t buf[10];
		size_t n = uc2_ots_varint_encode(cases[i], buf);
		uint64_t got;
		size_t consumed;
		assert(uc2_ots_varint_decode(buf, n, &got, &consumed) == UC2_OTS_OK);
		assert(got == cases[i]);
		assert(consumed == n);
	}
}

static void test_varint_truncated(void)
{
	uint8_t buf[] = { 0x80, 0x80 }; /* unterminated */
	uint64_t v;
	size_t c;
	assert(uc2_ots_varint_decode(buf, 2, &v, &c) == UC2_OTS_ERR_TRUNCATED);
}

static void test_varint_noncanonical(void)
{
	/* 0x80 0x00 encodes 0 with a redundant continuation byte. */
	uint8_t buf[] = { 0x80, 0x00 };
	uint64_t v;
	size_t c;
	assert(uc2_ots_varint_decode(buf, 2, &v, &c) == UC2_OTS_ERR_NONCANONICAL);
}

static void test_varint_overflow_64bit(void)
{
	/* 10-byte encoding where the final group has bits beyond bit 63.
	 * Bytes 1-9 fill shift positions 0..56 with all 1s; byte 10 at shift 63
	 * carries 0x02 (group=2), which would shift past bit 63. */
	uint8_t buf[] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x02
	};
	uint64_t v;
	size_t c;
	assert(uc2_ots_varint_decode(buf, sizeof buf, &v, &c) == UC2_OTS_ERR_OVERFLOW);
}

static void test_varint_max_64bit(void)
{
	/* Largest representable 64-bit value: 0xffffffffffffffff. */
	uint8_t buf[10];
	size_t n = uc2_ots_varint_encode((uint64_t)-1, buf);
	uint64_t v;
	size_t c;
	assert(uc2_ots_varint_decode(buf, n, &v, &c) == UC2_OTS_OK);
	assert(v == (uint64_t)-1);
}

static void test_file_envelope_roundtrip(void)
{
	uint8_t leaf[32];
	for (int i = 0; i < 32; i++) leaf[i] = (uint8_t)i;
	uint8_t body[] = "\x00\x83\xdf\xe3\x0d\x2e\xf9\x0c\x8e\x01x"; /* attest pending "x" */
	size_t body_len = sizeof body - 1;

	uint8_t out[256];
	int n = uc2_ots_serialize_file(UC2_OTS_OP_SHA256, leaf, 32,
	                               body, body_len, out, sizeof out);
	assert(n > 0);

	uint8_t hash_op;
	const uint8_t *got_leaf, *got_body;
	size_t got_leaf_len, got_body_len;
	int rc = uc2_ots_parse_file(out, (size_t)n, &hash_op,
	                            &got_leaf, &got_leaf_len,
	                            &got_body, &got_body_len);
	assert(rc == UC2_OTS_OK);
	assert(hash_op == UC2_OTS_OP_SHA256);
	assert(got_leaf_len == 32);
	assert(memcmp(got_leaf, leaf, 32) == 0);
	assert(got_body_len == body_len);
	assert(memcmp(got_body, body, body_len) == 0);
}

static void test_file_bad_magic(void)
{
	uint8_t buf[128];
	memset(buf, 0xaa, sizeof buf);
	uint8_t hash_op;
	const uint8_t *l, *b;
	size_t ll, bl;
	assert(uc2_ots_parse_file(buf, sizeof buf, &hash_op, &l, &ll, &b, &bl)
	       == UC2_OTS_ERR_BAD_MAGIC);
}

struct cb_ctx {
	int n_calls;
	uint8_t last_tag[8];
	uint8_t last_digest[64];
	size_t last_digest_len;
	const uint8_t *last_payload;
	size_t last_payload_len;
};

static int collect_cb(void *vctx,
                     const uint8_t *tag,
                     const uint8_t *payload, size_t payload_len,
                     const uint8_t *digest, size_t digest_len)
{
	struct cb_ctx *c = vctx;
	c->n_calls++;
	memcpy(c->last_tag, tag, 8);
	memcpy(c->last_digest, digest, digest_len);
	c->last_digest_len = digest_len;
	c->last_payload = payload;
	c->last_payload_len = payload_len;
	return 0;
}

static void test_walk_append_then_attest(void)
{
	/* Body: APPEND "ab", then attestation pending "x".
	 * Leaf "L" -> APPEND "ab" -> "Lab" -> pending. */
	uint8_t body[] = {
		UC2_OTS_OP_APPEND, 0x02, 'a', 'b',
		UC2_OTS_ATTESTATION,
		0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e,
		0x01, 'x'
	};
	struct cb_ctx ctx = {0};
	int rc = uc2_ots_walk(body, sizeof body, (uint8_t *)"L", 1,
	                      collect_cb, &ctx);
	assert(rc == UC2_OTS_RESULT_VERIFIED);
	assert(ctx.n_calls == 1);
	assert(ctx.last_digest_len == 3);
	assert(memcmp(ctx.last_digest, "Lab", 3) == 0);
	assert(memcmp(ctx.last_tag, UC2_OTS_TAG_PENDING, 8) == 0);
	assert(ctx.last_payload_len == 1 && ctx.last_payload[0] == 'x');
}

static void test_walk_two_siblings(void)
{
	/* Body: 0xff <att pending "a"> <att pending "b">.
	 * Two sibling attestations from the same leaf. */
	uint8_t body[] = {
		UC2_OTS_BRANCH,
		UC2_OTS_ATTESTATION, 0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e, 0x01, 'a',
		UC2_OTS_ATTESTATION, 0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e, 0x01, 'b'
	};
	struct cb_ctx ctx = {0};
	int rc = uc2_ots_walk(body, sizeof body, (uint8_t *)"L", 1,
	                      collect_cb, &ctx);
	assert(rc == UC2_OTS_RESULT_VERIFIED);
	assert(ctx.n_calls == 2);
}

static void test_walk_sha256_op(void)
{
	/* Body: SHA256 op then pending attestation. */
	uint8_t body[] = {
		UC2_OTS_OP_SHA256,
		UC2_OTS_ATTESTATION,
		0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e,
		0x01, 'x'
	};
	uint8_t leaf[32] = {0};
	struct cb_ctx ctx = {0};
	int rc = uc2_ots_walk(body, sizeof body, leaf, 32, collect_cb, &ctx);
	assert(rc == UC2_OTS_RESULT_VERIFIED);
	assert(ctx.n_calls == 1);
	assert(ctx.last_digest_len == 32);
	uint8_t expect[32];
	uc2_sha256_hash(leaf, 32, expect);
	assert(memcmp(ctx.last_digest, expect, 32) == 0);
}

static void test_walk_unsupported_op(void)
{
	/* SHA1 op -> structural-only. */
	uint8_t body[] = {
		UC2_OTS_OP_SHA1,
		UC2_OTS_ATTESTATION,
		0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e,
		0x01, 'x'
	};
	uint8_t leaf[32] = {0};
	struct cb_ctx ctx = {0};
	int rc = uc2_ots_walk(body, sizeof body, leaf, 32, collect_cb, &ctx);
	assert(rc == UC2_OTS_RESULT_STRUCTURAL);
	assert(ctx.n_calls == 1);
}

static void test_walk_truncated(void)
{
	uint8_t body[] = { UC2_OTS_OP_APPEND, 0x05, 'a' }; /* operand truncated */
	uint8_t leaf[1] = {'L'};
	int rc = uc2_ots_walk(body, sizeof body, leaf, 1, NULL, NULL);
	assert(rc == UC2_OTS_ERR_TRUNCATED);
}

static void test_walk_trailing_garbage(void)
{
	uint8_t body[] = {
		UC2_OTS_ATTESTATION,
		0x83, 0xdf, 0xe3, 0x0d, 0x2e, 0xf9, 0x0c, 0x8e,
		0x01, 'x',
		0xaa /* garbage */
	};
	uint8_t leaf[1] = {'L'};
	int rc = uc2_ots_walk(body, sizeof body, leaf, 1, NULL, NULL);
	assert(rc == UC2_OTS_ERR_OVERFLOW);
}

static void test_attest_name(void)
{
	assert(strcmp(uc2_ots_attest_name((uint8_t *)UC2_OTS_TAG_PENDING),  "pending") == 0);
	assert(strcmp(uc2_ots_attest_name((uint8_t *)UC2_OTS_TAG_BITCOIN),  "Bitcoin") == 0);
	assert(strcmp(uc2_ots_attest_name((uint8_t *)UC2_OTS_TAG_LITECOIN), "Litecoin") == 0);
	uint8_t unk[8] = {0};
	assert(uc2_ots_attest_name(unk) == NULL);
}

static void test_trailer_roundtrip(void)
{
	uint8_t fake_archive[100];
	for (int i = 0; i < 100; i++) fake_archive[i] = (uint8_t)i;
	uint8_t fake_proof[40];
	for (int i = 0; i < 40; i++) fake_proof[i] = (uint8_t)(0xa0 + i);

	uint8_t buf[300];
	memcpy(buf, fake_archive, 100);
	int n = uc2_ots_trailer_build(100, fake_proof, 40,
	                              buf + 100, sizeof buf - 100);
	assert(n > 0);
	size_t total = 100 + (size_t)n;

	uint32_t archive_len;
	const uint8_t *proof;
	size_t proof_len;
	int rc = uc2_ots_trailer_parse(buf, total, &archive_len, &proof, &proof_len);
	assert(rc == UC2_OTS_OK);
	assert(archive_len == 100);
	assert(proof_len == 40);
	assert(memcmp(proof, fake_proof, 40) == 0);
}

static void test_trailer_no_trailer(void)
{
	uint8_t buf[20] = {0};
	uint32_t al; const uint8_t *p; size_t pl;
	int rc = uc2_ots_trailer_parse(buf, sizeof buf, &al, &p, &pl);
	assert(rc == 1); /* no trailer */
}

static void test_trailer_corrupt_proof_len_mismatch(void)
{
	/* Build a valid trailer, then mutate the front proof_len so it
	 * disagrees with the back duplicate. */
	uint8_t buf[200];
	memset(buf, 0, sizeof buf);
	int n = uc2_ots_trailer_build(50, (uint8_t *)"\x01\x02\x03\x04", 4,
	                              buf + 50, sizeof buf - 50);
	assert(n > 0);
	size_t total = 50 + (size_t)n;
	/* Mutate front proof_len at offset 50+8+4+4 = 66 */
	buf[66] = 0xff;

	uint32_t al; const uint8_t *p; size_t pl;
	int rc = uc2_ots_trailer_parse(buf, total, &al, &p, &pl);
	assert(rc == UC2_OTS_ERR_NONCANONICAL);
}

static void test_trailer_truncated(void)
{
	/* Build a valid trailer, then chop the file in the middle of the
	 * proof.  Reverse-scan won't see back magic; should report no trailer. */
	uint8_t buf[200];
	memset(buf, 0, sizeof buf);
	int n = uc2_ots_trailer_build(50, (uint8_t *)"AAAAAA", 6,
	                              buf + 50, sizeof buf - 50);
	assert(n > 0);
	size_t total = 50 + (size_t)n - 5; /* chop last 5 bytes */
	uint32_t al; const uint8_t *p; size_t pl;
	int rc = uc2_ots_trailer_parse(buf, total, &al, &p, &pl);
	assert(rc == 1); /* back magic absent */
}

static void test_trailer_back_magic_collision(void)
{
	/* If the file happens to end with the back magic but the recorded
	 * proof_len is too large for the file size, trailer_parse must
	 * report a hard error, not silently accept. */
	uint8_t buf[16];
	memset(buf, 0, sizeof buf);
	/* Last 8 bytes = back magic; preceding 4 bytes = bogus proof_len. */
	memcpy(buf + 8, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN);
	buf[4] = 0xff; buf[5] = 0xff; buf[6] = 0xff; buf[7] = 0x00; /* huge proof_len */

	uint32_t al; const uint8_t *p; size_t pl;
	int rc = uc2_ots_trailer_parse(buf, sizeof buf, &al, &p, &pl);
	assert(rc == UC2_OTS_ERR_TOO_LARGE || rc == UC2_OTS_ERR_TRUNCATED);
}

int main(void)
{
	printf("OTS tests:\n");
	TEST(test_varint_roundtrip);
	TEST(test_varint_truncated);
	TEST(test_varint_noncanonical);
	TEST(test_varint_overflow_64bit);
	TEST(test_varint_max_64bit);
	TEST(test_file_envelope_roundtrip);
	TEST(test_file_bad_magic);
	TEST(test_walk_append_then_attest);
	TEST(test_walk_two_siblings);
	TEST(test_walk_sha256_op);
	TEST(test_walk_unsupported_op);
	TEST(test_walk_truncated);
	TEST(test_walk_trailing_garbage);
	TEST(test_attest_name);
	TEST(test_trailer_roundtrip);
	TEST(test_trailer_no_trailer);
	TEST(test_trailer_corrupt_proof_len_mismatch);
	TEST(test_trailer_truncated);
	TEST(test_trailer_back_magic_collision);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
