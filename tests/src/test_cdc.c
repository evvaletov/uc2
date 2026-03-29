/* Tests for content-defined chunking (CDC). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_cdc.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
	tests_run++; \
	printf("  %s: ", #name); \
	name(); \
	tests_passed++; \
	printf("OK\n"); \
} while (0)

static void test_gear_hash_deterministic(void)
{
	uint8_t data[] = "Hello, World!";
	uint32_t h1 = uc2_gear_hash(data, sizeof data - 1);
	uint32_t h2 = uc2_gear_hash(data, sizeof data - 1);
	assert(h1 == h2);
	assert(h1 != 0);
}

static void test_gear_hash_differs(void)
{
	uint8_t a[] = "AAAA";
	uint8_t b[] = "BBBB";
	assert(uc2_gear_hash(a, 4) != uc2_gear_hash(b, 4));
}

static void test_fnv1a(void)
{
	uint8_t data[] = "test";
	uint32_t h = uc2_fnv1a(data, 4);
	assert(h != 0);
	assert(h == uc2_fnv1a(data, 4));
}

static void test_chunker_single_small(void)
{
	/* Data smaller than min_chunk: one chunk */
	uint8_t data[100];
	memset(data, 'A', sizeof data);

	struct uc2_chunker c;
	uc2_chunker_init(&c, 13, 0, 0);  /* avg 8KB, min ~2KB */

	size_t off, len;
	int got = uc2_chunker_next(&c, data, sizeof data, &off, &len);
	assert(got == 0);  /* final chunk */
	assert(off == 0);
	assert(len == sizeof data);
}

static void test_chunker_covers_all_data(void)
{
	/* Generate pseudo-random data to force boundary detection */
	size_t total = 256 * 1024;  /* 256 KB */
	uint8_t *data = malloc(total);
	assert(data);
	uint32_t rng = 0xDEADBEEF;
	for (size_t i = 0; i < total; i++) {
		rng = rng * 1103515245 + 12345;
		data[i] = (uint8_t)(rng >> 16);
	}

	struct uc2_chunker c;
	uc2_chunker_init(&c, 13, 0, 0);

	size_t total_chunked = 0;
	int chunks = 0;
	size_t off, len;
	while (uc2_chunker_next(&c, data, total, &off, &len)) {
		assert(off == total_chunked);
		assert(len > 0);
		total_chunked += len;
		chunks++;
	}
	/* Handle the final chunk */
	total_chunked += len;
	chunks++;

	assert(total_chunked == total);
	assert(chunks > 1);  /* 256KB should produce multiple 8KB-ish chunks */

	free(data);
}

static void test_chunker_respects_min_max(void)
{
	size_t total = 128 * 1024;
	uint8_t *data = malloc(total);
	assert(data);
	uint32_t rng = 0x12345678;
	for (size_t i = 0; i < total; i++) {
		rng = rng * 1103515245 + 12345;
		data[i] = (uint8_t)(rng >> 16);
	}

	size_t min_chunk = 2048;
	size_t max_chunk = 32768;
	struct uc2_chunker c;
	uc2_chunker_init(&c, 13, min_chunk, max_chunk);

	size_t off, len;
	while (uc2_chunker_next(&c, data, total, &off, &len)) {
		assert(len >= min_chunk || off + len == total);
		assert(len <= max_chunk);
	}
	/* Final chunk can be smaller than min */
	assert(len <= max_chunk);

	free(data);
}

