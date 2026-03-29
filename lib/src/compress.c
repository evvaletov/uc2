/* UC2 LZ77+Huffman compressor.
   Produces bitstreams compatible with Bobrowski's decompressor (decompress.c).

   Algorithm: LZ77 sliding window with hash-chain match finding,
   Huffman entropy coding, delta-coded tree serialization.

   Copyright (c) 2026 Eremey Valetov
   License: GPL-3.0 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "uc2/libuc2.h"
#include "uc2_internal.h"

/* ---------- bitstream output ---------- */

struct bits_out {
	u8 *buf;
	unsigned pos;       /* byte position in output */
	unsigned capacity;
	u16 word;           /* accumulator */
	int bits_left;      /* bits remaining in word (16 = empty) */

	int (*write)(void *ctx, const void *ptr, unsigned len);
	void *ctx;
};

static void bitsout_init(struct bits_out *bo,
                         int (*write)(void *ctx, const void *ptr, unsigned len),
                         void *ctx, u8 *buf, unsigned capacity)
{
	bo->buf = buf;
	bo->pos = 0;
	bo->capacity = capacity;
	bo->word = 0;
	bo->bits_left = 16;
	bo->write = write;
	bo->ctx = ctx;
}

static int bitsout_flush_buf(struct bits_out *bo)
{
	if (bo->pos > 0) {
		int r = bo->write(bo->ctx, bo->buf, bo->pos);
		if (r < 0) return r;
		bo->pos = 0;
	}
	return 0;
}

static int bitsout_emit_word(struct bits_out *bo, u16 w)
{
	if (bo->pos + 2 > bo->capacity) {
		int r = bitsout_flush_buf(bo);
		if (r < 0) return r;
	}
	bo->buf[bo->pos++] = w & 0xff;
	bo->buf[bo->pos++] = w >> 8;
	return 0;
}

/* Put bits MSB-first into 16-bit LE words */
static int bitsout_put(struct bits_out *bo, unsigned val, int nbits)
{
	assert(nbits <= 16);
	int left = bo->bits_left;
	if (nbits <= left) {
		bo->word |= (u16)(val << (left - nbits));
		bo->bits_left = left - nbits;
		if (bo->bits_left == 0) {
			int r = bitsout_emit_word(bo, bo->word);
			if (r < 0) return r;
			bo->word = 0;
			bo->bits_left = 16;
		}
	} else {
		/* Split across word boundary */
		int first = left;
		int second = nbits - first;
		bo->word |= (u16)(val >> second);
		int r = bitsout_emit_word(bo, bo->word);
		if (r < 0) return r;
		bo->word = (u16)(val << (16 - second));
		bo->bits_left = 16 - second;
	}
	return 0;
}

static int bitsout_finish(struct bits_out *bo)
{
	if (bo->bits_left < 16) {
		int r = bitsout_emit_word(bo, bo->word);
		if (r < 0) return r;
	}
	return bitsout_flush_buf(bo);
}

/* ---------- Huffman tree generation ---------- */

/* Faithful port of the original UC2 Pro's TreeGen/RepairLengths/CodeGen
   from TREEGEN.CPP. Matching the original exactly is required for bitstream
   compatibility with the 1992 UC2 Pro decompressor (which uses an ASM kernel
   that depends on the exact same Huffman tree shapes). */

#define NO_MORE (-1)

/* Sift-down for min-heap.  Uses >= for child comparison (prefer right child
   on ties), matching the original Reheap() at TREEGEN.CPP:138. */
static void reheap(int entry, int *heap, int heap_len, const u32 *freq)
{
	int val = heap[entry];
	while (entry <= (heap_len >> 1)) {
		int child = entry << 1;
		if (child < heap_len)
			if (freq[heap[child]] >= freq[heap[child + 1]])
				child++;
		if (freq[val] < freq[heap[child]])
			break;
		heap[entry] = heap[child];
		entry = child;
	}
	heap[entry] = val;
}

