/* Merkle DAG for content-addressable deduplication.
 *
 * Builds a Merkle tree from CDC chunks: each file is represented as a
 * list of chunk hashes.  The file's root hash is derived from the
 * concatenated chunk hashes, enabling structural comparison.
 *
 * Usage:
 *   struct uc2_merkle tree;
 *   uc2_merkle_build(&tree, data, len, 13);
 *   uint64_t root = uc2_merkle_root(&tree);
 *   int shared = uc2_merkle_common(&tree_a, &tree_b);
 *   uc2_merkle_free(&tree);
 */

#ifndef UC2_MERKLE_H
#define UC2_MERKLE_H

#include <stdint.h>
#include <stddef.h>

/* A chunk in the Merkle tree. */
struct uc2_chunk {
	uint64_t hash;     /* content hash of this chunk */
	uint32_t offset;   /* offset within the file */
	uint32_t length;   /* chunk length in bytes */
};

/* Merkle tree for one file. */
struct uc2_merkle {
	struct uc2_chunk *chunks;
	int nchunks;
	int capacity;
	uint64_t root;     /* root hash (hash of chunk hash list) */
};

/* Build a Merkle tree from file data.
 *   tree:  output tree (caller must call uc2_merkle_free later)
 *   data:  file content
 *   len:   file length
 *   bits:  CDC chunk size exponent (13 = avg 8KB) */
void uc2_merkle_build(struct uc2_merkle *tree,
                      const uint8_t *data, size_t len, int bits);

/* Get the root hash of a Merkle tree. */
static inline uint64_t uc2_merkle_root(const struct uc2_merkle *tree)
{
	return tree->root;
}

/* Count chunks shared between two Merkle trees (by hash). */
int uc2_merkle_common(const struct uc2_merkle *a, const struct uc2_merkle *b);

/* Compute the fraction of bytes in tree A covered by shared chunks with B.
 * Returns 0.0 (no overlap) to 1.0 (identical content). */
double uc2_merkle_similarity(const struct uc2_merkle *a,
                             const struct uc2_merkle *b);

/* Free a Merkle tree's chunk array. */
void uc2_merkle_free(struct uc2_merkle *tree);

/* 64-bit content hash (FNV-1a 64-bit). */
uint64_t uc2_hash64(const uint8_t *data, size_t len);

#endif
