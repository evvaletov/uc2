/* Round-trip test: compress with uc2_compress, wrap in a minimal UC2 archive,
   decompress with the library's existing decompressor, verify byte identity.

   The archive format requires a compressed central directory, so we use
   uc2_compress for both the file data and the cdir. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>

static int failures;

/* --- Fletcher checksum (must match decompress.c / uc2_internal.h) --- */

static unsigned short fletcher_checksum(const unsigned char *data, unsigned len)
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

/* --- Growable buffer --- */

struct membuf {
	unsigned char *data;
	unsigned len, cap;
};

static void membuf_init(struct membuf *b)
{
	b->data = NULL;
	b->len = b->cap = 0;
}

static void membuf_free(struct membuf *b)
{
	free(b->data);
	membuf_init(b);
}

static int membuf_write(void *ctx, const void *ptr, unsigned len)
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

static void membuf_append(struct membuf *b, const void *data, unsigned len)
{
	membuf_write(b, data, len);
}

/* --- Compressor read callback --- */

struct mem_reader {
	const unsigned char *data;
	unsigned pos, len;
};

static int mem_read(void *ctx, void *buf, unsigned len)
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

/* --- Library I/O callbacks (read from membuf at offset) --- */

static int archive_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct membuf *mb = ctx;
	if (pos >= mb->len) return 0;
	unsigned avail = mb->len - pos;
	if (len > avail) len = avail;
	memcpy(buf, mb->data + pos, len);
	return (int)len;
}

static void *my_alloc(void *ctx, unsigned size) { (void)ctx; return malloc(size); }
static void my_free(void *ctx, void *ptr) { (void)ctx; free(ptr); }

/* --- Little-endian helpers --- */

static void put_u16le(unsigned char *p, unsigned v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
}

static void put_u32le(unsigned char *p, unsigned v)
{
	p[0] = v & 0xFF;
	p[1] = (v >> 8) & 0xFF;
	p[2] = (v >> 16) & 0xFF;
	p[3] = (v >> 24) & 0xFF;
}

/* --- Build a minimal UC2 archive containing one file --- */

static int compress_data(const unsigned char *data, unsigned len, int level,
                         const void *master, unsigned master_size,
                         struct membuf *out, unsigned short *csum_out)
{
	struct mem_reader mr = { .data = data, .pos = 0, .len = len };
	unsigned csize = 0;
	membuf_init(out);
	int ret = uc2_compress_ex(level, master, master_size, mem_read, &mr,
	                          membuf_write, out, len, csum_out, &csize);
	return ret;
}

