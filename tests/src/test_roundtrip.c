/* Round-trip test: compress with uc2_compress, wrap in a minimal UC2 archive,
   decompress with the library's existing decompressor, verify byte identity.

   The archive format requires a compressed central directory, so we use
   uc2_compress for both the file data and the cdir. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
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

static int payload_read_error(void *ctx, unsigned pos, void *ptr, unsigned len)
{
	if (pos == 29) return -1;
	return archive_read(ctx,pos,ptr,len);
}

static void test_decoder_error_and_skip(void)
{
	/* Default Huffman tree: distance 768, length 3 before the initial
	 * 512-byte zero history. The listed empty file has a valid empty
	 * checksum, so a swallowed structural error used to look successful. */
	const unsigned char invalid[] = {0x00,0xb1,0,0,0,0};
	struct membuf archive, output;
	int rc=build_archive(invalid,sizeof invalid,0,0xa55a,1,4,&archive);
	assert(rc==0);
	struct uc2_io io={archive_read,my_alloc,my_free,NULL};
	uc2_handle h=uc2_open(&io,&archive); assert(h);
	assert(uc2_read_cdir(h,NULL)==UC2_End);
	assert(uc2_finish_cdir(h,NULL)==0); uc2_close(h);
	h=uc2_open(&io,&archive); assert(h);
	assert(uc2_finish_cdir(h,NULL)==0); uc2_close(h);
	for (int mode=0; mode<2; mode++) {
		io.read=mode ? payload_read_error : archive_read;
		h=uc2_open(&io,&archive); assert(h);
		struct uc2_entry entry;
		assert(uc2_read_cdir(h,&entry)==UC2_BareEntry);
		assert(uc2_finish_cdir(h,NULL)==0);
		membuf_init(&output);
		rc=uc2_extract(h,&entry.xi,entry.size,membuf_write,&output);
		assert(rc<0 && output.len==0);
		membuf_free(&output); uc2_close(h);
	}
	membuf_free(&archive);
}

static void test_invalid_rans_header(void)
{
	for (int mode=0; mode<2; mode++) {
		struct membuf compressed, archive, output;
		unsigned short checksum;
		int status=compress_data(NULL,0,6,NULL,0,&compressed,&checksum);
		assert(status==0);
		/* Bits are MSB-first inside little-endian 16-bit words. Clear
		 * the state length or all frequencies in an otherwise empty block. */
		unsigned first=mode ? 33 : 17, last=mode ? 33+344*12 : 33;
		for (unsigned bit=first; bit<last; bit++) {
			unsigned shift=15-(bit%16), index=(bit/16)*2+(shift/8);
			assert(index<compressed.len);
			compressed.data[index] &= (unsigned char)~(1u<<(shift%8));
		}
		status=build_archive(compressed.data,compressed.len,0,checksum,1,6,&archive);
		assert(status==0);
		struct uc2_io io={archive_read,my_alloc,my_free,NULL};
		uc2_handle h=uc2_open(&io,&archive); assert(h);
		struct uc2_entry entry;
		assert(uc2_read_cdir(h,&entry)==UC2_BareEntry);
		assert(uc2_finish_cdir(h,NULL)==0);
		membuf_init(&output);
		int rc=uc2_extract(h,&entry.xi,0,membuf_write,&output);
		assert(rc<0 && output.len==0);
		uc2_close(h); membuf_free(&output); membuf_free(&archive); membuf_free(&compressed);
	}
}

