/* SPDX-License-Identifier: GPL-3.0-or-later */

/* UltraCompressor II extraction tool.
   Copyright © Jan Bobrowski 2020, 2021
   torinak.com/~jb/unuc2/

   This program is free software: you can redistribute it and modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/

#if !defined(_WIN32) && !defined(__DJGPP__)
#define _FILE_OFFSET_BITS 64
#endif

#include <limits.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <utime.h>
#ifdef __DJGPP__
#include <err.h>
#include <fnmatch.h>
void setprogname(const char *argv0);
#else
#include <fnmatch.h>
#include <getopt.h>
#include <err.h>
#endif
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#if defined(_WIN32) || defined(__DJGPP__)
#include <io.h>
#endif
#ifdef _WIN32
#include <windows.h>
#include <process.h>
FILE *compat__fopen(const char *path, const char *mode);
#define fopen compat__fopen
#define getpid _getpid
#define open _open
#define close _close
#define fdopen _fdopen
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

#include <uc2/libuc2.h>
#include <uc2/uc2_writer.h>
#include <uc2/uc2_path.h>
#include <uc2/uc2_cdc.h>
#include <uc2/uc2_ingest.h>
#include <uc2/uc2_merkle.h>
#include <uc2/uc2_lz4.h>
#include <uc2/uc2_ots.h>
#include <uc2/uc2_sha256.h>
#include <uc2/uc2_version.h>

#include "list.h"
#include "ingest_tar.h"
#define endof(T) (T + sizeof T/sizeof*T)
#define STR(S) STR_(S)
#define STR_(S) #S

enum ots_mode {
	OTS_MODE_NONE = 0,
	OTS_MODE_ATTACH,
	OTS_MODE_EXTRACT,
	OTS_MODE_INFO
};

enum ingest_mode {
	INGEST_MODE_NONE = 0,
	INGEST_MODE_WRITE,
	INGEST_MODE_RESTORE,
	INGEST_MODE_TAR
};

struct options {
	bool list:1;
	bool all:1;
	bool test:1;
	bool pipe:1;
	bool create:1;
	bool overwrite:1;
	bool no_dir_meta:1;
	bool no_file_meta:1;
	bool help:1;
	bool quiet:1;
	bool benchmark:1;
	bool chunked:1;
	int ots_mode;
	int ingest_mode;
	char sep;
	int level;
	char *archive;
	char *dest;
	char *ots_path;
} opt = {.sep = ' ', .level = 4};

static const char short_options[] = "xlatfd:C:cpDTh?wL:qB";

static const char *level_name(int level)
{
	switch (level) {
	case 2:  return "Fast";
	case 3:  return "Normal";
	case 5:  return "Ultra";
	case 6:  return "rANS Fast";
	case 7:  return "rANS Normal";
	case 8:  return "rANS Tight";
	case 9:  return "rANS Ultra";
	default: return "Tight";
	}
}

/* Quiet mode suppresses progress output for scripts. */
static void uc2_say(FILE *f, const char *fmt, ...)
{
	if (opt.quiet) return;
	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);
}

static void flush_stdout(void)
{
	if (fflush(stdout) != 0 || ferror(stdout))
		err(EXIT_FAILURE, "stdout");
}

static int seek_file(FILE *f, uint64_t offset, int whence)
{
	if (offset > INT64_MAX) { errno = EFBIG; return -1; }
#ifdef _WIN32
	return _fseeki64(f, (__int64)offset, whence);
#elif defined(__DJGPP__)
	if (offset > LONG_MAX) { errno = EFBIG; return -1; }
	return fseek(f, (long)offset, whence);
#else
	return fseeko(f, (off_t)offset, whence);
#endif
}

static int64_t tell_file(FILE *f)
{
#ifdef _WIN32
	return _ftelli64(f);
#elif defined(__DJGPP__)
	return ftell(f);
#else
	return ftello(f);
#endif
}

/* Archive positions are 32-bit in the UC2 container. */
static unsigned tell32(FILE *f)
{
	int64_t pos = tell_file(f);
	if (pos < 0)
		err(EXIT_FAILURE, "tell archive");
	if ((uint64_t)pos > UINT32_MAX)
		errx(EXIT_FAILURE, "archive exceeds the 4 GiB UC2 format limit");
	return (unsigned)pos;
}

static int my_read(void *ctx, unsigned pos, void *ptr, unsigned len)
{
	FILE *f = ctx;
	if (seek_file(f, pos, SEEK_SET) != 0)
		return -1;
	size_t got = fread(ptr, 1, len, f);
	return ferror(f) ? -1 : (int)got;
}

static void *my_alloc(void *ctx, unsigned size)
{
	return malloc(size);
}

static void my_free(void *ctx, void *ptr)
{
	free(ptr);
}

static void my_warn(void *ctx, char *f, ...)
{
	fprintf(stderr, "%s: ", opt.archive);
	va_list ap;
	va_start(ap, f);
	vfprintf(stderr, f, ap);
	va_end(ap);
}

static int had_error;

static void uc2err(uc2_handle uc2, int err, char *f, ...)
{
	had_error = 1;
	fprintf(stderr, "%s", opt.archive);
	if (f) {
		fprintf(stderr, " (");
		va_list ap;
		va_start(ap, f);
		vfprintf(stderr, f, ap);
		va_end(ap);
		fprintf(stderr, ")");
	}
	fprintf(stderr, ": %s\n", uc2_message(uc2, err));
}

static struct uc2_io io = {
	.read = my_read,
	.alloc = my_alloc,
	.free = my_free,
	.warn = my_warn
};

struct list files;
struct list dirs;

struct node {
	struct list by_type;
	struct list on_dir;
	struct list on_sel;
	struct list children; // head
	struct node *parent;
	struct node *name_group;
	struct node *first_version; /* owned by the name group's first node */
	unsigned file_count;
	unsigned version;
	bool visit:1;
	bool marked:1;
	bool expanded:1;
	struct uc2_entry entry;
};

struct node root = {
	.entry = {.is_dir = 1},
	.children = {.prev = &root.children, .next = &root.children}
};

/* These indexes exist only while reading the directory. Lists still own the
 * traversal order, including old versions and duplicate directory IDs. */
struct node_index {
	struct node **slots;
	size_t capacity, count;
};

static struct node_index directory_index, name_index;

static uint64_t node_hash(const struct node *ne, bool by_id)
{
	uint64_t hash = by_id ? ne->entry.id : ne->parent->entry.id;
	if (!by_id)
		for (unsigned i = 0; i < ne->entry.name_len; i++)
			hash = (hash ^ (unsigned char)ne->entry.name[i]) * UINT64_C(1099511628211);
	/* Mix directory IDs as well as names before masking to a power of two. */
	hash ^= hash >> 30;
	hash *= UINT64_C(0xbf58476d1ce4e5b9);
	hash ^= hash >> 27;
	hash *= UINT64_C(0x94d049bb133111eb);
	return hash ^ (hash >> 31);
}

static struct node **node_slot(struct node_index *index, const struct node *key, bool by_id)
{
	if (index->count >= index->capacity / 2) {
		size_t capacity = index->capacity ? index->capacity * 2 : 256;
		if (capacity <= index->capacity || capacity > SIZE_MAX / sizeof *index->slots)
			errx(EXIT_FAILURE, "Too many archive entries");
		struct node **slots = calloc(capacity, sizeof *slots);
		if (!slots) err(EXIT_FAILURE, 0);
		for (size_t i = 0; i < index->capacity; i++) {
			struct node *ne = index->slots[i];
			if (!ne) continue;
			size_t at = node_hash(ne, by_id) & (capacity - 1);
			while (slots[at]) at = (at + 1) & (capacity - 1);
			slots[at] = ne;
		}
		free(index->slots);
		index->slots = slots;
		index->capacity = capacity;
	}
	size_t at = node_hash(key, by_id) & (index->capacity - 1);
	while (index->slots[at]) {
		struct node *ne = index->slots[at];
		if (by_id ? ne->entry.id == key->entry.id :
		    ne->parent == key->parent && ne->entry.name_len == key->entry.name_len &&
		    memcmp(ne->entry.name, key->entry.name, key->entry.name_len) == 0)
			break;
		at = (at + 1) & (index->capacity - 1);
	}
	return &index->slots[at];
}

static void new_entry(struct node *ne)
{
	struct uc2_entry *e = &ne->entry;
	struct node *dir = &root;
	if (e->dirid) {
		struct node key = {.entry = {.id = e->dirid}};
		dir = *node_slot(&directory_index, &key, true);
		if (!dir)
			errx(EXIT_FAILURE, "%s: missing parent directory for %s",
			     opt.archive, e->name);
	}
	ne->parent = dir;
	struct node **slot = node_slot(&name_index, ne, false);
	if (!*slot) {
		*slot = ne;
		name_index.count++;
		ne->first_version = ne;
		ne->file_count = 0;
	}
	struct node *group = ne->name_group = *slot;
	struct list *at = 0;
	if (!e->is_dir) {
		if (group->file_count == UINT_MAX)
			errx(EXIT_FAILURE, "Too many versions of %s", e->name);
		group->file_count++;
		if (group->first_version != ne) at = &group->first_version->on_dir;
		group->first_version = ne;
	} else {
		slot = node_slot(&directory_index, ne, true);
		if (!*slot) {
			*slot = ne; /* The first directory with this ID wins, as before. */
			directory_index.count++;
		}
	}
	list_append(at ? at : &dir->children, &ne->on_dir);
	list_append(e->is_dir ? &dirs : &files, &ne->by_type);
	list_init(&ne->children);
	list_init(&ne->on_sel);
	/* Resolve versions once after reading: every later file of this name
	 * increments the version, including for a same-named directory. */
	ne->version = group->file_count;
	ne->visit = false;
	ne->marked = false;
	ne->expanded = false;
}

static void finish_entries(void)
{
	struct list *heads[] = {&files, &dirs};
	for (unsigned i = 0; i < sizeof heads / sizeof *heads; i++)
		for (struct list *l = heads[i]->next; l != heads[i]; l = l->next) {
			struct node *ne = list_item(l, struct node, by_type);
			ne->version = ne->name_group->file_count - ne->version;
		}
	free(directory_index.slots);
	free(name_index.slots);
}

static void print_dir_path(struct node *ne)
{
	ne = ne->parent;
	if (!ne) {
		printf("?/");
		return;
	}
	if (ne->entry.dirid)
		print_dir_path(ne);
	printf("%s/", ne->entry.name);
}

static void print_time(unsigned t)
{
	int w = 0;
	if (t) {
		w += printf("%04u-%02u-%02u %02u:%02u", 1980 + (t>>25), t>>21&15, t>>16&31, t>>11&31, t>>5&63);
		int s = t<<1&62;
		if (s < 60)
			w += printf(":%02u", s);
	}
	if (opt.sep == ' ')
		printf("%*s", 19 - w, "");
}

