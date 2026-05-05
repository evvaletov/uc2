/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "uc2/uc2_ingest.h"
#include "uc2/uc2_blockstore.h"
#include "uc2/uc2_merkle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INGEST_MAGIC[8] = { 'U','C','2','I','N','G','S','T' };
#define INGEST_VERSION_V1  1
#define INGEST_VERSION_V2  2
#define DEFAULT_CDC_BITS   13

#define ENTRY_SIZE_V1   12u  /* 8B hash + 4B length */
#define ENTRY_SIZE_V2   16u  /* 8B hash + 4B length + 4B offset */
#define HEADER_SIZE     16u

static char *make_blocks_path(const char *archive_path)
{
	size_t n = strlen(archive_path);
	char *p = malloc(n + 8);
	if (!p) return NULL;
	memcpy(p, archive_path, n);
	memcpy(p + n, ".blocks", 8); /* includes trailing NUL */
	return p;
}

static void put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static void put_le64(uint8_t *p, uint64_t v)
{
	for (int i = 0; i < 8; i++)
		p[i] = (uint8_t)(v >> (i * 8));
}

static uint32_t get_le32(const uint8_t *p)
{
	return (uint32_t)p[0]
	     | ((uint32_t)p[1] << 8)
	     | ((uint32_t)p[2] << 16)
	     | ((uint32_t)p[3] << 24);
}

static uint64_t get_le64(const uint8_t *p)
{
	uint64_t v = 0;
	for (int i = 0; i < 8; i++)
		v |= (uint64_t)p[i] << (i * 8);
	return v;
}

/* Linear-probed open-addressing hash map: hash -> file offset.
 * Used during write to record where each unique chunk lives so
 * subsequent appearances of the same hash share an offset. */
struct dedup_map {
	uint64_t *keys;     /* 0 = empty slot */
	uint32_t *offsets;  /* offset of chunk in archive */
	int       cap;
	int       len;
};

static int dedup_map_init(struct dedup_map *m, int initial_cap)
{
	/* Round up to power of two; mask-based probing requires it. */
	int cap = 16;
	while (cap < initial_cap) cap *= 2;
	m->keys = calloc((size_t)cap, sizeof *m->keys);
	m->offsets = calloc((size_t)cap, sizeof *m->offsets);
	if (!m->keys || !m->offsets) {
		free(m->keys); free(m->offsets);
		m->keys = NULL; m->offsets = NULL;
		return -1;
	}
	m->cap = cap;
	m->len = 0;
	return 0;
}

static void dedup_map_free(struct dedup_map *m)
{
	free(m->keys);
	free(m->offsets);
	m->keys = NULL;
	m->offsets = NULL;
}

static int dedup_map_grow(struct dedup_map *m);

/* Look up hash in map.  If present, return its offset via *out_off
 * and return 1.  Else return 0 (caller may insert). */
static int dedup_map_get(const struct dedup_map *m, uint64_t hash,
                         uint32_t *out_off)
{
	if (m->cap == 0) return 0;
	uint64_t mask = (uint64_t)m->cap - 1;
	uint64_t i = hash & mask;
	for (int probe = 0; probe < m->cap; probe++) {
		if (m->keys[i] == 0) return 0;
		if (m->keys[i] == hash) {
			*out_off = m->offsets[i];
			return 1;
		}
		i = (i + 1) & mask;
	}
	return 0;
}

static int dedup_map_put(struct dedup_map *m, uint64_t hash, uint32_t off)
{
	if (hash == 0) hash = 1;  /* sentinel collision: shift to 1 */
	if ((m->len + 1) * 2 > m->cap) {
		if (dedup_map_grow(m) != 0) return -1;
	}
	uint64_t mask = (uint64_t)m->cap - 1;
	uint64_t i = hash & mask;
	while (m->keys[i] != 0) {
		if (m->keys[i] == hash) return 0;  /* already inserted */
		i = (i + 1) & mask;
	}
	m->keys[i] = hash;
	m->offsets[i] = off;
	m->len++;
	return 0;
}

