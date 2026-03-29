/* UltraCompressor II extraction tool.
   Copyright © Jan Bobrowski 2020, 2021
   torinak.com/~jb/unuc2/

   This program is free software: you can redistribute it and modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
*/

#include <limits.h>
#include <ctype.h>
#include <stdlib.h>
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

#include <uc2/libuc2.h>
#include <uc2/uc2_version.h>

#include "list.h"
#define endof(T) (T + sizeof T/sizeof*T)
#define STR(S) STR_(S)
#define STR_(S) #S

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
	char sep;
	int level;
	char *archive;
	char *dest;
} opt = {.sep = ' ', .level = 4};

static int my_read(void *ctx, unsigned pos, void *ptr, unsigned len)
{
	if (fseek(ctx, pos, SEEK_SET) < 0)
		err(EXIT_FAILURE, "fseek");
	return fread(ptr, 1, len, ctx);
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

static void uc2err(uc2_handle uc2, int err, char *f, ...)
{
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
	int version;
	bool visit:1;
	bool marked:1;
	struct uc2_entry entry;
};

struct node root = {
	.entry = {.is_dir = 1},
	.children = {.prev = &root.children, .next = &root.children}
};

static void new_entry(struct node *ne)
{
	struct uc2_entry *e = &ne->entry;
	struct node *dir = &root;
	if (e->dirid) {
		for (struct list *l = dirs.next;; l = l->next) {
			if (l == &dirs) {
				warnx("Missing dir of %s\n", e->name);
				dir = &root;
				break;
			}
			dir = list_item(l, struct node, by_type);
			if (dir->entry.id == ne->entry.dirid)
				break;
		}
	}
	ne->parent = dir;
	struct list *at = 0;
	if (!e->is_dir) {
		for (struct list *l = dir->children.next; l != &dir->children; l = l->next) {
			struct node *fe = list_item(l, struct node, on_dir);
			if (fe->entry.name_len == ne->entry.name_len
			 && memcmp(fe->entry.name, ne->entry.name, ne->entry.name_len) == 0) {
				fe->version++;
				if (!at) at = &fe->on_dir;
			}
		}
	}
	list_append(at ? at : &dir->children, &ne->on_dir);
	list_append(e->is_dir ? &dirs : &files, &ne->by_type);
	list_init(&ne->children);
	list_init(&ne->on_sel);
	ne->version = 0;
	ne->visit = false;
	ne->marked = false;
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
	if (node->marked)
		return;
	node->marked = true;
	if (visit) {
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
			list_del(&dir->on_sel);
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
					list_del(&ne->on_sel);
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
		if (!ne->marked)
			continue;
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
		(void)chmod(path, 0444);
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
			printf("Testing %s %u bytes\n", e->name, e->size);
		int ret = uc2_extract(uc2, &e->xi, e->size, write_file, opt.test ? 0 : stdout);
		if (ret < 0)
			uc2err(uc2, ret, "%s", e->name);
	}
	return true;
}

static bool extract_cb(struct node *ne, void *ctx, enum cause cause)
{
	struct path *path = ctx;
	struct uc2_entry *e = &ne->entry;
	unsigned l = e->name_len;

	switch (cause) {
	case VisitFile:
	case EnterDir:;
		char *p = path->ptr + l;
		if (p + 1 >= endof(path->buffer))
			errx(EXIT_FAILURE, "Path too long");
		memcpy(path->ptr, e->name, l);

		if (cause == VisitFile) {
			*p = 0;
			if (!opt.overwrite) {
				if (access(path->buffer, F_OK) == 0) {
					errno = EEXIST;
					warn("%s", path->buffer);
					break;
				}
			} else
				(void) unlink(path->buffer);

			FILE *f = fopen(path->buffer, "wb");
			if (!f)
				err(EXIT_FAILURE, "%s", path->buffer);
			int ret = uc2_extract(path->uc2, &e->xi, e->size, write_file, f);
			if (ret < 0)
				uc2err(path->uc2, ret, "%s", e->name);
			fclose(f);
			if (!opt.no_file_meta)
				set_attrs(path->buffer, ne);
			break;
		}
		*p++ = '/';
		if (p == endof(path->buffer))
			errx(EXIT_FAILURE, "Path too long");
		path->ptr = p;
		*p = 0;
		int r = mkdir(path->buffer, 0777);
		if (r < 0) {
			if (errno != EEXIST)
				err(EXIT_FAILURE, "mkdir %s", path->buffer);
			ne->marked = false; // skip meta setting
		}
		break;

	case LeaveDir:
		assert(ne->entry.is_dir);
		if (!opt.no_dir_meta) {
			*path->ptr = 0;
			set_attrs(path->buffer, ne);
		}
		path->ptr -= l + 1;
	}
	return true;
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
	if (!tm || tm->tm_year < 80) return 0;
	return ((unsigned)(tm->tm_year - 80) << 25) |
	       ((unsigned)(tm->tm_mon + 1) << 21) |
	       ((unsigned)tm->tm_mday << 16) |
	       ((unsigned)tm->tm_hour << 11) |
	       ((unsigned)tm->tm_min << 5) |
	       ((unsigned)(tm->tm_sec / 2));
}

static unsigned fnv1a(const unsigned char *data, unsigned len)
{
	unsigned h = 2166136261u;
	for (unsigned i = 0; i < len; i++) {
		h ^= data[i];
		h *= 16777619u;
	}
	return h;
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
	return (int)fread(buf, 1, len, (FILE *)ctx);
}

static int fwrite_cb(void *ctx, const void *ptr, unsigned len)
{
	return fwrite(ptr, 1, len, (FILE *)ctx) == len ? 0 : -1;
}

struct dir_rec {
	char path[PATH_MAX];
	char name[300];
	unsigned char dos_name[11];
	unsigned id;       /* unique dir ID (starts from 1) */
	unsigned parent_id; /* 0 = root */
	unsigned dos_time;
};

struct file_rec {
	char path[PATH_MAX];
	char name[300];
	unsigned char dos_name[11];
	unsigned size;
	unsigned csize;
	unsigned short csum;
	unsigned offset;
	unsigned dos_time;
	unsigned parent_id; /* 0 = root, or dir_rec.id */
	int master_idx;     /* 0=SuperMaster, >=2=custom master */
};

struct master_rec {
	unsigned idx;
	unsigned char *data;
	unsigned size;
	unsigned csize;
	unsigned short csum;
	unsigned offset;
	unsigned key;
	unsigned ref_len;
	unsigned ref_ctr;
};

/* Growable arrays for scanning input paths */
static struct dir_rec *g_dirs;
static int g_ndirs, g_dir_cap;
static struct file_rec *g_files;
static int g_nfiles, g_file_cap;
static unsigned g_next_dirid = 1;

static void scan_path(const char *path, unsigned parent_id);

static void add_dir(const char *path, const char *name, unsigned parent_id, unsigned dos_time)
{
	if (g_ndirs >= g_dir_cap) {
		g_dir_cap = g_dir_cap ? g_dir_cap * 2 : 32;
		g_dirs = realloc(g_dirs, (unsigned)g_dir_cap * sizeof *g_dirs);
		if (!g_dirs) err(EXIT_FAILURE, "realloc");
	}
	struct dir_rec *d = &g_dirs[g_ndirs++];
	memset(d, 0, sizeof *d);
	snprintf(d->path, sizeof d->path, "%s", path);
	snprintf(d->name, sizeof d->name, "%s", name);
	make_dos_name(d->dos_name, name);
	d->id = g_next_dirid++;
	d->parent_id = parent_id;
	d->dos_time = dos_time;
}

static void add_file(const char *path, const char *name, unsigned parent_id,
                     unsigned size, unsigned dos_time)
{
	if (g_nfiles >= g_file_cap) {
		g_file_cap = g_file_cap ? g_file_cap * 2 : 64;
		g_files = realloc(g_files, (unsigned)g_file_cap * sizeof *g_files);
		if (!g_files) err(EXIT_FAILURE, "realloc");
	}
	struct file_rec *f = &g_files[g_nfiles++];
	memset(f, 0, sizeof *f);
	snprintf(f->path, sizeof f->path, "%s", path);
	snprintf(f->name, sizeof f->name, "%s", name);
	make_dos_name(f->dos_name, name);
	f->size = size;
	f->dos_time = dos_time;
	f->parent_id = parent_id;
}

static void scan_dir(const char *dirpath, unsigned dirid)
{
	DIR *d = opendir(dirpath);
	if (!d) err(EXIT_FAILURE, "%s", dirpath);
	struct dirent *de;
	while ((de = readdir(d))) {
		if (de->d_name[0] == '.' &&
		    (de->d_name[1] == '\0' ||
		     (de->d_name[1] == '.' && de->d_name[2] == '\0')))
			continue;
		char child[PATH_MAX];
		snprintf(child, sizeof child, "%s/%s", dirpath, de->d_name);
		scan_path(child, dirid);
	}
	closedir(d);
}

static void scan_path(const char *path, unsigned parent_id)
{
	struct stat st;
	if (stat(path, &st) < 0)
		err(EXIT_FAILURE, "%s", path);

	const char *base = strrchr(path, '/');
	base = base ? base + 1 : path;

	if (S_ISDIR(st.st_mode)) {
		add_dir(path, base, parent_id, to_dos_time(st.st_mtime));
		unsigned dirid = g_dirs[g_ndirs - 1].id;
		scan_dir(path, dirid);
	} else if (S_ISREG(st.st_mode)) {
		add_file(path, base, parent_id, (unsigned)st.st_size,
		         to_dos_time(st.st_mtime));
	} else {
		warnx("%s: skipping (not a regular file or directory)", path);
	}
}

static int create_archive(int nargs, char **args)
{
	/* Phase 0: Scan inputs (files and directories) */
	g_dirs = NULL; g_ndirs = 0; g_dir_cap = 0;
	g_files = NULL; g_nfiles = 0; g_file_cap = 0;
	g_next_dirid = 1;

	for (int i = 0; i < nargs; i++)
		scan_path(args[i], 0);

	int nfiles = g_nfiles, ndirs = g_ndirs;
	struct file_rec *recs = g_files;
	struct dir_rec *dirs = g_dirs;
	g_files = NULL; g_dirs = NULL;

	if (nfiles == 0)
		errx(EXIT_FAILURE, "No files to compress");

	/* Load the SuperMaster (49152-byte built-in dictionary) */
	unsigned char *supermaster = malloc(49152);
	if (!supermaster)
		err(EXIT_FAILURE, "malloc");
	int sm_ret = uc2_get_supermaster(supermaster, 49152);
	if (sm_ret < 0)
		errx(EXIT_FAILURE, "Failed to load SuperMaster (%d)", sm_ret);

	/* Phase 1: Fingerprint files for master-block grouping */
	enum { SampleSize = 4096, MinMasterFile = 1024, MaxMasterSize = 65535 };
	unsigned *fps = calloc(nfiles, sizeof *fps);
	bool *grouped = calloc(nfiles, sizeof *grouped);
	if (!fps || !grouped)
		err(EXIT_FAILURE, "malloc");

	for (int i = 0; i < nfiles; i++) {
		if (recs[i].size < MinMasterFile)
			continue;
		unsigned char sample[SampleSize];
		FILE *f = fopen(recs[i].path, "rb");
		if (!f) err(EXIT_FAILURE, "%s", recs[i].path);
		unsigned n = (unsigned)fread(sample, 1, SampleSize, f);
		fclose(f);
		fps[i] = fnv1a(sample, n);
	}

	int nmasters = 0, master_cap = 16;
	struct master_rec *masters = calloc(master_cap, sizeof *masters);
	if (!masters)
		err(EXIT_FAILURE, "malloc");

	for (int i = 0; i < nfiles; i++) {
		if (grouped[i] || recs[i].size < MinMasterFile)
			continue;

		int count = 0, largest = i;
		for (int j = i; j < nfiles; j++) {
			if (recs[j].size >= MinMasterFile && fps[j] == fps[i]) {
				count++;
				if (recs[j].size > recs[largest].size)
					largest = j;
			}
		}
		if (count < 2)
			continue;

		unsigned midx = 2 + (unsigned)nmasters;
		unsigned msz = recs[largest].size;
		if (msz > MaxMasterSize) msz = MaxMasterSize;
		unsigned char *mdata = malloc(msz);
		if (!mdata) err(EXIT_FAILURE, "malloc");
		FILE *mf = fopen(recs[largest].path, "rb");
		if (!mf) err(EXIT_FAILURE, "%s", recs[largest].path);
		msz = (unsigned)fread(mdata, 1, msz, mf);
		fclose(mf);

		unsigned ref_len = 0, ref_ctr = 0;
		for (int j = i; j < nfiles; j++) {
			if (recs[j].size >= MinMasterFile && fps[j] == fps[i]) {
				recs[j].master_idx = (int)midx;
				grouped[j] = true;
				ref_len += recs[j].size;
				ref_ctr++;
			}
		}

		if (nmasters >= master_cap) {
			master_cap *= 2;
			masters = realloc(masters, (unsigned)master_cap * sizeof *masters);
			if (!masters) err(EXIT_FAILURE, "realloc");
		}
		masters[nmasters] = (struct master_rec){
			.idx = midx, .data = mdata, .size = msz,
			.key = fnv1a(mdata, msz),
			.ref_len = ref_len, .ref_ctr = ref_ctr
		};
		nmasters++;
	}
	free(fps);
	free(grouped);

	FILE *out = fopen(opt.archive, "wb");
	if (!out)
		err(EXIT_FAILURE, "%s", opt.archive);

	/* Placeholder FHEAD + XHEAD (29 bytes) */
	unsigned char header[29];
	memset(header, 0, sizeof header);
	fwrite(header, 1, 29, out);

	/* Write master blocks (compressed with SuperMaster) */
	for (int i = 0; i < nmasters; i++) {
		masters[i].offset = (unsigned)ftell(out);
		struct mem_reader mr = {.data = masters[i].data, .pos = 0, .len = masters[i].size};
		unsigned csize = 0;
		unsigned short csum = 0;
		int ret = uc2_compress_ex(opt.level, supermaster, 49152,
		                          mem_read_cb, &mr, fwrite_cb, out,
		                          masters[i].size, &csum, &csize);
		if (ret < 0)
			errx(EXIT_FAILURE, "master %u: compression error %d", masters[i].idx, ret);
		masters[i].csize = csize;
		masters[i].csum = csum;
		fprintf(stderr, "  master[%u]: %u -> %u (%u files)\n",
		        masters[i].idx, masters[i].size, csize, masters[i].ref_ctr);
	}

	/* Phase 2: Compress each file */
	for (int i = 0; i < nfiles; i++) {
		recs[i].offset = (unsigned)ftell(out);

		FILE *inf = fopen(recs[i].path, "rb");
		if (!inf)
			err(EXIT_FAILURE, "%s", recs[i].path);

		const unsigned char *mdata = supermaster;
		unsigned msz = 49152;
		if (recs[i].master_idx >= 2) {
			for (int m = 0; m < nmasters; m++) {
				if (masters[m].idx == (unsigned)recs[i].master_idx) {
					mdata = masters[m].data;
					msz = masters[m].size;
					break;
				}
			}
		}

		unsigned csize = 0;
		unsigned short csum = 0;
		int ret = uc2_compress_ex(opt.level, mdata, msz,
		                          fread_cb, inf, fwrite_cb, out,
		                          recs[i].size, &csum, &csize);
		fclose(inf);
		if (ret < 0)
			errx(EXIT_FAILURE, "%s: compression error %d", recs[i].path, ret);

		recs[i].csize = csize;
		recs[i].csum = csum;
		fprintf(stderr, "  %s: %u -> %u%s\n", recs[i].name, recs[i].size, csize,
		        recs[i].master_idx >= 2 ? " (custom master)" : "");
	}

	/* Phase 3: Build raw central directory.
	   Order: masters, then dirs (parent before child), then files.
	   The scanner guarantees dirs are ordered parent-first. */
	unsigned cdir_cap = 22; /* EndOfCdir + XTAIL + aserial */
	for (int i = 0; i < nmasters; i++)
		cdir_cap += 39;
	for (int i = 0; i < ndirs; i++)
		cdir_cap += 27 + 21 + (unsigned)strlen(dirs[i].name) + 1;
	for (int i = 0; i < nfiles; i++)
		cdir_cap += 47 + 21 + (unsigned)strlen(recs[i].name) + 1;

	unsigned char *raw_cdir = malloc(cdir_cap);
	if (!raw_cdir)
		err(EXIT_FAILURE, "malloc");

	unsigned char *p = raw_cdir;

	/* Master entries */
	for (int i = 0; i < nmasters; i++) {
		*p++ = 3; /* MasterEntry */
		w32(p, masters[i].idx); p += 4;
		w32(p, masters[i].key); p += 4;
		w32(p, masters[i].ref_len); p += 4;
		w32(p, masters[i].ref_ctr); p += 4;
		w16(p, masters[i].size); p += 2;
		w16(p, masters[i].csum); p += 2;
		w32(p, masters[i].csize); p += 4;
		w16(p, 1); p += 2;
		w32(p, 0); p += 4;
		w32(p, 1); p += 4;
		w32(p, masters[i].offset); p += 4;
	}

	/* Directory entries: OHEAD(1) + OSMETA(22) + DIRMETA(4) + EXTMETA */
	for (int i = 0; i < ndirs; i++) {
		unsigned namelen = (unsigned)strlen(dirs[i].name);
		*p++ = 1; /* DirEntry */
		/* OSMETA (22 bytes) */
		w32(p, dirs[i].parent_id); p += 4;
		*p++ = 0x10; /* UC2_Attr_D = directory */
		w32(p, dirs[i].dos_time); p += 4;
		memcpy(p, dirs[i].dos_name, 11); p += 11;
		*p++ = 0; /* hidden */
		*p++ = 1; /* has tags */
		/* DIRMETA (4 bytes) */
		w32(p, dirs[i].id); p += 4;
		/* EXTMETA: long name tag */
		memcpy(p, "AIP:Win95 LongN", 16); p += 16;
		w32(p, namelen + 1); p += 4;
		*p++ = 0;
		memcpy(p, dirs[i].name, namelen + 1); p += namelen + 1;
	}

	/* File entries */
	for (int i = 0; i < nfiles; i++) {
		unsigned namelen = (unsigned)strlen(recs[i].name);
		*p++ = 2; /* FileEntry */
		/* OSMETA (22 bytes) */
		w32(p, recs[i].parent_id); p += 4;
		*p++ = 0x20;
		w32(p, recs[i].dos_time); p += 4;
		memcpy(p, recs[i].dos_name, 11); p += 11;
		*p++ = 0;
		*p++ = 1;
		/* FILEMETA (6 bytes) */
		w32(p, recs[i].size); p += 4;
		w16(p, recs[i].csum); p += 2;
		/* COMPRESS (10 bytes) */
		w32(p, recs[i].csize); p += 4;
		w16(p, 1); p += 2;
		w32(p, (unsigned)recs[i].master_idx); p += 4;
		/* LOCATION (8 bytes) */
		w32(p, 1); p += 4;
		w32(p, recs[i].offset); p += 4;
		/* EXTMETA: long name tag */
		memcpy(p, "AIP:Win95 LongN", 16); p += 16;
		w32(p, namelen + 1); p += 4;
		*p++ = 0;
		memcpy(p, recs[i].name, namelen + 1); p += namelen + 1;
	}

	/* EndOfCdir */
	*p++ = 4;
	*p++ = 0;
	*p++ = 0;
	w32(p, 0); p += 4;
	memset(p, ' ', 11); p += 11;
	w32(p, 0); p += 4;

	unsigned cdir_size = (unsigned)(p - raw_cdir);
	unsigned short cdir_csum = fletcher_csum(raw_cdir, cdir_size);

	unsigned cdir_offset = (unsigned)ftell(out);
	unsigned char crec[10];
	memset(crec, 0, 10);
	fwrite(crec, 1, 10, out);

	struct mem_reader mr = {.data = raw_cdir, .pos = 0, .len = cdir_size};
	unsigned cdir_csize = 0;
	unsigned short cdir_comp_csum = 0;
	int ret = uc2_compress(opt.level, mem_read_cb, &mr, fwrite_cb, out,
	                       cdir_size, &cdir_comp_csum, &cdir_csize);
	free(raw_cdir);
	if (ret < 0)
		errx(EXIT_FAILURE, "cdir compression error %d", ret);

	unsigned total = (unsigned)ftell(out);

	fseek(out, cdir_offset, SEEK_SET);
	w32(crec + 0, 0);                 /* csize=0 matches original UC2 Pro */
	w16(crec + 4, opt.level <= 1 ? 1 : opt.level); /* method = compression level */
	w32(crec + 6, 1);                 /* masterPrefix = NoMaster */
	fwrite(crec, 1, 10, out);

	fseek(out, 0, SEEK_SET);
	unsigned complen = total - 13;
	w32(header + 0, 0x1A324355);
	w32(header + 4, complen);
	w32(header + 8, complen + 0x01B2C3D4);
	header[12] = 0;
	w32(header + 13, 1);
	w32(header + 17, cdir_offset);
	w16(header + 21, cdir_csum);
	header[23] = 0;
	w16(header + 24, 300);
	w16(header + 26, 200);
	header[28] = 0;
	fwrite(header, 1, 29, out);

	fclose(out);
	for (int i = 0; i < nmasters; i++)
		free(masters[i].data);
	free(masters);
	free(supermaster);
	free(dirs);
	free(recs);
	printf("Created %s (%d file%s, %d dir%s, %d master%s, %u bytes)\n",
	       opt.archive, nfiles, nfiles == 1 ? "" : "s",
	       ndirs, ndirs == 1 ? "" : "s",
	       nmasters, nmasters == 1 ? "" : "s", total);
	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
#ifdef __DJGPP__
	setprogname(argv[0]);
#endif
	if (argc == 1)
		goto usage;

	for (;;) {
		int o = getopt(argc, argv, "xlatfd:C:cpDTh?wL:");
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
		case 'L':
			opt.level = atoi(optarg);
			if (opt.level < 2 || opt.level > 5)
				errx(EXIT_FAILURE, "Compression level must be 2..5");
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
			       "Decompression based on unuc2 by Jan Bobrowski\n\n");
usage:
			printf(
				"uc2 [-afpDT] [-d destination] archive.uc2 [files]...\n"
				"uc2 -l [-aT] archive.uc2 [files]...\n"
				"uc2 -t [-a] archive.uc2 [files]...\n"
				"uc2 -w [-L level] archive.uc2 files...\n"
			);
			if (!opt.help)
				printf("uc2 -h\n");
			else
				printf(
					" -l      List\n"
					" -t      Test\n"
					" -w      Create archive\n"
					" -L n    Compression level: 2=Fast 3=Normal 4=Tight(default) 5=Ultra\n"
					" -a      All versions of files\n"
					" -d path Destination to extract to\n"
					" -f      Overwrite\n"
					" -p      To stdout\n"
					" -D      Do not set time and permissions of dirs (also files: -DD)\n"
					" -T      Tab-separated\n"
					"\nhttps://github.com/evvaletov/uc2\n"
				);
			return opt.help ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}

	if (argc == optind)
		errx(EXIT_FAILURE, "Archive not given");
	opt.archive = argv[optind++];

	if (opt.create) {
		if (optind == argc)
			errx(EXIT_FAILURE, "No files to add");
		return create_archive(argc - optind, argv + optind);
	}

	FILE *f = fopen(opt.archive, "rb");
	if (!f) err(EXIT_FAILURE, "%s", opt.archive);

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
			return EXIT_FAILURE;
		}

		while (ret == UC2_TaggedEntry) {
			char *tag;
			void *data;
			unsigned size;
			ret = uc2_get_tag(uc2, &ne->entry, &tag, &data, &size);
			if (ret < 0) {
				uc2err(uc2, ret, 0);
				return EXIT_FAILURE;
			}
		}

		new_entry(ne);
	}

	char label[12];
	uc2_finish_cdir(uc2, label);

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
		visit_selected(&root, pipe_cb, uc2);
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

	uc2_close(uc2);
	return EXIT_SUCCESS;
}
