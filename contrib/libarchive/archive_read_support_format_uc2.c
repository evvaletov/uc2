/* SPDX-License-Identifier: GPL-3.0-or-later OR BSD-2-Clause */
/* Copyright (c) 2026 Eremey Valetov. See LICENSE.BSD-2-Clause. */

/* libarchive read handler for UC2 v3 archives.
 *
 * Input and decompressed entries are spooled to anonymous temporary
 * files. Payload RAM is bounded; directory metadata and libuc2's master
 * cache still scale with the archive. This also handles pipes/filters.
 *
 * Built against libarchive's internal API
 * (archive_read_private.h, __archive_read_ahead,
 * __archive_read_register_format), so it must compile inside a
 * libarchive source tree.  Pass -DLIBARCHIVE_SOURCE_DIR=<path> to
 * cmake to build standalone.
 */

#include "archive_platform.h"

#include "archive.h"
#include "archive_entry.h"
#include "archive_private.h"
#include "archive_read_private.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <uc2/libuc2.h>

#ifndef ARCHIVE_FORMAT_UC2
#define ARCHIVE_FORMAT_UC2  0xC0FF0000  /* local plugin ID, not an upstream allocation */
#endif

struct uc2_la_state {
	FILE *input;
	uint64_t len;
	int prepared;

	/* libuc2 */
	uc2_handle handle;

	/* Cached cdir entries.  uc2_read_cdir is single-pass; we capture
	 * everything on the first read_header call. */
	struct uc2_entry *entries;
	char **paths;         /* composed full path per entry */
	int n_entries;
	int n_capacity;
	int next_entry;
	char label[12];

	FILE *entry_file;
	uint8_t entry_data[65536];
	uint64_t entry_pos;
	int entry_ready;
	int entry_yielded;
};

static int
seek_file(FILE *f, uint64_t pos)
{
#ifdef _WIN32
	return _fseeki64(f, (__int64)pos, SEEK_SET);
#else
	return fseeko(f, (off_t)pos, SEEK_SET);
#endif
}

/* libuc2 requires random access, including on non-seekable input. */
static int
slurp_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct uc2_la_state *st = (struct uc2_la_state *)ctx;
	if ((uint64_t)pos >= st->len)
		return 0;
	uint64_t avail = st->len - pos;
	if (len > avail)
		len = (unsigned)avail;
	if (len > INT_MAX) len = INT_MAX;
	if (seek_file(st->input, pos) != 0) return -1;
	size_t n = fread(buf, 1, len, st->input);
	return ferror(st->input) ? -1 : (int)n;
}

static void *
slurp_alloc(void *ctx, unsigned size)
{
	(void)ctx;
	return malloc(size);
}

static void
slurp_free(void *ctx, void *ptr)
{
	(void)ctx;
	free(ptr);
}

static struct uc2_io slurp_io = {
	.read  = slurp_read,
	.alloc = slurp_alloc,
	.free  = slurp_free,
	.warn  = NULL,
};

/* Bridge libuc2's push API to libarchive's pull API through a temp file. */
struct extract_buf {
	FILE *file;
	uint64_t len, limit;
};

static int
extract_write(void *ctx, const void *p, unsigned len)
{
	struct extract_buf *eb = (struct extract_buf *)ctx;
	if (len > eb->limit - eb->len || fwrite(p, 1, len, eb->file) != len)
		return -1;
	eb->len += len;
	return 0;
}

/* DOS date/time -> Unix time_t (UTC; DOS times are local but we treat
 * them as UTC since timezone info is not present in the archive). */