static void treegen(const u32 *freqs, int nsym, int max_code_bits, u8 *lengths)
{
	/* Frequency array extended for internal nodes (original uses upper half
	   of a 2*nsym WORD array; we use u32 for safety). */
	u32 freq[2 * NumSymbols];
	int heap[NumSymbols + 1];
	int father[2 * NumSymbols];
	int heap_len = 0;

	memset(lengths, 0, nsym);
	for (int i = 0; i < nsym; i++)
		freq[i] = freqs[i];

	/* BuildHeap: select non-zero symbols, then heapify */
	for (int i = 0; i < nsym; i++)
		if (freq[i])
			heap[++heap_len] = i;

	if (heap_len > 1) {
		/* Heapify */
		for (int i = heap_len; i > 0; i--)
			reheap(i, heap, heap_len, freq);

		/* BuildCodeTree: extract-one-then-combine pattern (TREEGEN.CPP:154-181).
		   The first extracted min becomes one child; the current heap[1] after
		   reheap becomes the other.  The combined node replaces heap[1]. */
		while (heap_len > 1) {
			int extracted = heap[1];
			heap[1] = heap[heap_len--];
			reheap(1, heap, heap_len, freq);

			int internal = heap_len + nsym - 1;
			freq[internal] = freq[heap[1]] + freq[extracted];
			father[extracted] = internal;
			father[heap[1]] = -internal;  /* negative = right child */
			heap[1] = internal;
			reheap(1, heap, heap_len, freq);
		}
		father[nsym] = 0;  /* root sentinel */

		/* GenCodeLengths: walk father chain to compute depths */
		for (int i = 0; i < nsym; i++) {
			if (freq[i]) {
				int depth = 0;
				int p = father[i];
				while (p) {
					if (p < 0) p = -p;
					p = father[p];
					depth++;
				}
				lengths[i] = (u8)depth;
			}
		}

		/* RepairLengths: faithful port of TREEGEN.CPP:218-364.
		   Uses sorted linked lists to redistribute code lengths while
		   maintaining ascending symbol order within each length group. */
		struct { int ch; int next; } elems[NumSymbols];
		int scl[MaxCodeBits + 2];
		int length_count[MaxCodeBits + 2];

		for (int i = 0; i <= max_code_bits; i++)
			scl[i] = NO_MORE;
		memset(length_count, 0, sizeof length_count);

		/* Sort symbols by length (iterate high-to-low, prepend → ascending order) */
		int pp = 0;
		for (int i = nsym - 1; i >= 0; i--) {
			if (lengths[i] != 0) {
				elems[pp].ch = i;
				int len = lengths[i];
				if (len > max_code_bits)
					len = lengths[i] = (u8)max_code_bits;
				length_count[len]++;
				elems[pp].next = scl[len];
				scl[len] = pp++;
			}
		}

		/* Calculate DCode (Kraft sum) */
		u32 dcode = 0;
		for (int i = 1; i <= max_code_bits; i++)
			dcode += (1UL << (max_code_bits - i)) * (u32)length_count[i];

		/* Find first set bit in DCode (error position) */
		int bit = 0;
		while (bit < max_code_bits && !(dcode & (1UL << bit)))
			bit++;

		/* Repair loop */
		while (bit != max_code_bits) {
			int ilen = max_code_bits - bit;

			/* Find shortest length with symbols */
			int j = ilen;
			while (j > 0 && !length_count[j - 1])
				j--;
			j--;  /* j is the length to lengthen from */

			/* Extract first element from scl[j] and lengthen it */
			int epp = scl[j];
			scl[j] = elems[epp].next;

			/* Insert into scl[j+1] maintaining ascending symbol order */
			if (scl[j + 1] == NO_MORE || elems[scl[j + 1]].ch > elems[epp].ch) {
				elems[epp].next = scl[j + 1];
				scl[j + 1] = epp;
			} else {
				int qp = scl[j + 1];
				int rp = elems[qp].next;
				while (rp != NO_MORE && elems[rp].ch < elems[epp].ch) {
					qp = rp;
					rp = elems[rp].next;
				}
				elems[epp].next = rp;
				elems[qp].next = epp;
			}
			lengths[elems[epp].ch]++;
			dcode -= 1UL << (max_code_bits - j - 1);
			length_count[j]--;
			length_count[j + 1]++;

			/* Fill freed space by shortening codes from longest levels */
			if (ilen - j > 1) {
				int space = (1 << (ilen - j - 1)) - 1;
				while (space > 0) {
					int count = length_count[ilen] < space ? length_count[ilen] : space;
					/* Shorten 'count' elements from scl[ilen] to scl[ilen-1] */
					int headp = scl[ilen];
					int lastp = headp;
					for (int k = 0; k < count; k++) {
						lengths[elems[lastp].ch]--;
						if (k + 1 < count)
							lastp = elems[lastp].next;
					}
					if (count > 0) {
						/* Detach shortened chain from scl[ilen] */
						int tailp = elems[lastp].next;
						scl[ilen] = tailp;
						elems[lastp].next = NO_MORE;

						/* Merge chain into scl[ilen-1] in sorted order */
						int *insert_at = &scl[ilen - 1];
						int cp = headp;
						while (*insert_at != NO_MORE && cp != NO_MORE) {
							if (elems[*insert_at].ch > elems[cp].ch) {
								int nextp = elems[cp].next;
								int save = *insert_at;
								*insert_at = cp;
								elems[cp].next = save;
								insert_at = &elems[cp].next;
								cp = nextp;
							} else {
								insert_at = &elems[*insert_at].next;
							}
						}
						if (cp != NO_MORE)
							*insert_at = cp;
					}

					length_count[ilen - 1] += count;
					length_count[ilen] -= count;
					dcode += (1UL << (max_code_bits - ilen)) * (u32)count;
					space -= count;
					space <<= 1;
					ilen++;
				}
			}

			/* Re-check DCode */
			while (bit < max_code_bits && !(dcode & (1UL << bit)))
				bit++;
		}
	} else if (heap_len == 1) {
		int sym = heap[1];
		lengths[sym] = 1;
		lengths[(sym + 1) % nsym] = 1;
	} else {
		lengths[0] = 1;
		lengths[1] = 1;
	}
}

