/* SPDX-License-Identifier: GPL-3.0-or-later */

/* libarchive read handler for UC2 v3 archives.
 *
 * Skeleton: bid, read_header, read_data, read_data_skip, cleanup.
 * The implementation calls into libuc2 for format parsing and
 * decompression.  See contrib/libarchive/README.md for the architecture
 * and integration plan.
 *
 * This file is intended to be merged into libarchive's libarchive/
 * directory.  Until that merge lands, it builds out-of-tree against
 * an installed libarchive-devel and exports
 * archive_read_support_format_uc2() as a single entry point. */

#include <archive.h>
#include <archive_entry.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <uc2/libuc2.h>

/* Internal state kept across callbacks for one open archive. */
struct uc2_format_state {
	uc2_handle handle;
	int eof;
	int entries_read;
	/* TODO: ring buffer for read_data push-to-pull conversion;
	 *       master-block dependency tracking;
	 *       remembered entry list (uc2_read_cdir is one-pass). */
};

/* libarchive registers an int magic; pick something large and unused.
 * The libarchive convention is the file's magic number bytes interpreted
 * as a 32-bit integer; for UC2 that is 0x1A324355 (ASCII 'UC2', 0x1A). */
#define UC2_FORMAT_CODE  0x55433201u  /* little-endian 'UC2\x1A' */

static int uc2_la_bid(struct archive_read *_a, int best_bid);
static int uc2_la_read_header(struct archive_read *_a,
                              struct archive_entry *entry);
static int uc2_la_read_data(struct archive_read *_a,
                            const void **buff, size_t *size, int64_t *offset);
static int uc2_la_read_data_skip(struct archive_read *_a);
static int uc2_la_cleanup(struct archive_read *_a);

/* Public entry point.  Mirrors archive_read_support_format_zip. */
int
archive_read_support_format_uc2(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	struct uc2_format_state *state;

	state = calloc(1, sizeof(*state));
	if (state == NULL) {
		archive_set_error(_a, ENOMEM,
		                  "Out of memory allocating UC2 state");
		return ARCHIVE_FATAL;
	}

	/* TODO: __archive_read_register_format expects internal libarchive
	 * pointers.  Until this file is merged into libarchive's source
	 * tree, replace this call with the equivalent registration via
	 * libarchive's public API where possible, or document the patch
	 * that needs to be applied to libarchive's archive_read.c.
	 *
	 * Reference signature (libarchive internal):
	 *   __archive_read_register_format(a, state, name, bid, options,
	 *                                  read_header, read_data,
	 *                                  read_data_skip, NULL, cleanup,
	 *                                  NULL, NULL);
	 */
	(void)a;
	(void)uc2_la_bid;
	(void)uc2_la_read_header;
	(void)uc2_la_read_data;
	(void)uc2_la_read_data_skip;
	(void)uc2_la_cleanup;

	free(state);
	archive_set_error(_a, -1,
	    "UC2 read-format plugin: skeleton only, "
	    "see contrib/libarchive/README.md");
	return ARCHIVE_WARN;
}

static int
uc2_la_bid(struct archive_read *a, int best_bid)
{
	(void)best_bid;

	const void *h;
	/* TODO: replace with __archive_read_ahead(a, 4, NULL).
	 * Read the first four bytes; UC2 magic is 0x55 0x43 0x32 0x1A
	 * ('U' 'C' '2' 0x1A), little-endian uint32 = 0x1A324355. */
	(void)a;
	h = NULL;
	if (h == NULL)
		return 0;

	const uint8_t *bytes = h;
	if (bytes[0] == 0x55 && bytes[1] == 0x43 &&
	    bytes[2] == 0x32 && bytes[3] == 0x1A)
		return 64;
	return 0;
}

static int
uc2_la_read_header(struct archive_read *a, struct archive_entry *entry)
{
	/* TODO:
	 *   - On first call, instantiate uc2_io callbacks bound to libarchive's
	 *     filter stack (read=archive_read_ahead+seek, alloc=malloc,
	 *     free=free, warn=archive_set_error).
	 *   - Call uc2_open(); cache the handle in state.
	 *   - Iterate uc2_read_cdir(); skip directory entries that should
	 *     be reported as ARCHIVE_ENTRY_FILETYPE_DIR; map file entries
	 *     to archive_entry_set_pathname / size / mtime / mode.
	 *   - Walk through tagged entries with uc2_get_tag for long names
	 *     and extended attributes.
	 *   - Return ARCHIVE_OK / ARCHIVE_EOF.
	 */
	(void)a;
	(void)entry;
	return ARCHIVE_EOF;
}

static int
uc2_la_read_data(struct archive_read *a,
                 const void **buff, size_t *size, int64_t *offset)
{
	/* TODO:
	 *   - Convert libuc2's push-style uc2_extract callback into the
	 *     pull-style API libarchive expects.  Simplest: buffer the
	 *     whole entry once and yield slices on subsequent calls.
	 *     A streaming version uses a coroutine or a small ring buffer.
	 *   - Honour ARCHIVE_OK -> bytes returned, ARCHIVE_EOF -> entry done,
	 *     ARCHIVE_FATAL on libuc2 errors (translate via uc2_message()).
	 */
	(void)a;
	*buff = NULL;
	*size = 0;
	*offset = 0;
	return ARCHIVE_EOF;
}

static int
uc2_la_read_data_skip(struct archive_read *a)
{
	/* TODO:
	 *   - libuc2 cannot skip decompression cleanly because of
	 *     master-block dependencies.  Decompress the entry but
	 *     discard the bytes.  Equivalent to looping read_data
	 *     until ARCHIVE_EOF and ignoring the buffers.
	 */
	(void)a;
	return ARCHIVE_OK;
}

static int
uc2_la_cleanup(struct archive_read *a)
{
	/* TODO:
	 *   - uc2_close(state->handle);
	 *   - free state.
	 */
	(void)a;
	return ARCHIVE_OK;
}
