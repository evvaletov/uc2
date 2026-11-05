/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Streaming dedup ingest for UC2.
 *
 * This library module reads legacy v1/v2 ingestion containers and writes v2.
 * The CLI writes ordinary UC2 archives as ingest v3; its restore command
 * recognizes both legacy containers and v3. A tar stream is suitable input.
 * Neither cp nor rsync emits an archive byte stream for this interface.
 *
 * v1 stores a manifest and an external <archive>.blocks/ chunk pool.
 * v2 embeds the pool in the container; repeated chunks share an offset.
 * These containers are distinct from the ordinary UC2 archive format.
 *
 * Manifest layouts (all little-endian):
 *
 *   v1: +0   8B   magic "UC2INGST"
 *       +8   1B   version (1)
 *       +9   1B   cdc_bits
 *      +10   2B   reserved
 *      +12   4B   chunk_count
 *      +16   ...  chunk_count * 12B:  8B hash, 4B length
 *
 *   v2: +0   8B   magic "UC2INGST"
 *       +8   1B   version (2)
 *       +9   1B   cdc_bits
 *      +10   2B   reserved
 *      +12   4B   chunk_count
 *      +16   ...  chunk_count * 16B:  8B hash, 4B length, 4B offset
 *      ...  chunk pool: unique chunks back-to-back at recorded offsets
 *
 * These library entry points implement the self-contained v2 blob (and
 * read the legacy v1 sidecar-blockstore format).  The `uc2 --ingest`
 * CLI command instead emits a real, listable/extractable UC2 archive
 * (one `part-N` entry per chunk occurrence, unique chunks stored once);
 * that v3 writer lives in the CLI on top of the public compress/reader
 * API.  `uc2 --ingest-restore` auto-detects the format by magic.
 *
 * Limitations:
 *   - The whole stream is buffered in memory before chunking.  Suits
 *     CDC's locality-of-boundary requirement and is fine for streams
 *     up to a few GB.  True streaming is a future revision.
 */

#ifndef UC2_INGEST_H
#define UC2_INGEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

struct uc2_ingest_stats {
	uint64_t bytes_in;       /* input stream length */
	int      chunks_total;   /* total chunks in input */
	int      chunks_new;     /* chunks newly stored */
	int      chunks_dedup;   /* chunks already in the block store */
	uint64_t bytes_stored;   /* bytes physically written this call */
	uint64_t bytes_saved;    /* bytes saved by dedup */
};

/* Ingest len bytes of data into archive_path.  The block store lives
 * at <archive_path>.blocks/.  cdc_bits selects the average chunk
 * size (13 = 8 KiB; 0 picks a sensible default). */
int uc2_ingest_write(const char *archive_path,
                     const uint8_t *data, size_t len,
                     int cdc_bits,
                     struct uc2_ingest_stats *stats);

/* Restore the byte stream described by an ingest manifest.  Reads
 * chunks from <archive_path>.blocks/ and writes them in order to out. */
int uc2_ingest_restore(const char *archive_path, FILE *out);

#endif