/* Generate canonical Huffman codes from lengths (sorted by length, then symbol) */
static void codegen(const u8 *lengths, int nsym, u16 *codes)
{
	u16 code = 0;
	for (int len = 1; len <= MaxCodeBits; len++) {
		for (int i = 0; i < nsym; i++) {
			if (lengths[i] == len)
				codes[i] = code++;
		}
		code <<= 1;
	}
}

/* ---------- Huffman tree encoding (delta-coded, as in TREEENC.CPP) ---------- */

/* Encode a Huffman tree to the bitstream using delta coding against
   the previous block's tree (stored in symprev).

   Bitstream format (matches decompress.c ht_dec):
     tree-changed:1  — 0 = use default tree, 1 = new tree follows
     t:2             — (has_hi<<1)|has_lo
     tlengths:15×3   — tree-encoding tree (delta/repeat code lengths)
     stream:var      — delta-coded symbol lengths via tree-encoding tree

   The present[] array must exactly match the decoder's rle[][] table:
     t=0: {9,10,12,13, 32..127, 256..343}        = 188 symbols
     t=1: {0..127, 256..343}                      = 216 symbols
     t=2: {9,10,12,13, 32..343}                   = 316 symbols
     t=3: {0..343}                                 = 344 symbols

   RLE repeat code: RepeatCode followed by count c.
   Decoder emits c + MinRepeat - 1 copies of the previous value. */