static void mark(struct node *node, bool visit)
{
	if (node->marked && (!visit || node->expanded))
		return;
	node->marked = true;
	if (visit) {
		node->expanded = true;
		node->visit = true;
		for (struct list *l = node->children.next; l != &node->children; l = l->next) {
			struct node *ne = list_item(l, struct node, on_dir);
			if (opt.all || ne->version == 0)
				mark(ne, true);
		}
	}
	while ((node = node->parent) && !node->visit)
		node->visit = true;
}

static void match_pattern(char *p)
{
	enum {
		IntermediateDirs,
		FilesAndSpecificDirs,
		Dirs
	};
	struct list selected;
	list_init(&selected);
	list_add(&selected, &root.on_sel);
	int version = opt.all ? -1 : 0;
	for (;;) {
		char *q = strchr(p, '/');
		int mode;
		if (!q) {
			mode = FilesAndSpecificDirs;
			q = strchr(p, 0);
			if (q - p > 2) {
				if (memcmp(q - 2, ";*", 2) == 0) {
					version = -1;
					q[-2] = 0;
				} else if(isdigit(q[-1])) {
					do q--; while (q - p > 2 && isdigit(q[-1]));
					if (q[-1] == ';') {
						q[-1] = 0;
						version = atoi(q);
					}
				}
			}
		} else {
			mode = IntermediateDirs;
			*q = 0;
			if (!q[1])
				mode = Dirs;
		}
		struct list sentinel;
		list_append(&selected, &sentinel);
		while (selected.next != &sentinel) {
			struct node *dir = list_item(selected.next, struct node, on_sel);
			list_del_init(&dir->on_sel);
			for (struct list *l = dir->children.next; l != &dir->children; l = l->next) {
				struct node *ne = list_item(l, struct node, on_dir);
				if (!ne->entry.is_dir) {
					if (mode == FilesAndSpecificDirs
					 && (version < 0 || ne->version == version)
					 && fnmatch(p, ne->entry.name, 0) == 0)
						mark(ne, false);
					continue;
				}
				if (mode == IntermediateDirs) {
					list_del_init(&ne->on_sel);
					if (fnmatch(p, ne->entry.name, 0) == 0)
						list_append(&selected, &ne->on_sel);
					continue;
				}
				if (strcmp(ne->entry.name, p) == 0
				 || (fnmatch(p, ne->entry.name, 0) == 0))
					mark(ne, mode == Dirs);
			}
		}
		list_del(&sentinel);
		if (mode != IntermediateDirs)
			break;
		p = q + 1;
	}
}

enum cause {
	VisitFile,
	EnterDir,
	LeaveDir
};

static int visit_selected(struct node *dir, bool (*cb)(struct node *, void *ctx, enum cause), void *ctx)
{
	int r = 1;
	for (struct list *l=dir->children.next; l!=&dir->children; l=l->next) {
		struct node *ne = list_item(l, struct node, on_dir);
		if (ne->entry.is_dir)
			continue;
		if (!ne->visit && !ne->marked)
			continue;
		r = cb(ne, ctx, VisitFile);
		if (r <= 0)
			break;
	}
	if (!r)
		return r;
	for (struct list *l=dir->children.next; l!=&dir->children; l=l->next) {
		struct node *ne = list_item(l, struct node, on_dir);
		if (!ne->entry.is_dir)
			continue;
		if (!ne->visit && !ne->marked)
			continue;
		r = cb(ne, ctx, EnterDir);
		if (r <= 0)
			break;
		r = visit_selected(ne, cb, ctx);
		if (r <= 0)
			break;
		r = cb(ne, ctx, LeaveDir);
		if (r <= 0)
			break;
	}
	return r;
}

static void print_entry(struct node *ne, int size_w)
{
	struct uc2_entry *e = &ne->entry;
	char t[] = "adlshr";
	unsigned a = e->attr;
	for (char *p = t; *p; p++, a<<=1)
		if (!(a & 0x20))
			*p = '-';
	printf("%s", t);
	putchar(opt.sep);
	print_time(e->dos_time);
	putchar(opt.sep);
	if (opt.sep == ' ') {
		if (e->is_dir) printf("%*s", size_w, "");
		else printf("%*u", size_w, e->size);
	} else
		if (!e->is_dir) printf("%u", e->size);
	putchar(opt.sep);
	if (e->dirid)
		print_dir_path(ne);
	printf("%s", e->name);
	if (e->is_dir && opt.sep == ' ')
		putchar('/');
	if (ne->version) {
		putchar(opt.sep == ' ' ? ';' : opt.sep);
		printf("%u", ne->version);
	}
	putchar('\n');
}

static bool max_size_cb(struct node *ne, void *ctx, enum cause cause)
{
	if (cause == VisitFile) {
		unsigned *max = ctx;
		if (*max < ne->entry.size)
			*max = ne->entry.size;
	}
	return true;
}

static bool print_entry_cb(struct node *ne, void *ctx, enum cause cause)
{
	if (ne->marked && cause != LeaveDir) {
		int size_w = *(int*)ctx;
		print_entry(ne, size_w);
	}
	return true;
}

static void set_attrs(char *path, struct node *ne)
{
	unsigned dt = ne->entry.dos_time;
	time_t t = 0;
	if (dt) {
		struct tm tm = {
			.tm_year = 80 + (dt>>25),
			.tm_mon = (dt>>21 & 15) - 1,
			.tm_mday = dt>>16 & 31,
			.tm_hour = dt>>11 & 31,
			.tm_min = dt>>5 & 63,
			.tm_sec = dt<<1 & 62,
			.tm_isdst = -1
		};
		t = mktime(&tm);
	}
	if (t != (time_t)-1) {
		struct utimbuf ut = {.actime = t, .modtime = t};
		(void)utime(path, &ut);
	}
	if (ne->entry.attr & UC2_Attr_R)
		(void)chmod(path, ne->entry.is_dir ? 0555 : 0444);
}

static void binary_stream(FILE *file)
{
#ifdef _WIN32
	if (_setmode(_fileno(file), _O_BINARY) == -1)
		err(EXIT_FAILURE, "binary stream mode");
#elif defined(__DJGPP__)
	if (setmode(fileno(file), O_BINARY) == -1)
		err(EXIT_FAILURE, "binary stream mode");
#else
	(void)file;
#endif
}

static int write_file(void *file, const void *ptr, unsigned len)
{
	if (file)
		if (fwrite(ptr, 1, len, file) < len)
			return -1;
	return 0;
}

struct path {
	uc2_handle uc2;
	char *ptr;
	char buffer[PATH_MAX];
};

static bool pipe_cb(struct node *ne, void *ctx, enum cause cause)
{
	if (cause == VisitFile) {
		uc2_handle uc2 = ctx;
		struct uc2_entry *e = &ne->entry;
		if (opt.test)
			uc2_say(stdout, "Testing %s %u bytes\n", e->name, e->size);
		int ret = uc2_extract(uc2, &e->xi, e->size, write_file, opt.test ? 0 : stdout);
		if (ret < 0)
			uc2err(uc2, ret, "%s", e->name);
	}
	return true;
}

/* Check each archive-controlled component before using it.  The caller owns
   the destination tree; this rejects existing links and special files, but
   does not protect against another process replacing parent directories. */
static bool output_exists(const char *path, bool directory)
{
#ifdef _WIN32
	wchar_t wide[32768];
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
	                        wide, sizeof wide / sizeof *wide))
		errx(EXIT_FAILURE, "%s: invalid UTF-8 path", path);
	DWORD attributes = GetFileAttributesW(wide);
	if (attributes == INVALID_FILE_ATTRIBUTES) {
		DWORD error = GetLastError();
		if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			return false;
		errx(EXIT_FAILURE, "%s: cannot inspect output (Windows error %lu)",
		     path, (unsigned long)error);
	}
	if ((attributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DEVICE))
	    || !!(attributes & FILE_ATTRIBUTE_DIRECTORY) != directory)
		errx(EXIT_FAILURE, "%s: unsafe output type", path);
#else
	struct stat st;
#ifdef __DJGPP__
	int ret = stat(path, &st);
#else
	int ret = lstat(path, &st);
#endif
	if (ret < 0) {
		if (errno == ENOENT) return false;
		errx(EXIT_FAILURE, "%s: cannot inspect output: %s", path, strerror(errno));
	}
	if (directory ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode))
		errx(EXIT_FAILURE, "%s: unsafe output type", path);
#endif
	return true;
}

static FILE *create_output(const char *path)
{
	/* Exclusive creation also refuses a leaf link introduced after the check. */
#ifdef _WIN32
	wchar_t wide[32768];
	if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1,
	                        wide, sizeof wide / sizeof *wide))
		errx(EXIT_FAILURE, "%s: invalid UTF-8 path", path);
	int fd = _wopen(wide, _O_CREAT | _O_EXCL | _O_WRONLY | _O_BINARY, 0666);
#else
	int fd = open(path, O_CREAT | O_EXCL | O_WRONLY | O_BINARY, 0666);
#endif
	if (fd < 0) err(EXIT_FAILURE, "%s", path);
	FILE *file = fdopen(fd, "wb");
	if (!file) { close(fd); err(EXIT_FAILURE, "%s", path); }
	return file;
}

#if defined(_WIN32) || defined(__DJGPP__)
static bool unsafe_dos_name(const char *name, unsigned length)
{
	if (!length || name[length - 1] == '.' || name[length - 1] == ' ')
		return true;
	for (unsigned i = 0; i < length; i++) {
		unsigned char c = (unsigned char)name[i];
		if (c < 32 || c == 127 || strchr("<>:\"|?*", c))
			return true;
	}

	/* DOS device names remain reserved with an extension. Win32 also
	   ignores spaces immediately before that extension. */
	unsigned n = 0;
	while (n < length && name[n] != '.') n++;
	while (n && name[n - 1] == ' ') n--;
	if (n > 7) return false;
	char base[8];
	for (unsigned i = 0; i < n; i++) {
		unsigned char c = (unsigned char)name[i];
		base[i] = c >= 'a' && c <= 'z' ? c - ('a' - 'A') : c;
	}
	base[n] = 0;
	if (!strcmp(base, "CON") || !strcmp(base, "PRN")
	    || !strcmp(base, "AUX") || !strcmp(base, "NUL")
	    || !strcmp(base, "CONIN$") || !strcmp(base, "CONOUT$"))
		return true;
	if (n >= 4 && (!memcmp(base, "COM", 3) || !memcmp(base, "LPT", 3))) {
		if (n == 4 && base[3] >= '1' && base[3] <= '9')
			return true;
		/* UTF-8 encodings of the reserved superscript digits 1, 2 and 3. */
		if (n == 5 && (unsigned char)base[3] == 0xc2
		    && ((unsigned char)base[4] == 0xb9
		        || (unsigned char)base[4] == 0xb2
		        || (unsigned char)base[4] == 0xb3))
			return true;
	}
	return false;
}
#endif

