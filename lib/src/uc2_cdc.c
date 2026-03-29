/* Content-defined chunking (CDC) for UC2 deduplication.
 *
 * Gear hash: each byte updates the hash by shifting left and XORing
 * with a pre-computed random table entry.  This gives uniform
 * distribution and O(1) per-byte cost.  A chunk boundary is detected
 * when (hash & mask) == 0, giving an average chunk size of 2^bits.
 *
 * Reference: "A Framework for Analyzing and Improving Content-Based
 * Chunking Algorithms" (Xia et al., HP Labs, 2005).
 */

#include "uc2/uc2_cdc.h"
#include <string.h>

/* Gear hash lookup table: 256 random 32-bit values.
   Generated from a PRNG seeded with the string "UC2 Gear CDC". */
static const uint32_t gear_table[256] = {
	0x5c27b2e4, 0x8a3b9c01, 0xf7e52d9f, 0x3d14a867, 0xc6f893b2, 0x91d047e5, 0x2e6b1fa8, 0xe4a37c63,
	0x7f582b1d, 0xb90c64f6, 0x46d1e823, 0x13a95f7b, 0xd87e24c9, 0xa5430168, 0x6c9fb3d4, 0x028e7a1f,
	0xfb614d93, 0x3742c856, 0x84b50fea, 0xc1d6937e, 0x590a2eb1, 0xaef41c67, 0x67c385d2, 0x0dbf694a,
	0xe2984513, 0x76ab3dc8, 0x4517e29f, 0xb86a0c54, 0x1e23f7b6, 0xd3c58e41, 0x8a71b02d, 0xf09d43e8,
	0x2b06d175, 0x9f48a623, 0xc3e71bdf, 0x54b2f906, 0x1d65c48a, 0xe83a074b, 0x72196ed3, 0xa4de8b17,
	0x3fac5264, 0xd10738b9, 0x6ec4a1f5, 0x8593d642, 0x4a7f1d8e, 0xf6b2e071, 0x2748bc3a, 0xc981459d,
	0x50f37e26, 0xbe269ac3, 0x13da4587, 0x9c07b1f4, 0x614ed368, 0xa7bc2f15, 0xd4f56c89, 0x38a19047,
	0x876cb5e2, 0xe53d48ab, 0x42801d76, 0xfc17a93c, 0x0b9e62d1, 0x7654cf08, 0xcda37b94, 0x19e80e5f,
	0xab3c91d7, 0x6271f4a6, 0xd8bf2843, 0x3506de71, 0xf94a637b, 0x8ed5b02c, 0x471c89e5, 0x0a63d4f9,
	0xc4982e17, 0x7db15a8c, 0x12ef4360, 0xb637c9a5, 0x5f740ed8, 0xe1a8b524, 0x28c96f13, 0x93014876,
	0xdae27b9d, 0x3d8f15c2, 0x815ca04e, 0xf47e6d39, 0x4b93d2f7, 0xa620be81, 0x69d7014a, 0xc5b4f836,
	0x1c486aeb, 0x70a5931d, 0xef12dc64, 0x8279b508, 0xb6c34a9f, 0x57e82173, 0x0a1f7dc6, 0xde64c952,
	0x43b0a819, 0xad5e37e4, 0x6897cb71, 0xf1240f9c, 0x342bc6a5, 0x9d1852e8, 0xc7fa9b34, 0x586d4e07,
	0xb2a1d3f6, 0x2536ec89, 0x7ecb1047, 0xe408a5bd, 0x0f957e62, 0xd3ca81a0, 0x917f2d14, 0xfa42b6d9,
	0x45d968b3, 0xbbe50c37, 0x1274f1e5, 0x6a9e3db8, 0xcf538241, 0x87a0c96f, 0x5eb75423, 0x31dc0fa7,
	0xa41b63c4, 0xd96fae58, 0x4cd2891e, 0xf5863072, 0x0b17e4a6, 0x7c60bd9d, 0xe39845c1, 0xb85e2f17,
	0x21a37689, 0x9e4fc153, 0xd702dba4, 0x5384e96f, 0xaf51067c, 0x64c83db1, 0xc2e7f548, 0x3a198c24,
	0xf06b47d2, 0x85d2a19e, 0x4f3e5c63, 0x19c78b07, 0xe6a402db, 0x7b59d3f4, 0xbd146ea5, 0x0e82c917,
	0xc3f01b76, 0x5da564a9, 0x32b9f852, 0xa847201c, 0x6e9cb7e3, 0x81635d38, 0x470ad1bf, 0xfc718946,
	0x16ce3fa2, 0x9ab045e7, 0xd52c6814, 0x43f9bc79, 0xb8e213a6, 0x2f174e51, 0x657d90cd, 0xcda4f738,
	0x0198269b, 0x7e3cdb54, 0xe26f8013, 0x39c154e7, 0xa45db39c, 0xd792e841, 0x58067f2b, 0xb3adc466,
	0x1b41a5d0, 0x76e83917, 0xcf250b74, 0x84b7d2a8, 0x4dc69e53, 0xf01a47bf, 0x28f361c4, 0x93758c19,
	0xe5c24037, 0x3a8ef956, 0x7e51b682, 0xc107da4f, 0x5269031d, 0xad84c7e6, 0x6eb3589a, 0x0f4ea143,
	0xd8356fd7, 0x417c9e2b, 0xba20d364, 0x25f745a8, 0xf6c11e79, 0x7db8a30c, 0x830f52b4, 0x49617cd9,
	0x1cda0e63, 0xa7b23148, 0xde46c5f2, 0x63895db7, 0xb21ea481, 0x574c6f0e, 0x0a8392c5, 0xc5f7b84a,
	0x380e41d6, 0xed72d923, 0x91c5a687, 0x4a19f054, 0xf4a83b19, 0x673d8ec2, 0xbce1470b, 0x01567da4,
	0xd8abc196, 0x2490534e, 0x7de7bf83, 0xc3348217, 0x5f629ed5, 0xa6b70468, 0x1c43d7a9, 0x89f56b30,
	0x4508cfe1, 0xf27a1694, 0xb81e5d47, 0x05a9c3ba, 0xdac28f62, 0x61b740d5, 0x9e3f254c, 0x37d4a8e1,
	0x8b612c97, 0xc419f035, 0x5d8e7ba6, 0xa2f3d14c, 0x16458db9, 0xeb27c673, 0x70da0e28, 0xbf9c53e4,
	0x42a1679f, 0xde38b102, 0x95c42f56, 0x037bd8a1, 0xfc1645ed, 0x69ea9cb3, 0xad5f0374, 0x3487e1c9,
	0xc0b29d15, 0x5e617a48, 0x8714c6bf, 0x1da93273, 0xf2d5e804, 0x764b5f96, 0xab86031d, 0x41c8b4e2,
	0xd53a6927, 0x0f91dc83, 0xe8450b5a, 0x72f7a1c6, 0xbc234d90, 0x2dbe7641, 0x960cf5bd, 0x5b618a49,
};

