/* Integration test: extract reference UC2 archives and verify against corpus.
   Usage: test_extract <archives_dir> <corpus_dir> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>

static int failures;

struct membuf {
	unsigned char *data;
	unsigned len;
	unsigned cap;
};

static int write_membuf(void *ctx, const void *ptr, unsigned len)
{
	struct membuf *buf = ctx;
	if (buf->len + len > buf->cap) {
		unsigned newcap = buf->cap ? buf->cap * 2 : 4096;
		while (newcap < buf->len + len)
			newcap *= 2;
		unsigned char *p = realloc(buf->data, newcap);
		if (!p) return -1;
		buf->data = p;
		buf->cap = newcap;
	}
	memcpy(buf->data + buf->len, ptr, len);
	buf->len += len;
	return 0;
}

static int my_read(void *ctx, unsigned pos, void *ptr, unsigned len)
{
	if (fseek(ctx, pos, SEEK_SET) < 0) return -1;
	return (int)fread(ptr, 1, len, ctx);
}

static void *my_alloc(void *ctx, unsigned size) { (void)ctx; return malloc(size); }
static void my_free(void *ctx, void *ptr) { (void)ctx; free(ptr); }

static struct uc2_io io = {
	.read = my_read, .alloc = my_alloc, .free = my_free
};

static unsigned char *load_file(const char *path, unsigned *out_len)
{
	FILE *f = fopen(path, "rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (sz < 0) { fclose(f); return NULL; }
	unsigned char *data = malloc(sz ? sz : 1);
	if (!data) { fclose(f); return NULL; }
	if (sz > 0 && fread(data, 1, sz, f) != (size_t)sz) {
		free(data); fclose(f); return NULL;
	}
	fclose(f);
	*out_len = (unsigned)sz;
	return data;
}

/* Store file entries for post-cdir extraction */
struct file_entry {
	struct uc2_entry entry;
	struct file_entry *next;
};

static void dos_name_to_lower(const char *name, char *out, unsigned out_size)
{
	unsigned i;
	for (i = 0; i < out_size - 1 && name[i]; i++)
		out[i] = (name[i] >= 'A' && name[i] <= 'Z')
		        ? name[i] + 32 : name[i];
	out[i] = '\0';
}

static int test_archive(const char *archive_path, const char *corpus_dir)
{
	FILE *f = fopen(archive_path, "rb");
	if (!f) {
		fprintf(stderr, "FAIL: cannot open %s\n", archive_path);
		failures++;
		return 1;
	}

	uc2_handle uc2 = uc2_open(&io, f);
	struct file_entry *file_list = NULL;
	int file_count = 0;

	/* Phase 1: read all cdir entries */
	for (;;) {
		struct uc2_entry entry;
		int ret = uc2_read_cdir(uc2, &entry);
		if (ret == UC2_End) break;
		if (ret < 0) {
			fprintf(stderr, "FAIL: read_cdir %s: %s\n",
			        archive_path, uc2_message(uc2, ret));
			failures++;
			uc2_close(uc2);
			fclose(f);
			return 1;
		}
		while (ret == UC2_TaggedEntry) {
			char *tag; void *data; unsigned size;
			ret = uc2_get_tag(uc2, &entry, &tag, &data, &size);
			if (ret < 0) break;
		}
		if (entry.is_dir) continue;

		struct file_entry *fe = malloc(sizeof *fe);
		fe->entry = entry;
		fe->next = file_list;
		file_list = fe;
		file_count++;
	}

	/* Phase 2: finish cdir (required before extraction) */
	char label[12];
	uc2_finish_cdir(uc2, label);

	/* Phase 3: extract and verify each file */
	for (struct file_entry *fe = file_list; fe; fe = fe->next) {
		struct uc2_entry *e = &fe->entry;
		char corpus_name[300];
		dos_name_to_lower(e->name, corpus_name, sizeof corpus_name);

		/* Extract to memory */
		struct membuf buf = {0};
		int ret = uc2_extract(uc2, &e->xi, e->size, write_membuf, &buf);
		if (ret < 0) {
			fprintf(stderr, "FAIL: extract %s: %s\n",
			        corpus_name, uc2_message(uc2, ret));
			failures++;
			free(buf.data);
			continue;
		}

		/* Load expected corpus file */
		char path[1024];
		snprintf(path, sizeof path, "%s/%s", corpus_dir, corpus_name);
		unsigned expected_len;
		unsigned char *expected = load_file(path, &expected_len);
		if (!expected) {
			fprintf(stderr, "FAIL: cannot load corpus %s\n", path);
			failures++;
			free(buf.data);
			continue;
		}

		if (buf.len != expected_len) {
			fprintf(stderr, "FAIL: %s: size mismatch (got %u, expected %u)\n",
			        corpus_name, buf.len, expected_len);
			failures++;
		} else if (expected_len > 0 && memcmp(buf.data, expected, expected_len) != 0) {
			fprintf(stderr, "FAIL: %s: content mismatch\n", corpus_name);
			failures++;
		} else {
			printf("  OK: %s (%u bytes)\n", corpus_name, expected_len);
		}

		free(buf.data);
		free(expected);
	}

	/* Cleanup */
	while (file_list) {
		struct file_entry *next = file_list->next;
		free(file_list);
		file_list = next;
	}

	uc2_close(uc2);
	fclose(f);

	if (file_count == 0) {
		fprintf(stderr, "FAIL: %s: no files found\n", archive_path);
		failures++;
		return 1;
	}
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		fprintf(stderr, "usage: test_extract <archives_dir> <corpus_dir>\n");
		return EXIT_FAILURE;
	}

	const char *archives = argv[1];
	const char *corpus = argv[2];

	const char *test_archives[] = {
		"basic.uc2", "single.uc2", "empty.uc2", "zeros.uc2", "random.uc2"
	};

	for (unsigned i = 0; i < sizeof test_archives / sizeof *test_archives; i++) {
		char path[1024];
		snprintf(path, sizeof path, "%s/%s", archives, test_archives[i]);
		printf("Testing %s:\n", test_archives[i]);
		test_archive(path, corpus);
	}

	if (failures) {
		fprintf(stderr, "\n%d test(s) FAILED\n", failures);
		return EXIT_FAILURE;
	}
	printf("\ntest_extract: all tests passed\n");
	return EXIT_SUCCESS;
}
