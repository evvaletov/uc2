/* Tests for rANS entropy coder. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <uc2/uc2_rans.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_build_table(void)
{
	uint32_t freq[] = {100, 50, 25, 10, 5, 1};
	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 6);

	/* Frequencies should sum to PROB_SCALE */
	uint32_t sum = 0;
	for (int i = 0; i < 6; i++) {
		assert(tab.freq[i] > 0);
		sum += tab.freq[i];
	}
	assert(sum == UC2_RANS_PROB_SCALE);

	/* Cumulative should be monotonic */
	assert(tab.cumfreq[0] == 0);
	for (int i = 1; i < 6; i++)
		assert(tab.cumfreq[i] == tab.cumfreq[i-1] + tab.freq[i-1]);
}

static void test_roundtrip_uniform(void)
{
	/* Uniform distribution: all symbols equally likely */
	uint32_t freq[8];
	for (int i = 0; i < 8; i++) freq[i] = 100;
	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 8);

	/* Encode 100 symbols */
	int syms[100];
	for (int i = 0; i < 100; i++) syms[i] = i % 8;

	struct uc2_rans_enc enc;
	uc2_rans_enc_init(&enc, &tab);
	/* rANS encodes in reverse order */
	for (int i = 99; i >= 0; i--)
		uc2_rans_encode(&enc, syms[i]);
	uint8_t *data;
	size_t len = uc2_rans_enc_finish(&enc, &data);
	uc2_rans_enc_free(&enc);

	printf("(encoded %zu bytes for 100 syms) ", len);

	/* Decode and verify */
	struct uc2_rans_dec dec;
	uc2_rans_dec_init(&dec, &tab, data, len);
	for (int i = 0; i < 100; i++) {
		int s = uc2_rans_decode(&dec);
		assert(s == syms[i]);
	}

	free(data);
}

static void test_roundtrip_skewed(void)
{
	/* Highly skewed: symbol 0 dominates */
	uint32_t freq[4] = {1000, 10, 5, 1};
	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 4);

	int syms[200];
	for (int i = 0; i < 200; i++)
		syms[i] = (i < 180) ? 0 : (i < 195) ? 1 : (i < 199) ? 2 : 3;

	struct uc2_rans_enc enc;
	uc2_rans_enc_init(&enc, &tab);
	for (int i = 199; i >= 0; i--)
		uc2_rans_encode(&enc, syms[i]);
	uint8_t *data;
	size_t len = uc2_rans_enc_finish(&enc, &data);
	uc2_rans_enc_free(&enc);

	/* Skewed data should compress well */
	double entropy = 0;
	double total = 1016.0;
	double p[] = {1000.0/total, 10.0/total, 5.0/total, 1.0/total};
	for (int i = 0; i < 4; i++)
		if (p[i] > 0) entropy -= p[i] * log2(p[i]);
	double ideal = entropy * 200 / 8;  /* ideal bytes */
	printf("(encoded %zu bytes, ideal ~%.0f) ", len, ideal);
	assert(len < 200);  /* must compress (200 symbols × ~0.1 bits each) */

	struct uc2_rans_dec dec;
	uc2_rans_dec_init(&dec, &tab, data, len);
	for (int i = 0; i < 200; i++)
		assert(uc2_rans_decode(&dec) == syms[i]);

	free(data);
}

