/* Tests for LZ4 ultra-fast compression. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_lz4.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_roundtrip_text(void)
{
	const char *text = "The quick brown fox jumps over the lazy dog. "
	                   "The quick brown fox jumps over the lazy dog.";
	size_t len = strlen(text);
	size_t bound = uc2_lz4_bound(len);
	uint8_t *comp = malloc(bound);
	size_t clen = uc2_lz4_compress((const uint8_t *)text, len, comp, bound);
	assert(clen > 0 && clen < len);  /* repeated text should compress */
	printf("(%zu -> %zu) ", len, clen);

	uint8_t *dec = malloc(len);
	size_t dlen = uc2_lz4_decompress(comp, clen, dec, len);
	assert(dlen == len);
	{ int _r = memcmp(dec, text, len); (void)_r; assert(_r == 0); }
	free(comp); free(dec);
}

static void test_roundtrip_binary(void)
{
	size_t len = 16384;
	uint8_t *data = malloc(len);
	uint32_t rng = 0xDEADBEEF;
	/* Semi-random: some patterns for LZ4 to find */
	for (size_t i = 0; i < len; i++) {
		rng = rng * 1103515245 + 12345;
		data[i] = (uint8_t)((rng >> 16) & 0x3F);  /* limited range = more matches */
	}

	size_t bound = uc2_lz4_bound(len);
	uint8_t *comp = malloc(bound);
	size_t clen = uc2_lz4_compress(data, len, comp, bound);
	assert(clen > 0);
	printf("(%zu -> %zu) ", len, clen);

	uint8_t *dec = malloc(len);
	size_t dlen = uc2_lz4_decompress(comp, clen, dec, len);
	assert(dlen == len);
	{ int _r = memcmp(dec, data, len); (void)_r; assert(_r == 0); }
	free(data); free(comp); free(dec);
}

static void test_all_same(void)
{
	/* Highly compressible: all same byte */
	size_t len = 4096;
	uint8_t *data = malloc(len);
	memset(data, 'A', len);

	size_t bound = uc2_lz4_bound(len);
	uint8_t *comp = malloc(bound);
	size_t clen = uc2_lz4_compress(data, len, comp, bound);
	assert(clen > 0 && clen < len / 4);
	printf("(%zu -> %zu) ", len, clen);

	uint8_t *dec = malloc(len);
	size_t dlen = uc2_lz4_decompress(comp, clen, dec, len);
	assert(dlen == len);
	{ int _r = memcmp(dec, data, len); (void)_r; assert(_r == 0); }
	free(data); free(comp); free(dec);
}

static void test_incompressible(void)
{
	/* Fully random: should still round-trip */
	size_t len = 1024;
	uint8_t *data = malloc(len);
	uint32_t rng = 0x12345678;
	for (size_t i = 0; i < len; i++) {
		rng = rng * 1103515245 + 12345;
		data[i] = (uint8_t)(rng >> 16);
	}

	size_t bound = uc2_lz4_bound(len);
	uint8_t *comp = malloc(bound);
	size_t clen = uc2_lz4_compress(data, len, comp, bound);
	assert(clen > 0);

	uint8_t *dec = malloc(len);
	size_t dlen = uc2_lz4_decompress(comp, clen, dec, len);
	assert(dlen == len);
	{ int _r = memcmp(dec, data, len); (void)_r; assert(_r == 0); }
	free(data); free(comp); free(dec);
}

static void test_small(void)
{
	uint8_t data[] = "Hi!";
	size_t bound = uc2_lz4_bound(3);
	uint8_t *comp = malloc(bound);
	size_t clen = uc2_lz4_compress(data, 3, comp, bound);
	assert(clen > 0);

	uint8_t dec[3];
	size_t dlen = uc2_lz4_decompress(comp, clen, dec, 3);
	assert(dlen == 3);
	{ int _r = memcmp(dec, data, 3); (void)_r; assert(_r == 0); }
	free(comp);
}

static void test_empty(void)
{
	size_t clen = uc2_lz4_compress(NULL, 0, NULL, 0);
	assert(clen == 0);
}

int main(void)
{
	printf("LZ4 tests:\n");
	TEST(test_roundtrip_text);
	TEST(test_roundtrip_binary);
	TEST(test_all_same);
	TEST(test_incompressible);
	TEST(test_small);
	TEST(test_empty);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
