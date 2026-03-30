/* Content-aware preprocessing filters. */

#include "uc2/uc2_preprocess.h"
#include <stdlib.h>
#include <string.h>

/* --- BCJ (E8/E9 transform for x86) --- */

/* Convert relative CALL (E8) and JMP (E9) addresses to absolute.
 * The 4-byte displacement after E8/E9 is replaced with an absolute
 * address relative to position 0.  This normalizes calls to the same
 * function from different locations, improving LZ77 matching. */

int uc2_bcj_apply(uint8_t *data, size_t len)
{
	if (len < 5) return 0;
	for (size_t i = 0; i + 4 < len; i++) {
		if (data[i] == 0xE8 || data[i] == 0xE9) {
			int32_t rel = (int32_t)(data[i+1] | (data[i+2] << 8) |
			              (data[i+3] << 16) | (data[i+4] << 24));
			int32_t abs_addr = rel + (int32_t)(i + 5);
			data[i+1] = (uint8_t)(abs_addr);
			data[i+2] = (uint8_t)(abs_addr >> 8);
			data[i+3] = (uint8_t)(abs_addr >> 16);
			data[i+4] = (uint8_t)(abs_addr >> 24);
			i += 4;  /* skip the address bytes */
		}
	}
	return 0;
}

int uc2_bcj_revert(uint8_t *data, size_t len)
{
	if (len < 5) return 0;
	for (size_t i = 0; i + 4 < len; i++) {
		if (data[i] == 0xE8 || data[i] == 0xE9) {
			int32_t abs_addr = (int32_t)(data[i+1] | (data[i+2] << 8) |
			                   (data[i+3] << 16) | (data[i+4] << 24));
			int32_t rel = abs_addr - (int32_t)(i + 5);
			data[i+1] = (uint8_t)(rel);
			data[i+2] = (uint8_t)(rel >> 8);
			data[i+3] = (uint8_t)(rel >> 16);
			data[i+4] = (uint8_t)(rel >> 24);
			i += 4;
		}
	}
	return 0;
}

/* --- BWT (Burrows-Wheeler Transform) --- */

/* Simple BWT using suffix array (O(n log^2 n) via qsort). */

static const uint8_t *bwt_data;
static size_t bwt_len;

static int bwt_cmp(const void *a, const void *b)
{
	uint32_t ia = *(const uint32_t *)a;
	uint32_t ib = *(const uint32_t *)b;
	for (size_t k = 0; k < bwt_len; k++) {
		uint8_t ca = bwt_data[(ia + k) % bwt_len];
		uint8_t cb = bwt_data[(ib + k) % bwt_len];
		if (ca != cb) return (int)ca - (int)cb;
	}
	return 0;
}

int uc2_bwt_apply(const uint8_t *data, size_t len,
                  uint8_t **out, uint32_t *primary_index)
{
	if (len == 0) { *out = NULL; *primary_index = 0; return 0; }

	uint32_t *sa = malloc(len * sizeof(uint32_t));
	uint8_t *result = malloc(len);
	if (!sa || !result) { free(sa); free(result); return -1; }

	for (size_t i = 0; i < len; i++) sa[i] = (uint32_t)i;
	bwt_data = data;
	bwt_len = len;
	qsort(sa, len, sizeof(uint32_t), bwt_cmp);

	*primary_index = 0;
	for (size_t i = 0; i < len; i++) {
		if (sa[i] == 0) *primary_index = (uint32_t)i;
		result[i] = data[(sa[i] + len - 1) % len];
	}

	free(sa);
	*out = result;
	return 0;
}

int uc2_bwt_revert(const uint8_t *data, size_t len,
                   uint32_t primary_index, uint8_t **out)
{
	if (len == 0) { *out = NULL; return 0; }

	uint8_t *result = malloc(len);
	uint32_t *T = malloc(len * sizeof(uint32_t));
	if (!result || !T) { free(result); free(T); return -1; }

	/* Build the LF-mapping (Last-to-First column mapping).
	   T[i] = position in first column corresponding to last column position i. */
	uint32_t count[256];
	memset(count, 0, sizeof count);
	for (size_t i = 0; i < len; i++) count[data[i]]++;

	uint32_t sum = 0;
	uint32_t start[256];
	for (int c = 0; c < 256; c++) {
		start[c] = sum;
		sum += count[c];
	}

	/* Reset count for building T */
	memset(count, 0, sizeof count);
	for (size_t i = 0; i < len; i++) {
		T[i] = start[data[i]] + count[data[i]];
		count[data[i]]++;
	}

	/* Reconstruct: follow T from primary_index, reading in reverse */
	uint32_t idx = primary_index;
	for (size_t i = len; i > 0; i--) {
		result[i - 1] = data[idx];
		idx = T[idx];
	}

	free(T);
	*out = result;
	return 0;
}

/* --- Delta filter --- */

void uc2_delta_filter_apply(uint8_t *data, size_t len, int stride)
{
	if (stride < 1) stride = 1;
	/* Process from end to start to avoid overwriting needed values */
	for (size_t i = len; i > (size_t)stride; ) {
		i--;
		data[i] = (uint8_t)(data[i] - data[i - stride]);
	}
}

void uc2_delta_filter_revert(uint8_t *data, size_t len, int stride)
{
	if (stride < 1) stride = 1;
	for (size_t i = (size_t)stride; i < len; i++)
		data[i] = (uint8_t)(data[i] + data[i - stride]);
}

/* --- Content detection --- */

int uc2_detect_content(const uint8_t *data, size_t len)
{
	if (len < 4) return UC2_CONTENT_BINARY;

	/* Check for x86 executable signatures */
	if (data[0] == 'M' && data[1] == 'Z')
		return UC2_CONTENT_X86;  /* DOS/PE executable */
	if (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
		return UC2_CONTENT_X86;  /* ELF executable */

	/* Count printable ASCII characters */
	size_t check = len > 4096 ? 4096 : len;
	size_t printable = 0;
	for (size_t i = 0; i < check; i++)
		if ((data[i] >= 32 && data[i] <= 126) ||
		    data[i] == '\n' || data[i] == '\r' || data[i] == '\t')
			printable++;

	if (printable * 100 / check > 85)
		return UC2_CONTENT_TEXT;

	/* Check for structured data: regular byte-value patterns */
	if (len >= 64) {
		size_t zeros = 0;
		for (size_t i = 0; i < check; i++)
			if (data[i] == 0) zeros++;
		if (zeros * 100 / check > 20)
			return UC2_CONTENT_STRUCT;
	}

	return UC2_CONTENT_BINARY;
}