static void test_empty_long_name(void)
{
	for (int boundary=0; boundary<4; boundary++) {
		struct membuf payload, compressed, archive;
		unsigned short sum;
		int rc=compress_data(NULL,0,4,NULL,0,&payload,&sum); assert(rc==0);
		unsigned char raw[450]={0};
		raw[0]=2; raw[5]=0x20; memcpy(raw+10,"EMPTY   DAT",11); raw[22]=1;
		put_u16le(raw+27,sum); put_u32le(raw+29,payload.len);
		put_u16le(raw+33,1); put_u32le(raw+35,1);
		put_u32le(raw+39,1); put_u32le(raw+43,29);
		memcpy(raw+47,"AIP:Win95 LongN",15); /* zero payload, no next tag */
		unsigned tail=68;
		if (boundary>=2) {
			put_u32le(raw+63,3); raw[67]=1; memcpy(raw+68,"ok",3);
			memcpy(raw+71,"AIP:Win95 LongN",15); put_u32le(raw+87,300);
			memset(raw+92,boundary==2 ? 'a' : 0xb3,300); tail=392;
		}
		raw[tail]=4; memset(raw+tail+7,' ',11);
		unsigned length=boundary==1 ? 68 : tail+22;
		rc=compress_data(raw,length,4,NULL,0,&compressed,&sum); assert(rc==0);
		unsigned char header[29]={0}, descriptor[10]={0};
		unsigned offset=29+payload.len, size=offset+10+compressed.len;
		put_u32le(header,0x1a324355); put_u32le(header+4,size-13);
		put_u32le(header+8,size-13+0x01b2c3d4); put_u32le(header+13,1);
		put_u32le(header+17,offset); put_u16le(header+21,fletcher_checksum(raw,length));
		put_u16le(header+24,200); put_u16le(header+26,200);
		put_u32le(descriptor,compressed.len); put_u16le(descriptor+4,1); put_u32le(descriptor+6,1);
		membuf_init(&archive); membuf_append(&archive,header,sizeof header);
		membuf_append(&archive,payload.data,payload.len); membuf_append(&archive,descriptor,sizeof descriptor);
		membuf_append(&archive,compressed.data,compressed.len);
		struct uc2_io io={archive_read,my_alloc,my_free,NULL};
		uc2_handle h=uc2_open(&io,&archive); assert(h);
		struct uc2_entry entry;
		rc=uc2_read_cdir(h,&entry); assert(rc==UC2_TaggedEntry);
		rc=uc2_get_tag(h,&entry,NULL,NULL,NULL); assert(rc==UC2_End);
		assert(strcmp(entry.name,"empty.dat")==0 && entry.name_len==9);
		rc=uc2_finish_cdir(h,NULL); assert(boundary==1 ? rc<0 : rc==0);
		uc2_close(h); membuf_free(&payload); membuf_free(&compressed); membuf_free(&archive);
	}
}

static void test_turbo_directory(void)
{
	for (unsigned count=0; count<=700; count+=700) {
		unsigned length=count*27+22;
		unsigned char *raw=calloc(1,length); assert(raw);
		for (unsigned i=0; i<count; i++) {
			unsigned char *p=raw+i*27; p[0]=1; p[5]=0x10;
			char name[12]; snprintf(name,sizeof name,"D%07u   ",i);
			memcpy(p+10,name,11); put_u32le(p+23,i+1);
		}
		raw[count*27]=4; memset(raw+count*27+7,' ',11);
		/* A valid method-80 block using only literal groups. */
		struct membuf coded,archive; membuf_init(&coded); membuf_init(&archive);
		unsigned char sizeword[2]; put_u16le(sizeword,length+(length+7)/8);
		membuf_append(&coded,sizeword,2);
		for (unsigned i=0; i<length; i+=8) {
			unsigned char control=0; membuf_append(&coded,&control,1);
			unsigned n=length-i<8 ? length-i : 8; membuf_append(&coded,raw+i,n);
		}
		put_u16le(sizeword,0); membuf_append(&coded,sizeword,2);
		unsigned char header[39]={0};
		put_u32le(header,0x1a324355); put_u32le(header+4,26+coded.len);
		put_u32le(header+8,26+coded.len+0x01b2c3d4); put_u32le(header+13,1);
		put_u32le(header+17,29); put_u16le(header+21,fletcher_checksum(raw,length));
		put_u16le(header+24,200); put_u16le(header+26,200);
		put_u32le(header+29,coded.len); put_u16le(header+33,80); put_u32le(header+35,1);
		membuf_append(&archive,header,sizeof header); membuf_append(&archive,coded.data,coded.len);
		struct uc2_io io={archive_read,my_alloc,my_free,NULL};
		uc2_handle h=uc2_open(&io,&archive); assert(h);
		for (unsigned i=0; i<count; i++) {
			struct uc2_entry entry; int rc=uc2_read_cdir(h,&entry);
			assert(rc==UC2_BareEntry && entry.is_dir && entry.id==i+1);
		}
		int rc=uc2_finish_cdir(h,NULL); assert(rc==0);
		uc2_close(h); free(raw); membuf_free(&coded); membuf_free(&archive);
	}
}

int main(void)
{
	test_decoder_error_and_skip();
	test_invalid_rans_header();
	test_empty_long_name();
	test_turbo_directory();
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
