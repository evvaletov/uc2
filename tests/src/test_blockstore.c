/* Tests for cross-archive block store. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <process.h>
#include <io.h>
#include <direct.h>
#define getpid _getpid
#define rmdir _rmdir
#define create_dir(path) _mkdir(path)
#else
#include <unistd.h>
#include <dirent.h>
#define create_dir(path) mkdir(path, 0755)
#endif
#include <uc2/uc2_blockstore.h>
#include <uc2/uc2_merkle.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static char store_path[256];

/* Temp-file base: %TEMP% on Windows, /tmp elsewhere. */
static const char *tmpdir(void)
{
#ifdef _WIN32
	const char *t = getenv("TEMP");
	if (!t) t = getenv("TMP");
	return t ? t : ".";
#else
	return "/tmp";
#endif
}

static void fill_random(uint8_t *buf, size_t len, uint32_t seed)
{
	for (size_t i = 0; i < len; i++) {
		seed = seed * 1103515245 + 12345;
		buf[i] = (uint8_t)(seed >> 16);
	}
}

/* Portable recursive removal for the store's two-level layout. */
static void rmtree(const char *path)
{
#ifdef _MSC_VER
	char pattern[512];
	struct _finddata_t fd;
	snprintf(pattern, sizeof pattern, "%s/*", path);
	intptr_t h = _findfirst(pattern, &fd);
	if (h != -1) {
		do {
			if (strcmp(fd.name, ".") == 0 || strcmp(fd.name, "..") == 0)
				continue;
			char sub[512];
			snprintf(sub, sizeof sub, "%s/%s", path, fd.name);
			if (fd.attrib & _A_SUBDIR)
				rmtree(sub);
			else
				remove(sub);
		} while (_findnext(h, &fd) == 0);
		_findclose(h);
	}
#else
	DIR *d = opendir(path);
	if (d) {
		struct dirent *e;
		while ((e = readdir(d))) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			char sub[512];
			snprintf(sub, sizeof sub, "%s/%s", path, e->d_name);
			struct stat st;
			if (stat(sub, &st) == 0 && S_ISDIR(st.st_mode))
				rmtree(sub);
			else
				remove(sub);
		}
		closedir(d);
	}
#endif
	rmdir(path);
}

