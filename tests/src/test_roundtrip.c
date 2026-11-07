/* Round-trip test: compress with uc2_compress, wrap in a minimal UC2 archive,
   decompress with the library's existing decompressor, verify byte identity.

   The archive format requires a compressed central directory, so we use
   uc2_compress for both the file data and the cdir. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>

static int failures;

#include "fixture_archive.h"

static void test_roundtrip_master(const char *name, const unsigned char *input,
                                  unsigned input_len, int level,
                                  const void *master, unsigned master_size,
                                  unsigned master_id)
{
	printf("  %s (level %d, %u bytes, master=%u): ", name, level, input_len, master_id);

	/* Compress file data */
	struct membuf file_compressed;
	unsigned short file_csum = 0;
	int ret = compress_data(input, input_len, level, master, master_size,
	                        &file_compressed, &file_csum);
	if (ret < 0) {
		printf("FAIL (compress returned %d)\n", ret);
		failures++;
		membuf_free(&file_compressed);
		return;
	}
	printf("compressed %u -> %u, ", input_len, file_compressed.len);

	/* Build archive */
	struct membuf archive;
	ret = build_archive(file_compressed.data, file_compressed.len,
	                    input_len, file_csum, master_id, level, &archive);
	membuf_free(&file_compressed);
	if (ret < 0) {
		printf("FAIL (build_archive returned %d)\n", ret);
		failures++;
		membuf_free(&archive);
		return;
	}

	/* Open archive with the library */
	struct uc2_io io = {
		.read = archive_read,
		.alloc = my_alloc,
		.free = my_free
	};
	uc2_handle uc2 = uc2_open(&io, &archive);
	if (!uc2) {
		printf("FAIL (uc2_open)\n");
		failures++;
		membuf_free(&archive);
		return;
	}

	/* Read cdir */
	struct uc2_entry entry;
	ret = uc2_read_cdir(uc2, &entry);
	if (ret < 0) {
		printf("FAIL (read_cdir: %s)\n", uc2_message(uc2, ret));
		failures++;
		uc2_close(uc2);
		membuf_free(&archive);
		return;
	}

	/* Skip tags */
	while (ret == UC2_TaggedEntry) {
		char *tag; void *data; unsigned size;
		ret = uc2_get_tag(uc2, &entry, &tag, &data, &size);
		if (ret < 0) break;
	}

	/* Read to end */
	struct uc2_entry dummy;
	ret = uc2_read_cdir(uc2, &dummy);
	if (ret != UC2_End) {
		printf("FAIL (expected UC2_End, got %d)\n", ret);
		failures++;
		uc2_close(uc2);
		membuf_free(&archive);
		return;
	}

	char label[12];
	uc2_finish_cdir(uc2, label);

	/* Extract */
	struct membuf output;
	membuf_init(&output);
	ret = uc2_extract(uc2, &entry.xi, entry.size, membuf_write, &output);
	uc2_close(uc2);
	membuf_free(&archive);

	if (ret < 0) {
		printf("FAIL (extract: error %d)\n", ret);
		failures++;
		membuf_free(&output);
		return;
	}

	/* Verify */
	if (output.len != input_len) {
		printf("FAIL (size: got %u, expected %u)\n", output.len, input_len);
		failures++;
	} else if (input_len > 0 && memcmp(output.data, input, input_len) != 0) {
		unsigned diff = 0;
		while (diff < input_len && output.data[diff] == input[diff])
			diff++;
		printf("FAIL (mismatch at byte %u: got 0x%02x, expected 0x%02x)\n",
		       diff, output.data[diff], input[diff]);
		failures++;
	} else {
		printf("OK\n");
	}

	membuf_free(&output);
}

static void test_roundtrip(const char *name, const unsigned char *input,
                           unsigned input_len, int level)
{
	test_roundtrip_master(name, input, input_len, level, NULL, 0, 1);
}

/* Test data generators */

static unsigned char *gen_zeros(unsigned len)
{
	return calloc(1, len ? len : 1);
}

static unsigned char *gen_random(unsigned len, unsigned seed)
{
	unsigned char *data = malloc(len ? len : 1);
	unsigned s = seed;
	for (unsigned i = 0; i < len; i++) {
		s = s * 1103515245 + 12345;
		data[i] = (unsigned char)(s >> 16);
	}
	return data;
}

