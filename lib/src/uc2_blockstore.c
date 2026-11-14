/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Cross-archive block store for content-addressable deduplication.
 *
 * Chunks are stored as individual files named by their 64-bit hash
 * (hex encoded). The first two hex characters select a subdirectory,
 * distributing files across 256 directories. */

#include "uc2/uc2_blockstore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>
#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & _S_IFMT) == _S_IFREG)
#endif
#endif

static int hash_path(const struct uc2_blockstore *bs, uint64_t hash,
                     char *buf, size_t buf_size)
{
	/* Two-level: store/AB/ABCDEF0123456789 */
	if (!bs || !bs->path) return -1;
	int n = snprintf(buf, buf_size, "%s/%02x/%016llx",
	         bs->path, (unsigned)(hash >> 56) & 0xFF,
	         (unsigned long long)hash);
	return n < 0 || (size_t)n >= buf_size ? -1 : 0;
}

static int ensure_directory(const char *path)
{
	if (mkdir(path, 0755) < 0 && errno != EEXIST) return -1;
	struct stat st;
	return stat(path, &st) == 0 && S_ISDIR(st.st_mode) ? 0 : -1;
}

static int ensure_subdir(const struct uc2_blockstore *bs, uint64_t hash)
{
	char dir[4096];
	int n = snprintf(dir, sizeof dir, "%s/%02x",
	         bs->path, (unsigned)(hash >> 56) & 0xFF);
	if (n < 0 || (size_t)n >= sizeof dir) return -1;
	return ensure_directory(dir);
}

/* Distinguish a missing object from an inspection error for ingestion. */
static int inspect_chunk(const struct uc2_blockstore *bs, uint64_t hash,
                          char *path, size_t cap, struct stat *st)
{
	if (hash_path(bs, hash, path, cap) < 0) return -1;
	if (stat(path, st) < 0) return errno == ENOENT ? 0 : -1;
	return S_ISREG(st->st_mode) ? 1 : -1;
}

int uc2_blockstore_open(struct uc2_blockstore *bs, const char *path)
{
	memset(bs, 0, sizeof *bs);
	if (!path) return -1;
	bs->path = strdup(path);
	if (!bs->path) return -1;

	/* Create store directory if it doesn't exist */
	if (ensure_directory(path) < 0) {
		free(bs->path);
		bs->path = NULL;
		return -1;
	}
	return 0;
}

int uc2_blockstore_has(const struct uc2_blockstore *bs, uint64_t hash)
{
	char fpath[4096];
	struct stat st;
	return inspect_chunk(bs, hash, fpath, sizeof fpath, &st) > 0;
}

int uc2_blockstore_ingest(struct uc2_blockstore *bs,
                          const struct uc2_merkle *tree,
                          const uint8_t *data, size_t len)
{
	if (!bs || !bs->path || !tree || tree->nchunks < 0 ||
	    (tree->nchunks && !tree->chunks) || (!data && len) || len > UINT32_MAX)
		return -1;
	/* Reject invalid caller-supplied ranges before storing any chunks. */
	for (int i = 0; i < tree->nchunks; i++) {
		uint32_t off = tree->chunks[i].offset, clen = tree->chunks[i].length;
		if (!clen || off > len || clen > len - off) return -1;
	}
	int new_chunks = 0;
	for (int i = 0; i < tree->nchunks; i++) {
		uint64_t h = tree->chunks[i].hash;
		uint32_t off = tree->chunks[i].offset;
		uint32_t clen = tree->chunks[i].length;

		char fpath[4096];
		struct stat st;
		int present = inspect_chunk(bs, h, fpath, sizeof fpath, &st);
		if (present < 0) return -1;
		if (present) {
			if (st.st_size < 0 || (uint64_t)st.st_size != clen ||
			    bs->saved_bytes > INT64_MAX - clen) return -1;
			bs->saved_bytes += clen;
			continue;
		}

		if (bs->nblocks == INT_MAX || bs->total_bytes > INT64_MAX - clen ||
		    ensure_subdir(bs, h) < 0) return -1;
		FILE *f = fopen(fpath, "wb");
		if (!f) return -1;
		size_t written = fwrite(data + off, 1, clen, f);
		int closed = fclose(f);
		if (written != clen || closed != 0) {
			remove(fpath);
			return -1;
		}

		bs->nblocks++;
		bs->total_bytes += clen;
		new_chunks++;
	}
	return new_chunks;
}

int uc2_blockstore_read(const struct uc2_blockstore *bs,
                        uint64_t hash, uint8_t *buf, size_t buf_size)
{
	if ((!buf && buf_size) || buf_size > INT_MAX) return -1;
	char fpath[4096];
	struct stat st;
	if (inspect_chunk(bs, hash, fpath, sizeof fpath, &st) != 1) return -1;
	FILE *f = fopen(fpath, "rb");
	if (!f) return -1;
	size_t n = fread(buf, 1, buf_size, f);
	int failed = ferror(f);
	int closed = fclose(f);
	return failed || closed != 0 ? -1 : (int)n;
}

void uc2_blockstore_close(struct uc2_blockstore *bs)
{
	free(bs->path);
	memset(bs, 0, sizeof *bs);
}
