/* Tests for Merkle DAG content addressing. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <uc2/uc2_merkle.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void fill_random(uint8_t *buf, size_t len, uint32_t seed)
{
	for (size_t i = 0; i < len; i++) {
		seed = seed * 1103515245 + 12345;
		buf[i] = (uint8_t)(seed >> 16);
	}
}

static void test_hash64_deterministic(void)
{
	uint8_t data[] = "Hello, World!";
	uint64_t h1 = uc2_hash64(data, sizeof data - 1);
	uint64_t h2 = uc2_hash64(data, sizeof data - 1);
	assert(h1 == h2);
	assert(h1 != 0);
}

static void test_build_empty(void)
{
	struct uc2_merkle tree;
	uc2_merkle_build(&tree, NULL, 0, 13);
	assert(tree.nchunks == 0);
	{ int _r = uc2_merkle_root(&tree); (void)_r; assert(_r == 0); }
	uc2_merkle_free(&tree);
}

static void test_build_small(void)
{
	uint8_t data[] = "short";
	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, 5, 13);
	assert(tree.nchunks == 1);
	assert(tree.chunks[0].offset == 0);
	assert(tree.chunks[0].length == 5);
	assert(uc2_merkle_root(&tree) != 0);
	uc2_merkle_free(&tree);
}

static void test_identical_files(void)
{
	size_t len = 64 * 1024;
	uint8_t *data = malloc(len);
	fill_random(data, len, 0xDEADBEEF);

	struct uc2_merkle t1, t2;
	uc2_merkle_build(&t1, data, len, 13);
	uc2_merkle_build(&t2, data, len, 13);

	{ int _r = uc2_merkle_root(&t1); (void)_r; assert(_r == uc2_merkle_root(&t2)); }
	assert(t1.nchunks == t2.nchunks);
	{ int _r = uc2_merkle_common(&t1, &t2); (void)_r; assert(_r == t1.nchunks); }
	assert(fabs(uc2_merkle_similarity(&t1, &t2) - 1.0) < 0.001);

	uc2_merkle_free(&t1);
	uc2_merkle_free(&t2);
	free(data);
}

static void test_different_files(void)
{
	size_t len = 64 * 1024;
	uint8_t *a = malloc(len);
	uint8_t *b = malloc(len);
	fill_random(a, len, 0x11111111);
	fill_random(b, len, 0x22222222);

	struct uc2_merkle ta, tb;
	uc2_merkle_build(&ta, a, len, 13);
	uc2_merkle_build(&tb, b, len, 13);

	assert(uc2_merkle_root(&ta) != uc2_merkle_root(&tb));
	int common = uc2_merkle_common(&ta, &tb);
	double sim = uc2_merkle_similarity(&ta, &tb);
	printf("(common=%d sim=%.2f) ", common, sim);
	assert(sim < 0.2);  /* random data: minimal overlap */

	uc2_merkle_free(&ta);
	uc2_merkle_free(&tb);
	free(a);
	free(b);
}

static void test_partial_overlap(void)
{
	/* File A: [unique_A (32KB) | shared (256KB)]
	   File B: [shared (256KB) | unique_B (32KB)] */
	size_t shared_len = 256 * 1024;
	size_t unique_len = 32 * 1024;

	uint8_t *shared = malloc(shared_len);
	fill_random(shared, shared_len, 0xCAFEBABE);

	uint8_t *fa = malloc(unique_len + shared_len);
	uint8_t *fb = malloc(shared_len + unique_len);
	fill_random(fa, unique_len, 0xAAAAAAAA);
	memcpy(fa + unique_len, shared, shared_len);
	memcpy(fb, shared, shared_len);
	fill_random(fb + shared_len, unique_len, 0xBBBBBBBB);

	struct uc2_merkle ta, tb;
	uc2_merkle_build(&ta, fa, unique_len + shared_len, 13);
	uc2_merkle_build(&tb, fb, shared_len + unique_len, 13);

	int common = uc2_merkle_common(&ta, &tb);
	double sim = uc2_merkle_similarity(&ta, &tb);
	printf("(chunks: a=%d b=%d common=%d sim=%.2f) ", ta.nchunks, tb.nchunks, common, sim);
	/* ~89% of each file is shared, CDC should find substantial overlap */
	assert(common > 0);
	assert(sim > 0.5);

	uc2_merkle_free(&ta);
	uc2_merkle_free(&tb);
	free(shared);
	free(fa);
	free(fb);
}

static void test_root_changes_with_content(void)
{
	size_t len = 32 * 1024;
	uint8_t *data = malloc(len);
	fill_random(data, len, 0x12345678);

	struct uc2_merkle t1;
	uc2_merkle_build(&t1, data, len, 13);
	uint64_t root1 = uc2_merkle_root(&t1);

	/* Change one byte in the middle */
	data[len / 2] ^= 0xFF;
	struct uc2_merkle t2;
	uc2_merkle_build(&t2, data, len, 13);
	uint64_t root2 = uc2_merkle_root(&t2);

	/* Root should change (the affected chunk's hash changes) */
	assert(root1 != root2);
	/* But most chunks should still match */
	int common = uc2_merkle_common(&t1, &t2);
	printf("(changed 1 byte: %d/%d chunks survived) ", common, t1.nchunks);
	assert(common >= t1.nchunks - 2);  /* at most 1-2 chunks affected */

	uc2_merkle_free(&t1);
	uc2_merkle_free(&t2);
	free(data);
}

static void test_covers_all_bytes(void)
{
	size_t len = 100 * 1024;
	uint8_t *data = malloc(len);
	fill_random(data, len, 0xFEEDFACE);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, len, 13);

	uint32_t total = 0;
	for (int i = 0; i < tree.nchunks; i++) {
		if (i > 0)
			assert(tree.chunks[i].offset ==
			       tree.chunks[i-1].offset + tree.chunks[i-1].length);
		total += tree.chunks[i].length;
	}
	assert(total == len);

	uc2_merkle_free(&tree);
	free(data);
}

int main(void)
{
	printf("Merkle DAG tests:\n");
	TEST(test_hash64_deterministic);
	TEST(test_build_empty);
	TEST(test_build_small);
	TEST(test_identical_files);
	TEST(test_different_files);
	TEST(test_partial_overlap);
	TEST(test_root_changes_with_content);
	TEST(test_covers_all_bytes);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