static bool extract_cb(struct node *ne, void *ctx, enum cause cause)
{
	struct path *path = ctx;
	struct uc2_entry *e = &ne->entry;
	unsigned l = e->name_len;

	switch (cause) {
	case VisitFile:
	case EnterDir:;
		/* Each UC2 entry name is a single path component (the directory
		   tree is rebuilt from dirid parents).  A name that is empty,
		   ".", "..", or contains a path separator is malformed or a
		   path-traversal attempt; refuse to extract it rather than
		   write outside the destination. */
		if (l == 0
		    || (l == 1 && e->name[0] == '.')
		    || (l == 2 && e->name[0] == '.' && e->name[1] == '.')
		    || memchr(e->name, '/', l)
		    || memchr(e->name, '\\', l)
#if defined(_WIN32) || defined(__DJGPP__)
		    || unsafe_dos_name(e->name, l)
#endif
		    )
			errx(EXIT_FAILURE, "unsafe archive entry name: %.*s",
			     (int)l, e->name);

		char *p = path->ptr + l;
		if (p + 1 >= endof(path->buffer))
			errx(EXIT_FAILURE, "Path too long");
		memcpy(path->ptr, e->name, l);
		*p = 0;

		if (cause == VisitFile) {
			if (output_exists(path->buffer, false)) {
				if (!opt.overwrite) {
					errno = EEXIST;
					warn("%s", path->buffer);
					break;
				}
#ifdef _WIN32
				if (chmod(path->buffer, 0666) < 0)
					err(EXIT_FAILURE, "%s", path->buffer);
#endif
				if (unlink(path->buffer) < 0)
					err(EXIT_FAILURE, "%s", path->buffer);
			}

			FILE *f = create_output(path->buffer);
			int ret = uc2_extract(path->uc2, &e->xi, e->size, write_file, f);
			if (ret < 0)
				uc2err(path->uc2, ret, "%s", e->name);
			/* Report a write error (e.g. a full disk) surfaced at close
			   rather than silently truncating the extracted file. */
			if (fclose(f) != 0 && ret >= 0)
				err(EXIT_FAILURE, "%s", path->buffer);
			if (!opt.no_file_meta)
				set_attrs(path->buffer, ne);
			break;
		}
		int r = mkdir(path->buffer, 0777);
		if (r < 0) {
			if (errno != EEXIST)
				err(EXIT_FAILURE, "mkdir %s", path->buffer);
			/* Do not append '/' until after lstat: a trailing slash can
			   cause it to follow a directory symlink. */
			if (!output_exists(path->buffer, true))
				errx(EXIT_FAILURE, "%s: output directory disappeared", path->buffer);
			ne->marked = false; // skip meta setting
		}
		*p++ = '/';
		path->ptr = p;
		*p = 0;
		break;

	case LeaveDir:
		assert(ne->entry.is_dir);
		if (ne->marked && !opt.no_dir_meta) {
			*path->ptr = 0;
			set_attrs(path->buffer, ne);
		}
		path->ptr -= l + 1;
	}
	return true;
}

/* --- OpenTimestamps (Phase 7) --- */

#ifdef _WIN32
#  include <io.h>
static int uc2_truncate(FILE *f, uint64_t len)
{
	if (len > INT64_MAX) { errno = EFBIG; return -1; }
	int rc = _chsize_s(_fileno(f), (__int64)len);
	if (rc) errno = rc;
	return rc == 0 ? 0 : -1;
}
#else
static int uc2_truncate(FILE *f, uint64_t len)
{
#ifdef __DJGPP__
	if (len > LONG_MAX) { errno = EFBIG; return -1; }
#else
	if (len > INT64_MAX) { errno = EFBIG; return -1; }
#endif
	return ftruncate(fileno(f), (off_t)len);
}
#endif

static int file_length(FILE *f, uint64_t *out)
{
	if (seek_file(f, 0, SEEK_END) != 0) return -1;
	int64_t end = tell_file(f);
	if (end < 0) return -1;
	*out = (uint64_t)end;
	return 0;
}

static int read_ots_proof(const char *path, uint8_t **out_data, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	uint64_t file_len;
	if (file_length(f, &file_len) < 0 || seek_file(f, 0, SEEK_SET) != 0) {
		fclose(f); return -1;
	}
	if (file_len > UC2_OTS_TRAILER_MAX_PROOF) {
		fclose(f); errno = EFBIG; return -1;
	}
	size_t len = (size_t)file_len;
	uint8_t *buf = malloc(len ? len : 1);
	if (!buf) { fclose(f); return -1; }
	size_t got = fread(buf, 1, len, f);
	int failed = ferror(f);
	fclose(f);
	if (failed || got != len) { free(buf); errno = EIO; return -1; }
	*out_data = buf;
	*out_len = len;
	return 0;
}

static int sha256_of_prefix(const char *path, uint64_t prefix_len,
                            uint8_t out[32])
{
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	struct uc2_sha256 ctx;
	uc2_sha256_init(&ctx);
	uint8_t buf[8192];
	uint64_t remaining = prefix_len;
	while (remaining) {
		size_t want = remaining < sizeof buf ? (size_t)remaining : sizeof buf;
		size_t got = fread(buf, 1, want, f);
		if (got == 0) { fclose(f); return -1; }
		uc2_sha256_update(&ctx, buf, got);
		remaining -= got;
	}
	uc2_sha256_final(&ctx, out);
	fclose(f);
	return 0;
}

static void print_hex(const uint8_t *p, size_t n)
{
	for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
}