static int tree_enc(struct bits_out *bo, const u8 lengths[NumSymbols], u8 symprev[NumSymbols])
{
	int r;

	/* tree-changed = 0 means "use default tree" (resets symprev) */
	u8 def_tree[NumSymbols];
	uc2_default_lengths(def_tree);
	if (memcmp(lengths, def_tree, NumSymbols) == 0) {
		r = bitsout_put(bo, 0, 1);
		if (r < 0) return r;
		memcpy(symprev, def_tree, NumSymbols);
		return 0;
	}

	r = bitsout_put(bo, 1, 1);  /* new tree */
	if (r < 0) return r;

	/* has_lo: need full 0..127 encoding?
	   Symbols 9,10,12,13 (tab,LF,FF,CR) are always individually coded.
	   has_lo must be set if ANY of {0..8, 11, 14..31} have non-zero length. */
	int has_lo = 0;
	for (int i = 0; i <= 8 && !has_lo; i++)
		if (lengths[i] > 0) has_lo = 1;
	if (!has_lo && lengths[11] > 0) has_lo = 1;
	if (!has_lo)
		for (int i = 14; i <= 31; i++)
			if (lengths[i] > 0) { has_lo = 1; break; }

	int has_hi = 0;
	for (int i = 128; i < 256; i++)
		if (lengths[i] > 0) { has_hi = 1; break; }

	int t = (has_hi << 1) | has_lo;
	r = bitsout_put(bo, t, 2);
	if (r < 0) return r;

	/* Build present[] to exactly match decoder's rle[][] regions */
	int present[NumSymbols];
	memset(present, 0, sizeof present);
	switch (t) {
	case 0: /* no lo, no hi */
		present[9] = present[10] = 1;
		present[12] = present[13] = 1;
		for (int i = 32; i < 128; i++) present[i] = 1;
		for (int i = 256; i < NumSymbols; i++) present[i] = 1;
		break;
	case 1: /* has_lo */
		for (int i = 0; i < 128; i++) present[i] = 1;
		for (int i = 256; i < NumSymbols; i++) present[i] = 1;
		break;
	case 2: /* has_hi */
		present[9] = present[10] = 1;
		present[12] = present[13] = 1;
		for (int i = 32; i < NumSymbols; i++) present[i] = 1;
		break;
	case 3: /* both */
		for (int i = 0; i < NumSymbols; i++) present[i] = 1;
		break;
	}

	/* Generate delta stream for present symbols */
	u8 stream[NumSymbols];
	int stream_len = 0;
	for (int i = 0; i < NumSymbols; i++)
		if (present[i])
			stream[stream_len++] = ivval[symprev[i]][lengths[i]];

	/* Compute frequencies matching the RLE pattern in the original
	   InsertExtraCode: trigger at run > MinRepeat (>6), one RepeatCode
	   per chunk of max RepeatCode+MinRepeat (20), remainder handled by
	   next iteration. */
	u32 tfreqs[NumLenCodes];
	memset(tfreqs, 0, sizeof tfreqs);
	for (int i = 0; i < stream_len; ) {
		int run = 1;
		while (i + run < stream_len && stream[i + run] == stream[i])
			run++;
		if (run > (int)MinRepeat) {
			int chunk = run;
			if (chunk > (int)(RepeatCode + MinRepeat))
				chunk = RepeatCode + MinRepeat;
			tfreqs[stream[i]]++;
			tfreqs[RepeatCode]++;
			tfreqs[chunk - MinRepeat]++;
			i += chunk;
		} else {
			tfreqs[stream[i]]++;
			i++;
		}
	}

	/* Generate tree-encoding tree (15 symbols, max 7-bit codes) */
	u8 tlengths[NumLenCodes];
	treegen(tfreqs, NumLenCodes, 7, tlengths);

	u16 tcodes[NumLenCodes];
	codegen(tlengths, NumLenCodes, tcodes);

	/* Write tree-encoding tree lengths (15 × 3 bits) */
	for (int i = 0; i < NumLenCodes; i++) {
		r = bitsout_put(bo, tlengths[i], 3);
		if (r < 0) return r;
	}

	/* Write delta-coded symbol stream with RLE.
	   Matches original InsertExtraCode + encoding loop:
	   - Trigger at run > MinRepeat (>6, i.e. >=7)
	   - One RepeatCode per run chunk, max RepeatCode+MinRepeat (20)
	   - Remainder handled by next iteration */
	for (int i = 0; i < stream_len; ) {
		int run = 1;
		while (i + run < stream_len && stream[i + run] == stream[i])
			run++;
		if (run > (int)MinRepeat) {
			int chunk = run;
			if (chunk > (int)(RepeatCode + MinRepeat))
				chunk = RepeatCode + MinRepeat;
			r = bitsout_put(bo, tcodes[stream[i]], tlengths[stream[i]]);
			if (r < 0) return r;
			int c = chunk - MinRepeat;
			r = bitsout_put(bo, tcodes[RepeatCode], tlengths[RepeatCode]);
			if (r < 0) return r;
			r = bitsout_put(bo, tcodes[c], tlengths[c]);
			if (r < 0) return r;
			i += chunk;
		} else {
			r = bitsout_put(bo, tcodes[stream[i]], tlengths[stream[i]]);
			if (r < 0) return r;
			i++;
		}
	}

	/* Update symprev for next block */
	for (int i = 0; i < NumSymbols; i++)
		symprev[i] = lengths[i];

	return 0;
}

/* ---------- LZ77 compressor core ---------- */

struct compressor {
	/* Sliding window (64KB circular buffer, u16 index wraps naturally) */
	u8 data[UC2_BUF_SIZE];

	/* Hash chains */
	u16 head[8192];     /* hash -> most recent position */
	u16 prev[UC2_BUF_SIZE]; /* position -> previous position with same hash */