static time_t
dos_to_unix_time(unsigned dos_time)
{
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	tm.tm_sec  = (dos_time & 0x1f) * 2;
	tm.tm_min  = (dos_time >> 5)  & 0x3f;
	tm.tm_hour = (dos_time >> 11) & 0x1f;
	tm.tm_mday = (dos_time >> 16) & 0x1f;
	tm.tm_mon  = ((dos_time >> 21) & 0x0f) - 1;
	tm.tm_year = ((dos_time >> 25) & 0x7f) + 80;
#if defined(_WIN32)
	return _mkgmtime(&tm);
#elif defined(__GLIBC__) || defined(__APPLE__) || defined(__FreeBSD__) || \
      defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
	return timegm(&tm);
#else
	return mktime(&tm);
#endif
}

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
	struct uc2_la_state *state;
	int r;

	archive_check_magic(_a, ARCHIVE_READ_MAGIC,
	    ARCHIVE_STATE_NEW, "archive_read_support_format_uc2");

	state = (struct uc2_la_state *)calloc(1, sizeof(*state));
	if (state == NULL) {
		archive_set_error(_a, ENOMEM,
		    "Out of memory allocating UC2 reader state");
		return (ARCHIVE_FATAL);
	}

	r = __archive_read_register_format(a,
	    state,
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

	if (r != ARCHIVE_OK)
		free(state);
	return (r);
}

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

/* Spool without holding compressed payloads in RAM. */
static int
slurp_archive(struct archive_read *a, struct uc2_la_state *st)
{
	st->input = tmpfile();
	if (!st->input) {
		archive_set_error(&a->archive, errno, "UC2: cannot create input spool");
		return ARCHIVE_FATAL;
	}
	for (;;) {
		ssize_t avail;
		const void *p = __archive_read_ahead(a, 1, &avail);
		if (p == NULL) {
			if (avail < 0) {
				archive_set_error(&a->archive, EIO,
				    "UC2: read error while slurping archive");
				return (ARCHIVE_FATAL);
			}
			break; /* clean EOF */
		}
		if (avail <= 0)
			break;

		if (avail > 65536) avail = 65536;
		if ((uint64_t)avail > UINT32_MAX - st->len) {
			archive_set_error(&a->archive, EFBIG, "UC2: archive exceeds 4 GiB format limit");
			return ARCHIVE_FATAL;
		}
		if (fwrite(p, 1, (size_t)avail, st->input) != (size_t)avail ||
		    __archive_read_consume(a, avail) != avail) {
			archive_set_error(&a->archive, EIO, "UC2: input spool write failed");
			return ARCHIVE_FATAL;
		}
		st->len += (uint64_t)avail;
	}
	if (fflush(st->input) != 0) {
		archive_set_error(&a->archive, EIO, "UC2: input spool flush failed");
		return ARCHIVE_FATAL;
	}
	return (ARCHIVE_OK);
}

/* Walk uc2_read_cdir and cache all entries.  Tagged entries have
 * uc2_get_tag called to fully resolve names. */
static int
collect_entries(struct archive_read *a, struct uc2_la_state *st)
{
	st->handle = uc2_open(&slurp_io, st);
	if (st->handle == NULL) {
		archive_set_error(&a->archive, EINVAL,
		    "UC2: uc2_open failed");
		return (ARCHIVE_FATAL);
	}

	for (;;) {
		if (st->n_entries >= st->n_capacity) {
			if (st->n_capacity > INT_MAX / 2 ||
			    (size_t)(st->n_capacity ? st->n_capacity : 16) >
			    SIZE_MAX / (2 * sizeof *st->entries)) {
				archive_set_error(&a->archive, ENOMEM, "UC2: too many entries");
				return ARCHIVE_FATAL;
			}
			int ncap = st->n_capacity ? st->n_capacity * 2 : 32;
			struct uc2_entry *ne = (struct uc2_entry *)realloc(
			    st->entries, (size_t)ncap * sizeof *ne);
			if (!ne) {
				archive_set_error(&a->archive, ENOMEM,
				    "UC2: out of memory collecting entries");
				return (ARCHIVE_FATAL);
			}
			st->entries = ne;
			st->n_capacity = ncap;
		}

		struct uc2_entry *e = &st->entries[st->n_entries];
		int ret = uc2_read_cdir(st->handle, e);
		if (ret == UC2_End)
			break;
		if (ret < 0) {
			archive_set_error(&a->archive, EINVAL,
			    "UC2: uc2_read_cdir failed: %s",
			    uc2_message(st->handle, ret));
			return (ARCHIVE_FATAL);
		}

		while (ret == UC2_TaggedEntry) {
			char *tag;
			void *data;
			unsigned size;
			ret = uc2_get_tag(st->handle, e, &tag, &data, &size);
			if (ret < 0) {
				archive_set_error(&a->archive, EINVAL,
				    "UC2: uc2_get_tag failed: %s",
				    uc2_message(st->handle, ret));
				return (ARCHIVE_FATAL);
			}
		}

		st->n_entries++;
	}

	if (uc2_finish_cdir(st->handle, st->label) < 0) {
		archive_set_error(&a->archive, EINVAL, "UC2: invalid directory trailer");
		return ARCHIVE_FATAL;
	}
	return (ARCHIVE_OK);
}

