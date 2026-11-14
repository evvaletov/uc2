/* Tests for streaming dedup ingest (uc2 --ingest). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <assert.h>
#ifdef _MSC_VER
#include <process.h>
#include <direct.h>
#define rmdir _rmdir
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <uc2/uc2_ingest.h>
#include <uc2/uc2_merkle.h>

/* Exercise the private map and force an oversized tree without allocating
 * millions of chunks. Normal cases still use the public Merkle builder.
 * This source definition replaces the static library's ingest object only
 * in this executable; production code needs no fault-injection switches. */
static int forced_chunk_count;
static int test_merkle_build(struct uc2_merkle *tree, const uint8_t *data,
                              size_t len, int bits)
{
	if (forced_chunk_count) {
		memset(tree, 0, sizeof *tree);
		tree->nchunks = forced_chunk_count;
		return 0;
	}
	return uc2_merkle_build(tree, data, len, bits);
}
#define uc2_merkle_build test_merkle_build
#include "../../lib/src/uc2_ingest.c"
#undef uc2_merkle_build

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static char tmp_archive[256];

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

/* Remove the archive and its derived files.  v2 archives never create
 * the .blocks sidecar, so plain remove() covers everything. */
static void cleanup(const char *path)
{
	char buf[320];
	remove(path);
	snprintf(buf, sizeof buf, "%s.out", path);
	remove(buf);
	snprintf(buf, sizeof buf, "%s.blocks", path);
	rmdir(buf);
}

/* fopen that fails the test loudly: assert() is compiled out in
 * Release builds, and continuing with a NULL stream trips the MSVC
 * CRT invalid-parameter fail-fast instead of a test failure. */
static FILE *xfopen(const char *path, const char *mode)
{
	FILE *f = fopen(path, mode);
	if (!f) {
		fprintf(stderr, "FATAL: cannot open %s (mode %s)\n", path, mode);
		exit(1);
	}
	return f;
}

static void fill_random(uint8_t *buf, size_t len, uint32_t seed)
{
	for (size_t i = 0; i < len; i++) {
		seed = seed * 1103515245 + 12345;
		buf[i] = (uint8_t)(seed >> 16);
	}
}

/* Read whole file into a freshly-malloc'd buffer.  Caller frees. */
static uint8_t *slurp(const char *path, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "FATAL: cannot slurp %s\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *buf = malloc(n > 0 ? (size_t)n : 1);
	size_t got = fread(buf, 1, (size_t)n, f);
	fclose(f);
	*out_len = got;
	return buf;
}

static void test_roundtrip_small(void)
{
	cleanup(tmp_archive);
	const char *msg = "hello world";
	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive,
	                          (const uint8_t *)msg, strlen(msg), 0, &st);
	assert(rc == 0);
	(void)rc;
	assert(st.bytes_in == strlen(msg));
	assert(st.chunks_total >= 1);
	assert(st.chunks_new == st.chunks_total);
	assert(st.chunks_dedup == 0);

	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = xfopen(restored, "wb");
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == strlen(msg));
	assert(memcmp(got, msg, got_len) == 0);
	free(got);
	remove(restored);
	cleanup(tmp_archive);
}

static void test_roundtrip_multichunk(void)
{
	cleanup(tmp_archive);
	const size_t N = 200000;
	uint8_t *data = malloc(N);
	fill_random(data, N, 0x12345678);

	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive, data, N, 0, &st);
	assert(rc == 0);
	(void)rc;
	assert(st.bytes_in == N);
	assert(st.chunks_total > 1);  /* CDC should find boundaries in 200 KB */

	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = xfopen(restored, "wb");
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == N);
	assert(memcmp(got, data, N) == 0);

	free(got);
	free(data);
	remove(restored);
	cleanup(tmp_archive);
}

static void test_intra_call_dedup(void)
{
	cleanup(tmp_archive);
	/* Concatenate the same random buffer twice -- CDC produces the
	 * same chunk hashes for both halves, so half the chunks should
	 * dedup within a single ingest call. */
	const size_t HALF = 200000;
	const size_t N = 2 * HALF;
	uint8_t *data = malloc(N);
	fill_random(data, HALF, 0xCAFEBABE);
	memcpy(data + HALF, data, HALF);

	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive, data, N, 0, &st);
	assert(rc == 0);
	(void)rc;
	assert(st.chunks_total > 1);
	assert(st.chunks_dedup > 0);  /* at least some chunks repeat */
	assert(st.bytes_saved > 0);
	assert(st.chunks_new + st.chunks_dedup == st.chunks_total);

	/* Round-trip must still reconstruct the full N bytes -- dedup is
	 * structurally transparent. */
	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = xfopen(restored, "wb");
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == N);
	assert(memcmp(got, data, N) == 0);

	free(got);
	free(data);
	remove(restored);
	cleanup(tmp_archive);
}

