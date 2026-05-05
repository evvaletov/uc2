/* SPDX-License-Identifier: GPL-3.0-or-later */

/* Streaming dedup ingest for UC2.
 *
 * uc2 --ingest <archive> reads a byte stream (typically stdin from
 * tar / rsync / cp -a), splits it via CDC, deduplicates chunks, and
 * writes a self-contained archive file.  uc2 --ingest-restore <archive>
 * reverses this.
 *
 * Two on-disk formats are supported:
 *
 *   v1 (legacy): manifest in <archive>, chunk data in a sidecar
 *   blockstore directory at <archive>.blocks/.  Cross-archive dedup
 *   works through shared blockstore directories.  Read-only now;
 *   writer defaults to v2.
 *
 *   v2 (default): archive is self-contained -- chunks are stored in
 *   an embedded pool inside the archive itself.  No sidecar
 *   directory.  Each manifest entry carries its chunk's absolute
 *   file offset; deduplicated chunks share a single offset.
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
 * Limitations:
 *   - The whole stream is buffered in memory before chunking.  Suits
 *     CDC's locality-of-boundary requirement and is fine for streams
 *     up to a few GB.  True streaming is a future revision.
 *   - The format is not yet a UC2 v3 archive consumable by uc2 -x /
 *     -l; integrating with the master-block layout is a follow-up.
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
