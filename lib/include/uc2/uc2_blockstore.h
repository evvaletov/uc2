/* Cross-archive block store for content-addressable deduplication.
 *
 * Stores unique CDC chunks indexed by 64-bit content hash.  Multiple
 * archives can share blocks through the store, enabling cross-archive
 * and cross-version dedup.
 *
 * The store is a directory of chunk files named by their hash.  A
 * manifest maps (archive, file, chunk_index) -> chunk_hash, enabling
 * reconstruction of any file from its chunk list.
 *
 * Usage:
 *   struct uc2_blockstore bs;
 *   uc2_blockstore_open(&bs, "/path/to/store");
 *   uc2_blockstore_ingest(&bs, &merkle_tree, data, len);
 *   // ... later, from a different archive:
 *   int new_chunks = uc2_blockstore_ingest(&bs, &tree2, data2, len2);
 *   // new_chunks < tree2.nchunks means dedup happened
 *   uc2_blockstore_close(&bs);
 */

#ifndef UC2_BLOCKSTORE_H
#define UC2_BLOCKSTORE_H

#include <stdint.h>
#include <stddef.h>
#include "uc2_merkle.h"

/* Block store state. */
struct uc2_blockstore {
	char *path;        /* store directory path */
	int nblocks;       /* number of unique blocks stored */
	int64_t total_bytes; /* total bytes of unique block data */
	int64_t saved_bytes; /* bytes saved by dedup */
};

/* Open or create a block store at the given directory path.
 * Returns 0 on success, -1 on error. */
int uc2_blockstore_open(struct uc2_blockstore *bs, const char *path);

/* Ingest a file's chunks into the store.  Only stores chunks not
 * already present (dedup).  Returns the number of NEW chunks stored
 * (0 = fully deduplicated). */
int uc2_blockstore_ingest(struct uc2_blockstore *bs,
                          const struct uc2_merkle *tree,
                          const uint8_t *data, size_t len);

/* Check if a chunk exists in the store. */
int uc2_blockstore_has(const struct uc2_blockstore *bs, uint64_t hash);

/* Read a chunk from the store into buf (must be large enough).
 * Returns chunk length, or -1 on error. */
int uc2_blockstore_read(const struct uc2_blockstore *bs,
                        uint64_t hash, uint8_t *buf, size_t buf_size);

/* Get dedup statistics. */
static inline int64_t uc2_blockstore_saved(const struct uc2_blockstore *bs)
{
	return bs->saved_bytes;
}

/* Close the block store (frees internal state, does not delete files). */
void uc2_blockstore_close(struct uc2_blockstore *bs);

#endif
