/* Cross-archive block store for content-addressable deduplication.
 *
 * Chunks are stored as individual files named by their 64-bit hash
 * (hex encoded).  This is simple and portable  --  no database needed.
 * For large stores, a two-level directory structure (first 2 hex chars
 * as subdirectory) prevents filesystem performance issues. */

#include "uc2/uc2_blockstore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

static void hash_path(const struct uc2_blockstore *bs, uint64_t hash,
                      char *buf, size_t buf_size)
{
	/* Two-level: store/AB/ABCDEF0123456789 */
	snprintf(buf, buf_size, "%s/%02x/%016llx",
	         bs->path, (unsigned)(hash >> 56) & 0xFF,
	         (unsigned long long)hash);
}

static void ensure_subdir(const struct uc2_blockstore *bs, uint64_t hash)
{
	char dir[4096];
	snprintf(dir, sizeof dir, "%s/%02x",
	         bs->path, (unsigned)(hash >> 56) & 0xFF);
	mkdir(dir, 0755);
}

int uc2_blockstore_open(struct uc2_blockstore *bs, const char *path)
{
	memset(bs, 0, sizeof *bs);
	bs->path = strdup(path);
	if (!bs->path) return -1;

	/* Create store directory if it doesn't exist */
	if (mkdir(path, 0755) < 0 && errno != EEXIST) {
		free(bs->path);
		bs->path = NULL;
		return -1;
	}
	return 0;
}

int uc2_blockstore_has(const struct uc2_blockstore *bs, uint64_t hash)
{
	char fpath[4096];
	hash_path(bs, hash, fpath, sizeof fpath);
	struct stat st;
	return stat(fpath, &st) == 0;
}

int uc2_blockstore_ingest(struct uc2_blockstore *bs,
                          const struct uc2_merkle *tree,
                          const uint8_t *data, size_t len)
{
	int new_chunks = 0;
	for (int i = 0; i < tree->nchunks; i++) {
		uint64_t h = tree->chunks[i].hash;
		uint32_t off = tree->chunks[i].offset;
		uint32_t clen = tree->chunks[i].length;

		if (off + clen > len) continue;

		if (uc2_blockstore_has(bs, h)) {
			bs->saved_bytes += clen;
			continue;
		}

		ensure_subdir(bs, h);
		char fpath[4096];
		hash_path(bs, h, fpath, sizeof fpath);
		FILE *f = fopen(fpath, "wb");
		if (!f) continue;
		fwrite(data + off, 1, clen, f);
		fclose(f);

		bs->nblocks++;
		bs->total_bytes += clen;
		new_chunks++;
	}
	return new_chunks;
}

int uc2_blockstore_read(const struct uc2_blockstore *bs,
                        uint64_t hash, uint8_t *buf, size_t buf_size)
{
	char fpath[4096];
	hash_path(bs, hash, fpath, sizeof fpath);
	FILE *f = fopen(fpath, "rb");
	if (!f) return -1;
	int n = (int)fread(buf, 1, buf_size, f);
	fclose(f);
	return n;
}

void uc2_blockstore_close(struct uc2_blockstore *bs)
{
	free(bs->path);
	memset(bs, 0, sizeof *bs);
}
