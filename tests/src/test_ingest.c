/* Tests for streaming dedup ingest (uc2 --ingest). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#ifdef _MSC_VER
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <uc2/uc2_ingest.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static char tmp_archive[256];

static void rmrf(const char *path)
{
	char cmd[768];
	snprintf(cmd, sizeof cmd, "rm -rf '%s' '%s.blocks'", path, path);
	system(cmd);
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
	if (!f) return NULL;
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
	rmrf(tmp_archive);
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
	FILE *out = fopen(restored, "wb");
	assert(out);
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == strlen(msg));
	assert(memcmp(got, msg, got_len) == 0);
	free(got);
	unlink(restored);
	rmrf(tmp_archive);
}

static void test_roundtrip_multichunk(void)
{
	rmrf(tmp_archive);
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
	FILE *out = fopen(restored, "wb");
	assert(out);
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == N);
	assert(memcmp(got, data, N) == 0);

	free(got);
	free(data);
	unlink(restored);
	rmrf(tmp_archive);
}

static void test_dedup_idempotent(void)
{
	rmrf(tmp_archive);
	/* Repeated short pattern -> stable CDC boundaries -> high dedup. */
	const size_t N = 200000;
	uint8_t *data = malloc(N);
	const char *pattern = "the quick brown fox jumps over the lazy dog\n";
	size_t plen = strlen(pattern);
	for (size_t i = 0; i < N; i++) data[i] = (uint8_t)pattern[i % plen];

	struct uc2_ingest_stats st1, st2;
	int rc = uc2_ingest_write(tmp_archive, data, N, 0, &st1);
	assert(rc == 0);
	(void)rc;
	assert(st1.chunks_new == st1.chunks_total);
	assert(st1.chunks_dedup == 0);

	rc = uc2_ingest_write(tmp_archive, data, N, 0, &st2);
	assert(rc == 0);
	assert(st2.chunks_total == st1.chunks_total);
	assert(st2.chunks_new == 0);
	assert(st2.chunks_dedup == st2.chunks_total);
	assert(st2.bytes_saved == (uint64_t)N);

	free(data);
	rmrf(tmp_archive);
}

static void test_empty_stream(void)
{
	rmrf(tmp_archive);
	struct uc2_ingest_stats st;
	int rc = uc2_ingest_write(tmp_archive, NULL, 0, 0, &st);
	assert(rc == 0);
	(void)rc;
	assert(st.bytes_in == 0);
	assert(st.chunks_total == 0);

	char restored[320];
	snprintf(restored, sizeof restored, "%s.out", tmp_archive);
	FILE *out = fopen(restored, "wb");
	assert(out);
	rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc == 0);

	size_t got_len;
	uint8_t *got = slurp(restored, &got_len);
	assert(got_len == 0);
	free(got);
	unlink(restored);
	rmrf(tmp_archive);
}

static void test_bad_magic_rejected(void)
{
	rmrf(tmp_archive);
	FILE *f = fopen(tmp_archive, "wb");
	assert(f);
	const char garbage[16] = "not-a-uc2-ingest";
	fwrite(garbage, 1, sizeof garbage, f);
	fclose(f);

	FILE *out = fopen("/dev/null", "wb");
#ifdef _MSC_VER
	if (!out) out = fopen("NUL", "wb");
#endif
	assert(out);
	int rc = uc2_ingest_restore(tmp_archive, out);
	fclose(out);
	assert(rc != 0);
	(void)rc;
	rmrf(tmp_archive);
}

int main(void)
{
	snprintf(tmp_archive, sizeof tmp_archive,
	         "/tmp/uc2_ingest_test_%d.uc2", (int)getpid());

	printf("Running uc2_ingest tests...\n");
	TEST(test_roundtrip_small);
	TEST(test_roundtrip_multichunk);
	TEST(test_dedup_idempotent);
	TEST(test_empty_stream);
	TEST(test_bad_magic_rejected);
	printf("Passed: %d/%d\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