static uint32_t trailer_u32(const uint8_t *p)
{
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Locate an existing OTS trailer in the archive on disk.
 * Returns 0 if a well-formed trailer is found (and fills outputs),
 *         1 if no trailer (back magic absent),
 *         negative on parse error (back magic present but malformed).
 * `*out_buf` is malloc'd; caller frees on success. */
static int load_trailer(const char *archive_path,
                        uint8_t **out_buf,
                        uint32_t *out_archive_len,
                        const uint8_t **out_proof, size_t *out_proof_len,
                        uint64_t *out_file_len)
{
	*out_buf = NULL;
	FILE *f = fopen(archive_path, "rb");
	if (!f) err(EXIT_FAILURE, "%s", archive_path);
	uint8_t *buf = NULL;
	uint8_t tail[UC2_OTS_TRAILER_TAIL_LEN];
	int rc = 1;
	uint64_t file_len;
	if (file_length(f, &file_len) < 0)
		goto io_error;
	if (out_file_len) *out_file_len = file_len;
	if (file_len < sizeof tail) goto done;
	if (seek_file(f, file_len - sizeof tail, SEEK_SET) != 0 ||
	    fread(tail, 1, sizeof tail, f) != sizeof tail)
		goto io_error;
	if (memcmp(tail + 4, UC2_OTS_TRAILER_MAGIC,
	           UC2_OTS_TRAILER_MAGIC_LEN) != 0)
		goto done;

	/* Match uc2_ots_trailer_parse's framing checks without loading the
	   preceding archive. Allocation and reads are bounded by the proof cap. */
	uint32_t proof_len = trailer_u32(tail);
	if (proof_len > UC2_OTS_TRAILER_MAX_PROOF) {
		rc = UC2_OTS_ERR_TOO_LARGE; goto done;
	}
	size_t total = UC2_OTS_TRAILER_OVERHEAD + (size_t)proof_len;
	if (total > file_len) { rc = UC2_OTS_ERR_TRUNCATED; goto done; }
	buf = malloc(total);
	if (!buf) goto io_error;
	if (seek_file(f, file_len - total, SEEK_SET) != 0 ||
	    fread(buf, 1, total, f) != total)
		goto io_error;
	if (memcmp(buf, UC2_OTS_TRAILER_MAGIC, UC2_OTS_TRAILER_MAGIC_LEN) != 0)
		rc = UC2_OTS_ERR_BAD_MAGIC;
	else if (trailer_u32(buf + 8) != UC2_OTS_TRAILER_VERSION)
		rc = UC2_OTS_ERR_BAD_VERSION;
	else if (trailer_u32(buf + 16) != proof_len)
		rc = UC2_OTS_ERR_NONCANONICAL;
	else if ((uint64_t)trailer_u32(buf + 12) != file_len - total)
		rc = UC2_OTS_ERR_OVERFLOW;
	else {
		*out_archive_len = trailer_u32(buf + 12);
		*out_proof = buf + UC2_OTS_TRAILER_HEAD_LEN;
		*out_proof_len = proof_len;
		*out_buf = buf;
		rc = UC2_OTS_OK;
	}
done:
	fclose(f);
	if (rc != UC2_OTS_OK) free(buf);
	return rc;
io_error:;
	int saved_errno = errno ? errno : EIO;
	fclose(f);
	free(buf);
	errno = saved_errno;
	err(EXIT_FAILURE, "%s", archive_path);
	return -1;
}

static int cmd_ots_attach(const char *archive_path, const char *proof_path,
                          int force)
{
	/* Read the .ots proof and validate its envelope. */
	uint8_t *proof; size_t proof_len;
	if (read_ots_proof(proof_path, &proof, &proof_len) < 0)
		err(EXIT_FAILURE, "%s", proof_path);
	uint8_t hash_op;
	const uint8_t *leaf, *body;
	size_t leaf_len, body_len;
	int rc = uc2_ots_parse_file(proof, proof_len, &hash_op,
	                            &leaf, &leaf_len, &body, &body_len);
	if (rc < 0) {
		free(proof);
		errx(EXIT_FAILURE, "%s: malformed .ots file (%d)", proof_path, rc);
	}
	if (hash_op != UC2_OTS_OP_SHA256) {
		free(proof);
		errx(EXIT_FAILURE, "%s: only SHA-256 .ots files are supported", proof_path);
	}

	/* Determine the archive byte range to attest (strip any existing trailer). */
	uint64_t attest_len;
	{
		uint8_t *abuf;
		uint32_t existing_al;
		const uint8_t *existing_proof;
		size_t existing_pl;
		int trc = load_trailer(archive_path, &abuf, &existing_al,
		                       &existing_proof, &existing_pl, &attest_len);
		free(abuf);
		if (trc == UC2_OTS_OK) {
			if (!force)
				errx(EXIT_FAILURE,
				     "%s: OTS trailer already present (use -f to replace)",
				     archive_path);
			attest_len = existing_al;
		} else if (trc < 0) {
			errx(EXIT_FAILURE,
			     "%s: existing trailer is malformed (%d); aborting",
			     archive_path, trc);
		}
	}

	if (attest_len > 0xffffffffu) {
		free(proof);
		errx(EXIT_FAILURE, "%s: archive too large for v1 OTS trailer", archive_path);
	}

	/* Verify leaf digest matches the archive's SHA-256. */
	uint8_t archive_sha[32];
	if (sha256_of_prefix(archive_path, attest_len, archive_sha) < 0) {
		free(proof);
		err(EXIT_FAILURE, "%s", archive_path);
	}
	if (leaf_len != 32 || memcmp(archive_sha, leaf, 32) != 0) {
		fprintf(stderr, "%s: proof leaf does not match archive SHA-256\n",
		        archive_path);
		fprintf(stderr, "  archive: "); print_hex(archive_sha, 32); fprintf(stderr, "\n");
		fprintf(stderr, "  proof:   "); print_hex(leaf, leaf_len); fprintf(stderr, "\n");
		free(proof);
		return EXIT_FAILURE;
	}

	/* Build trailer and rewrite archive. */
	size_t trailer_cap = UC2_OTS_TRAILER_OVERHEAD + proof_len;
	uint8_t *trailer = malloc(trailer_cap);
	if (!trailer) { free(proof); err(EXIT_FAILURE, "malloc"); }
	int tn = uc2_ots_trailer_build((uint32_t)attest_len,
	                               proof, proof_len, trailer, trailer_cap);
	if (tn < 0) { free(trailer); free(proof);
		errx(EXIT_FAILURE, "trailer build failed (%d)", tn); }

	FILE *f = fopen(archive_path, "rb+");
	if (!f) { free(trailer); free(proof); err(EXIT_FAILURE, "%s", archive_path); }
	if (seek_file(f, attest_len, SEEK_SET) != 0) {
		fclose(f); free(trailer); free(proof);
		err(EXIT_FAILURE, "%s", archive_path);
	}
	if (fwrite(trailer, 1, (size_t)tn, f) != (size_t)tn) {
		fclose(f); free(trailer); free(proof);
		err(EXIT_FAILURE, "%s", archive_path);
	}
	/* Truncate any leftover bytes (e.g. when replacing a longer prior trailer). */
	uint64_t new_end = attest_len + (unsigned)tn;
	if (fflush(f) != 0 || uc2_truncate(f, new_end) < 0) {
		fclose(f); free(trailer); free(proof);
		err(EXIT_FAILURE, "truncate");
	}
	if (fclose(f) != 0) {
		free(trailer); free(proof);
		err(EXIT_FAILURE, "%s", archive_path);
	}

	free(trailer);
	free(proof);
	uc2_say(stderr, "Attached %zu-byte OTS proof to %s\n",
	        proof_len, archive_path);
	uc2_say(stderr, "Everything went OK\n");
	return EXIT_SUCCESS;
}

static int cmd_ots_extract(const char *archive_path, const char *out_path)
{
	uint8_t *buf;
	uint32_t archive_len;
	const uint8_t *proof; size_t proof_len;
	int rc = load_trailer(archive_path, &buf,
	                      &archive_len, &proof, &proof_len, NULL);
	if (rc == 1)
		errx(EXIT_FAILURE, "%s: no OTS trailer present", archive_path);
	if (rc < 0)
		errx(EXIT_FAILURE, "%s: malformed OTS trailer (%d)", archive_path, rc);

	FILE *f = fopen(out_path, "wb");
	if (!f) err(EXIT_FAILURE, "%s", out_path);
	if (fwrite(proof, 1, proof_len, f) != proof_len)
		err(EXIT_FAILURE, "%s", out_path);
	if (fclose(f) != 0)
		err(EXIT_FAILURE, "%s", out_path);
	free(buf);
	uc2_say(stderr, "Wrote %zu-byte .ots proof to %s\n", proof_len, out_path);
	return EXIT_SUCCESS;
}

struct ots_info_ctx {
	int n_attestations;
};

static int ots_info_cb(void *vctx,
                       const uint8_t *tag,
                       const uint8_t *payload, size_t payload_len,
                       const uint8_t *digest, size_t digest_len)
{
	struct ots_info_ctx *c = vctx;
	(void)digest; (void)digest_len;
	c->n_attestations++;
	const char *name = uc2_ots_attest_name(tag);
	printf("  attestation: %s", name ? name : "<unknown>");
	if (name && strcmp(name, "pending") == 0) {
		/* Pending payload is varbytes(uri). */
		uint64_t uri_len;
		size_t consumed;
		if (uc2_ots_varint_decode(payload, payload_len, &uri_len, &consumed) < 0 ||
		    consumed > payload_len || uri_len > payload_len - consumed)
			return -1;
		printf("  (calendar: ");
		fwrite(payload + consumed, 1, (size_t)uri_len, stdout);
		printf(")");
	} else if (name && strcmp(name, "Bitcoin") == 0) {
		uint64_t height; size_t consumed;
		if (uc2_ots_varint_decode(payload, payload_len, &height, &consumed) == 0)
			printf("  (block height: %llu)", (unsigned long long)height);
	}
	putchar('\n');
	return 0;
}

static int cmd_ots_info(const char *archive_path)
{
	uint8_t *buf;
	uint32_t archive_len;
	const uint8_t *proof; size_t proof_len;
	int rc = load_trailer(archive_path, &buf,
	                      &archive_len, &proof, &proof_len, NULL);
	if (rc == 1)
		errx(EXIT_FAILURE, "%s: no OTS trailer present", archive_path);
	if (rc < 0)
		errx(EXIT_FAILURE, "%s: malformed OTS trailer (%d)", archive_path, rc);

	uint8_t hash_op;
	const uint8_t *leaf, *body;
	size_t leaf_len, body_len;
	rc = uc2_ots_parse_file(proof, proof_len, &hash_op,
	                        &leaf, &leaf_len, &body, &body_len);
	if (rc < 0)
		errx(EXIT_FAILURE, "%s: malformed .ots envelope (%d)", archive_path, rc);

	printf("OTS proof for %s\n", archive_path);
	printf("  archive bytes attested: %u\n", archive_len);
	printf("  hash:    %s\n",
	       hash_op == UC2_OTS_OP_SHA256 ? "SHA-256" : "<other>");
	printf("  leaf:    "); print_hex(leaf, leaf_len); printf("\n");

	uint8_t archive_sha[32];
	if (sha256_of_prefix(archive_path, archive_len, archive_sha) < 0)
		err(EXIT_FAILURE, "%s", archive_path);
	int leaf_match = (leaf_len == 32 && memcmp(archive_sha, leaf, 32) == 0);
	printf("  leaf matches archive: %s\n", leaf_match ? "yes" : "NO");

	struct ots_info_ctx ctx = {0};
	int wr = uc2_ots_walk(body, body_len, leaf, leaf_len, ots_info_cb, &ctx);
	if (wr < 0)
		errx(EXIT_FAILURE, "%s: walker error %d", archive_path, wr);
	printf("  attestations: %d\n", ctx.n_attestations);
	if (wr == UC2_OTS_RESULT_STRUCTURAL)
		printf("  status: structurally valid; contains unsupported ops\n"
		       "          run `ots verify` for full cryptographic check\n");
	else
		printf("  status: structurally valid (calendar-path ops only)\n");

	free(buf);
	flush_stdout();
	return leaf_match && wr >= 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Called from -t after the existing archive integrity check.
 * Returns 0 if no trailer or trailer verifies; non-zero on mismatch. */
static int verify_trailer_if_present(const char *archive_path)
{
	uint8_t *buf;
	uint32_t archive_len;
	const uint8_t *proof; size_t proof_len;
	int rc = load_trailer(archive_path, &buf,
	                      &archive_len, &proof, &proof_len, NULL);
	if (rc == 1) return 0; /* no trailer */
	if (rc < 0) {
		fprintf(stderr, "%s: malformed OTS trailer (%d)\n", archive_path, rc);
		return 1;
	}
	uint8_t hash_op;
	const uint8_t *leaf, *body;
	size_t leaf_len, body_len;
	rc = uc2_ots_parse_file(proof, proof_len, &hash_op,
	                        &leaf, &leaf_len, &body, &body_len);
	if (rc < 0) {
		fprintf(stderr, "%s: malformed .ots envelope (%d)\n", archive_path, rc);
		free(buf);
		return 1;
	}
	uint8_t archive_sha[32];
	if (sha256_of_prefix(archive_path, archive_len, archive_sha) < 0) {
		free(buf);
		return 1;
	}
	if (hash_op != UC2_OTS_OP_SHA256 || leaf_len != 32 ||
	    memcmp(archive_sha, leaf, 32) != 0) {
		fprintf(stderr, "%s: OTS leaf does not match archive SHA-256\n",
		        archive_path);
		free(buf);
		return 1;
	}
	int wr = uc2_ots_walk(body, body_len, leaf, leaf_len, NULL, NULL);
	free(buf);
	if (wr < 0) {
		fprintf(stderr, "%s: OTS walker error %d\n", archive_path, wr);
		return 1;
	}
	if (wr == UC2_OTS_RESULT_STRUCTURAL)
		uc2_say(stderr, "OTS proof: structurally valid (run `ots verify` for full check)\n");
	else
		uc2_say(stderr, "OTS proof: leaf matches; structure verified\n");
	return 0;
}

/* --- Archive creation --- */

static void w16(unsigned char *p, unsigned v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
}

static void w32(unsigned char *p, unsigned v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF;
	p[3] = (v >> 24) & 0xFF;
}

static unsigned short fletcher_csum(const unsigned char *data, unsigned len)
{
	if (!len) return 0xA55A;
	unsigned v = 0xA55A;
	const unsigned char *p = data;
	const unsigned char *e = p + len - 1;
	if (v > 0xFFFF)
		v ^= *p++ << 8;
	while (p < e) {
		v ^= p[0] | p[1] << 8;
		p += 2;
	}
	v &= 0xFFFF;
	if (p == e)
		v ^= *p | 0x10000;
	return (unsigned short)(v & 0xFFFF);
}

static unsigned to_dos_time(time_t t)
{
	struct tm *tm = localtime(&t);
	if (!tm || tm->tm_year < 80 || tm->tm_year > 207) return 0;
	return ((unsigned)(tm->tm_year - 80) << 25) |
	       ((unsigned)(tm->tm_mon + 1) << 21) |
	       ((unsigned)tm->tm_mday << 16) |
	       ((unsigned)tm->tm_hour << 11) |
	       ((unsigned)tm->tm_min << 5) |
	       ((unsigned)(tm->tm_sec / 2));
}

static void make_dos_name(unsigned char dos_name[11], const char *filename)
{
	const char *base = strrchr(filename, '/');
	base = base ? base + 1 : filename;
	const char *dot = strrchr(base, '.');
	int namelen = dot ? (int)(dot - base) : (int)strlen(base);

	memset(dos_name, ' ', 11);
	for (int i = 0; i < 8 && i < namelen; i++)
		dos_name[i] = toupper((unsigned char)base[i]);
	if (dot) {
		const char *ext = dot + 1;
		for (int i = 0; i < 3 && ext[i]; i++)
			dos_name[8 + i] = toupper((unsigned char)ext[i]);
	}
}

struct mem_reader {
	const unsigned char *data;
	unsigned pos, len;
};

static int mem_read_cb(void *ctx, void *buf, unsigned len)
{
	struct mem_reader *mr = ctx;
	unsigned avail = mr->len - mr->pos;
	if (len > avail) len = avail;
	if (len > 0) {
		memcpy(buf, mr->data + mr->pos, len);
		mr->pos += len;
	}
	return (int)len;
}

static int fread_cb(void *ctx, void *buf, unsigned len)
{
	FILE *f = ctx;
	size_t n = fread(buf, 1, len, f);
	return ferror(f) ? -1 : (int)n;
}

static int fwrite_cb(void *ctx, const void *ptr, unsigned len)
{
	return fwrite(ptr, 1, len, (FILE *)ctx) == len ? 0 : -1;
}

static FILE *stage_output(const char *path);
static void commit_staged_output(const char *path);

static void writer_check(uc2_writer *writer, int result, const char *path)
{
    if (result < 0) errx(EXIT_FAILURE, "%s: %s (%d)", path, uc2_writer_error(writer), result);
}

static void writer_scan(uc2_writer *writer, const char *path, const char *name,
                        unsigned parent, unsigned depth)
{
    if (depth > 64) errx(EXIT_FAILURE, "%s: directory nesting exceeds 64", path);
#ifdef _WIN32
    struct _stat64 st;
    wchar_t wide[32768];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, sizeof wide / sizeof *wide))
        errx(EXIT_FAILURE, "%s: invalid UTF-8 path", path);
    DWORD flags = GetFileAttributesW(wide);
    if (flags == INVALID_FILE_ATTRIBUTES || (flags & FILE_ATTRIBUTE_REPARSE_POINT))
        errx(EXIT_FAILURE, "%s: missing input or unsupported reparse point", path);
    if (_wstat64(wide, &st) < 0) err(EXIT_FAILURE, "%s", path);
#else
    struct stat st;
#ifdef __DJGPP__
    if (stat(path, &st) < 0) err(EXIT_FAILURE, "%s", path);
#else
    if (lstat(path, &st) < 0) err(EXIT_FAILURE, "%s", path);
#endif
#endif
    if (S_ISDIR(st.st_mode)) {
        unsigned id;
        writer_check(writer, uc2_writer_add_directory(writer, parent, name,
                     to_dos_time(st.st_mtime), 0, &id), path);
        DIR *dir = opendir(path);
        if (!dir) err(EXIT_FAILURE, "%s", path);
        for (;;) {
            errno = 0;
            struct dirent *de = readdir(dir);
            if (!de) {
                if (errno) err(EXIT_FAILURE, "%s", path);
                break;
            }
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            char child[PATH_MAX];
            int n = snprintf(child, sizeof child, "%s/%s", path, de->d_name);
            if (n < 0 || (size_t)n >= sizeof child) errx(EXIT_FAILURE, "input path too long: %s", path);
            writer_scan(writer, child, de->d_name, id, depth + 1);
        }
        if (closedir(dir) != 0) err(EXIT_FAILURE, "%s", path);
    } else if (S_ISREG(st.st_mode)) {
#if !defined(_WIN32) && !defined(__DJGPP__)
        int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
        if (fd < 0) err(EXIT_FAILURE, "%s", path);
        if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) errx(EXIT_FAILURE, "%s: input is no longer a regular file", path);
        FILE *f = fdopen(fd, "rb");
        if (!f) { close(fd); err(EXIT_FAILURE, "%s", path); }
#else
        FILE *f = fopen(path, "rb");
        if (!f) err(EXIT_FAILURE, "%s", path);
#endif
        if (st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX)
            errx(EXIT_FAILURE, "%s: input exceeds the 32-bit size limit", path);
        int result = uc2_writer_add_file(writer, parent, name, to_dos_time(st.st_mtime),
                                         0x20, (uint64_t)st.st_size, fread_cb, f);
        int closed = fclose(f);
        writer_check(writer, result, path);
        if (closed != 0) err(EXIT_FAILURE, "%s", path);
    } else errx(EXIT_FAILURE, "%s: only regular files and directories are supported", path);
}