static void test_v2_self_contained(void)
{
	cleanup(tmp_archive);
	/* A v2 archive must restore correctly even if the legacy sidecar
	 * blockstore directory is absent.  The chunk pool lives inside
	 * the archive file itself. */
	const size_t N = 50000;
	uint8_t *data = malloc(N);
	fill_random(data, N, 0xDEADBEEF);

	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive, data, N, 0, &st);
	assert(rc == 0);
	(void)rc;

	/* Verify that the .blocks/ sidecar was NOT created for v2. */
	char blocks_path[320];
	snprintf(blocks_path, sizeof blocks_path, "%s.blocks", tmp_archive);
	struct stat sb;
	int stat_rc = stat(blocks_path, &sb);
	assert(stat_rc != 0);  /* blockstore dir must not exist for v2 */
	(void)stat_rc;

	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = xfopen(restored, "wb");
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == N);
	assert(memcmp(got, data, N) == 0);

	free(got);
	free(data);
	remove(restored);
	cleanup(tmp_archive);
}

static void test_empty_stream(void)
{
	cleanup(tmp_archive);
	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive, NULL, 0, 0, &st);
	assert(rc == 0);
	(void)rc;
	assert(st.bytes_in == 0);
	assert(st.chunks_total == 0);

	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = xfopen(restored, "wb");
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == 0);
	free(got);
	remove(restored);
	cleanup(tmp_archive);
}

static void test_bad_magic_rejected(void)
{
	cleanup(tmp_archive);
	FILE *f = xfopen(tmp_archive, "wb");
	const char garbage[16] = "not-a-uc2-ingest";
	fwrite(garbage, 1, sizeof garbage, f);
	fclose(f);

	FILE *out = fopen("/dev/null", "wb");
	if (!out) out = fopen("NUL", "wb");
	if (!out) {
		fprintf(stderr, "FATAL: no null device\n");
		exit(1);
	}
	int rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc != 0);
	(void)rc;
	cleanup(tmp_archive);
}

static void test_v1_oversized_chunk(void)
{
	uint8_t record[28] = {'U','C','2','I','N','G','S','T',1};
	record[12]=1; /* one manifest record */
	memset(record+24,0xff,4); /* UINT32_MAX, not an int-sized read */
	FILE *f=xfopen(tmp_archive,"wb");
	size_t written=fwrite(record,1,sizeof record,f); assert(written==sizeof record);
	fclose(f);
	FILE *out=tmpfile(); assert(out);
	int rc=uc2_ingest_restore(tmp_archive,out);
	assert(rc<0 && ftell(out)==0);
	fclose(out); cleanup(tmp_archive);
}

static void test_zero_and_one_hashes(void)
{
	for (int reverse = 0; reverse < 2; reverse++) {
		struct dedup_map map;
		int rc = dedup_map_init(&map, 16);
		assert(rc == 0);
		uint32_t offset = 999;
		assert(!dedup_map_get(&map, 0, &offset));
		assert(!dedup_map_get(&map, 1, &offset));
		assert(offset == 999);
		for (int i = 0; i < 2; i++) {
			uint64_t hash = (uint64_t)(i ^ reverse);
			rc = dedup_map_put(&map, hash, (uint32_t)(100 + hash));
			assert(rc == 0);
		}
		/* Repeated insertion retains the first offset, including hash 0. */
		rc = dedup_map_put(&map, 0, 700); assert(rc == 0);
		rc = dedup_map_put(&map, 1, 701); assert(rc == 0);
		for (uint64_t hash = 2; hash < 300; hash++) {
			rc = dedup_map_put(&map, hash, (uint32_t)(100 + hash));
			assert(rc == 0);
		}
		assert(map.cap > 16);
		for (uint64_t hash = 0; hash < 300; hash++) {
			rc = dedup_map_get(&map, hash, &offset);
			assert(rc == 1 && offset == 100 + hash);
		}
		dedup_map_free(&map);
	}
}

static void test_chunk_limit_preserves_destination(void)
{
	const int counts[] = {-1, (1 << 24) + 1};
	for (size_t i = 0; i < sizeof counts / sizeof *counts; i++) {
		cleanup(tmp_archive);
		FILE *file = xfopen(tmp_archive, "wb");
		const char original[] = "keep original archive";
		size_t written = fwrite(original, 1, sizeof original, file);
		assert(written == sizeof original);
		int rc = fclose(file); assert(rc == 0);
		forced_chunk_count = counts[i];
		rc = uc2_ingest_write(tmp_archive, (const uint8_t *)"x", 1, 13, NULL);
		forced_chunk_count = 0;
		assert(rc == -1);
		size_t size;
		uint8_t *saved = slurp(tmp_archive, &size);
		assert(size == sizeof original && !memcmp(saved, original, size));
		free(saved);
		cleanup(tmp_archive);
		forced_chunk_count = counts[i];
		rc = uc2_ingest_write(tmp_archive, (const uint8_t *)"x", 1, 13, NULL);
		forced_chunk_count = 0;
		assert(rc == -1);
		struct stat st;
		rc = stat(tmp_archive, &st);
		assert(rc != 0);
	}
}

int main(void)
{
	snprintf(tmp_archive, sizeof tmp_archive,
	         "%s/uc2_ingest_test_%d.uc2", tmpdir(), (int)getpid());

	printf("Running uc2_ingest tests...\n");
	TEST(test_roundtrip_small);
	TEST(test_roundtrip_multichunk);
	TEST(test_intra_call_dedup);
	TEST(test_v2_self_contained);
	TEST(test_empty_stream);
	TEST(test_bad_magic_rejected);
	TEST(test_v1_oversized_chunk);
	TEST(test_zero_and_one_hashes);
	TEST(test_chunk_limit_preserves_destination);
	printf("Passed: %d/%d\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
