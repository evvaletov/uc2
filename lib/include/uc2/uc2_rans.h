/* rANS (range Asymmetric Numeral Systems) entropy coder.
 *
 * Drop-in replacement for Huffman coding with ~5-15% better compression
 * on skewed distributions.  Uses table-based rANS with 32-bit state
 * and frequencies normalized to a power of 2.
 *
 * Usage:
 *   struct uc2_rans_enc enc;
 *   uc2_rans_enc_init(&enc, freqs, nsym);
 *   for each symbol: uc2_rans_encode(&enc, sym, &out_buf, &out_pos);
 *   uc2_rans_enc_flush(&enc, &out_buf, &out_pos);
 *
 *   struct uc2_rans_dec dec;
 *   uc2_rans_dec_init(&dec, freqs, nsym, in_buf, in_len);
 *   for each symbol: int sym = uc2_rans_decode(&dec);
 */

#ifndef UC2_RANS_H
#define UC2_RANS_H

#include <stdint.h>
#include <stddef.h>

/* Frequency table precision: frequencies sum to 1 << PROB_BITS */
#define UC2_RANS_PROB_BITS 12
#define UC2_RANS_PROB_SCALE (1 << UC2_RANS_PROB_BITS)

/* Maximum symbols supported */
#define UC2_RANS_MAX_SYMS 344

/* Normalized frequency table. */
struct uc2_rans_table {
	uint16_t freq[UC2_RANS_MAX_SYMS];     /* normalized frequencies */
	uint16_t cumfreq[UC2_RANS_MAX_SYMS];  /* cumulative frequencies */
	int nsym;
};

/* Build normalized frequency table from raw counts.
 * Frequencies are scaled to sum to UC2_RANS_PROB_SCALE. */
void uc2_rans_build_table(struct uc2_rans_table *tab,
                          const uint32_t *raw_freq, int nsym);

/* --- Encoder --- */

struct uc2_rans_enc {
	uint32_t state;
	const struct uc2_rans_table *tab;
	/* Reverse buffer: rANS encodes in reverse order */
	uint8_t *rev_buf;
	size_t rev_pos;
	size_t rev_cap;
};

/* Initialize encoder. */
void uc2_rans_enc_init(struct uc2_rans_enc *enc,
                       const struct uc2_rans_table *tab);

/* Encode one symbol.  Symbols must be encoded in REVERSE order
 * (last symbol first).  Use uc2_rans_enc_flush to finalize. */
void uc2_rans_encode(struct uc2_rans_enc *enc, int sym);

/* Finalize encoding: write state and return the compressed data.
 * Caller must free *out_data. Returns compressed size. */
size_t uc2_rans_enc_finish(struct uc2_rans_enc *enc,
                           uint8_t **out_data);

/* Free encoder resources. */
void uc2_rans_enc_free(struct uc2_rans_enc *enc);

/* --- Decoder --- */

struct uc2_rans_dec {
	uint32_t state;
	const struct uc2_rans_table *tab;
	const uint8_t *data;
	size_t pos;
	size_t len;
	/* Reverse lookup: cumfreq → symbol (for fast decoding) */
	uint16_t lookup[UC2_RANS_PROB_SCALE];
};

/* Initialize decoder from compressed data. */
void uc2_rans_dec_init(struct uc2_rans_dec *dec,
                       const struct uc2_rans_table *tab,
                       const uint8_t *data, size_t len);

/* Decode one symbol. */
int uc2_rans_decode(struct uc2_rans_dec *dec);

#endif
