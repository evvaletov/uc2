/* Near-duplicate detection via SimHash.
 *
 * SimHash produces a fixed-size fingerprint where similar documents
 * have fingerprints with small Hamming distance.  Two files are
 * "near-duplicates" if their SimHash fingerprints differ in fewer
 * than a threshold number of bits.
 *
 * This detects patched executables, slightly edited documents, and
 * minor revisions — cases where CDC chunks might not align but the
 * overall content is structurally similar.
 *
 * Usage:
 *   uint64_t h1 = uc2_simhash(data1, len1);
 *   uint64_t h2 = uc2_simhash(data2, len2);
 *   int dist = uc2_hamming(h1, h2);
 *   if (dist <= 10) // near-duplicates
 */

#ifndef UC2_SIMHASH_H
#define UC2_SIMHASH_H

#include <stdint.h>
#include <stddef.h>

/* Compute a 64-bit SimHash fingerprint.
 * Uses 4-byte shingles hashed with FNV-1a, accumulated into a
 * 64-bit vector where each bit is the majority vote of all
 * shingle hash bits. */
uint64_t uc2_simhash(const uint8_t *data, size_t len);

/* Hamming distance between two SimHash fingerprints (0-64). */
int uc2_hamming(uint64_t a, uint64_t b);

/* Check if two fingerprints are near-duplicates.
 * threshold: max Hamming distance (recommended: 6-10 for text,
 * 3-6 for binary). */
static inline int uc2_is_near_dup(uint64_t a, uint64_t b, int threshold)
{
	return uc2_hamming(a, b) <= threshold;
}

#endif
