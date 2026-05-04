/* SPDX-License-Identifier: GPL-3.0-or-later */

#include "uc2/uc2_ingest.h"
#include "uc2/uc2_blockstore.h"
#include "uc2/uc2_merkle.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char INGEST_MAGIC[8] = { 'U','C','2','I','N','G','S','T' };
#define INGEST_VERSION  1
#define DEFAULT_CDC_BITS 13

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

int uc2_ingest_write(const char *archive_path,
                     const uint8_t *data, size_t len,
                     int cdc_bits,
                     struct uc2_ingest_stats *stats)
{
	if (!archive_path)
		return -1;
	if (cdc_bits <= 0)
		cdc_bits = DEFAULT_CDC_BITS;

	char *blocks_path = make_blocks_path(archive_path);
	if (!blocks_path)
		return -1;

	struct uc2_blockstore bs;
	if (uc2_blockstore_open(&bs, blocks_path) != 0) {
		free(blocks_path);
		return -1;
	}
	free(blocks_path);

	struct uc2_merkle tree;
	uc2_merkle_build(&tree, data, len, cdc_bits);

	int new_chunks = 0;
	if (tree.nchunks > 0)
		new_chunks = uc2_blockstore_ingest(&bs, &tree, data, len);

	FILE *f = fopen(archive_path, "wb");
	if (!f) {
		uc2_merkle_free(&tree);
		uc2_blockstore_close(&bs);
		return -1;
	}

	uint8_t hdr[16];
	memcpy(hdr, INGEST_MAGIC, 8);
	hdr[8]  = INGEST_VERSION;
	hdr[9]  = (uint8_t)cdc_bits;
	hdr[10] = 0;
	hdr[11] = 0;
	put_le32(hdr + 12, (uint32_t)tree.nchunks);
	if (fwrite(hdr, 1, sizeof hdr, f) != sizeof hdr) {
		fclose(f);
		uc2_merkle_free(&tree);
		uc2_blockstore_close(&bs);
		return -1;
	}

	for (int i = 0; i < tree.nchunks; i++) {
		uint8_t rec[12];
		put_le64(rec, tree.chunks[i].hash);
		put_le32(rec + 8, tree.chunks[i].length);
		if (fwrite(rec, 1, sizeof rec, f) != sizeof rec) {
			fclose(f);
			uc2_merkle_free(&tree);
			uc2_blockstore_close(&bs);
			return -1;
		}
	}

	if (fclose(f) != 0) {
		uc2_merkle_free(&tree);
		uc2_blockstore_close(&bs);
		return -1;
	}

	if (stats) {
		stats->bytes_in     = (uint64_t)len;
		stats->chunks_total = tree.nchunks;
		stats->chunks_new   = new_chunks;
		stats->chunks_dedup = tree.nchunks - new_chunks;
		stats->bytes_stored = (uint64_t)bs.total_bytes;
		stats->bytes_saved  = (uint64_t)bs.saved_bytes;
	}

	uc2_merkle_free(&tree);
	uc2_blockstore_close(&bs);
	return 0;
}

int uc2_ingest_restore(const char *archive_path, FILE *out)
{
	if (!archive_path || !out)
		return -1;

	FILE *f = fopen(archive_path, "rb");
	if (!f)
		return -1;

	uint8_t hdr[16];
	if (fread(hdr, 1, sizeof hdr, f) != sizeof hdr) {
		fclose(f);
		return -1;
	}
	if (memcmp(hdr, INGEST_MAGIC, 8) != 0 || hdr[8] != INGEST_VERSION) {
		fclose(f);
		return -1;
	}
	uint32_t nchunks = get_le32(hdr + 12);

	char *blocks_path = make_blocks_path(archive_path);
	if (!blocks_path) {
		fclose(f);
		return -1;
	}

	struct uc2_blockstore bs;
	if (uc2_blockstore_open(&bs, blocks_path) != 0) {
		free(blocks_path);
		fclose(f);
		return -1;
	}
	free(blocks_path);

	uint8_t *buf = NULL;
	size_t buf_cap = 0;
	int rc = 0;

	for (uint32_t i = 0; i < nchunks; i++) {
		uint8_t rec[12];
		if (fread(rec, 1, sizeof rec, f) != sizeof rec) {
			rc = -1;
			break;
		}
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
	fclose(f);
	return rc;
}
