/* SPDX-License-Identifier: GPL-3.0-or-later */

/* rANS (range Asymmetric Numeral Systems) entropy coder.
 *
 * Table-based rANS with 32-bit state.  The state represents a position
 * in a virtual number line partitioned proportionally to symbol
 * frequencies.  Encoding maps the state forward (growing), decoding
 * maps it backward (shrinking).
 *
 * Key properties vs Huffman:
 * - Fractional bit costs: symbols can use e.g. 2.3 bits (not rounded to 3)
 * - 5-15% better on skewed distributions (many symbols with freq < 2^-N)
 * - Same O(1) encode/decode per symbol with lookup tables
 *
 * Reference: Duda, "Asymmetric Numeral Systems" (2009). */

#include "uc2/uc2_rans.h"
#include <stdlib.h>
#include <string.h>

#define RANS_L (1u << 23)  /* lower bound of state range */

void uc2_rans_build_table(struct uc2_rans_table *tab,
                          const uint32_t *raw_freq, int nsym)
{
	if (nsym > UC2_RANS_MAX_SYMS)
		nsym = UC2_RANS_MAX_SYMS;
	tab->nsym = nsym;

	/* Sum raw frequencies */
	uint64_t total = 0;
	for (int i = 0; i < nsym; i++)
		total += raw_freq[i];

	if (total == 0) {
		memset(tab->freq, 0, sizeof tab->freq);
		memset(tab->cumfreq, 0, sizeof tab->cumfreq);
		return;
	}

	/* Scale to PROB_SCALE, ensuring every non-zero symbol gets freq >= 1 */
	uint32_t assigned = 0;
	for (int i = 0; i < nsym; i++) {
		if (raw_freq[i] == 0) {
			tab->freq[i] = 0;
		} else {
			uint32_t f = (uint32_t)((uint64_t)raw_freq[i] * UC2_RANS_PROB_SCALE / total);
			if (f == 0) f = 1;
			tab->freq[i] = (uint16_t)f;
			assigned += f;
		}
	}

	/* Adjust without taking a present symbol below one. A skewed
	 * distribution can require more correction than one symbol holds. */
	while (assigned != UC2_RANS_PROB_SCALE) {
		int largest = 0;
		for (int i = 1; i < nsym; i++)
			if (tab->freq[i] > tab->freq[largest])
				largest = i;
		if (assigned < UC2_RANS_PROB_SCALE) {
			tab->freq[largest] += UC2_RANS_PROB_SCALE - assigned;
			assigned = UC2_RANS_PROB_SCALE;
		} else {
			uint32_t take = assigned - UC2_RANS_PROB_SCALE;
			if (take >= tab->freq[largest]) take = tab->freq[largest] - 1;
			tab->freq[largest] -= take;
			assigned -= take;
		}
	}

	/* Build cumulative frequencies */
	tab->cumfreq[0] = 0;
	for (int i = 1; i < nsym; i++)
		tab->cumfreq[i] = tab->cumfreq[i - 1] + tab->freq[i - 1];
}

/* --- Encoder --- */

static int enc_grow(struct uc2_rans_enc *enc)
{
	if (enc->rev_cap > SIZE_MAX / 2) { enc->error = 1; return -1; }
	size_t newcap = enc->rev_cap ? enc->rev_cap * 2 : 4096;
	uint8_t *buf = realloc(enc->rev_buf, newcap);
	if (!buf) { enc->error = 1; return -1; }
	enc->rev_buf = buf;
	enc->rev_cap = newcap;
	return 0;
}

static void enc_put_byte(struct uc2_rans_enc *enc, uint8_t b)
{
	if (enc->error) return;
	if (enc->rev_pos >= enc->rev_cap && enc_grow(enc) < 0) return;
	enc->rev_buf[enc->rev_pos++] = b;
}

void uc2_rans_enc_init(struct uc2_rans_enc *enc,
                       const struct uc2_rans_table *tab)
{
	enc->state = RANS_L;
	enc->tab = tab;
	enc->rev_buf = NULL;
	enc->rev_pos = 0;
	enc->rev_cap = 0;
	enc->error = 0;
}

void uc2_rans_encode(struct uc2_rans_enc *enc, int sym)
{
	if (enc->error) return;
	uint32_t freq = enc->tab->freq[sym];
	if (freq == 0) return;  /* skip zero-freq symbols */

	/* Renormalize: output bytes until state is in range */
	uint32_t upper = ((RANS_L >> UC2_RANS_PROB_BITS) << 8) * freq;
	while (enc->state >= upper) {
		enc_put_byte(enc, (uint8_t)(enc->state & 0xFF));
		enc->state >>= 8;
	}

	/* Encode: state = (state / freq) * PROB_SCALE + cumfreq + (state % freq) */
	uint32_t cumfreq = enc->tab->cumfreq[sym];
	enc->state = ((enc->state / freq) << UC2_RANS_PROB_BITS) +
	             cumfreq + (enc->state % freq);
}

size_t uc2_rans_enc_finish(struct uc2_rans_enc *enc, uint8_t **out_data)
{
	*out_data = NULL;
	if (enc->error) return 0;
	/* Write final state (4 bytes, little-endian) */
	for (int i = 0; i < 4; i++) {
		enc_put_byte(enc, (uint8_t)(enc->state & 0xFF));
		enc->state >>= 8;
	}
	if (enc->error) return 0;

	/* Reverse the buffer (rANS produces output in reverse) */
	size_t len = enc->rev_pos;
	uint8_t *out = malloc(len);
	if (!out) { enc->error = 1; return 0; }
	{
		for (size_t i = 0; i < len; i++)
			out[i] = enc->rev_buf[len - 1 - i];
	}

	*out_data = out;
	return len;
}

void uc2_rans_enc_free(struct uc2_rans_enc *enc)
{
	free(enc->rev_buf);
	enc->rev_buf = NULL;
	enc->rev_pos = 0;
	enc->rev_cap = 0;
}

/* --- Decoder --- */

void uc2_rans_dec_init(struct uc2_rans_dec *dec,
                       const struct uc2_rans_table *tab,
                       const uint8_t *data, size_t len)
{
	dec->tab = tab;
	dec->data = data;
	dec->len = len;
	dec->pos = 0;

	/* Build reverse lookup table: cumfreq → symbol */
	memset(dec->lookup, 0, sizeof dec->lookup);
	for (int s = 0; s < tab->nsym; s++)
		for (uint32_t i = tab->cumfreq[s];
		     i < tab->cumfreq[s] + tab->freq[s] && i < UC2_RANS_PROB_SCALE; i++)
			dec->lookup[i] = (uint16_t)s;

	/* Read initial state (4 bytes, little-endian) */
	dec->state = 0;
	for (int i = 3; i >= 0; i--) {
		dec->state <<= 8;
		if (dec->pos < len)
			dec->state |= data[dec->pos++];
	}
}

int uc2_rans_decode(struct uc2_rans_dec *dec)
{
	/* Find symbol from state */
	uint32_t slot = dec->state & (UC2_RANS_PROB_SCALE - 1);
	int sym = dec->lookup[slot];
	uint32_t freq = dec->tab->freq[sym];
	uint32_t cumfreq = dec->tab->cumfreq[sym];

	/* Update state: state = freq * (state >> PROB_BITS) + slot - cumfreq */
	dec->state = freq * (dec->state >> UC2_RANS_PROB_BITS) + slot - cumfreq;

	/* Renormalize: read bytes to keep state in range */
	while (dec->state < RANS_L && dec->pos < dec->len) {
		dec->state = (dec->state << 8) | dec->data[dec->pos++];
	}

	return sym;
}
