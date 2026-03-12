/* UC2 format constants and shared types.
   Used by both the compressor and decompressor. */

#ifndef UC2_INTERNAL_H
#define UC2_INTERNAL_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

/* Huffman tree parameters */
enum {
	MaxCodeBits  = 13,
	LookupSize   = 1 << MaxCodeBits,   /* 8192 */

	NumByteSym   = 256,
	NumDistSym   = 60,
	NumLenSym    = 28,
	NumSymbols   = NumByteSym + NumDistSym + NumLenSym,   /* 344 */

	NumLoAsciiSym = 28,    /* symbols 4..31 (0-3 are control) */
	NumHiByteSym  = 128,   /* symbols 128..255 */

	NumDeltaCodes = MaxCodeBits + 1,   /* 14 (code lengths 0..13) */
	NumExtraCodes = 1,                 /* repeat code */
	NumLenCodes   = NumDeltaCodes + NumExtraCodes,   /* 15 */
	RepeatCode    = MaxCodeBits + 1,   /* 14 */
	MinRepeat     = 6,
};

/* LZ77 parameters */
enum {
	UC2_MAX_DIST  = 125 * 512,       /* 64000 */
	UC2_READ_SIZE = 512,
	UC2_BUF_SIZE  = 65536,           /* circular buffer: u16 index wraps */
	UC2_EOB_MARK  = 125 * 512 + 1,   /* 64001 — end-of-block distance */
	UC2_MIN_MATCH = 3,
	UC2_MAX_LEN   = 200,             /* direct match limit */
	UC2_MAX_XLEN  = 32760,           /* extended match limit */
};

/* Distance encoding: 60 codes in 4 tiers.
   tier 0: dist 1..15     (15 codes, 0 extra bits)
   tier 1: dist 16..255   (15 codes, 4 extra bits)
   tier 2: dist 256..4095 (15 codes, 8 extra bits)
   tier 3: dist 4096..64000 (15 codes, 12 extra bits) */

/* Length encoding: 28 codes.
   0..7:   len 3..10      (0 extra bits)
   8..15:  len 11..26     (1 extra bit)
   16..23: len 27..90     (3 extra bits)
   24:     len 91..154    (6 extra bits)
   25:     len 155..666   (9 extra bits)
   26:     len 667..2714  (11 extra bits)
   27:     len 2715..35482 (15 extra bits) */

/* Delta-to-absolute table for tree decoding (from decompress.c).
   vval[prev_length][delta_code] = absolute_length */
extern const u8 vval[NumDeltaCodes][NumDeltaCodes];

/* Inverse: absolute-to-delta table for tree encoding.
   ivval[prev_length][abs_length] = delta_code */
extern const u8 ivval[NumDeltaCodes][NumDeltaCodes];

/* Default Huffman code lengths for the first block */
void uc2_default_lengths(u8 d[NumSymbols]);

/* Little-endian record types */
typedef struct u16le { u8 b[2]; } u16le;
typedef struct u32le { u8 b[4]; } u32le;

static inline u16 get16(u16le v) { return v.b[0] | v.b[1] << 8; }
static inline u32 get32(u32le v) { return v.b[0] | v.b[1] << 8 | v.b[2] << 16 | (u32)v.b[3] << 24; }
static inline u16le put16(u16 v) { return (u16le){{v & 0xff, v >> 8}}; }
static inline u32le put32(u32 v) { return (u32le){{v & 0xff, v >> 8 & 0xff, v >> 16 & 0xff, v >> 24}}; }

/* Fletcher checksum (XOR-based, as used by UC2) */
struct csum { u32 value; };

static inline void csum_init(struct csum *cs) { cs->value = 0xA55A; }

static inline void csum_update(struct csum *cs, const u8 *p, unsigned n)
{
	if (!n) return;
	u32 v = cs->value;
	const u8 *e = p + n - 1;
	if (v > 0xffff)
		v ^= *p++ << 8;
	while (p < e) {
		v ^= p[0] | p[1] << 8;
		p += 2;
	}
	v &= 0xffff;
	if (p == e)
		v ^= *p | 0x10000;
	cs->value = v;
}

static inline u16 csum_get(struct csum *cs) { return (u16)cs->value; }

#endif