static int build_archive(const unsigned char *file_compressed, unsigned file_csize,
                         unsigned file_orig_size, unsigned short file_csum,
                         unsigned master_id, int level, struct membuf *archive)
{
	/*
	 * Archive layout:
	 *   [0]              FHEAD (13 bytes)
	 *   [13]             XHEAD (16 bytes)
	 *   [29]             file compressed bitstream
	 *   [29+fc]          COMPRESS record for cdir (10 bytes)
	 *   [29+fc+10]       cdir compressed bitstream
	 *
	 * Raw cdir layout:
	 *   OHEAD(type=2)                          1 byte
	 *   OSMETA(parent4,attr1,time4,name11,hid1,tag1) 22 bytes
	 *   FILEMETA(length4,fletch2)              6 bytes
	 *   COMPRESS(clen4,method2,master4)        10 bytes
	 *   LOCATION(volume4,offset4)              8 bytes
	 *   OHEAD(type=4 EndOfCdir)                1 byte
	 *   XTAIL(beta1,lock1,serial4,label11)     17 bytes
	 *   aserial(4)                             4 bytes
	 *   Total: 69 bytes
	 */

	unsigned file_data_offset = 29;

	/* Build raw cdir */
	unsigned char raw_cdir[69];
	unsigned char *p = raw_cdir;

	/* OHEAD: FileEntry */
	*p++ = 2;

	/* OSMETA */
	put_u32le(p, 0); p += 4;          /* parent = root */
	*p++ = 0x20;                        /* attrib = archive */
	put_u32le(p, 0); p += 4;          /* time */
	memcpy(p, "TEST    DAT", 11); p += 11; /* name */
	*p++ = 0;                           /* hidden */
	*p++ = 0;                           /* tag */

	/* FILEMETA */
	put_u32le(p, file_orig_size); p += 4;  /* length */
	put_u16le(p, file_csum); p += 2;       /* fletch */

	/* COMPRESS */
	put_u32le(p, file_csize); p += 4;     /* compressedLength */
	put_u16le(p, 1); p += 2;              /* method = ultra */
	put_u32le(p, master_id); p += 4;      /* masterPrefix */

	/* LOCATION */
	put_u32le(p, 1); p += 4;              /* volume = 1 */
	put_u32le(p, file_data_offset); p += 4; /* offset */

	/* EndOfCdir */
	*p++ = 4;

	/* XTAIL */
	*p++ = 0;                              /* beta */
	*p++ = 0;                              /* lock */
	put_u32le(p, 0); p += 4;              /* serial */
	memset(p, ' ', 11); p += 11;          /* label */

	/* aserial */
	put_u32le(p, 0); p += 4;

	unsigned raw_cdir_len = (unsigned)(p - raw_cdir);

	/* Compute Fletcher checksum of raw cdir */
	unsigned short cdir_csum = fletcher_checksum(raw_cdir, raw_cdir_len);

	/* Compress the cdir */
	struct membuf cdir_compressed;
	unsigned short cdir_compress_csum = 0;
	int ret = compress_data(raw_cdir, raw_cdir_len, level, NULL, 0,
	                        &cdir_compressed, &cdir_compress_csum);
	if (ret < 0) {
		membuf_free(&cdir_compressed);
		return ret;
	}

	unsigned cdir_compress_offset = 29 + file_csize;
	unsigned total_size = cdir_compress_offset + 10 + cdir_compressed.len;

	/* Assemble archive */
	membuf_init(archive);
	unsigned char header[29 + 10]; /* FHEAD + XHEAD + room for COMPRESS */

	/* FHEAD (13 bytes) */
	unsigned component_length = total_size - 13;
	put_u32le(header + 0, 0x1A324355);                    /* "UC2\x1a" */
	put_u32le(header + 4, component_length);               /* componentLength */
	put_u32le(header + 8, component_length + 0x01B2C3D4); /* componentLength2 */
	header[12] = 0;                                        /* damageProtected */

	/* XHEAD (16 bytes) */
	put_u32le(header + 13, 1);                     /* cdir.volume */
	put_u32le(header + 17, cdir_compress_offset);  /* cdir.offset */
	put_u16le(header + 21, cdir_csum);             /* fletch */
	header[23] = 0;                                 /* busy */
	put_u16le(header + 24, 200);                   /* versionMadeBy = 2.00 */
	put_u16le(header + 26, 200);                   /* versionNeededToExtract = 2.00 */
	header[28] = 0;                                 /* dummy */

	membuf_append(archive, header, 29);

	/* File compressed bitstream */
	membuf_append(archive, file_compressed, file_csize);

	/* COMPRESS record for cdir (10 bytes) */
	unsigned char cdir_compress_rec[10];
	put_u32le(cdir_compress_rec + 0, cdir_compressed.len); /* compressedLength */
	put_u16le(cdir_compress_rec + 4, 1);                    /* method = ultra */
	put_u32le(cdir_compress_rec + 6, 1);                    /* masterPrefix = NoMaster */
	membuf_append(archive, cdir_compress_rec, 10);

	/* Cdir compressed bitstream */
	membuf_append(archive, cdir_compressed.data, cdir_compressed.len);

	membuf_free(&cdir_compressed);
	return 0;
}

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
	int levels[] = {2, 3, 4, 5};
	int nlev = (int)(sizeof levels / sizeof *levels);

	for (int li = 0; li < nlev; li++) {
		int level = levels[li];
		printf("Level %d:\n", level);

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
