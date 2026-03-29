/* Tests for SimHash near-duplicate detection. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_simhash.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_identical(void)
{
	uint8_t data[] = "The quick brown fox jumps over the lazy dog.";
	uint64_t h1 = uc2_simhash(data, sizeof data - 1);
	uint64_t h2 = uc2_simhash(data, sizeof data - 1);
	assert(h1 == h2);
	assert(uc2_hamming(h1, h2) == 0);
}

static void test_similar(void)
{
	/* Nearly identical strings should have small Hamming distance */
	uint8_t a[] = "The quick brown fox jumps over the lazy dog.";
	uint8_t b[] = "The quick brown fox leaps over the lazy dog.";
	uint64_t ha = uc2_simhash(a, sizeof a - 1);
	uint64_t hb = uc2_simhash(b, sizeof b - 1);
	int dist = uc2_hamming(ha, hb);
	printf("(dist=%d) ", dist);
	assert(dist <= 15);  /* similar: small distance */
	assert(uc2_is_near_dup(ha, hb, 15));
}

static void test_different(void)
{
	uint8_t a[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
	uint8_t b[] = "ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ";
	uint64_t ha = uc2_simhash(a, sizeof a - 1);
	uint64_t hb = uc2_simhash(b, sizeof b - 1);
	int dist = uc2_hamming(ha, hb);
	printf("(dist=%d) ", dist);
	assert(dist > 15);  /* very different */
	assert(!uc2_is_near_dup(ha, hb, 10));
}

static void test_patched_binary(void)
{
	/* Simulate a patched executable: mostly same, few bytes changed */
	size_t len = 8192;
	uint8_t *orig = malloc(len);
	uint8_t *patched = malloc(len);
	uint32_t rng = 0xDEADBEEF;
	for (size_t i = 0; i < len; i++) {
		rng = rng * 1103515245 + 12345;
		orig[i] = (uint8_t)(rng >> 16);
	}
	memcpy(patched, orig, len);
	/* Patch 16 bytes at offset 4096 */
	for (int i = 0; i < 16; i++) patched[4096 + i] ^= 0xFF;

	uint64_t ho = uc2_simhash(orig, len);
	uint64_t hp = uc2_simhash(patched, len);
	int dist = uc2_hamming(ho, hp);
	printf("(dist=%d) ", dist);
	assert(dist <= 8);  /* 16 changed bytes in 8KB: very similar */

	free(orig);
	free(patched);
}

static void test_hamming(void)
{
	assert(uc2_hamming(0, 0) == 0);
	assert(uc2_hamming(0, 1) == 1);
	assert(uc2_hamming(0xFF, 0x00) == 8);
	assert(uc2_hamming(0xFFFFFFFFFFFFFFFFULL, 0) == 64);
}

static void test_short_input(void)
{
	uint8_t a[] = "Hi";
	uint64_t h = uc2_simhash(a, 2);
	assert(h != 0);
}

int main(void)
{
	printf("SimHash tests:\n");
	TEST(test_identical);
	TEST(test_similar);
	TEST(test_different);
	TEST(test_patched_binary);
	TEST(test_hamming);
	TEST(test_short_input);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