	/* Current position and limits */
	u16 pos;            /* current compression position */
	u16 end;            /* end of valid data */
	unsigned data_len;  /* total bytes loaded so far */

	/* Intermediate buffer for literals/distances/lengths */
	u16 ibuf[32768];
	unsigned ibuf_pos;

	/* Frequency counts for current block */
	u32 bd_freq[NumByteSym + NumDistSym];
	u32 l_freq[NumLenSym];

	/* Previous tree lengths (for delta coding) */
	u8 symprev[NumSymbols];
	int block_count;

	/* Output bitstream */
	struct bits_out bo;
	u8 outbuf[4096];

	/* Compression parameters */
	unsigned max_search;
	unsigned lazy_depth;
	unsigned lazy_limit;
	unsigned give_up;

	/* Total compressed bytes written */
	unsigned compressed_bytes;
};

static inline u16 hash3(const u8 *p)
{
	return (u16)(p[0] ^ (p[1] << 3) ^ ((0x7f & p[2]) << 6));
}

/* Find longest match at current position. Returns match length (0 if none). */
static unsigned find_match(struct compressor *c, u16 pos, unsigned max_depth,
                           unsigned give_up, unsigned *match_dist)
{
	unsigned best_len = UC2_MIN_MATCH - 1;
	unsigned best_dist = 0;

	u16 h = hash3(c->data + pos);
	u16 chain = c->head[h];
	unsigned depth = 0;

	while (depth < max_depth) {
		u16 dist = (u16)(pos - chain);
		if (dist == 0 || dist > UC2_MAX_DIST)
			break;

		/* Quick filter: check byte at best_len position first */
		if (c->data[(u16)(chain + best_len)] == c->data[(u16)(pos + best_len)]) {
			/* Full comparison */
			unsigned len = 0;
			unsigned max_len = UC2_MAX_LEN;
			u16 avail = (u16)(c->end - pos);
			if (max_len > avail) max_len = avail;

			while (len < max_len &&
			       c->data[(u16)(chain + len)] == c->data[(u16)(pos + len)])
				len++;

			if (len > best_len) {
				best_len = len;
				best_dist = dist;
				if (len >= give_up)
					break;
			}
		}

		chain = c->prev[chain];
		depth++;
		if ((u16)(pos - chain) > UC2_MAX_DIST)
			break;
	}

	*match_dist = best_dist;
	return best_len >= UC2_MIN_MATCH ? best_len : 0;
}

static void hash_enter(struct compressor *c, u16 pos)
{
	u16 h = hash3(c->data + pos);
	c->prev[pos] = c->head[h];
	c->head[h] = pos;
}

/* Encode a distance into the intermediate buffer */
static void encode_dist(struct compressor *c, unsigned dist)
{
	unsigned sym, extra, nbits;

	if (dist <= 15) {
		sym = dist - 1 + NumByteSym;   /* symbols 256..270 */
		c->bd_freq[sym]++;
		c->ibuf[c->ibuf_pos++] = (u16)(dist + 256 - 1);
		return;
	}
	if (dist <= 255) {
		unsigned slot = (dist - 16) / 16;
		sym = slot + 15 + NumByteSym;  /* symbols 271..285 */
		extra = (dist - 16) % 16;
		nbits = 4;
	} else if (dist <= 4095) {
		unsigned slot = (dist - 256) / 256;
		sym = slot + 30 + NumByteSym;  /* symbols 286..300 */
		extra = (dist - 256) % 256;
		nbits = 8;
	} else {
		unsigned slot = (dist - 4096) / 4096;
		sym = slot + 45 + NumByteSym;  /* symbols 301..315 */
		extra = (dist - 4096) % 4096;
		nbits = 12;
	}

	c->bd_freq[sym]++;
	c->ibuf[c->ibuf_pos++] = (u16)(sym - NumByteSym + 256);
	c->ibuf[c->ibuf_pos++] = (u16)extra;
	(void)nbits;
}

static void encode_len(struct compressor *c, unsigned len)
{
	unsigned sym, extra;

	if (len <= 10) {
		sym = len - 3;
		c->l_freq[sym]++;
		c->ibuf[c->ibuf_pos++] = (u16)len;
		return;
	}
	if (len <= 26) {
		sym = (len - 11) / 2 + 8;
		extra = (len - 11) % 2;
	} else if (len <= 90) {
		sym = (len - 27) / 8 + 16;
		extra = (len - 27) % 8;
	} else if (len <= 154) {
		sym = 24;
		extra = len - 91;
	} else if (len <= 666) {
		sym = 25;
		extra = len - 155;
	} else if (len <= 2714) {
		sym = 26;
		extra = len - 667;
	} else {
		sym = 27;
		extra = len - 2715;
	}

	c->l_freq[sym]++;
	c->ibuf[c->ibuf_pos++] = (u16)len;
	(void)extra;
}