static int create_archive(int nargs, char **args)
{
    struct uc2_writer_options options = UC2_WRITER_OPTIONS_INIT;
    options.level = opt.level;
    options.chunked = opt.chunked;
    uc2_writer *writer = uc2_writer_create(&options);
    if (!writer) errx(EXIT_FAILURE, "cannot initialize archive writer");
    uc2_say(stderr, "UC2 compression level: %s\n", level_name(opt.level));
    for (int i = 0; i < nargs; ++i) {
        char path[PATH_MAX];
        int n = snprintf(path, sizeof path, "%s", args[i]);
        if (n <= 0 || (size_t)n >= sizeof path) errx(EXIT_FAILURE, "input path too long or empty");
        while (n > 1 && (path[n-1] == '/' || path[n-1] == '\\')) path[--n] = 0;
        char *name = strrchr(path, '/');
#ifdef _WIN32
        char *back = strrchr(path, '\\');
        if (back && (!name || back > name)) name = back;
#endif
        name = name ? name + 1 : path;
        char current[PATH_MAX];
        if (!strcmp(path, ".") || !strcmp(path, "./.")) {
            if (!getcwd(current, sizeof current)) err(EXIT_FAILURE, "getcwd");
            name = strrchr(current, '/');
#ifdef _WIN32
            char *back = strrchr(current, '\\');
            if (back && (!name || back > name)) name = back;
#endif
            name = name ? name + 1 : current;
        }
        writer_scan(writer, path, name, 0, 0);
    }
    FILE *out = stage_output(opt.archive);
    int result = uc2_writer_finish(writer, fwrite_cb, out);
    writer_check(writer, result, opt.archive);
    commit_staged_output(opt.archive);
    struct uc2_writer_stats stats;
    uc2_writer_get_stats(writer, &stats);
    uc2_writer_free(writer);
    if (stats.masters)
        uc2_say(stderr, "  master[2]: custom master dictionaries: %u\n", stats.masters);
    if (opt.chunked)
        uc2_say(stderr,"  shared chunks: %u unique, %u references, %llu payload bytes reused\n",
                stats.unique_chunks,stats.chunk_references,(unsigned long long)stats.shared_bytes);
    uc2_say(stderr, "Created %s (%u files, %u dirs, %u masters, %u bytes)\n",
            opt.archive, stats.files, stats.directories, stats.masters, stats.archive_size);
    uc2_say(stderr, "Everything went OK\n");
    return EXIT_SUCCESS;
}

/* Memory read/write callbacks for benchmark */
struct bench_reader { const uint8_t *data; unsigned pos; unsigned len; };
static int bench_read(void *ctx, void *buf, unsigned len)
{
	struct bench_reader *r = ctx;
	unsigned avail = r->len - r->pos;
	if (len > avail) len = avail;
	if (len > 0) { memcpy(buf, r->data + r->pos, len); r->pos += len; }
	return (int)len;
}
struct bench_writer { uint8_t *data; size_t pos; size_t cap; };
static int bench_write(void *ctx, const void *ptr, unsigned len)
{
	struct bench_writer *w = ctx;
	if (w->pos + len > w->cap) return -1;
	memcpy(w->data + w->pos, ptr, len);
	w->pos += len;
	return 0;
}

/* Benchmark: compress input with all methods and report ratio + speed. */
static int run_benchmark(int nfiles, char **files)
{
	/* Read all input files into memory */
	size_t total_size = 0;
	size_t *sizes = calloc((size_t)nfiles, sizeof *sizes);
	if (!sizes) err(EXIT_FAILURE, "calloc");
	for (int i = 0; i < nfiles; i++) {
		FILE *f = fopen(files[i], "rb");
		if (!f) err(EXIT_FAILURE, "%s", files[i]);
		uint64_t size;
		if (file_length(f, &size) < 0) err(EXIT_FAILURE, "%s", files[i]);
		fclose(f);
		if (size > UINT32_MAX - total_size || size > SIZE_MAX - total_size)
			errx(EXIT_FAILURE, "benchmark input exceeds the 4 GiB UC2 limit");
		sizes[i] = (size_t)size;
		total_size += sizes[i];
	}
	if (total_size > SIZE_MAX - total_size / 8 - 4096)
		errx(EXIT_FAILURE, "benchmark output buffer is too large");
	uint8_t *data = malloc(total_size ? total_size : 1);
	if (!data) err(EXIT_FAILURE, "malloc");
	size_t pos = 0;
	for (int i = 0; i < nfiles; i++) {
		FILE *f = fopen(files[i], "rb");
		if (!f) err(EXIT_FAILURE, "%s", files[i]);
		size_t got = fread(data + pos, 1, sizes[i], f);
		int extra = fgetc(f);
		if (got != sizes[i] || extra != EOF || ferror(f))
			errx(EXIT_FAILURE, "%s: input changed or read failed", files[i]);
		pos += got;
		fclose(f);
	}
	free(sizes);

	printf("UC2 Benchmark: %zu bytes input (%d file%s)\n\n",
	       total_size, nfiles, nfiles == 1 ? "" : "s");
	printf("%-16s %10s %8s %10s\n",
	       "Method", "Compressed", "Ratio", "Enc (ms)");
	printf("%-16s %10s %8s %10s\n",
	       "----------------", "----------", "--------", "----------");

	struct { int level; const char *name; } methods[] = {
		{2, "Huffman Fast"},
		{3, "Huffman Normal"},
		{4, "Huffman Tight"},
		{5, "Huffman Ultra"},
		{6, "rANS Fast"},
		{7, "rANS Normal"},
		{8, "rANS Tight"},
		{9, "rANS Ultra"},
	};

	/* Benchmark each method against the same in-memory input. */

	for (int m = 0; m < (int)(sizeof methods / sizeof methods[0]); m++) {
		/* Compress */
		size_t out_cap = total_size + total_size / 8 + 4096;
		uint8_t *out = malloc(out_cap);
		if (!out) continue;

		struct bench_reader rctx = {data, 0, (unsigned)total_size};
		struct bench_writer wctx = {out, 0, out_cap};
		unsigned short csum;
		unsigned csize;

		clock_t t0 = clock();
		int ret = uc2_compress(methods[m].level,
			bench_read, &rctx, bench_write, &wctx,
			(unsigned)total_size, &csum, &csize);
		clock_t t1 = clock();

		if (ret < 0) {
			printf("%-16s %10s\n", methods[m].name, "ERROR");
			free(out);
			continue;
		}

		double enc_ms = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;
		double ratio = total_size ? (double)csize / total_size * 100.0 : 0;

		printf("%-16s %10u %7.1f%% %9.1f ms\n",
		       methods[m].name, csize, ratio, enc_ms);

		free(out);
	}

	/* Also test LZ4 */
	{
		size_t bound = total_size + total_size / 255 + 16;
		uint8_t *lz4out = malloc(bound);
		if (lz4out) {
			clock_t t0 = clock();
			size_t clen = uc2_lz4_compress(data, total_size, lz4out, bound);
			clock_t t1 = clock();
			double enc_ms = (double)(t1 - t0) * 1000.0 / CLOCKS_PER_SEC;
			double ratio = total_size ? (double)clen / total_size * 100.0 : 0;
			printf("%-16s %10zu %7.1f%% %9.1f ms\n",
			       "LZ4 Ultra-fast", clen, ratio, enc_ms);
			free(lz4out);
		}
	}

	printf("\n");
	free(data);
	flush_stdout();
	return EXIT_SUCCESS;
}

