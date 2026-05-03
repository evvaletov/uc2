/* Tests for SHA-256 (FIPS 180-4 vectors). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_sha256.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void hex(const uint8_t *h, int n, char *out)
{
	for (int i = 0; i < n; i++) sprintf(out + i*2, "%02x", h[i]);
	out[n*2] = 0;
}

static int hex_eq(const uint8_t *h, int n, const char *want)
{
	char got[65];
	hex(h, n, got);
	return strcmp(got, want) == 0;
}

static void test_empty(void)
{
	uint8_t h[32];
	uc2_sha256_hash("", 0, h);
	assert(hex_eq(h, 32,
		"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
}

static void test_abc(void)
{
	uint8_t h[32];
	uc2_sha256_hash("abc", 3, h);
	assert(hex_eq(h, 32,
		"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

static void test_56_byte(void)
{
	const char *m = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	uint8_t h[32];
	uc2_sha256_hash(m, strlen(m), h);
	assert(hex_eq(h, 32,
		"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"));
}

static void test_million_a(void)
{
	struct uc2_sha256 ctx;
	uc2_sha256_init(&ctx);
	uint8_t buf[1000];
	memset(buf, 'a', sizeof buf);
	for (int i = 0; i < 1000; i++)
		uc2_sha256_update(&ctx, buf, sizeof buf);
	uint8_t h[32];
	uc2_sha256_final(&ctx, h);
	assert(hex_eq(h, 32,
		"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"));
}

static void test_incremental(void)
{
	const char *m = "The quick brown fox jumps over the lazy dog";
	size_t len = strlen(m);

	uint8_t oneshot[32];
	uc2_sha256_hash(m, len, oneshot);

	struct uc2_sha256 ctx;
	uc2_sha256_init(&ctx);
	for (size_t i = 0; i < len; i++)
		uc2_sha256_update(&ctx, m + i, 1);
	uint8_t piecemeal[32];
	uc2_sha256_final(&ctx, piecemeal);

	assert(memcmp(oneshot, piecemeal, 32) == 0);
}

static void test_block_boundaries(void)
{
	uint8_t buf[200];
	for (size_t i = 0; i < sizeof buf; i++) buf[i] = (uint8_t)(i & 0xFF);

	uint8_t oneshot[32];
	uc2_sha256_hash(buf, sizeof buf, oneshot);

	for (size_t split = 1; split < sizeof buf; split++) {
		struct uc2_sha256 ctx;
		uc2_sha256_init(&ctx);
		uc2_sha256_update(&ctx, buf, split);
		uc2_sha256_update(&ctx, buf + split, sizeof buf - split);
		uint8_t h[32];
		uc2_sha256_final(&ctx, h);
		assert(memcmp(oneshot, h, 32) == 0);
	}
}

int main(void)
{
	printf("SHA-256 tests:\n");
	TEST(test_empty);
	TEST(test_abc);
	TEST(test_56_byte);
	TEST(test_million_a);
	TEST(test_incremental);
	TEST(test_block_boundaries);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