/* Distance/length encoding tables (for Huffman encoding phase) */
static const struct { u16 base; u8 bits; } dist_enc[] = {
	/* 0..14: dist 1..15,  0 extra bits */
	{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},{8,0},
	{9,0},{10,0},{11,0},{12,0},{13,0},{14,0},{15,0},
	/* 15..29: dist 16..240 base, 4 extra bits */
	{16,4},{32,4},{48,4},{64,4},{80,4},{96,4},{112,4},{128,4},
	{144,4},{160,4},{176,4},{192,4},{208,4},{224,4},{240,4},
	/* 30..44: dist 256..3840 base, 8 extra bits */
	{256,8},{512,8},{768,8},{1024,8},{1280,8},{1536,8},{1792,8},{2048,8},
	{2304,8},{2560,8},{2816,8},{3072,8},{3328,8},{3584,8},{3840,8},
	/* 45..59: dist 4096..61440 base, 12 extra bits */
	{4096,12},{8192,12},{12288,12},{16384,12},{20480,12},{24576,12},
	{28672,12},{32768,12},{36864,12},{40960,12},{45056,12},{49152,12},
	{53248,12},{57344,12},{61440,12},
};

static const struct { u16 base; u8 bits; } len_enc[] = {
	{3,0},{4,0},{5,0},{6,0},{7,0},{8,0},{9,0},{10,0},
	{11,1},{13,1},{15,1},{17,1},{19,1},{21,1},{23,1},{25,1},
	{27,3},{35,3},{43,3},{51,3},{59,3},{67,3},{75,3},{83,3},
	{91,6},{155,9},{667,11},{2715,15},
};

/* Find the distance symbol for a given distance */
static int dist_to_sym(unsigned dist)
{
	for (int i = NumDistSym - 1; i >= 0; i--)
		if (dist >= dist_enc[i].base)
			return i;
	return 0;
}

/* Find the length symbol for a given length */
static int len_to_sym(unsigned len)
{
	for (int i = NumLenSym - 1; i >= 0; i--)
		if (len >= len_enc[i].base)
			return i;
	return 0;
}

