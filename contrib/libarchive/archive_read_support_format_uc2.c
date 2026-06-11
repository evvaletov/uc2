/* SPDX-License-Identifier: GPL-3.0-or-later */

/* libarchive read handler for UC2 v3 archives.
 *
 * Status: milestones 1-3.
 *   M1 -- bid() with UC2 magic check.
 *   M2 -- read_header iterates uc2_read_cdir, maps each cdir entry to
 *         libarchive's archive_entry shape (name, size, mode, mtime).
 *   M3 -- read_data uses uc2_extract to decompress an entry, buffers
 *         the result, then yields it via libarchive's pull-style API.
 *
 * Strategy: on the first read_header call we slurp the entire archive
 * into memory through __archive_read_ahead, then drive libuc2 against
 * that buffer.  This is correct for any input but uses memory equal
 * to the archive size; future revisions can swap in a seekable adapter
 * when the underlying filter supports __archive_read_seek.
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <uc2/libuc2.h>

#define ARCHIVE_FORMAT_UC2  0xC0FF0000  /* placeholder format code */

struct uc2_la_state {
	/* Slurped archive */
	uint8_t *data;
	size_t   len;
	int      slurped;     /* 0 = not yet, 1 = done */

	/* libuc2 */
	uc2_handle handle;

	/* Cached cdir entries.  uc2_read_cdir is single-pass; we capture
	 * everything on the first read_header call. */
	struct uc2_entry *entries;
	int n_entries;
	int n_capacity;
	int next_entry;
	char label[12];

	/* Per-entry decompressed buffer for read_data. */
	uint8_t *entry_data;
	size_t   entry_cap;
	size_t   entry_len;
	int      entry_yielded;
};

/* libuc2 IO callbacks bound to the slurped buffer. */
static int
slurp_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct uc2_la_state *st = (struct uc2_la_state *)ctx;
	if ((size_t)pos >= st->len)
		return 0;
	unsigned avail = (unsigned)(st->len - pos);
	if (len > avail)
		len = avail;
	memcpy(buf, st->data + pos, len);
	return (int)len;
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

/* Push-style write callback for uc2_extract.  Buffer everything and
 * let read_data yield it in one slice. */
struct extract_buf {
	uint8_t *data;
	size_t   cap;
	size_t   len;
	int      err;
};

static int
extract_write(void *ctx, const void *p, unsigned len)
{
	struct extract_buf *eb = (struct extract_buf *)ctx;
	if (eb->len + len > eb->cap) {
		size_t ncap = eb->cap ? eb->cap * 2 : 4096;
		while (ncap < eb->len + len) ncap *= 2;
		uint8_t *np = realloc(eb->data, ncap);
		if (!np) { eb->err = 1; return -1; }
		eb->data = np;
		eb->cap = ncap;
	}
	memcpy(eb->data + eb->len, p, len);
	eb->len += len;
	return (int)len;
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

/* Slurp the entire archive into state->data via __archive_read_ahead +
 * __archive_read_consume.  Returns ARCHIVE_OK or ARCHIVE_FATAL. */
static int
slurp_archive(struct archive_read *a, struct uc2_la_state *st)
{
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

		if (st->len + (size_t)avail > st->len /* overflow guard */) {
			size_t need = st->len + (size_t)avail;
			if (need > (size_t)0x80000000u) {
				archive_set_error(&a->archive, ENOMEM,
				    "UC2: archive too large to slurp (>2GB)");
				return (ARCHIVE_FATAL);
			}
			/* grow to power-of-two */
			size_t cap = st->len ? st->len : 4096;
			while (cap < need) cap *= 2;
			uint8_t *np = (uint8_t *)realloc(st->data, cap);
			if (!np) {
				archive_set_error(&a->archive, ENOMEM,
				    "UC2: out of memory slurping archive");
				return (ARCHIVE_FATAL);
			}
			st->data = np;
		}
		memcpy(st->data + st->len, p, (size_t)avail);
		st->len += (size_t)avail;
		__archive_read_consume(a, avail);
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

	uc2_finish_cdir(st->handle, st->label);
	return (ARCHIVE_OK);
}

static int
uc2_la_read_header(struct archive_read *a, struct archive_entry *entry)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;

	a->archive.archive_format = ARCHIVE_FORMAT_UC2;
	a->archive.archive_format_name = "UC2";

	if (!st->slurped) {
		int r = slurp_archive(a, st);
		if (r != ARCHIVE_OK) return r;
		st->slurped = 1;

		r = collect_entries(a, st);
		if (r != ARCHIVE_OK) return r;
	}

	if (st->next_entry >= st->n_entries)
		return (ARCHIVE_EOF);

	struct uc2_entry *e = &st->entries[st->next_entry++];

	/* Reset per-entry buffer state. */
	st->entry_len = 0;
	st->entry_yielded = 0;

	archive_entry_set_pathname(entry, e->name);
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

	/* Decompress the whole entry once. */
	struct extract_buf eb = { .data = st->entry_data, .cap = st->entry_cap };
	int ret = uc2_extract(st->handle, &e->xi, e->size,
	                      extract_write, &eb);
	st->entry_data = eb.data;
	st->entry_cap = eb.cap;
	st->entry_len = eb.len;

	if (ret < 0 || eb.err) {
		archive_set_error(&a->archive, EIO,
		    "UC2: uc2_extract failed: %s",
		    uc2_message(st->handle, ret));
		return (ARCHIVE_FATAL);
	}

	st->entry_yielded = 1;
	*buff = st->entry_data;
	*size = st->entry_len;
	*offset = 0;
	return (ARCHIVE_OK);
}

static int
uc2_la_read_data_skip(struct archive_read *a)
{
	struct uc2_la_state *st = (struct uc2_la_state *)a->format->data;
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
	free(st->data);
	free(st->entries);
	free(st->entry_data);
	free(st);
	a->format->data = NULL;
	return (ARCHIVE_OK);
}