/* Append the full path of directory `id` (with a trailing slash) to
 * buf.  Returns the new offset, or -1 on overflow.  UC2 directory ids
 * are archive-global; root is 0.  The depth cap breaks cycles in
 * damaged directories. */
static int
build_dir_path(struct uc2_la_state *st, unsigned id,
               char *buf, size_t cap, int depth)
{
	int i;

	if (id == 0)
		return (0);
	if (depth > 64)
		return (-1); /* cyclic or pathologically deep: corrupt cdir */
	for (i = 0; i < st->n_entries; i++) {
		struct uc2_entry *d = &st->entries[i];
		if (d->is_dir && d->id == id) {
			int off = build_dir_path(st, d->dirid, buf, cap,
			                         depth + 1);
			int n;
			if (off < 0)
				return (-1);
			n = snprintf(buf + off, cap - off, "%s/", d->name);
			if (n < 0 || (size_t)n >= cap - off)
				return (-1);
			return (off + n);
		}
	}
	return (-1); /* a missing parent is a damaged directory */
}

/* Compose a full path for every entry: parent directories joined with
 * '/', directories themselves carrying a trailing slash. */
static int
compose_paths(struct archive_read *a, struct uc2_la_state *st)
{
	int i;

	st->paths = (char **)calloc((size_t)st->n_entries,
	                            sizeof *st->paths);
	if (st->paths == NULL && st->n_entries > 0) {
		archive_set_error(&a->archive, ENOMEM,
		    "UC2: out of memory composing paths");
		return (ARCHIVE_FATAL);
	}

	for (i = 0; i < st->n_entries; i++) {
		struct uc2_entry *e = &st->entries[i];
		char buf[2048];
		int off = build_dir_path(st, e->dirid, buf, sizeof buf, 0);
		int n;
		if (off < 0) {
			archive_set_error(&a->archive, EINVAL,
			    "UC2: directory path too long");
			return (ARCHIVE_FATAL);
		}
		n = snprintf(buf + off, sizeof buf - off, "%s%s",
		             e->name, e->is_dir ? "/" : "");
		if (n < 0 || (size_t)n >= sizeof buf - off) {
			archive_set_error(&a->archive, EINVAL,
			    "UC2: entry path too long");
			return (ARCHIVE_FATAL);
		}
		st->paths[i] = strdup(buf);
		if (st->paths[i] == NULL) {
			archive_set_error(&a->archive, ENOMEM,
			    "UC2: out of memory composing paths");
			return (ARCHIVE_FATAL);
		}
	}
	return (ARCHIVE_OK);
}