static void test_roundtrip_large_alphabet(void)
{
	/* 344-symbol alphabet (matching UC2's symbol count) */
	uint32_t freq[344];
	for (int i = 0; i < 344; i++)
		freq[i] = (i < 128) ? 50 : (i < 256) ? 10 : (i < 316) ? 5 : 2;

	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 344);

	int nsyms = 500;
	int *syms = malloc(nsyms * sizeof(int));
	uint32_t rng = 0xDEAD;
	for (int i = 0; i < nsyms; i++) {
		rng = rng * 1103515245 + 12345;
		/* Weighted random: bias toward lower symbols */
		int s = (rng >> 16) % 344;
		syms[i] = s;
	}

	struct uc2_rans_enc enc;
	uc2_rans_enc_init(&enc, &tab);
	for (int i = nsyms - 1; i >= 0; i--)
		uc2_rans_encode(&enc, syms[i]);
	uint8_t *data;
	size_t len = uc2_rans_enc_finish(&enc, &data);
	uc2_rans_enc_free(&enc);

	printf("(344-sym, %d syms -> %zu bytes) ", nsyms, len);

	struct uc2_rans_dec dec;
	uc2_rans_dec_init(&dec, &tab, data, len);
	for (int i = 0; i < nsyms; i++)
		assert(uc2_rans_decode(&dec) == syms[i]);

	free(syms);
	free(data);
}

static void test_single_symbol(void)
{
	/* Only one symbol: should compress to nearly nothing */
	uint32_t freq[4] = {100, 0, 0, 0};
	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 4);

	struct uc2_rans_enc enc;
	uc2_rans_enc_init(&enc, &tab);
	for (int i = 0; i < 100; i++)
		uc2_rans_encode(&enc, 0);
	uint8_t *data;
	size_t len = uc2_rans_enc_finish(&enc, &data);
	uc2_rans_enc_free(&enc);

	printf("(100 same syms -> %zu bytes) ", len);
	assert(len <= 8);  /* near-zero information */

	struct uc2_rans_dec dec;
	uc2_rans_dec_init(&dec, &tab, data, len);
	for (int i = 0; i < 100; i++)
		assert(uc2_rans_decode(&dec) == 0);

	free(data);
}

static void test_vs_huffman_size(void)
{
	/* Compare rANS vs theoretical Huffman for skewed distribution.
	   rANS should be closer to Shannon entropy. */
	uint32_t freq[8] = {500, 200, 100, 80, 50, 30, 20, 20};
	struct uc2_rans_table tab;
	uc2_rans_build_table(&tab, freq, 8);

	int nsyms = 1000;
	int *syms = malloc(nsyms * sizeof(int));
	/* Generate symbols matching the frequency distribution */
	int idx = 0;
	for (int s = 0; s < 8 && idx < nsyms; s++)
		for (uint32_t j = 0; j < freq[s] && idx < nsyms; j++)
			syms[idx++] = s;

	struct uc2_rans_enc enc;
	uc2_rans_enc_init(&enc, &tab);
	for (int i = nsyms - 1; i >= 0; i--)
		uc2_rans_encode(&enc, syms[i]);
	uint8_t *data;
	size_t len = uc2_rans_enc_finish(&enc, &data);
	uc2_rans_enc_free(&enc);

	/* Shannon entropy for this distribution */
	double total = 1000.0;
	double entropy = 0;
	for (int i = 0; i < 8; i++) {
		double p = freq[i] / total;
		if (p > 0) entropy -= p * log2(p);
	}
	double ideal_bytes = entropy * nsyms / 8;
	double overhead = ((double)len - ideal_bytes) / ideal_bytes * 100;
	printf("(rANS=%zu ideal=%.0f overhead=%.1f%%) ", len, ideal_bytes, overhead);
	assert(overhead < 5.0);  /* rANS should be within 5% of Shannon */

	/* Verify round-trip */
	struct uc2_rans_dec dec;
	uc2_rans_dec_init(&dec, &tab, data, len);
	for (int i = 0; i < nsyms; i++)
		assert(uc2_rans_decode(&dec) == syms[i]);

	free(syms);
	free(data);
}

int main(void)
{
	printf("rANS tests:\n");
	TEST(test_build_table);
	TEST(test_roundtrip_uniform);
	TEST(test_roundtrip_skewed);
	TEST(test_roundtrip_large_alphabet);
	TEST(test_single_symbol);
	TEST(test_vs_huffman_size);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
