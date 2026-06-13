/* Round-trip verification of the libarchive UC2 read plugin.
 *
 * Usage: test_libarchive_uc2 <archive.uc2> <originals-dir>
 *
 * Opens the archive through libarchive's public API with the UC2
 * format registered, walks every entry, extracts the data, and
 * compares it byte-for-byte against <originals-dir>/<entry-name>.
 * Exit 0 only if every file entry matches.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <archive.h>
#include <archive_entry.h>

extern int archive_read_support_format_uc2(struct archive *);

static unsigned char *slurp(const char *path, size_t *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "FAIL: cannot open original %s\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0) {
		fprintf(stderr, "FAIL: ftell %s\n", path);
		exit(1);
	}
	unsigned char *buf = malloc(n > 0 ? (size_t)n : 1);
	if (!buf) {
		fprintf(stderr, "FAIL: malloc\n");
		exit(1);
	}
	*out_len = fread(buf, 1, (size_t)n, f);
	fclose(f);
	return buf;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: %s <archive.uc2> <originals-dir>\n",
		        argv[0]);
		return 2;
	}

	struct archive *a = archive_read_new();
	if (!a) return 2;
	if (archive_read_support_format_uc2(a) != ARCHIVE_OK) {
		fprintf(stderr, "FAIL: cannot register UC2 format: %s\n",
		        archive_error_string(a));
		return 1;
	}
	if (archive_read_open_filename(a, argv[1], 65536) != ARCHIVE_OK) {
		fprintf(stderr, "FAIL: open %s: %s\n", argv[1],
		        archive_error_string(a));
		return 1;
	}

	int nfiles = 0, ndirs = 0, bad = 0;
	struct archive_entry *e;
	int r;
	while ((r = archive_read_next_header(a, &e)) == ARCHIVE_OK) {
		const char *name = archive_entry_pathname(e);
		if (archive_entry_filetype(e) == AE_IFDIR) {
			ndirs++;
			continue;
		}
		la_int64_t want = archive_entry_size(e);

		size_t cap = want > 0 ? (size_t)want : 1;
		unsigned char *got = malloc(cap);
		if (!got) {
			fprintf(stderr, "FAIL: malloc\n");
			return 1;
		}
		size_t got_len = 0;
		for (;;) {
			la_ssize_t n = archive_read_data(a, got + got_len,
			                                 cap - got_len);
			if (n < 0) {
				fprintf(stderr, "FAIL: read_data %s: %s\n",
				        name, archive_error_string(a));
				return 1;
			}
			if (n == 0)
				break;
			got_len += (size_t)n;
			if (got_len == cap)
				break;
		}

		if ((la_int64_t)got_len != want) {
			fprintf(stderr, "BAD: %s: size %zu, header said %lld\n",
			        name, got_len, (long long)want);
			bad++;
			free(got);
			nfiles++;
			continue;
		}

		char opath[4096];
		snprintf(opath, sizeof opath, "%s/%s", argv[2], name);
		size_t ref_len;
		unsigned char *ref = slurp(opath, &ref_len);
		if (ref_len != got_len || memcmp(ref, got, got_len) != 0) {
			fprintf(stderr, "BAD: %s: content mismatch "
			        "(%zu vs %zu bytes)\n", name, got_len, ref_len);
			bad++;
		}
		free(ref);
		free(got);
		nfiles++;
	}
	if (r != ARCHIVE_EOF) {
		fprintf(stderr, "FAIL: next_header: %s\n",
		        archive_error_string(a));
		return 1;
	}
	archive_read_free(a);

	printf("libarchive round-trip: %d files (%d dirs), %d bad\n",
	       nfiles, ndirs, bad);
	if (nfiles == 0) {
		fprintf(stderr, "FAIL: no file entries found\n");
		return 1;
	}
	return bad ? 1 : 0;
}
