/* Tests for delta compression. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_delta.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_identical(void)
{
	uint8_t data[] = "Hello, World! This is a test of delta compression.";
	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(data, sizeof data, data, sizeof data,
	                        &delta, &delta_len) == 0);

	/* Delta of identical data should be mostly COPY */
	printf("(delta=%zu vs orig=%zu) ", delta_len, sizeof data);
	assert(delta_len < sizeof data + 20);

	/* Apply and verify */
	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(data, sizeof data, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == sizeof data);
	assert(memcmp(recon, data, sizeof data) == 0);

	free(delta);
	free(recon);
}

static void test_small_change(void)
{
	/* Source and target differ by a few bytes */
	uint8_t src[] = "The quick brown fox jumps over the lazy dog. AAAA";
	uint8_t tgt[] = "The quick brown FOX jumps over the lazy dog. BBBB";
	size_t slen = sizeof src, tlen = sizeof tgt;

	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(src, slen, tgt, tlen, &delta, &delta_len) == 0);

	printf("(delta=%zu vs tgt=%zu) ", delta_len, tlen);
	assert(delta_len < tlen);  /* delta should be smaller than full target */

	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(src, slen, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == tlen);
	assert(memcmp(recon, tgt, tlen) == 0);

	free(delta);
	free(recon);
}

static void test_completely_different(void)
{
	uint8_t src[] = "AAAAAAAAAA";
	uint8_t tgt[] = "ZZZZZZZZZZ";

	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(src, sizeof src, tgt, sizeof tgt,
	                        &delta, &delta_len) == 0);

	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(src, sizeof src, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == sizeof tgt);
	assert(memcmp(recon, tgt, sizeof tgt) == 0);

	free(delta);
	free(recon);
}

static void test_binary_patch(void)
{
	/* Simulate patched executable: mostly same, few bytes changed */
	size_t len = 16384;
	uint8_t *src = malloc(len);
	uint8_t *tgt = malloc(len);
	uint32_t rng = 0xCAFEBABE;
	for (size_t i = 0; i < len; i++) {
		rng = rng * 1103515245 + 12345;
		src[i] = (uint8_t)(rng >> 16);
	}
	memcpy(tgt, src, len);
	/* Patch 32 bytes at 3 locations */
	for (int i = 0; i < 32; i++) {
		tgt[1000 + i] ^= 0xFF;
		tgt[8000 + i] ^= 0xAA;
		tgt[12000 + i] ^= 0x55;
	}

	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(src, len, tgt, len, &delta, &delta_len) == 0);

	printf("(delta=%zu vs orig=%zu = %.0f%% savings) ",
	       delta_len, len, (1.0 - (double)delta_len / len) * 100);
	assert(delta_len < len / 2);  /* should be much smaller */

	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(src, len, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == len);
	assert(memcmp(recon, tgt, len) == 0);

	free(src);
	free(tgt);
	free(delta);
	free(recon);
}

static void test_empty_target(void)
{
	uint8_t src[] = "source data";
	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(src, sizeof src, NULL, 0, &delta, &delta_len) == 0);

	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(src, sizeof src, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == 0);

	free(delta);
	free(recon);
}

static void test_append(void)
{
	/* Target = source + new data at the end */
	uint8_t src[] = "This is the original content.";
	uint8_t tgt[] = "This is the original content. Plus new stuff!";
	size_t slen = sizeof src, tlen = sizeof tgt;

	uint8_t *delta; size_t delta_len;
	assert(uc2_delta_encode(src, slen, tgt, tlen, &delta, &delta_len) == 0);

	uint8_t *recon; size_t recon_len;
	assert(uc2_delta_apply(src, slen, delta, delta_len,
	                       &recon, &recon_len) == 0);
	assert(recon_len == tlen);
	assert(memcmp(recon, tgt, tlen) == 0);

	free(delta);
	free(recon);
}

int main(void)
{
	printf("Delta compression tests:\n");
	TEST(test_identical);
	TEST(test_small_change);
	TEST(test_completely_different);
	TEST(test_binary_patch);
	TEST(test_empty_target);
	TEST(test_append);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
