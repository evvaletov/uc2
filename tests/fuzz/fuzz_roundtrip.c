/* libFuzzer harness for the UC2 compress / round-trip path.
 *
 * Where fuzz_extract.c hammers the decoder with untrusted archive
 * bytes, this harness drives the *encoder*: it compresses the
 * fuzzer-provided bytes (the first byte selects the level), wraps the
 * bitstream in a minimal one-file archive, decompresses it back through
 * the public read path, and asserts the output is byte-identical to the
 * input.  It catches encoder crashes/OOB and any compress<->decompress
 * mismatch.
 *
 * Build (clang):
 *   clang -fsanitize=fuzzer,address -O1 -g -Ilib/include -Ilib/src \
 *       -I<builddir>/lib tests/fuzz/fuzz_roundtrip.c lib/src/*.c \
 *       <builddir>/lib/super_data.S -lm -o fuzz_roundtrip
 * Run: ./fuzz_roundtrip -max_len=65536 corpus/
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>

/* --- Fletcher checksum (matches decompress.c) --- */
static unsigned short fletcher(const unsigned char *data, unsigned len)
{
	if (!len) return 0xA55A;
	unsigned v = 0xA55A;
	const unsigned char *p = data, *e = p + len - 1;
	if (v > 0xFFFF) v ^= *p++ << 8;
	while (p < e) { v ^= p[0] | p[1] << 8; p += 2; }
	v &= 0xFFFF;
	if (p == e) v ^= *p | 0x10000;
	return (unsigned short)(v & 0xFFFF);
}

/* --- Growable buffer --- */
struct membuf { unsigned char *data; unsigned len, cap; };

static int membuf_write(void *ctx, const void *ptr, unsigned len)
{
	struct membuf *b = ctx;
	if (b->len + len > b->cap) {
		unsigned nc = b->cap ? b->cap * 2 : 4096;
		while (nc < b->len + len) nc *= 2;
		unsigned char *p = realloc(b->data, nc);
		if (!p) return -1;
		b->data = p; b->cap = nc;
	}
	memcpy(b->data + b->len, ptr, len);
	b->len += len;
	return 0;
}
static void membuf_free(struct membuf *b) { free(b->data); b->data = 0; b->len = b->cap = 0; }

/* --- callbacks --- */
struct mem_reader { const unsigned char *data; unsigned pos, len; };
static int comp_read(void *ctx, void *buf, unsigned len)
{
	struct mem_reader *m = ctx;
	unsigned avail = m->len - m->pos;
	if (len > avail) len = avail;
	if (len) { memcpy(buf, m->data + m->pos, len); m->pos += len; }
	return (int)len;
}
static int arc_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct membuf *b = ctx;
	if (pos >= b->len) return 0;
	unsigned avail = b->len - pos;
	if (len > avail) len = avail;
	memcpy(buf, b->data + pos, len);
	return (int)len;
}
static void *m_alloc(void *c, unsigned s) { (void)c; return malloc(s); }
static void m_free(void *c, void *p) { (void)c; free(p); }

