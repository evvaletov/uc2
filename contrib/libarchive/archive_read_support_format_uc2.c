/* SPDX-License-Identifier: GPL-3.0-or-later */

/* libarchive read handler for UC2 v3 archives.
 *
 * This file uses libarchive's internal API
 * (archive_read_private.h, __archive_read_ahead,
 * __archive_read_register_format), so it must be built against a
 * libarchive source tree, not just installed -devel headers.  Pass
 * -DLIBARCHIVE_SOURCE_DIR=<libarchive checkout> to cmake to enable
 * the build.  Eventual home is libarchive/libarchive/ upstream.
 *
 * Status: milestone 1 -- bid() with magic check.  read_header,
 * read_data, read_data_skip, and cleanup are stubs that report
 * end-of-archive; wiring to libuc2 is milestone 2+.
 */

#include "archive_platform.h"

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_FORMAT_UC2  0xC0FF0000  /* placeholder format code */

static int  uc2_la_bid(struct archive_read *, int);
static int  uc2_la_read_header(struct archive_read *, struct archive_entry *);
static int  uc2_la_read_data(struct archive_read *, const void **,
                             size_t *, int64_t *);
static int  uc2_la_read_data_skip(struct archive_read *);
static int  uc2_la_cleanup(struct archive_read *);

int
archive_read_support_format_uc2(struct archive *_a)
{
	struct archive_read *a = (struct archive_read *)_a;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_uc2");

	r = __archive_read_register_format(a,
	    NULL,
	    "uc2",
	    uc2_la_bid,
	    NULL,
	    uc2_la_read_header,
	    uc2_la_read_data,
	    uc2_la_read_data_skip,
	    NULL,
	    uc2_la_cleanup,
	    NULL,
	    NULL);

	return (r);
}

/* Bid: read the first 4 bytes and look for the UC2 magic
 * (0x55 0x43 0x32 0x1A == 'U' 'C' '2' SUB).  Return 64 on a strong
 * match; libarchive uses the highest bid to pick the format. */
static int
uc2_la_bid(struct archive_read *a, int best_bid)
{
	const unsigned char *p;

	(void)best_bid;

	p = __archive_read_ahead(a, 4, NULL);
	if (p == NULL)
		return (-1);

	if (p[0] == 0x55 && p[1] == 0x43 && p[2] == 0x32 && p[3] == 0x1A)
		return (64);
	return (0);
}

static int
uc2_la_read_header(struct archive_read *a, struct archive_entry *entry)
{
	(void)entry;

	a->archive.archive_format = ARCHIVE_FORMAT_UC2;
	a->archive.archive_format_name = "UC2";

	/* Milestone 2: open libuc2 handle, iterate central directory,
	 * map entries to archive_entry_set_*.  For now report empty so
	 * 'bsdtar -tf <archive>.uc2' returns gracefully without error. */
	return (ARCHIVE_EOF);
}

static int
uc2_la_read_data(struct archive_read *a,
                 const void **buff, size_t *size, int64_t *offset)
{
	(void)a;
	*buff = NULL;
	*size = 0;
	*offset = 0;
	return (ARCHIVE_EOF);
}

static int
uc2_la_read_data_skip(struct archive_read *a)
{
	(void)a;
	return (ARCHIVE_OK);
}

static int
uc2_la_cleanup(struct archive_read *a)
{
	(void)a;
	return (ARCHIVE_OK);
}