/* Flush intermediate buffer: generate Huffman trees and encode data */
static int flush_block(struct compressor *c, int is_last)
{
	int r;

	/* Generate Huffman trees from frequency data */
	u8 lengths[NumSymbols];

	/* Use the default tree for all blocks.  The original UC2 Pro's ASM
	   decompressor (nuke1) depends on exact Huffman tree shapes from
	   the original TreeGen.  Until the treegen port is bit-exact, using
	   the default tree ensures full backward compatibility at the cost
	   of ~5-15% worse compression ratios. */
	uc2_default_lengths(lengths);

	/* Emit block-present flag */
	r = bitsout_put(&c->bo, 1, 1);
	if (r < 0) return r;

	/* Encode and emit Huffman tree */
	r = tree_enc(&c->bo, lengths, c->symprev);
	if (r < 0) return r;

	/* Generate canonical codes */
	u16 bd_codes[NumByteSym + NumDistSym];
	u16 l_codes[NumLenSym];
	codegen(lengths, NumByteSym + NumDistSym, bd_codes);
	codegen(lengths + NumByteSym + NumDistSym, NumLenSym, l_codes);

	/* Encode buffered literals/matches.
	   ibuf format: literal = byte (0..255), distance = sym_idx + 256,
	   followed by extra value (if dist_enc[sym_idx].bits > 0),
	   followed by raw length value. */
	unsigned i = 0;
	while (i < c->ibuf_pos) {
		u16 val = c->ibuf[i++];
		if (val < 256) {
			/* Literal byte */
			r = bitsout_put(&c->bo, bd_codes[val], lengths[val]);
			if (r < 0) return r;
		} else {
			/* Distance: val - 256 is the distance symbol index */
			int dsym = val - 256;
			r = bitsout_put(&c->bo, bd_codes[NumByteSym + dsym],
			                lengths[NumByteSym + dsym]);
			if (r < 0) return r;
			if (dist_enc[dsym].bits > 0) {
				u16 extra = c->ibuf[i++];
				r = bitsout_put(&c->bo, extra, dist_enc[dsym].bits);
				if (r < 0) return r;
			}

			/* Length follows distance */
			u16 len = c->ibuf[i++];
			int lsym = len_to_sym(len);
			r = bitsout_put(&c->bo, l_codes[lsym],
			                lengths[NumByteSym + NumDistSym + lsym]);
			if (r < 0) return r;
			if (len_enc[lsym].bits > 0) {
				r = bitsout_put(&c->bo, len - len_enc[lsym].base,
				                len_enc[lsym].bits);
				if (r < 0) return r;
			}
		}
	}

	/* Emit end-of-block marker: distance = EOB_MARK, length = 3 */
	{
		int dsym = dist_to_sym(UC2_EOB_MARK);
		r = bitsout_put(&c->bo, bd_codes[NumByteSym + dsym],
		                lengths[NumByteSym + dsym]);
		if (r < 0) return r;
		if (dist_enc[dsym].bits > 0) {
			r = bitsout_put(&c->bo, UC2_EOB_MARK - dist_enc[dsym].base,
			                dist_enc[dsym].bits);
			if (r < 0) return r;
		}
		/* Length = 3 (symbol 0) */
		r = bitsout_put(&c->bo, l_codes[0],
		                lengths[NumByteSym + NumDistSym]);
		if (r < 0) return r;
	}

	/* Reset intermediate buffer and frequencies */
	c->ibuf_pos = 0;
	c->block_count++;
	memset(c->bd_freq, 0, sizeof c->bd_freq);
	memset(c->l_freq, 0, sizeof c->l_freq);

	/* If last block, emit end-of-stream (block-present = 0) */
	if (is_last) {
		r = bitsout_put(&c->bo, 0, 1);
		if (r < 0) return r;
	}

	return 0;
}

/* ---------- Public compression API ---------- */

struct compress_ctx {
	struct compressor comp;
	struct csum csum;
	unsigned total_in;
	unsigned total_out;
	int finished;
};

/* Counting writer: wraps user writer to track compressed size */
struct count_writer {
	int (*write)(void *ctx, const void *ptr, unsigned len);
	void *ctx;
	unsigned *count;
};

static int count_write(void *ctx, const void *ptr, unsigned len)
{
	struct count_writer *cw = ctx;
	*cw->count += len;
	return cw->write(cw->ctx, ptr, len);
}