static void put16(unsigned char *p, unsigned v) { p[0] = v; p[1] = v >> 8; }
static void put32(unsigned char *p, unsigned v)
{ p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	if (size < 1 || size > (1u << 20))
		return 0;

	int level = 2 + (data[0] & 3);   /* 2..5 */
	const unsigned char *payload = data + 1;
	unsigned plen = (unsigned)(size - 1);

	/* 1. Compress the payload. */
	struct mem_reader mr = { payload, 0, plen };
	struct membuf comp = {0};
	unsigned short fcsum = 0;
	unsigned csize = 0;
	int ret = uc2_compress(level, comp_read, &mr, membuf_write, &comp, plen,
	                       &fcsum, &csize);
	if (ret < 0) { membuf_free(&comp); return 0; }

	/* 2. Build a minimal one-file archive (see test_roundtrip.c). */
	unsigned char raw[69];
	unsigned char *p = raw;
	*p++ = 2;                                  /* OHEAD FileEntry */
	put32(p, 0); p += 4;                       /* parent root */
	*p++ = 0x20;                               /* attrib */
	put32(p, 0); p += 4;                       /* time */
	memcpy(p, "TEST    DAT", 11); p += 11;     /* name */
	*p++ = 0; *p++ = 0;                        /* hidden, tag */
	put32(p, plen); p += 4;                    /* FILEMETA length */
	put16(p, fcsum); p += 2;                   /* fletch */
	put32(p, comp.len); p += 4;                /* COMPRESS clen */
	put16(p, 1); p += 2;                       /* method ultra */
	put32(p, 1); p += 4;                       /* masterPrefix NoMaster */
	put32(p, 1); p += 4;                       /* LOCATION volume */
	put32(p, 29); p += 4;                      /* offset */
	*p++ = 4;                                  /* EndOfCdir */
	*p++ = 0; *p++ = 0;                        /* beta, lock */
	put32(p, 0); p += 4;                       /* serial */
	memset(p, ' ', 11); p += 11;               /* label */
	put32(p, 0); p += 4;                       /* aserial */
	unsigned rawlen = (unsigned)(p - raw);
	unsigned short cdir_fcsum = fletcher(raw, rawlen);

	struct mem_reader cmr = { raw, 0, rawlen };
	struct membuf cdir = {0};
	unsigned short cdir_compress_csum = 0;
	unsigned cdir_csize = 0;
	ret = uc2_compress(level, comp_read, &cmr, membuf_write, &cdir, rawlen,
	                   &cdir_compress_csum, &cdir_csize);
	if (ret < 0) { membuf_free(&comp); membuf_free(&cdir); return 0; }

	unsigned cdir_off = 29 + comp.len;
	unsigned total = cdir_off + 10 + cdir.len;

	struct membuf arc = {0};
	unsigned char hdr[29];
	unsigned clen = total - 13;
	put32(hdr + 0, 0x1A324355);
	put32(hdr + 4, clen);
	put32(hdr + 8, clen + 0x01B2C3D4);
	hdr[12] = 0;
	put32(hdr + 13, 1);
	put32(hdr + 17, cdir_off);
	put16(hdr + 21, cdir_fcsum);
	hdr[23] = 0;
	put16(hdr + 24, 200);
	put16(hdr + 26, 200);
	hdr[28] = 0;
	membuf_write(&arc, hdr, 29);
	membuf_write(&arc, comp.data, comp.len);
	unsigned char crec[10];
	put32(crec + 0, cdir.len);
	put16(crec + 4, 1);
	put32(crec + 6, 1);
	membuf_write(&arc, crec, 10);
	membuf_write(&arc, cdir.data, cdir.len);

	membuf_free(&comp);
	membuf_free(&cdir);

	/* 3. Decompress and verify identity.  The archive we just built is
	   well-formed by construction, so every step below must succeed and
	   reproduce the payload byte-for-byte.  Any failure is a real
	   compress<->decompress asymmetry, so it aborts and libFuzzer records
	   the input -- a silent early return would mask exactly the encoder
	   bugs this harness exists to find. */
	struct uc2_io io = { .read = arc_read, .alloc = m_alloc, .free = m_free };
	uc2_handle h = uc2_open(&io, &arc);
	if (!h)
		abort();

	struct uc2_entry e;
	ret = uc2_read_cdir(h, &e);
	if (ret < 0)
		abort();
	while (ret == UC2_TaggedEntry) {
		char *t; void *d; unsigned s;
		ret = uc2_get_tag(h, &e, &t, &d, &s);
		if (ret < 0)
			abort();
	}

	struct uc2_entry tail;
	if (uc2_read_cdir(h, &tail) != UC2_End)   /* exactly one file entry */
		abort();

	char label[12];
	uc2_finish_cdir(h, label);

	struct membuf out = {0};
	ret = uc2_extract(h, &e.xi, e.size, membuf_write, &out);
	if (ret < 0 || out.len != plen ||
	    (plen && memcmp(out.data, payload, plen) != 0))
		abort();

	membuf_free(&out);
	uc2_close(h);
	membuf_free(&arc);
	return 0;
}
