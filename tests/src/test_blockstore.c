/* Tests for cross-archive block store. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <uc2/uc2_blockstore.h>
#include <uc2/uc2_merkle.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static char store_path[256];

static void fill_random(uint8_t *buf, size_t len, uint32_t seed)
{
	for (size_t i = 0; i < len; i++) {
		seed = seed * 1103515245 + 12345;
		buf[i] = (uint8_t)(seed >> 16);
	}
}

/* Recursive rm -rf (simple, for test cleanup) */
static void rmrf(const char *path)
{
	char cmd[512];
	snprintf(cmd, sizeof cmd, "rm -rf '%s'", path);
	system(cmd);
}

static void test_open_close(void)
{
	struct uc2_blockstore bs;
	assert(uc2_blockstore_open(&bs, store_path) == 0);
	assert(bs.nblocks == 0);
	assert(bs.total_bytes == 0);
	assert(bs.saved_bytes == 0);
	uc2_blockstore_close(&bs);
}

static void test_ingest_single(void)
{
	uint8_t data[4096];
	fill_random(data, sizeof data, 0xABCD);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, sizeof data, 12);

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);
	int new_chunks = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(new_chunks == tree.nchunks);
	assert(bs.nblocks == tree.nchunks);
	assert(bs.total_bytes == sizeof data);
	assert(bs.saved_bytes == 0);
	uc2_blockstore_close(&bs);
	uc2_merkle_free(&tree);
}

static void test_dedup_identical(void)
{
	/* Ingest same data twice: second ingest should store 0 new chunks */
	uint8_t data[8192];
	fill_random(data, sizeof data, 0x1234);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, sizeof data, 12);

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);

	int n1 = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(n1 == tree.nchunks);

	int n2 = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(n2 == 0);  /* fully deduplicated */
	assert(bs.saved_bytes == sizeof data);

	printf("(%d chunks, %lld saved) ", tree.nchunks, (long long)bs.saved_bytes);
	uc2_blockstore_close(&bs);
	uc2_merkle_free(&tree);
}

static void test_read_back(void)
{
	uint8_t data[2048];
	fill_random(data, sizeof data, 0x5678);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, sizeof data, 12);

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);
	uc2_blockstore_ingest(&bs, &tree, data, sizeof data);

	/* Read each chunk back and verify */
	for (int i = 0; i < tree.nchunks; i++) {
		uint8_t buf[65536];
		int n = uc2_blockstore_read(&bs, tree.chunks[i].hash, buf, sizeof buf);
		assert(n == (int)tree.chunks[i].length);
		assert(memcmp(buf, data + tree.chunks[i].offset, n) == 0);
	}

	uc2_blockstore_close(&bs);
	uc2_merkle_free(&tree);
}

static void test_cross_archive_dedup(void)
{
	/* Simulate two archives with shared content */
	size_t shared_len = 32 * 1024;
	uint8_t *shared = malloc(shared_len);
	fill_random(shared, shared_len, 0xFEED);

	/* Archive 1: [shared] */
	struct uc2_merkle t1;
	uc2_merkle_build(&t1, shared, shared_len, 12);

	/* Archive 2: [shared + unique(8KB)] */
	size_t f2_len = shared_len + 8192;
	uint8_t *f2 = malloc(f2_len);
	memcpy(f2, shared, shared_len);
	fill_random(f2 + shared_len, 8192, 0xBEEF);
	struct uc2_merkle t2;
	uc2_merkle_build(&t2, f2, f2_len, 12);

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);

	/* Ingest archive 1 */
	int n1 = uc2_blockstore_ingest(&bs, &t1, shared, shared_len);
	int64_t bytes1 = bs.total_bytes;

	/* Ingest archive 2: shared chunks should dedup */
	int n2 = uc2_blockstore_ingest(&bs, &t2, f2, f2_len);
	int64_t saved = bs.saved_bytes;

	printf("(a1=%d new, a2=%d new, saved=%lld) ", n1, n2, (long long)saved);
	assert(n2 < t2.nchunks);  /* some chunks deduplicated */
	assert(saved > 0);        /* bytes saved */

	uc2_blockstore_close(&bs);
	uc2_merkle_free(&t1);
	uc2_merkle_free(&t2);
	free(shared);
	free(f2);
}

static void test_has(void)
{
	uint8_t data[1024];
	fill_random(data, sizeof data, 0x9999);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, sizeof data, 12);

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);

	/* Before ingest: chunk should not exist */
	assert(!uc2_blockstore_has(&bs, tree.chunks[0].hash));

	uc2_blockstore_ingest(&bs, &tree, data, sizeof data);

	/* After ingest: chunk should exist */
	assert(uc2_blockstore_has(&bs, tree.chunks[0].hash));

	/* Random hash: should not exist */
	assert(!uc2_blockstore_has(&bs, 0x1234567890ABCDEFULL));

	uc2_blockstore_close(&bs);
	uc2_merkle_free(&tree);
}

int main(void)
{
	snprintf(store_path, sizeof store_path, "/tmp/uc2_blockstore_test_%d",
	         (int)getpid());

	printf("Block store tests:\n");
	rmrf(store_path);  /* clean start */

	TEST(test_open_close);
	rmrf(store_path);
	TEST(test_ingest_single);
	rmrf(store_path);
	TEST(test_dedup_identical);
	rmrf(store_path);
	TEST(test_read_back);
	rmrf(store_path);
	TEST(test_cross_archive_dedup);
	rmrf(store_path);
	TEST(test_has);
	rmrf(store_path);

	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