int uc2_compress_ex(
	int level,
	const void *master, unsigned master_size,
	int (*read)(void *context, void *buf, unsigned len),
	void *read_ctx,
	int (*write)(void *context, const void *ptr, unsigned len),
	void *write_ctx,
	unsigned size,
	unsigned short *checksum_out,
	unsigned *compressed_size_out)
{
	struct compress_ctx *ctx = calloc(1, sizeof *ctx);
	if (!ctx) return UC2_UserFault;

	struct compressor *c = &ctx->comp;

	/* Set compression parameters based on level */
	switch (level) {
	case 2: c->max_search = 15;    c->lazy_depth = 2;    c->lazy_limit = 15;  c->give_up = 25;  break;
	case 3: c->max_search = 70;    c->lazy_depth = 10;   c->lazy_limit = 30;  c->give_up = 50;  break;
	case 5: c->max_search = 10000; c->lazy_depth = 5000; c->lazy_limit = 200; c->give_up = 100; break;
	default: /* level 4 = Tight, default */
	         c->max_search = 600;   c->lazy_depth = 50;   c->lazy_limit = 40;  c->give_up = 100; break;
	}

	/* Initialize */
	csum_init(&ctx->csum);
	uc2_default_lengths(c->symprev);
	memset(c->head, 0, sizeof c->head);
	memset(c->bd_freq, 0, sizeof c->bd_freq);
	memset(c->l_freq, 0, sizeof c->l_freq);
	c->ibuf_pos = 0;
	c->block_count = 0;

	struct count_writer cw = { .write = write, .ctx = write_ctx, .count = &ctx->total_out };
	bitsout_init(&c->bo, count_write, &cw, c->outbuf, sizeof c->outbuf);

	/* Read all input data into circular buffer and compress */
	unsigned remaining = size;
	u16 load_pos = 0;

	/* Pre-fill circular buffer with master data (LZ77 dictionary prefix) */
	if (master && master_size > 0) {
		unsigned ms = master_size;
		if (ms > UC2_BUF_SIZE - UC2_MIN_MATCH)
			ms = UC2_BUF_SIZE - UC2_MIN_MATCH;
		memcpy(c->data, master, ms);
		for (unsigned i = 0; i + 2 < ms; i++)
			hash_enter(c, (u16)i);
		load_pos = (u16)ms;
		c->pos = load_pos;
		c->end = load_pos;
	}

	/* Pre-count EOB distance symbol frequency so the tree includes it */
	c->bd_freq[NumByteSym + dist_to_sym(UC2_EOB_MARK)]++;
	c->l_freq[0]++;  /* length = 3 for EOB marker */

	while (remaining > 0) {
		/* Load a chunk into the circular buffer */
		unsigned chunk = remaining;
		if (chunk > UC2_READ_SIZE) chunk = UC2_READ_SIZE;

		int nread = read(read_ctx, c->data + load_pos, chunk);
		if (nread <= 0) break;

		csum_update(&ctx->csum, c->data + load_pos, nread);
		remaining -= nread;

		load_pos = (u16)(load_pos + nread);
		c->end = load_pos;

		/* Compress loaded data */
		while ((u16)(c->end - c->pos) >= UC2_MIN_MATCH) {
			/* Enter current position into hash */
			if ((u16)(c->end - c->pos) >= 3)
				hash_enter(c, c->pos);

			unsigned dist;
			unsigned len = find_match(c, c->pos, c->max_search, c->give_up, &dist);

			if (len == 0) {
				/* Literal */
				c->bd_freq[c->data[c->pos]]++;
				c->ibuf[c->ibuf_pos++] = c->data[c->pos];
				c->pos++;
			} else {
				/* Lazy evaluation: if match is short, check next position */
				if (len < c->lazy_limit && (u16)(c->end - c->pos) > len) {
					unsigned dist2;
					if ((u16)(c->end - (u16)(c->pos + 1)) >= 3)
						hash_enter(c, (u16)(c->pos + 1));
					unsigned len2 = find_match(c, (u16)(c->pos + 1),
					                           c->lazy_depth, c->give_up, &dist2);
					if (len2 > len) {
						/* Better match at next position — emit literal */
						c->bd_freq[c->data[c->pos]]++;
						c->ibuf[c->ibuf_pos++] = c->data[c->pos];
						c->pos++;
						len = len2;
						dist = dist2;
					}
				}

				/* Emit match */
				encode_dist(c, dist);
				encode_len(c, len);

				/* Enter skipped positions into hash */
				for (unsigned j = 1; j < len && (u16)(c->end - (u16)(c->pos + j)) >= 3; j++)
					hash_enter(c, (u16)(c->pos + j));
				c->pos = (u16)(c->pos + len);
			}

			/* Flush block if intermediate buffer is getting full */
			if (c->ibuf_pos > 27000) {
				int r = flush_block(c, 0);
				if (r < 0) { free(ctx); return r; }
				/* Re-add EOB marker frequency for next block */
				c->bd_freq[NumByteSym + dist_to_sym(UC2_EOB_MARK)]++;
				c->l_freq[0]++;
			}
		}
	}

	/* Handle trailing bytes (less than MIN_MATCH) */
	while (c->pos != c->end) {
		c->bd_freq[c->data[c->pos]]++;
		c->ibuf[c->ibuf_pos++] = c->data[c->pos];
		c->pos++;
	}

	/* Flush final block */
	int r = flush_block(c, 1);
	if (r < 0) { free(ctx); return r; }

	r = bitsout_finish(&c->bo);
	if (r < 0) { free(ctx); return r; }

	if (checksum_out)
		*checksum_out = csum_get(&ctx->csum);
	if (compressed_size_out)
		*compressed_size_out = ctx->total_out;

	free(ctx);
	return 0;
}

int uc2_compress(
	int level,
	int (*read)(void *context, void *buf, unsigned len),
	void *read_ctx,
	int (*write)(void *context, const void *ptr, unsigned len),
	void *write_ctx,
	unsigned size,
	unsigned short *checksum_out,
	unsigned *compressed_size_out)
{
	return uc2_compress_ex(level, NULL, 0, read, read_ctx, write, write_ctx,
	                       size, checksum_out, compressed_size_out);
}
