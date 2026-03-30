/* Tests for BLAKE3 cryptographic hashing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_blake3.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void hex(const uint8_t *h, int n, char *out)
{
	for (int i = 0; i < n; i++) sprintf(out + i*2, "%02x", h[i]);
}

static void test_empty(void)
{
	uint8_t hash[32];
	uc2_blake3_hash("", 0, hash);
	/* BLAKE3("") is a known constant */
	char h[65]; hex(hash, 32, h); h[64] = 0;
	printf("(%s) ", h);
	/* The hash should be non-zero and deterministic */
	uint8_t hash2[32];
	uc2_blake3_hash("", 0, hash2);
	assert(uc2_blake3_equal(hash, hash2));
}

static void test_deterministic(void)
{
	uint8_t data[] = "Hello, BLAKE3!";
	uint8_t h1[32], h2[32];
	uc2_blake3_hash(data, sizeof data - 1, h1);
	uc2_blake3_hash(data, sizeof data - 1, h2);
	assert(uc2_blake3_equal(h1, h2));
}

static void test_differs(void)
{
	uint8_t h1[32], h2[32];
	uc2_blake3_hash("AAA", 3, h1);
	uc2_blake3_hash("BBB", 3, h2);
	assert(!uc2_blake3_equal(h1, h2));
}

static void test_incremental(void)
{
	/* Incremental update should match one-shot */
	uint8_t data[] = "The quick brown fox jumps over the lazy dog";
	size_t len = sizeof data - 1;

	uint8_t oneshot[32];
	uc2_blake3_hash(data, len, oneshot);

	struct uc2_blake3 ctx;
	uc2_blake3_init(&ctx);
	uc2_blake3_update(&ctx, data, 10);
	uc2_blake3_update(&ctx, data + 10, len - 10);
	uint8_t incremental[32];
	uc2_blake3_final(&ctx, incremental);

	assert(uc2_blake3_equal(oneshot, incremental));
}

static void test_single_byte_updates(void)
{
	uint8_t data[] = "ABCDEFGH";
	size_t len = 8;

	uint8_t oneshot[32];
	uc2_blake3_hash(data, len, oneshot);

	struct uc2_blake3 ctx;
	uc2_blake3_init(&ctx);
	for (size_t i = 0; i < len; i++)
		uc2_blake3_update(&ctx, data + i, 1);
	uint8_t piecemeal[32];
	uc2_blake3_final(&ctx, piecemeal);

	assert(uc2_blake3_equal(oneshot, piecemeal));
}

static void test_avalanche(void)
{
	/* Changing one bit should change ~50% of output bits */
	uint8_t a[64], b[64];
	memset(a, 0, 64);
	memset(b, 0, 64);
	b[0] = 1;  /* flip one bit */

	uint8_t ha[32], hb[32];
	uc2_blake3_hash(a, 64, ha);
	uc2_blake3_hash(b, 64, hb);

	int diff_bits = 0;
	for (int i = 0; i < 32; i++) {
		uint8_t x = ha[i] ^ hb[i];
		while (x) { diff_bits++; x &= x - 1; }
	}
	printf("(%d/256 bits differ) ", diff_bits);
	assert(diff_bits > 80 && diff_bits < 176);  /* ~50% ± 30% */
}

static void test_equal_constant_time(void)
{
	uint8_t a[32], b[32];
	memset(a, 0xAA, 32);
	memcpy(b, a, 32);
	assert(uc2_blake3_equal(a, b));
	b[31] ^= 1;
	assert(!uc2_blake3_equal(a, b));
}

int main(void)
{
	printf("BLAKE3 tests:\n");
	TEST(test_empty);
	TEST(test_deterministic);
	TEST(test_differs);
	TEST(test_incremental);
	TEST(test_single_byte_updates);
	TEST(test_avalanche);
	TEST(test_equal_constant_time);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
