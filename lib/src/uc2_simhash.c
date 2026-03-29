/* Near-duplicate detection via SimHash.
 *
 * Algorithm: extract overlapping 4-byte shingles from the input,
 * hash each with FNV-1a 64-bit, then for each bit position, count
 * how many hashes have that bit set vs clear.  The final SimHash
 * bit is 1 if the majority of shingle hashes had 1 in that position.
 *
 * This gives a locality-sensitive hash: similar inputs produce
 * fingerprints with small Hamming distance. */

#include "uc2/uc2_simhash.h"

static uint64_t fnv1a_64(const uint8_t *data, size_t len)
{
	uint64_t h = 14695981039346656037ULL;
	for (size_t i = 0; i < len; i++) {
		h ^= data[i];
		h *= 1099511628211ULL;
	}
	return h;
}

uint64_t uc2_simhash(const uint8_t *data, size_t len)
{
	if (len < 4) {
		/* Too short for shingles: just hash directly */
		return fnv1a_64(data, len);
	}

	/* Accumulate bit votes: positive = more 1s, negative = more 0s */
	int32_t votes[64];
	for (int i = 0; i < 64; i++)
		votes[i] = 0;

	/* Slide 4-byte shingles */
	size_t nshingles = len - 3;
	for (size_t i = 0; i < nshingles; i++) {
		uint64_t h = fnv1a_64(data + i, 4);
		for (int b = 0; b < 64; b++) {
			if (h & ((uint64_t)1 << b))
				votes[b]++;
			else
				votes[b]--;
		}
	}

	/* Majority vote */
	uint64_t result = 0;
	for (int b = 0; b < 64; b++)
		if (votes[b] > 0)
			result |= (uint64_t)1 << b;

	return result;
}

int uc2_hamming(uint64_t a, uint64_t b)
{
	uint64_t x = a ^ b;
	int count = 0;
	while (x) {
		count++;
		x &= x - 1;  /* clear lowest set bit */
	}
	return count;
}