/* Keep each short option's operand together while removing long options.
 * An attached value consumes the rest of its short-option cluster. */
static int short_option_span(const char *arg)
{
	if (arg[0] != '-' || arg[1] == '-') return 1;
	for (const char *p = arg + 1; *p; p++) {
		const char *option = strchr(short_options, *p);
		if (!option || *p == ':') break;
		if (option[1] == ':') return p[1] ? 1 : 2;
	}
	return 1;
}

/* A single pass owns both long-option operands and short-option operands.
 * getopt receives the retained arguments with their original grouping. */
static void extract_long_options(int *argcp, char **argv, char **ingest_path)
{
	static const struct {
		const char *name;
		int ingest, ots;
		bool takes_path;
	} options[] = {
		{"--ingest", INGEST_MODE_WRITE, 0, true},
		{"--ingest-tar", INGEST_MODE_TAR, 0, true},
		{"--ingest-restore", INGEST_MODE_RESTORE, 0, true},
		{"--ots-attach", 0, OTS_MODE_ATTACH, true},
		{"--ots-extract", 0, OTS_MODE_EXTRACT, false},
		{"--ots-info", 0, OTS_MODE_INFO, false}
	};
	int argc = *argcp, kept = 1;
	for (int i = 1; i < argc; ) {
		const char *arg = argv[i];
		if (!strcmp(arg, "--")) {
			while (i < argc) argv[kept++] = argv[i++];
			break;
		}
		if (!strcmp(arg, "--chunked")) {
			opt.chunked = true;
			i++;
			continue;
		}
		unsigned index;
		for (index = 0; index < sizeof options / sizeof *options; index++) {
			size_t n = strlen(options[index].name);
			if (!strncmp(arg, options[index].name, n)
			    && (!arg[n] || (options[index].takes_path && arg[n] == '=')))
				break;
		}
		if (index == sizeof options / sizeof *options) {
			int count = short_option_span(arg);
			while (count-- && i < argc) argv[kept++] = argv[i++];
			continue;
		}
		if (opt.ingest_mode || opt.ots_mode)
			errx(EXIT_FAILURE, "only one ingestion or OTS operation may be specified");
		opt.ingest_mode = options[index].ingest;
		opt.ots_mode = options[index].ots;
		i++;
		if (options[index].takes_path) {
			char *value;
			const char *equals = strchr(arg, '=');
			if (equals) value = (char *)equals + 1;
			else {
				if (i == argc || argv[i][0] == '-')
					errx(EXIT_FAILURE, "%s requires a path (use %s=<path> for a leading '-')",
					     options[index].name, options[index].name);
				value = argv[i++];
			}
			if (!*value) errx(EXIT_FAILURE, "%s requires a nonempty path", options[index].name);
			if (options[index].ingest) *ingest_path = value;
			else opt.ots_path = value;
		}
	}
	argv[kept] = NULL;
	*argcp = kept;
}

/* --- ingest v3: real UC2 archive output (git-bug 59bec0d, approach A) ---
 *
 * The ingested byte stream is split into CDC chunks and deduplicated by
 * content hash.  Each unique chunk is stored once as a NoMaster-compressed
 * data block; the central directory carries one file entry per chunk
 * *occurrence* in stream order.  The result is a genuine UC2 v3 archive:
 * `uc2 -l` lists it and `uc2 -x` extracts the parts.  Occurrences of a
 * repeated chunk share a single stored block, so the payload is
 * deduplicated even though the listing is one line per chunk.
 * `uc2 --ingest-restore` walks the entries in cdir (= stream) order and
 * concatenates them, reproducing the original stream exactly.
 *
 * v1/v2 (the custom UC2INGST blob in libuc2) remain readable; restore
 * auto-detects the format by magic. */

#define INGEST_CDC_BITS 13  /* avg 8 KiB chunks (uc2_ingest default) */

/* hash -> unique-chunk slot.  Open addressing, power-of-two capacity,
 * grown as unique chunks arrive to keep the load factor <= 0.5.
 * A key collision (or the 0-hash
 * sentinel shift) is resolved by probing to a distinct slot, and every
 * hit is confirmed with a byte compare, so distinct chunks never share
 * storage regardless of hash behaviour. */
struct ing_map {
	uint64_t *keys;   /* 0 = empty slot */
	int      *slots;  /* index into the unique-chunk table */
	size_t    cap;    /* power of two; may exceed 2^31 for huge streams */
};

static int ing_map_init(struct ing_map *m, size_t want)
{
	size_t cap = 16;
	while (cap < want) cap *= 2;
	m->keys = calloc(cap, sizeof *m->keys);
	m->slots = malloc(cap * sizeof *m->slots);
	if (!m->keys || !m->slots) {
		free(m->keys); free(m->slots);
		m->keys = NULL; m->slots = NULL;
		return -1;
	}
	m->cap = cap;
	return 0;
}

static void ing_map_free(struct ing_map *m)
{
	free(m->keys); free(m->slots);
	m->keys = NULL; m->slots = NULL;
}

static int ing_map_grow(struct ing_map *m)
{
	struct ing_map next;
	if (m->cap > SIZE_MAX / (2 * sizeof *m->keys) ||
	    ing_map_init(&next, m->cap * 2) != 0) return -1;
	for (size_t i = 0; i < m->cap; i++) {
		if (!m->keys[i]) continue;
		size_t p = m->keys[i] & (next.cap - 1);
		while (next.keys[p]) p = (p + 1) & (next.cap - 1);
		next.keys[p] = m->keys[i]; next.slots[p] = m->slots[i];
	}
	ing_map_free(m); *m = next;
	return 0;
}

static char *staged_path;
static FILE *staged_stream;

static int close_staged_output(void)
{
	FILE *stream = staged_stream;
	staged_stream = NULL;
	return stream ? fclose(stream) : 0;
}

static void discard_staged_output(void)
{
	close_staged_output();
	if (staged_path) {
		unlink(staged_path);
		free(staged_path);
		staged_path = NULL;
	}
}

static FILE *stage_output(const char *path)
{
	char *candidate = malloc(strlen(path) + 96);
	if (!candidate) err(EXIT_FAILURE, "malloc");
	for (unsigned i = 0; i < 128; i++) {
		sprintf(candidate, "%s.tmp-%lu-%lu-%u", path,
		        (unsigned long)getpid(), (unsigned long)time(NULL), i);
#ifdef _WIN32
		wchar_t wide[32768];
		if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, candidate, -1,
		                        wide, sizeof wide / sizeof *wide))
			errx(EXIT_FAILURE, "invalid output path");
		int fd = _wopen(wide, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, 0600);
#else
		int fd = open(candidate, O_CREAT | O_EXCL | O_RDWR | O_BINARY, 0600);
#endif
		if (fd >= 0) {
			staged_path = candidate;
			if (atexit(discard_staged_output) != 0) {
				close(fd);
				discard_staged_output();
				errx(EXIT_FAILURE, "cannot register output cleanup");
			}
			staged_stream = fdopen(fd, "w+b");
			if (!staged_stream) {
				int error = errno;
				close(fd);
				errno = error;
				err(EXIT_FAILURE, "fdopen");
			}
			return staged_stream;
		}
		if (errno != EEXIST) err(EXIT_FAILURE, "%s", candidate);
	}
	errx(EXIT_FAILURE, "cannot create temporary output for %s", path);
	return NULL;
}