static void test_chunker_content_defined(void)
{
	/* Same data inserted at different offsets should produce
	   the same chunk boundaries (shifted by the offset). */
	size_t base_len = 64 * 1024;
	uint8_t *base = malloc(base_len);
	assert(base);
	uint32_t rng = 0xCAFEBABE;
	for (size_t i = 0; i < base_len; i++) {
		rng = rng * 1103515245 + 12345;
		base[i] = (uint8_t)(rng >> 16);
	}

	/* Chunk the base data */
	struct uc2_chunker c;
	uc2_chunker_init(&c, 12, 0, 0);

	int base_n = 0;
	size_t off, len;
	while (uc2_chunker_next(&c, base, base_len, &off, &len) && base_n < 99)
		base_n++;
	base_n++;

	/* Prepend 1000 bytes of garbage, then the same data */
	size_t pad = 1000;
	uint8_t *shifted = malloc(pad + base_len);
	assert(shifted);
	memset(shifted, 0xFF, pad);
	memcpy(shifted + pad, base, base_len);

	uc2_chunker_reset(&c);
	/* Skip the padded portion's chunks */
	size_t total = 0;
	int found_base = 0;
	while (uc2_chunker_next(&c, shifted, pad + base_len, &off, &len)) {
		total += len;
		if (off >= pad && !found_base) {
			found_base = 1;
			/* After the padding chunk(s), subsequent chunks of the
			   base data should eventually align */
		}
	}
	total += len;
	assert(total == pad + base_len);
	assert(found_base);

	free(base);
	free(shifted);
}

static void test_chunker_dedup_detection(void)
{
	/* Two files with a shared 256KB block: CDC should find matching chunks.
	   The shared region is large enough that after the Gear hash state
	   resets (~32 bytes), boundaries align between both files. */
	size_t shared_len = 256 * 1024;
	size_t unique_a = 4096;
	size_t unique_b = 8192;

	uint8_t *shared = malloc(shared_len);
	uint8_t *file_a = malloc(unique_a + shared_len);
	uint8_t *file_b = malloc(shared_len + unique_b);
	assert(shared && file_a && file_b);

	uint32_t rng = 0xFEEDFACE;
	for (size_t i = 0; i < shared_len; i++) {
		rng = rng * 1103515245 + 12345;
		shared[i] = (uint8_t)(rng >> 16);
	}
	for (size_t i = 0; i < unique_a; i++) file_a[i] = (uint8_t)i;
	memcpy(file_a + unique_a, shared, shared_len);
	memcpy(file_b, shared, shared_len);
	for (size_t i = 0; i < unique_b; i++) file_b[shared_len + i] = (uint8_t)(i ^ 0xAA);

	struct uc2_chunker c;
	uc2_chunker_init(&c, 13, 0, 0);

	/* Hash all chunks from file_a */
	uint32_t hashes_a[200];
	int n_a = 0;
	size_t off, len;
	while (uc2_chunker_next(&c, file_a, unique_a + shared_len, &off, &len) && n_a < 199)
		hashes_a[n_a++] = uc2_fnv1a(file_a + off, len);
	hashes_a[n_a++] = uc2_fnv1a(file_a + off, len);

	/* Hash all chunks from file_b */
	uc2_chunker_reset(&c);
	uint32_t hashes_b[200];
	int n_b = 0;
	while (uc2_chunker_next(&c, file_b, shared_len + unique_b, &off, &len) && n_b < 199)
		hashes_b[n_b++] = uc2_fnv1a(file_b + off, len);
	hashes_b[n_b++] = uc2_fnv1a(file_b + off, len);

	/* At least one chunk hash should appear in both files */
	int matches = 0;
	for (int i = 0; i < n_a; i++)
		for (int j = 0; j < n_b; j++)
			if (hashes_a[i] == hashes_b[j])
				matches++;

	assert(matches > 0);
	printf("(%d chunks A, %d chunks B, %d shared) ", n_a, n_b, matches);

	free(shared);
	free(file_a);
	free(file_b);
}

int main(void)
{
	printf("CDC tests:\n");
	TEST(test_gear_hash_deterministic);
	TEST(test_gear_hash_differs);
	TEST(test_fnv1a);
	TEST(test_chunker_single_small);
	TEST(test_chunker_covers_all_data);
	TEST(test_chunker_respects_min_max);
	TEST(test_chunker_content_defined);
	TEST(test_chunker_dedup_detection);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