uint32_t uc2_gear_hash(const uint8_t *data, size_t len)
{
	uint32_t h = 0;
	for (size_t i = 0; i < len; i++)
		h = (h << 1) + gear_table[data[i]];
	return h;
}

void uc2_chunker_init(struct uc2_chunker *c, int bits,
                      size_t min_chunk, size_t max_chunk)
{
	if (bits < 8)  bits = 8;
	if (bits > 20) bits = 20;
	c->mask = ((uint32_t)1 << bits) - 1;
	c->min_chunk = min_chunk ? min_chunk : ((size_t)1 << (bits - 2));
	c->max_chunk = max_chunk ? max_chunk : ((size_t)1 << (bits + 2));
	c->pos = 0;
}

void uc2_chunker_reset(struct uc2_chunker *c)
{
	c->pos = 0;
}

int uc2_chunker_next(struct uc2_chunker *c,
                     const uint8_t *data, size_t len,
                     size_t *chunk_off, size_t *chunk_len)
{
	if (c->pos >= len)
		return 0;

	size_t start = c->pos;
	size_t end = start + c->max_chunk;
	if (end > len) end = len;

	/* Skip minimum chunk size before checking boundaries */
	size_t scan = start + c->min_chunk;
	if (scan > end) scan = end;

	uint32_t h = 0;
	/* Prime the hash over the min_chunk prefix */
	for (size_t i = start; i < scan; i++)
		h = (h << 1) + gear_table[data[i]];

	/* Scan for boundary: (hash & mask) == 0 */
	for (size_t i = scan; i < end; i++) {
		h = (h << 1) + gear_table[data[i]];
		if ((h & c->mask) == 0) {
			*chunk_off = start;
			*chunk_len = i + 1 - start;
			c->pos = i + 1;
			return 1;
		}
	}

	/* No boundary found: emit max_chunk or remaining data */
	*chunk_off = start;
	*chunk_len = end - start;
	c->pos = end;
	return (c->pos < len) ? 1 : 0;
}

uint32_t uc2_fnv1a(const uint8_t *data, size_t len)
{
	uint32_t h = 2166136261u;
	for (size_t i = 0; i < len; i++) {
		h ^= data[i];
		h *= 16777619u;
	}
	return h;
}
