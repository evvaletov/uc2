/* Merkle DAG for content-addressable deduplication.
 *
 * Each file is split into CDC chunks (Gear hash), each chunk hashed
 * with FNV-1a 64-bit.  The file's root hash is computed from the
 * concatenated chunk hashes, forming a single-level Merkle tree.
 *
 * Comparison operations find shared chunks between trees, enabling
 * dedup decisions based on structural content similarity rather than
 * simple byte-prefix matching. */

#include "uc2/uc2_merkle.h"
#include "uc2/uc2_cdc.h"
#include <stdlib.h>
#include <string.h>

uint64_t uc2_hash64(const uint8_t *data, size_t len)
{
	uint64_t h = 14695981039346656037ULL;
	for (size_t i = 0; i < len; i++) {
		h ^= data[i];
		h *= 1099511628211ULL;
	}
	return h;
}

void uc2_merkle_build(struct uc2_merkle *tree,
                      const uint8_t *data, size_t len, int bits)
{
	tree->chunks = NULL;
	tree->nchunks = 0;
	tree->capacity = 0;
	tree->root = 0;

	if (!data || len == 0)
		return;

	struct uc2_chunker chunker;
	uc2_chunker_init(&chunker, bits, 0, 0);

	size_t off, clen;
	int more = 1;
	while (more) {
		more = uc2_chunker_next(&chunker, data, len, &off, &clen);
		if (clen == 0) break;

		if (tree->nchunks >= tree->capacity) {
			tree->capacity = tree->capacity ? tree->capacity * 2 : 16;
			tree->chunks = realloc(tree->chunks,
			                       (size_t)tree->capacity * sizeof *tree->chunks);
		}
		struct uc2_chunk *c = &tree->chunks[tree->nchunks++];
		c->hash = uc2_hash64(data + off, clen);
		c->offset = (uint32_t)off;
		c->length = (uint32_t)clen;
	}

	/* Root hash = hash of concatenated chunk hashes */
	if (tree->nchunks > 0) {
		uint8_t *hashbuf = malloc((size_t)tree->nchunks * 8);
		if (hashbuf) {
			for (int i = 0; i < tree->nchunks; i++) {
				uint64_t h = tree->chunks[i].hash;
				for (int j = 0; j < 8; j++)
					hashbuf[i * 8 + j] = (uint8_t)(h >> (j * 8));
			}
			tree->root = uc2_hash64(hashbuf, (size_t)tree->nchunks * 8);
			free(hashbuf);
		}
	}
}

int uc2_merkle_common(const struct uc2_merkle *a, const struct uc2_merkle *b)
{
	int count = 0;
	for (int i = 0; i < a->nchunks; i++)
		for (int j = 0; j < b->nchunks; j++)
			if (a->chunks[i].hash == b->chunks[j].hash) {
				count++;
				break;  /* count each A chunk at most once */
			}
	return count;
}

double uc2_merkle_similarity(const struct uc2_merkle *a,
                             const struct uc2_merkle *b)
{
	if (a->nchunks == 0) return 0.0;

	uint32_t shared_bytes = 0;
	uint32_t total_bytes = 0;
	for (int i = 0; i < a->nchunks; i++) {
		total_bytes += a->chunks[i].length;
		for (int j = 0; j < b->nchunks; j++)
			if (a->chunks[i].hash == b->chunks[j].hash) {
				shared_bytes += a->chunks[i].length;
				break;
			}
	}
	return total_bytes > 0 ? (double)shared_bytes / total_bytes : 0.0;
}

void uc2_merkle_free(struct uc2_merkle *tree)
{
	free(tree->chunks);
	tree->chunks = NULL;
	tree->nchunks = 0;
	tree->capacity = 0;
}
