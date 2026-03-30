/* Tests for content-aware preprocessing filters. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_preprocess.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

/* --- BCJ tests --- */

static void test_bcj_roundtrip(void)
{
	/* Simulate x86 code with E8 (CALL) instructions */
	uint8_t code[] = {
		0x90,                         /* NOP */
		0xE8, 0x10, 0x00, 0x00, 0x00, /* CALL +16 (relative) */
		0x90,                         /* NOP */
		0xE8, 0x20, 0x00, 0x00, 0x00, /* CALL +32 (relative) */
		0x90, 0x90, 0x90, 0x90,       /* NOPs */
	};
	uint8_t orig[sizeof code];
	memcpy(orig, code, sizeof code);

	uc2_bcj_apply(code, sizeof code);
	/* After apply, the relative addresses should be absolute */
	assert(memcmp(code, orig, sizeof code) != 0);

	uc2_bcj_revert(code, sizeof code);
	assert(memcmp(code, orig, sizeof code) == 0);
}

static void test_bcj_normalizes(void)
{
	/* Two different calls to the same target from different positions.
	   After BCJ, both should have the same absolute address. */
	uint8_t a[] = { 0xE8, 0x0A, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90, 0x90, 0x90 };
	uint8_t b[] = { 0x90, 0x90, 0xE8, 0x07, 0x00, 0x00, 0x00, 0x90, 0x90, 0x90 };
	/* Both call offset 15 from start: a: 5+10=15, b: 7+8=15... let me compute:
	   a at pos 0: rel=10, abs=10+5=15
	   b at pos 2: rel=7, abs=7+7=14... not same. Adjust: */
	/* a: E8 at pos 0, rel=0x0A=10, abs=10+5=15 → target 15
	   b: E8 at pos 2, rel=0x0A=10, abs=10+7=17 → target 17
	   For same target (15): b needs rel=15-7=8 → 0x08 */
	b[3] = 0x08; b[4] = 0x00; b[5] = 0x00; b[6] = 0x00;
	/* Now both target absolute address 15 */

	uc2_bcj_apply(a, sizeof a);
	uc2_bcj_apply(b, sizeof b);

	/* Both should now have abs=15 in the displacement bytes */
	int32_t abs_a = a[1] | (a[2]<<8) | (a[3]<<16) | (a[4]<<24);
	int32_t abs_b = b[3] | (b[4]<<8) | (b[5]<<16) | (b[6]<<24);
	assert(abs_a == 15);
	assert(abs_b == 15);
}

static void test_bcj_short_data(void)
{
	uint8_t data[] = { 0xE8, 0x01 };
	uc2_bcj_apply(data, 2);  /* too short, no transform */
	assert(data[0] == 0xE8 && data[1] == 0x01);
}

/* --- BWT tests --- */

static void test_bwt_roundtrip(void)
{
	uint8_t data[] = "banana";
	size_t len = 6;
	uint8_t *bwt;
	uint32_t pidx;
	assert(uc2_bwt_apply(data, len, &bwt, &pidx) == 0);

	/* BWT of "banana" is well-known: "nnbaaa" with primary index at 3 */
	printf("(bwt='%.*s' idx=%u) ", (int)len, bwt, pidx);

	uint8_t *orig;
	assert(uc2_bwt_revert(bwt, len, pidx, &orig) == 0);
	assert(memcmp(orig, data, len) == 0);

	free(bwt);
	free(orig);
}

static void test_bwt_roundtrip_binary(void)
{
	size_t len = 256;
	uint8_t *data = malloc(len);
	for (size_t i = 0; i < len; i++) data[i] = (uint8_t)(i * 37 + 13);

	uint8_t *bwt;
	uint32_t pidx;
	assert(uc2_bwt_apply(data, len, &bwt, &pidx) == 0);

	uint8_t *orig;
	assert(uc2_bwt_revert(bwt, len, pidx, &orig) == 0);
	assert(memcmp(orig, data, len) == 0);

	free(data);
	free(bwt);
	free(orig);
}

/* --- Delta filter tests --- */

static void test_delta_roundtrip(void)
{
	uint8_t data[] = {10, 12, 14, 16, 18, 20, 22, 24};
	uint8_t orig[sizeof data];
	memcpy(orig, data, sizeof data);

	uc2_delta_filter_apply(data, sizeof data, 1);
	/* After delta: differences should be constant (2) for arithmetic sequence */
	for (size_t i = 1; i < sizeof data; i++)
		assert(data[i] == 2);

	uc2_delta_filter_revert(data, sizeof data, 1);
	assert(memcmp(data, orig, sizeof data) == 0);
}

static void test_delta_stride2(void)
{
	/* Interleaved stereo: L0 R0 L1 R1 L2 R2 ... */
	uint8_t data[] = {100, 200, 102, 202, 104, 204, 106, 206};
	uint8_t orig[sizeof data];
	memcpy(orig, data, sizeof data);

	uc2_delta_filter_apply(data, sizeof data, 2);
	/* With stride 2: each channel has constant delta of 2 */
	assert(data[2] == 2 && data[3] == 2);
	assert(data[4] == 2 && data[5] == 2);

	uc2_delta_filter_revert(data, sizeof data, 2);
	assert(memcmp(data, orig, sizeof data) == 0);
}

/* --- Content detection tests --- */

static void test_detect_text(void)
{
	uint8_t data[] = "This is plain text content with newlines\n"
	                 "and more text on the second line.\n";
	assert(uc2_detect_content(data, sizeof data - 1) == UC2_CONTENT_TEXT);
}

static void test_detect_x86_mz(void)
{
	uint8_t data[] = {'M', 'Z', 0x90, 0x00};
	assert(uc2_detect_content(data, sizeof data) == UC2_CONTENT_X86);
}

static void test_detect_x86_elf(void)
{
	uint8_t data[] = {0x7F, 'E', 'L', 'F', 0x02};
	assert(uc2_detect_content(data, sizeof data) == UC2_CONTENT_X86);
}

static void test_detect_binary(void)
{
	uint8_t data[64];
	for (int i = 0; i < 64; i++) data[i] = (uint8_t)(i * 7);
	assert(uc2_detect_content(data, sizeof data) == UC2_CONTENT_BINARY);
}

int main(void)
{
	printf("Preprocessing filter tests:\n");
	TEST(test_bcj_roundtrip);
	TEST(test_bcj_normalizes);
	TEST(test_bcj_short_data);
	TEST(test_bwt_roundtrip);
	TEST(test_bwt_roundtrip_binary);
	TEST(test_delta_roundtrip);
	TEST(test_delta_stride2);
	TEST(test_detect_text);
	TEST(test_detect_x86_mz);
	TEST(test_detect_x86_elf);
	TEST(test_detect_binary);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