static int dedup_map_grow(struct dedup_map *m)
{
	int ncap = m->cap ? m->cap * 2 : 16;
	uint64_t *nkeys = calloc((size_t)ncap, sizeof *nkeys);
	uint32_t *noffs = calloc((size_t)ncap, sizeof *noffs);
	if (!nkeys || !noffs) {
		free(nkeys); free(noffs);
		return -1;
	}
	uint64_t mask = (uint64_t)ncap - 1;
	for (int j = 0; j < m->cap; j++) {
		uint64_t k = m->keys[j];
		if (k == 0) continue;
		uint64_t i = k & mask;
		while (nkeys[i] != 0) i = (i + 1) & mask;
		nkeys[i] = k;
		noffs[i] = m->offsets[j];
	}
	free(m->keys);
	free(m->offsets);
	m->keys = nkeys;
	m->offsets = noffs;
	m->cap = ncap;
	return 0;
}

int uc2_ingest_write(const char *archive_path,
                     const uint8_t *data, size_t len,
                     int cdc_bits,
                     struct uc2_ingest_stats *stats)
{
	if (!archive_path)
		return -1;
	if (cdc_bits <= 0)
		cdc_bits = DEFAULT_CDC_BITS;

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, len, cdc_bits);

	FILE *f = fopen(archive_path, "wb");
	if (!f) {
		uc2_merkle_free(&tree);
		return -1;
	}

	/* Header */
	uint8_t hdr[HEADER_SIZE];
	memcpy(hdr, INGEST_MAGIC, 8);
	hdr[8]  = INGEST_VERSION_V2;
	hdr[9]  = (uint8_t)cdc_bits;
	hdr[10] = 0;
	hdr[11] = 0;
	put_le32(hdr + 12, (uint32_t)tree.nchunks);
	if (fwrite(hdr, 1, sizeof hdr, f) != sizeof hdr) {
		fclose(f);
		uc2_merkle_free(&tree);
		return -1;
	}

	/* Reserve manifest entry table; we'll backfill offsets after
	 * appending the chunk pool. */
	long manifest_off = ftell(f);
	size_t manifest_size = (size_t)tree.nchunks * ENTRY_SIZE_V2;
	if (tree.nchunks > 0) {
		uint8_t *zero = calloc(manifest_size, 1);
		if (!zero) {
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
		size_t w = fwrite(zero, 1, manifest_size, f);
		free(zero);
		if (w != manifest_size) {
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
	}

	/* Append unique chunks; record offset per hash. */
	struct dedup_map dmap;
	if (dedup_map_init(&dmap, tree.nchunks > 16 ? tree.nchunks * 2 : 16) != 0) {
		fclose(f);
		uc2_merkle_free(&tree);
		return -1;
	}

	uint32_t *entry_offsets = calloc((size_t)tree.nchunks, sizeof *entry_offsets);
	if (tree.nchunks > 0 && !entry_offsets) {
		dedup_map_free(&dmap);
		fclose(f);
		uc2_merkle_free(&tree);
		return -1;
	}

	int new_chunks = 0;
	uint64_t bytes_appended = 0;
	uint64_t bytes_saved = 0;
	for (int i = 0; i < tree.nchunks; i++) {
		uint64_t h = tree.chunks[i].hash;
		uint32_t clen = tree.chunks[i].length;
		uint32_t off;
		if (dedup_map_get(&dmap, h, &off)) {
			entry_offsets[i] = off;
			bytes_saved += clen;
			continue;
		}
		long here = ftell(f);
		if (here < 0 || (uint64_t)here > 0xFFFFFFFFu) {
			free(entry_offsets);
			dedup_map_free(&dmap);
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
		off = (uint32_t)here;
		entry_offsets[i] = off;
		if (dedup_map_put(&dmap, h, off) != 0) {
			free(entry_offsets);
			dedup_map_free(&dmap);
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
		size_t w = fwrite(data + tree.chunks[i].offset, 1, clen, f);
		if (w != clen) {
			free(entry_offsets);
			dedup_map_free(&dmap);
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
		bytes_appended += clen;
		new_chunks++;
	}

	/* Backfill manifest entries. */
	if (tree.nchunks > 0) {
		if (fseek(f, manifest_off, SEEK_SET) != 0) {
			free(entry_offsets);
			dedup_map_free(&dmap);
			fclose(f);
			uc2_merkle_free(&tree);
			return -1;
		}
		for (int i = 0; i < tree.nchunks; i++) {
			uint8_t rec[ENTRY_SIZE_V2];
			put_le64(rec, tree.chunks[i].hash);
			put_le32(rec + 8, tree.chunks[i].length);
			put_le32(rec + 12, entry_offsets[i]);
			if (fwrite(rec, 1, sizeof rec, f) != sizeof rec) {
				free(entry_offsets);
				dedup_map_free(&dmap);
				fclose(f);
				uc2_merkle_free(&tree);
				return -1;
			}
		}
	}

	free(entry_offsets);
	dedup_map_free(&dmap);

	if (fclose(f) != 0) {
		uc2_merkle_free(&tree);
		return -1;
	}

	if (stats) {
		stats->bytes_in     = (uint64_t)len;
		stats->chunks_total = tree.nchunks;
		stats->chunks_new   = new_chunks;
		stats->chunks_dedup = tree.nchunks - new_chunks;
		stats->bytes_stored = bytes_appended;
		stats->bytes_saved  = bytes_saved;
	}

	uc2_merkle_free(&tree);
	return 0;
}

/* v1 restore: read manifest, fetch chunks from sidecar blockstore. */
static int restore_v1(FILE *f, uint32_t nchunks, const char *archive_path,
                      FILE *out)
{
	char *blocks_path = make_blocks_path(archive_path);
	if (!blocks_path) return -1;

	struct uc2_blockstore bs;
	if (uc2_blockstore_open(&bs, blocks_path) != 0) {
		free(blocks_path);
		return -1;
	}
	free(blocks_path);

	uint8_t *buf = NULL;
	size_t buf_cap = 0;
	int rc = 0;

	for (uint32_t i = 0; i < nchunks; i++) {
		uint8_t rec[ENTRY_SIZE_V1];
		if (fread(rec, 1, sizeof rec, f) != sizeof rec) { rc = -1; break; }
		uint64_t hash = get_le64(rec);
		uint32_t clen = get_le32(rec + 8);

		if (clen > buf_cap) {
			uint8_t *p = realloc(buf, clen);
			if (!p) { rc = -1; break; }
			buf = p;
			buf_cap = clen;
		}

		int n = uc2_blockstore_read(&bs, hash, buf, clen);
		if (n != (int)clen) { rc = -1; break; }
		if (fwrite(buf, 1, clen, out) != clen) { rc = -1; break; }
	}

	free(buf);
	uc2_blockstore_close(&bs);
	return rc;
}

/* v2 restore: chunk pool is in the same file; manifest entries
 * carry absolute offsets. */
static int restore_v2(FILE *f, uint32_t nchunks, FILE *out)
{
	/* Read full manifest table first, then seek to each chunk. */
	if (nchunks == 0)
		return 0;

	uint8_t *manifest = malloc((size_t)nchunks * ENTRY_SIZE_V2);
	if (!manifest) return -1;
	if (fread(manifest, 1, (size_t)nchunks * ENTRY_SIZE_V2, f)
	    != (size_t)nchunks * ENTRY_SIZE_V2) {
		free(manifest);
		return -1;
	}

	uint8_t *buf = NULL;
	size_t buf_cap = 0;
	int rc = 0;
	for (uint32_t i = 0; i < nchunks; i++) {
		const uint8_t *rec = manifest + (size_t)i * ENTRY_SIZE_V2;
		uint32_t clen = get_le32(rec + 8);
		uint32_t off  = get_le32(rec + 12);

		if (clen > buf_cap) {
			uint8_t *p = realloc(buf, clen);
			if (!p) { rc = -1; break; }
			buf = p;
			buf_cap = clen;
		}

		if (fseek(f, (long)off, SEEK_SET) != 0) { rc = -1; break; }
		if (fread(buf, 1, clen, f) != clen) { rc = -1; break; }
		if (fwrite(buf, 1, clen, out) != clen) { rc = -1; break; }
	}

	free(buf);
	free(manifest);
	return rc;
}

int uc2_ingest_restore(const char *archive_path, FILE *out)
{
	if (!archive_path || !out)
		return -1;

	FILE *f = fopen(archive_path, "rb");
	if (!f)
		return -1;

	uint8_t hdr[HEADER_SIZE];
	if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
		fclose(f);
		return -1;
	}
	if (memcmp(hdr, INGEST_MAGIC, 8) != 0) {
		fclose(f);
		return -1;
	}
	uint32_t nchunks = get_le32(hdr + 12);

	int rc;
	if (hdr[8] == INGEST_VERSION_V2) {
		rc = restore_v2(f, nchunks, out);
	} else if (hdr[8] == INGEST_VERSION_V1) {
		rc = restore_v1(f, nchunks, archive_path, out);
	} else {
		rc = -1;
	}

	fclose(f);
	return rc;
}