static void commit_staged_output(const char *path)
{
	if (close_staged_output() != 0)
		err(EXIT_FAILURE, "%s: output close failed", path);
#ifdef _WIN32
	int na = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, staged_path, -1, NULL, 0);
	int nb = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
	wchar_t *a = malloc((size_t)na * sizeof *a), *b = malloc((size_t)nb * sizeof *b);
	if (!na || !nb || !a || !b) errx(EXIT_FAILURE, "invalid output path");
	MultiByteToWideChar(CP_UTF8, 0, staged_path, -1, a, na);
	MultiByteToWideChar(CP_UTF8, 0, path, -1, b, nb);
	if (!MoveFileExW(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		errx(EXIT_FAILURE, "cannot commit %s (Windows error %lu)", path, GetLastError());
	free(a); free(b);
#else
	if (rename(staged_path, path) != 0) err(EXIT_FAILURE, "%s", path);
#endif
	free(staged_path); staged_path = NULL;
}

struct ing_uniq {
	unsigned       off;      /* block offset in the archive */
	unsigned       csize;    /* compressed size */
	unsigned       usize;    /* uncompressed (chunk) size */
	unsigned short csum;     /* fletcher checksum of raw chunk */
	uint32_t       src_off;  /* chunk offset in the raw temporary spool */
};

/* Build a real UC2 v3 archive with bounded payload RAM. Returns 0 and
 * fills stats on success; fatal errors exit via err()/errx() as in
 * create_archive(). */
static int ingest_write_v3(const char *archive_path,
                           FILE *input,
                           struct uc2_ingest_stats *stats)
{
	int nocc = 0;
	FILE *out = stage_output(archive_path);
	FILE *raw = tmpfile();
	if (!raw) err(EXIT_FAILURE, "temporary chunk spool");

	unsigned char header[29];
	memset(header, 0, sizeof header);
	if (fwrite(header, 1, 29, out) != 29)
		err(EXIT_FAILURE, "%s", archive_path);

	int *occ_uniq = NULL;
	struct ing_uniq *uniq = NULL;
	size_t occ_cap = 0, uniq_cap = 0;

	struct ing_map map;
	if (ing_map_init(&map, 16) != 0)
		err(EXIT_FAILURE, "malloc");

	unsigned method = opt.level >= 6 ? 10 : (opt.level <= 1 ? 1 : (unsigned)opt.level);
	int nuniq = 0;
	uint64_t bytes_stored = 0, bytes_saved = 0;
	uint64_t bytes_in = 0, raw_size = 0;
	uint8_t data[1 << (INGEST_CDC_BITS + 2)], compare[sizeof data];
	size_t buffered = 0;
	int at_end = 0;
	for (;;) {
		if (!at_end && buffered < sizeof data) {
			size_t got = fread(data + buffered, 1, sizeof data - buffered, input);
			buffered += got;
			if (ferror(input)) err(EXIT_FAILURE, "read stdin");
			at_end = feof(input);
		}
		if (!buffered) break;
		struct uc2_chunker chunker;
		uc2_chunker_init(&chunker, INGEST_CDC_BITS, 0, 0);
		size_t coff, length;
		uc2_chunker_next(&chunker, data, buffered, &coff, &length);
		unsigned clen = (unsigned)length;
		if (clen > UINT32_MAX - bytes_in)
			errx(EXIT_FAILURE, "input exceeds the 4 GiB UC2 ingest limit");
		bytes_in += clen;
		if ((size_t)nocc == occ_cap) {
			if (occ_cap > INT_MAX / 2) errx(EXIT_FAILURE, "too many chunks");
			occ_cap = occ_cap ? occ_cap * 2 : 64;
			int *grown = realloc(occ_uniq, occ_cap * sizeof *grown);
			if (!grown) err(EXIT_FAILURE, "realloc");
			occ_uniq = grown;
		}
		if ((size_t)nuniq >= map.cap / 2 && ing_map_grow(&map) != 0)
			err(EXIT_FAILURE, "dedup index");
		uint64_t mask = (uint64_t)map.cap - 1;
		uint64_t key = uc2_hash64(data, clen);
		if (key == 0) key = 1;  /* keep 0 as the empty-slot sentinel */

		uint64_t p = key & mask;
		int slot = -1;
		while (map.keys[p]) {
			int s = map.slots[p];
			if (map.keys[p] == key && uniq[s].usize == clen) {
				if (seek_file(raw, uniq[s].src_off, SEEK_SET) != 0 ||
				    fread(compare, 1, clen, raw) != clen)
					err(EXIT_FAILURE, "chunk spool read");
				if (memcmp(compare, data, clen) == 0) { slot = s; break; }
			}
			p = (p + 1) & mask;
		}
		if (slot >= 0) {
			occ_uniq[nocc++] = slot;
			bytes_saved += clen;
			memmove(data, data + clen, buffered - clen);
			buffered -= clen;
			continue;
		}

		/* New unique chunk: compress as a standalone NoMaster block. */
		unsigned here = tell32(out);
		struct mem_reader mr = { data, 0, clen };
		unsigned csize = 0;
		unsigned short csum = 0;
		int cr = uc2_compress(opt.level, mem_read_cb, &mr, fwrite_cb, out,
		                      clen, &csum, &csize);
		if (cr < 0)
			errx(EXIT_FAILURE, "chunk compression error %d", cr);

		if ((size_t)nuniq == uniq_cap) {
			uniq_cap = uniq_cap ? uniq_cap * 2 : 64;
			struct ing_uniq *grown = realloc(uniq, uniq_cap * sizeof *grown);
			if (!grown) err(EXIT_FAILURE, "realloc");
			uniq = grown;
		}
		slot = nuniq++;
		uniq[slot] = (struct ing_uniq){
			.off = here, .csize = csize, .usize = clen,
			.csum = csum, .src_off = (uint32_t)raw_size
		};
		if (seek_file(raw, raw_size, SEEK_SET) != 0 || fwrite(data, 1, clen, raw) != clen)
			err(EXIT_FAILURE, "chunk spool write");
		raw_size += clen;
		occ_uniq[nocc++] = slot;
		bytes_stored += csize;

		map.keys[p] = key;   /* p is the empty slot the probe stopped on */
		map.slots[p] = slot;
		memmove(data, data + clen, buffered - clen);
		buffered -= clen;
	}
	ing_map_free(&map);
	if (fclose(raw) != 0) err(EXIT_FAILURE, "chunk spool close");

	/* Synthetic entry names, zero-padded so lexical order == stream order. */
	int width = 1;
	for (int t = nocc - 1; t >= 10; t /= 10) width++;

	/* Match the decoder's 64 MiB limit; check before multiplying on 32-bit hosts. */
	const size_t cdir_limit = 64u * 1024u * 1024u;
	const size_t entry_size = 47 + 21 + (5 + width) + 1;
	if ((size_t)nocc > (cdir_limit - 22) / entry_size)
		errx(EXIT_FAILURE, "central directory too large");
	size_t cdir_cap = 22 + (size_t)nocc * entry_size; /* Includes the directory tail. */

	unsigned char *raw_cdir = malloc(cdir_cap);
	if (!raw_cdir) err(EXIT_FAILURE, "malloc");
	unsigned char *p = raw_cdir;

	for (int i = 0; i < nocc; i++) {
		char name[32];
		int namelen = snprintf(name, sizeof name, "part-%0*d", width, i);
		const struct ing_uniq *u = &uniq[occ_uniq[i]];

		*p++ = 2; /* FileEntry */
		/* OSMETA (22 bytes) */
		w32(p, 0); p += 4;           /* parent = root */
		*p++ = 0x20;                 /* archive attribute */
		w32(p, 0); p += 4;           /* dos_time */
		make_dos_name(p, name); p += 11;
		*p++ = 0;                    /* hidden */
		*p++ = 1;                    /* has tags */
		/* FILEMETA (6 bytes) */
		w32(p, u->usize); p += 4;
		w16(p, u->csum); p += 2;
		/* COMPRESS (10 bytes) */
		w32(p, u->csize); p += 4;
		w16(p, method); p += 2;
		w32(p, 1); p += 4;           /* masterPrefix = NoMaster */
		/* LOCATION (8 bytes) */
		w32(p, 1); p += 4;           /* volume */
		w32(p, u->off); p += 4;
		/* EXTMETA: long name tag */
		memcpy(p, "AIP:Win95 LongN", 16); p += 16;
		w32(p, (unsigned)namelen + 1); p += 4;
		*p++ = 0;
		memcpy(p, name, (size_t)namelen + 1); p += namelen + 1;
	}

	/* EndOfCdir + XTAIL + aserial */
	*p++ = 4;
	*p++ = 0;
	*p++ = 0;
	w32(p, 0); p += 4;
	memset(p, ' ', 11); p += 11;
	w32(p, 0); p += 4;

	unsigned cdir_size = (unsigned)(p - raw_cdir);
	unsigned short cdir_csum = fletcher_csum(raw_cdir, cdir_size);

	unsigned cdir_offset = tell32(out);
	unsigned char crec[10];
	memset(crec, 0, 10);
	if (fwrite(crec, 1, 10, out) != 10) err(EXIT_FAILURE, "write directory placeholder");

	struct mem_reader mr = { raw_cdir, 0, cdir_size };
	unsigned cdir_csize = 0;
	unsigned short cdir_comp_csum = 0;
	int ret = uc2_compress(opt.level, mem_read_cb, &mr, fwrite_cb, out,
	                       cdir_size, &cdir_comp_csum, &cdir_csize);
	free(raw_cdir);
	if (ret < 0)
		errx(EXIT_FAILURE, "cdir compression error %d", ret);

	unsigned total = tell32(out);

	if (seek_file(out, cdir_offset, SEEK_SET) != 0) err(EXIT_FAILURE, "seek output");
	w32(crec + 0, 0);
	w16(crec + 4, method);
	w32(crec + 6, 1);                /* masterPrefix = NoMaster */
	if (fwrite(crec, 1, 10, out) != 10) err(EXIT_FAILURE, "write directory header");

	if (seek_file(out, 0, SEEK_SET) != 0) err(EXIT_FAILURE, "seek output");
	unsigned complen = total - 13;
	w32(header + 0, 0x1A324355);
	w32(header + 4, complen);
	w32(header + 8, complen + 0x01B2C3D4);
	header[12] = 0;
	w32(header + 13, 1);
	w32(header + 17, cdir_offset);
	w16(header + 21, cdir_csum);
	header[23] = 0;
	w16(header + 24, 203);
	w16(header + 26, 200);
	header[28] = 0;
	if (fwrite(header, 1, 29, out) != 29) err(EXIT_FAILURE, "write archive header");

	free(occ_uniq);
	free(uniq);
	commit_staged_output(archive_path);

	if (stats) {
		stats->bytes_in     = bytes_in;
		stats->chunks_total = nocc;
		stats->chunks_new   = nuniq;
		stats->chunks_dedup = nocc - nuniq;
		stats->bytes_stored = bytes_stored;
		stats->bytes_saved  = bytes_saved;
	}
	return 0;
}

/* Restore a v3 (real UC2) ingest archive by walking its file entries in
 * central-directory order through the ordinary reader and concatenating
 * their contents to out.  Returns 0 on success, -1 on any read/extract
 * error. */
static int ingest_restore_v3(const char *archive_path, FILE *out)
{
	FILE *f = fopen(archive_path, "rb");
	if (!f)
		return -1;
	uc2_handle uc2 = uc2_open(&io, f);
	if (!uc2) {
		fclose(f);
		return -1;
	}

	struct { struct uc2_xinfo xi; unsigned size; } *ents = NULL;
	size_t nents = 0, cap = 0;
	int rc = 0;

	for (;;) {
		struct uc2_entry e;
		int ret = uc2_read_cdir(uc2, &e);
		if (ret < 0) { rc = -1; break; }
		if (ret == UC2_End)
			break;

		bool is_dir = e.is_dir;
		struct uc2_xinfo xi = e.xi;
		unsigned size = e.size;

		while (ret == UC2_TaggedEntry) {
			ret = uc2_get_tag(uc2, &e, NULL, NULL, NULL);
			if (ret < 0) { rc = -1; break; }
		}
		if (rc < 0)
			break;

		if (!is_dir) {
			if (nents >= cap) {
				cap = cap ? cap * 2 : 64;
				void *np = realloc(ents, cap * sizeof *ents);
				if (!np) { rc = -1; break; }
				ents = np;
			}
			ents[nents].xi = xi;
			ents[nents].size = size;
			nents++;
		}
	}

	if (rc == 0 && uc2_finish_cdir(uc2, NULL) < 0)
		rc = -1;
	if (rc == 0) {
		for (size_t i = 0; i < nents; i++) {
			if (uc2_extract(uc2, &ents[i].xi, ents[i].size,
			                write_file, out) < 0) {
				rc = -1;
				break;
			}
		}
	}

	free(ents);
	uc2_close(uc2);
	fclose(f);
	return rc;
}

static int cmd_ingest_tar(const char *archive_path)
{
    binary_stream(stdin);
    struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT;
    options.chunked=opt.chunked;
    uc2_writer *writer=uc2_writer_create(&options);
    if (!writer) errx(EXIT_FAILURE,"cannot initialize tar archive writer");
    char error[256];
    if (uc2_tar_read(writer,stdin,error,sizeof error)<0) errx(EXIT_FAILURE,"%s",error);
    FILE *out=stage_output(archive_path);
    int result=uc2_writer_finish(writer,fwrite_cb,out);
    writer_check(writer,result,archive_path);
    commit_staged_output(archive_path);
    struct uc2_writer_stats stats;
    uc2_writer_get_stats(writer,&stats);
    uc2_writer_free(writer);
    uc2_say(stderr,"Ingested tar: %u files, %u directories, %u archive bytes\n",stats.files,stats.directories,stats.archive_size);
    if (opt.chunked)
        uc2_say(stderr,"Shared chunks: %u unique, %u references, %llu payload bytes reused\n",
                stats.unique_chunks,stats.chunk_references,(unsigned long long)stats.shared_bytes);
    return EXIT_SUCCESS;
}

static int cmd_ingest_write(const char *archive_path)
{
	binary_stream(stdin);
    if (opt.chunked) {
        struct uc2_writer_options options=UC2_WRITER_OPTIONS_INIT; options.chunked=1;
        uc2_writer *writer=uc2_writer_create(&options);
        if (!writer) errx(EXIT_FAILURE,"cannot initialize chunked stream writer");
        writer_check(writer,uc2_writer_add_file(writer,0,"data",0,0x20,UC2_WRITER_UNKNOWN_SIZE,
                                              fread_cb,stdin),archive_path);
        FILE *out=stage_output(archive_path);
        int result=uc2_writer_finish(writer,fwrite_cb,out);
        writer_check(writer,result,archive_path);
        commit_staged_output(archive_path);
        struct uc2_writer_stats stats; uc2_writer_get_stats(writer,&stats); uc2_writer_free(writer);
        uc2_say(stderr,"Ingested stream: %u unique chunks, %u references, %llu payload bytes reused\n",
                stats.unique_chunks,stats.chunk_references,(unsigned long long)stats.shared_bytes);
        return EXIT_SUCCESS;
    }
	struct uc2_ingest_stats st;
	int rc = ingest_write_v3(archive_path, stdin, &st);
	if (rc != 0)
		errx(EXIT_FAILURE, "ingest write failed: %s", archive_path);

	uc2_say(stderr,
	        "ingested %llu bytes -> %d chunks (%d new, %d deduped, %llu bytes saved)\n",
	        (unsigned long long)st.bytes_in,
	        st.chunks_total, st.chunks_new, st.chunks_dedup,
	        (unsigned long long)st.bytes_saved);
	return EXIT_SUCCESS;
}

static int cmd_ingest_restore(const char *archive_path)
{
	binary_stream(stdout);
	/* Auto-detect the on-disk format: the legacy v1/v2 writer emits a
	   "UC2INGST" blob handled by libuc2; the v3 writer emits a real UC2
	   archive restored by walking its entries through the reader. */
	FILE *f = fopen(archive_path, "rb");
	if (!f)
		err(EXIT_FAILURE, "%s", archive_path);
	char magic[8];
	size_t n = fread(magic, 1, sizeof magic, f);
	fclose(f);

	int rc;
	if (n == sizeof magic && memcmp(magic, "UC2INGST", 8) == 0)
		rc = uc2_ingest_restore(archive_path, stdout);
	else
		rc = ingest_restore_v3(archive_path, stdout);

	if (rc != 0)
		errx(EXIT_FAILURE, "ingest restore failed: %s", archive_path);
	flush_stdout();
	return EXIT_SUCCESS;
}


int main(int argc, char *argv[])
{
#ifdef __DJGPP__
	setprogname(argv[0]);
#endif
	if (argc == 1)
		goto usage;

	{
		char *ingest_path = NULL;
		extract_long_options(&argc, argv, &ingest_path);
		if (opt.ingest_mode != INGEST_MODE_NONE) {
			if (argc != 1 && !(argc == 2 && !strcmp(argv[1], "--")))
				errx(EXIT_FAILURE, "ingestion modes do not accept short options or extra operands");
			opt.archive = ingest_path;
		}
		if (opt.ingest_mode == INGEST_MODE_TAR)
			return cmd_ingest_tar(ingest_path);
		if (opt.ingest_mode == INGEST_MODE_WRITE)
			return cmd_ingest_write(ingest_path);
		if (opt.ingest_mode == INGEST_MODE_RESTORE) {
			if (opt.chunked) errx(EXIT_FAILURE,"--chunked is a creation option");
			return cmd_ingest_restore(ingest_path);
		}
	}


	for (;;) {
		int o = getopt(argc, argv, short_options);
		if (o == -1)
			break;
		switch (o) {
		case 'x':
			opt.list = opt.test = false;
			break;
		case 'l':
			opt.list = true;
			break;
		case 'a':
			opt.all = true;
			break;
		case 't':
			opt.test = true;
			break;
		case 'f':
			opt.overwrite = true;
			break;
		case 'd':
			opt.dest = *optarg ? optarg : 0;
			break;
		case 'C':
			if (chdir(optarg) < 0)
				err(EXIT_FAILURE, "%s", optarg);
			break;
		case 'c':
		case 'p':
			opt.pipe = true;
			break;
		case 'D':
			opt.no_file_meta = opt.no_dir_meta;
			opt.no_dir_meta = true;
			break;
		case 'w':
			opt.create = true;
			break;
		case 'q':
			opt.quiet = true;
			break;
		case 'B':
			opt.benchmark = true;
			break;
		case 'L':
			opt.level = atoi(optarg);
			if (opt.level < 2 || opt.level > 9)
				errx(EXIT_FAILURE, "Compression level must be 2..9 (2-5=Huffman, 6-9=rANS)");
			break;
		case 'T':
			opt.sep = '\t';
			break;
		case '?':
			if (optopt)
				return EXIT_FAILURE;
		case 'h':
			opt.help = true;
			printf("UC2 " UC2_VERSION_STRING " (UltraCompressor II)\n"
			       "\"Fast, reliable and superior compression.\"\n\n");
usage:
			printf(
				"uc2 [-afpqDT] [-d destination] archive.uc2 [files]...\n"
				"uc2 -l [-aqT] archive.uc2 [files]...\n"
				"uc2 -t [-aq] archive.uc2 [files]...\n"
				"uc2 -w [-qL level] archive.uc2 files...\n"
				"uc2 -B files...   (benchmark all methods)\n"
				"uc2 --ots-attach <proof.ots> [-f] archive.uc2\n"
				"uc2 --ots-extract archive.uc2 <out.ots>\n"
				"uc2 --ots-info archive.uc2\n"
				"uc2 --ingest <archive>            # stdin -> dedup UC2 archive\n"
				"uc2 --ingest-tar <archive>        # tar stdin -> named UC2 entries\n"
				"uc2 --ingest-restore <archive>    # archive -> stdout\n"
			);
			if (!opt.help)
				printf("uc2 -h\n");
			else
				printf(
					" -l      List\n"
					" -t      Test\n"
					" -w      Create archive\n"
					" --chunked  Share chunks across files (requires revival 3.0.0a11+)\n"
					" -L n    Level: 2=Fast 3=Normal 4=Tight(default) 5=Ultra 6-9=rANS\n"
					" -a      All versions of files\n"
					" -d path Destination to extract to\n"
					" -f      Overwrite\n"
					" -p      To stdout\n"
					" -B      Benchmark all methods on input files\n"
					" -q      Quiet (suppress status messages)\n"
					" -D      Do not set time and permissions of dirs (also files: -DD)\n"
					" -T      Tab-separated\n"
					"\nhttps://github.com/evvaletov/uc2\n"
				);
			flush_stdout();
			return opt.help ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	if (argc == optind)
		errx(EXIT_FAILURE, "Archive not given");
	opt.archive = argv[optind++];
    if (opt.chunked && !opt.create) errx(EXIT_FAILURE,"--chunked is a creation option");

	if (opt.benchmark) {
		/* -B: benchmark uses remaining args as input files (archive arg is first file) */
		return run_benchmark(argc - optind + 1, argv + optind - 1);
	}

	if (opt.ots_mode == OTS_MODE_ATTACH)
		return cmd_ots_attach(opt.archive, opt.ots_path, opt.overwrite);
	if (opt.ots_mode == OTS_MODE_EXTRACT) {
		const char *out = optind < argc ? argv[optind] : NULL;
		if (!out)
			errx(EXIT_FAILURE, "--ots-extract requires an output path");
		return cmd_ots_extract(opt.archive, out);
	}
	if (opt.ots_mode == OTS_MODE_INFO)
		return cmd_ots_info(opt.archive);

	if (opt.create) {
		if (optind == argc)
			errx(EXIT_FAILURE, "No files to add");
		return create_archive(argc - optind, argv + optind);
	}
	if (opt.pipe && !opt.test && !opt.list)
		binary_stream(stdout);

	char open_error[256];
	FILE *f = uc2_open_path(opt.archive, open_error, sizeof open_error);
	if (!f) errx(EXIT_FAILURE, "%s: %s", opt.archive, open_error);

	uc2_handle uc2 = uc2_open(&io, f);

	list_init(&files);
	list_init(&dirs);

	for (;;) {
		struct node *ne = malloc(sizeof *ne);
		if (!ne) err(EXIT_FAILURE, 0);

		int ret = uc2_read_cdir(uc2, &ne->entry);
		if (ret < 0 || ret == UC2_End) {
			free(ne);
			if (ret == UC2_End)
				break;
			uc2err(uc2, ret, 0);
			uc2_close(uc2);
			fclose(f);
			return EXIT_FAILURE;
		}

		while (ret == UC2_TaggedEntry) {
			char *tag;
			void *data;
			unsigned size;
			ret = uc2_get_tag(uc2, &ne->entry, &tag, &data, &size);
			if (ret < 0) {
				uc2err(uc2, ret, 0);
				free(ne);
				uc2_close(uc2);
				fclose(f);
				return EXIT_FAILURE;
			}
		}

		new_entry(ne);
	}

	char label[12];
	finish_entries();
	int ret = uc2_finish_cdir(uc2, label);
	if (ret < 0) {
		uc2err(uc2, ret, 0);
		uc2_close(uc2);
		fclose(f);
		return EXIT_FAILURE;
	}

	if (optind == argc) {
		mark(&root, true);
	} else do {
		match_pattern(argv[optind++]);
	} while (optind < argc);

	if (opt.list) {
		unsigned max = 0;
		int size_w = 0;
		if (opt.sep == ' ') {
			visit_selected(&root, max_size_cb, &max);
			size_w = snprintf(0, 0, "%u", max);
		}
		visit_selected(&root, print_entry_cb, &size_w);
		if (opt.sep == ' ') {
			if (*label)
				printf("Label: %s\n", label);
		}
	}

	if (opt.pipe || opt.test) {
		if (opt.test)
			uc2_say(stderr, "Testing archive integrity...\n");
		visit_selected(&root, pipe_cb, uc2);
		flush_stdout();
		if (opt.test) {
			if (verify_trailer_if_present(opt.archive)) {
				uc2_close(uc2);
				fclose(f);
				return EXIT_FAILURE;
			}
			if (!had_error) uc2_say(stderr, "Everything went OK\n");
		}
	} else if (!opt.list) {
		struct path path = {.uc2 = uc2};
		char *p = path.buffer;
		if (opt.dest) {
			unsigned n = strlen(opt.dest);
			assert(n);
			if (opt.dest[n-1] == '/')
				n--;
			if (n >= sizeof path.buffer)
				errx(EXIT_FAILURE, "Destination too long");
			memcpy(p, opt.dest, n);
			p += n;
			*p++ = '/';
		}
		path.ptr = p;
		visit_selected(&root, extract_cb, &path);
	}

	if (!opt.list && !opt.test && !opt.pipe)
		uc2_say(stderr, "Decompression complete\n");
	uc2_close(uc2);
	fclose(f);
	flush_stdout();
	return had_error ? EXIT_FAILURE : EXIT_SUCCESS;
}