static unsigned char *gen_text(unsigned *out_len)
{
	const char *text =
		"The quick brown fox jumps over the lazy dog. "
		"Pack my box with five dozen liquor jugs. "
		"How vexingly quick daft zebras jump. ";
	unsigned base = (unsigned)strlen(text);
	unsigned len = base * 15;
	unsigned char *data = malloc(len);
	for (unsigned i = 0; i < len; i++)
		data[i] = (unsigned char)text[i % base];
	*out_len = len;
	return data;
}

int main(void)
{
	int levels[] = {2, 3, 4, 5, 6, 7, 8, 9};
	int nlev = (int)(sizeof levels / sizeof *levels);

	for (int li = 0; li < nlev; li++) {
		int level = levels[li];
		printf("Level %d:\n", level);

		/* A literal-only encoder used to pass every round-trip test.  Require
		   actual LZ matching, including repeated circular-window generations. */
		{
			unsigned length = 262147;
			unsigned char *pattern = gen_random(length, 712);
			for (unsigned i = 4096; i < length; i++) pattern[i] = pattern[i % 4096];
			struct membuf compressed;
			unsigned short checksum;
			int rc = compress_data(pattern, length, level, NULL, 0, &compressed, &checksum);
			if (rc < 0 || compressed.len > length / 8) {
				fprintf(stderr, "LZ regression at level %d: %u bytes\n", level, compressed.len);
				failures++;
			}
			membuf_free(&compressed);
			test_roundtrip("repeated_random_256k", pattern, length, level);
			free(pattern);
		}

		/* Empty */
		{
			unsigned char empty = 0;
			test_roundtrip("empty", &empty, 0, level);
		}

		/* Single byte */
		{
			unsigned char one = 'A';
			test_roundtrip("single_byte", &one, 1, level);
		}

		/* Short string */
		{
			const unsigned char *hi = (const unsigned char *)"Hi";
			test_roundtrip("short_2", hi, 2, level);
		}

		/* All zeros 1K */
		{
			unsigned char *z = gen_zeros(1024);
			test_roundtrip("zeros_1k", z, 1024, level);
			free(z);
		}

		/* All zeros 64K */
		{
			unsigned char *z = gen_zeros(65536);
			test_roundtrip("zeros_64k", z, 65536, level);
			free(z);
		}

		/* Random data (incompressible) */
		{
			unsigned char *r = gen_random(1024, 42);
			test_roundtrip("random_1k", r, 1024, level);
			free(r);
		}

		/* Repeated text */
		{
			unsigned tlen;
			unsigned char *t = gen_text(&tlen);
			test_roundtrip("text_2k", t, tlen, level);
			free(t);
		}

		/* Pattern */
		{
			unsigned len = 4096;
			unsigned char *d = malloc(len);
			for (unsigned i = 0; i < len; i++)
				d[i] = (unsigned char)(i % 17);
			test_roundtrip("pattern_4k", d, len, level);
			free(d);
		}

		printf("\n");
	}

	/* SuperMaster round-trip tests (level 4 only for speed) */
	printf("SuperMaster:\n");
	{
		unsigned char *sm = malloc(49152);
		if (!sm) { fprintf(stderr, "malloc failed\n"); return EXIT_FAILURE; }
		int sm_ret = uc2_get_supermaster(sm, 49152);
		if (sm_ret < 0) {
			fprintf(stderr, "uc2_get_supermaster failed: %d\n", sm_ret);
			free(sm);
			return EXIT_FAILURE;
		}

		unsigned char empty = 0;
		test_roundtrip_master("sm_empty", &empty, 0, 4, sm, 49152, 0);

		unsigned char one = 'A';
		test_roundtrip_master("sm_single", &one, 1, 4, sm, 49152, 0);

		unsigned char *z = gen_zeros(4096);
		test_roundtrip_master("sm_zeros_4k", z, 4096, 4, sm, 49152, 0);
		free(z);

		unsigned char *r = gen_random(1024, 42);
		test_roundtrip_master("sm_random_1k", r, 1024, 4, sm, 49152, 0);
		free(r);

		unsigned tlen;
		unsigned char *t = gen_text(&tlen);
		test_roundtrip_master("sm_text", t, tlen, 4, sm, 49152, 0);
		free(t);

		free(sm);
	}
	printf("\n");

	if (failures) {
		fprintf(stderr, "%d test(s) FAILED\n", failures);
		return EXIT_FAILURE;
	}
	printf("test_roundtrip: all tests passed\n");
	return EXIT_SUCCESS;
}