static void test_open_close(void)
{
	struct uc2_blockstore bs;
	{ int _r = uc2_blockstore_open(&bs, store_path); (void)_r; assert(_r == 0); }
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
	{ int rc = uc2_merkle_build(&tree, data, sizeof data, 12); assert(rc == 0); }

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
	{ int rc = uc2_merkle_build(&tree, data, sizeof data, 12); assert(rc == 0); }

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
	{ int rc = uc2_merkle_build(&tree, data, sizeof data, 12); assert(rc == 0); }

	struct uc2_blockstore bs;
	uc2_blockstore_open(&bs, store_path);
	uc2_blockstore_ingest(&bs, &tree, data, sizeof data);

	/* Read each chunk back and verify */
	for (int i = 0; i < tree.nchunks; i++) {
		uint8_t buf[65536];
		int n = uc2_blockstore_read(&bs, tree.chunks[i].hash, buf, sizeof buf);
		assert(n == (int)tree.chunks[i].length);
		{ int _r = memcmp(buf, data + tree.chunks[i].offset, n); (void)_r; assert(_r == 0); }
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
	{ int rc = uc2_merkle_build(&t1, shared, shared_len, 12); assert(rc == 0); }

	/* Archive 2: [shared + unique(8KB)] */
	size_t f2_len = shared_len + 8192;
	uint8_t *f2 = malloc(f2_len);
	memcpy(f2, shared, shared_len);
	fill_random(f2 + shared_len, 8192, 0xBEEF);
	struct uc2_merkle t2;
	{ int rc = uc2_merkle_build(&t2, f2, f2_len, 12); assert(rc == 0); }

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
	{ int rc = uc2_merkle_build(&tree, data, sizeof data, 12); assert(rc == 0); }

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

static void test_invalid_ranges(void)
{
	uint8_t data[64] = {0};
	struct uc2_chunk chunks[2] = {
		{uc2_hash64(data, 32), 0, 32},
		{1, UINT32_MAX, 2}
	};
	struct uc2_merkle tree = {chunks, 2, 2, 0};
	struct uc2_blockstore bs;
	int rc = uc2_blockstore_open(&bs, store_path);
	assert(rc == 0);
	rc = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(rc == -1);
	assert(bs.nblocks == 0 && bs.total_bytes == 0 && bs.saved_bytes == 0);
	assert(!uc2_blockstore_has(&bs, chunks[0].hash));
	chunks[1].offset = 0;
	chunks[1].length = 0;
	rc = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(rc == -1);
	uc2_blockstore_close(&bs);
}

static void test_bad_storage_paths(void)
{
	FILE *file = fopen(store_path, "wb");
	assert(file);
	int rc = fclose(file);
	assert(rc == 0);
	struct uc2_blockstore bs;
	rc = uc2_blockstore_open(&bs, store_path);
	assert(rc == -1 && !bs.path);
	rc = remove(store_path);
	assert(rc == 0);
	rc = uc2_blockstore_open(&bs, store_path);
	assert(rc == 0);
	uint8_t data[] = "complete chunk";
	struct uc2_merkle tree;
	rc = uc2_merkle_build(&tree, data, sizeof data, 13);
	assert(rc == 0 && tree.nchunks == 1);
	uint64_t hash = tree.chunks[0].hash;
	char prefix[512], path[512];
	snprintf(prefix, sizeof prefix, "%s/%02x", store_path, (unsigned)(hash >> 56));
	snprintf(path, sizeof path, "%s/%02x/%016llx", store_path,
	         (unsigned)(hash >> 56), (unsigned long long)hash);
	/* A prefix path that is a file must not look like deduplication. */
	file = fopen(prefix, "wb");
	assert(file);
	rc = fclose(file);
	assert(rc == 0);
	rc = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(rc == -1);
	rc = remove(prefix);
	assert(rc == 0);
	rc = create_dir(prefix);
	assert(rc == 0);
	/* Neither a directory nor a short file is a usable stored chunk. */
	rc = create_dir(path);
	assert(rc == 0);
	rc = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(rc == -1 && !uc2_blockstore_has(&bs, hash));
	uint8_t out[sizeof data];
	rc = uc2_blockstore_read(&bs, hash, out, sizeof out);
	assert(rc == -1);
	rc = rmdir(path);
	assert(rc == 0);
	file = fopen(path, "wb");
	assert(file);
	size_t written = fwrite(data, 1, 1, file);
	assert(written == 1);
	rc = fclose(file);
	assert(rc == 0);
	rc = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
	assert(rc == -1);
	assert(bs.nblocks == 0 && bs.total_bytes == 0 && bs.saved_bytes == 0);
	rc = uc2_blockstore_read(&bs, hash, out, (size_t)INT_MAX + 1);
	assert(rc == -1);
	uc2_merkle_free(&tree);
	uc2_blockstore_close(&bs);
}

int main(void)
{
	snprintf(store_path, sizeof store_path, "%s/uc2_blockstore_test_%d",
	         tmpdir(), (int)getpid());

	printf("Block store tests:\n");
	rmtree(store_path);  /* clean start */

	TEST(test_open_close);
	rmtree(store_path);
	TEST(test_ingest_single);
	rmtree(store_path);
	TEST(test_dedup_identical);
	rmtree(store_path);
	TEST(test_read_back);
	rmtree(store_path);
	TEST(test_cross_archive_dedup);
	rmtree(store_path);
	TEST(test_has);
	rmtree(store_path);
	TEST(test_invalid_ranges);
	rmtree(store_path);
	TEST(test_bad_storage_paths);
	rmtree(store_path);

	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