static int
uc2_la_read_header(struct archive_read *a, struct archive_entry *entry)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;

	a->archive.archive_format = ARCHIVE_FORMAT_UC2;
	a->archive.archive_format_name = "UC2";

	if (!st->prepared) {
		int r = slurp_archive(a, st);
		if (r != ARCHIVE_OK) return r;
		st->prepared = 1;

		r = collect_entries(a, st);
		if (r != ARCHIVE_OK) return r;

		r = compose_paths(a, st);
		if (r != ARCHIVE_OK) return r;
	}

	if (st->next_entry >= st->n_entries)
		return (ARCHIVE_EOF);

	struct uc2_entry *e = &st->entries[st->next_entry++];

	if (st->entry_file) fclose(st->entry_file);
	st->entry_file = NULL;
	st->entry_pos = 0;
	st->entry_ready = 0;
	st->entry_yielded = 0;

	archive_entry_set_pathname(entry, st->paths[st->next_entry - 1]);
	archive_entry_set_size(entry, (la_int64_t)e->size);
	archive_entry_set_mtime(entry, dos_to_unix_time(e->dos_time), 0);

	if (e->is_dir) {
		archive_entry_set_filetype(entry, AE_IFDIR);
		archive_entry_set_perm(entry, 0755);
	} else {
		archive_entry_set_filetype(entry, AE_IFREG);
		mode_t mode = 0644;
		if (e->attr & UC2_Attr_R) mode &= ~0222;
		archive_entry_set_perm(entry, mode);
	}

	return (ARCHIVE_OK);
}

static int
uc2_la_read_data(struct archive_read *a,
                 const void **buff, size_t *size, int64_t *offset)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;

	if (st->next_entry == 0 || st->entry_yielded) {
		*buff = NULL;
		*size = 0;
		*offset = 0;
		return (ARCHIVE_EOF);
	}

	struct uc2_entry *e = &st->entries[st->next_entry - 1];
	if (e->is_dir || e->size == 0) {
		st->entry_yielded = 1;
		*buff = NULL;
		*size = 0;
		*offset = 0;
		return (ARCHIVE_EOF);
	}

	if (!st->entry_ready) {
		st->entry_file = tmpfile();
		if (!st->entry_file) {
			archive_set_error(&a->archive, errno, "UC2: cannot create entry spool");
			return ARCHIVE_FATAL;
		}
		struct extract_buf eb = { .file = st->entry_file, .limit = e->size };
		int ret = uc2_extract(st->handle, &e->xi, e->size, extract_write, &eb);
		if (ret < 0 || eb.len != e->size || fflush(st->entry_file) != 0 ||
		    seek_file(st->entry_file, 0) != 0) {
			archive_set_error(&a->archive, EIO, "UC2: extraction or entry spool failed (%d)", ret);
			return ARCHIVE_FATAL;
		}
		st->entry_ready = 1;
	}
	size_t n = fread(st->entry_data, 1, sizeof st->entry_data, st->entry_file);
	if (ferror(st->entry_file)) {
		archive_set_error(&a->archive, EIO, "UC2: entry spool read failed");
		return ARCHIVE_FATAL;
	}
	*buff = st->entry_data;
	*size = n;
	*offset = (int64_t)st->entry_pos;
	st->entry_pos += n;
	if (!n) st->entry_yielded = 1;
	return n ? ARCHIVE_OK : ARCHIVE_EOF;
}

static int
uc2_la_read_data_skip(struct archive_read *a)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;
	if (st->entry_file) fclose(st->entry_file);
	st->entry_file = NULL;
	st->entry_yielded = 1;
	return (ARCHIVE_OK);
}

static int
uc2_la_cleanup(struct archive_read *a)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;
	if (st == NULL)
		return (ARCHIVE_OK);
	if (st->handle)
		uc2_close(st->handle);
	if (st->paths) {
		int i;
		for (i = 0; i < st->n_entries; i++)
			free(st->paths[i]);
		free(st->paths);
	}
	if (st->input) fclose(st->input);
	if (st->entry_file) fclose(st->entry_file);
	free(st->entries);
	free(st);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}
