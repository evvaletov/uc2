/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Shared in-memory archive fixtures for round-trip and malformed stream tests. */
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

static int build_archive_parts(const unsigned char *file_compressed, unsigned file_csize,
                         unsigned file_orig_size, unsigned short file_csum,
                         unsigned master_id, int level, struct membuf *archive,
                         const unsigned char *master_records, unsigned records_size,
                         const unsigned char *master_data, unsigned masters_size)
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
	put_u16le(p, level == 80 ? 80 : level >= 6 ? 10 : 1); p += 2; /* entropy method */
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
	struct membuf directory;
	membuf_init(&directory);
	if (records_size) membuf_append(&directory, master_records, records_size);
	membuf_append(&directory, raw_cdir, raw_cdir_len);

	/* Compute Fletcher checksum of raw cdir */
	unsigned short cdir_csum = fletcher_checksum(directory.data, directory.len);

	/* Compress the cdir */
	struct membuf cdir_compressed;
	unsigned short cdir_compress_csum = 0;
	int ret = compress_data(directory.data, directory.len, level == 80 ? 4 : level, NULL, 0,
	                        &cdir_compressed, &cdir_compress_csum);
	membuf_free(&directory);
	if (ret < 0) {
		membuf_free(&cdir_compressed);
		return ret;
	}

	unsigned cdir_compress_offset = 29 + file_csize + masters_size;
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
	if (masters_size) membuf_append(archive, master_data, masters_size);

	/* COMPRESS record for cdir (10 bytes) */
	unsigned char cdir_compress_rec[10];
	put_u32le(cdir_compress_rec + 0, cdir_compressed.len); /* compressedLength */
	put_u16le(cdir_compress_rec + 4, level == 80 ? 1 : level >= 6 ? 10 : 1);                    /* method = ultra */
	put_u32le(cdir_compress_rec + 6, 1);                    /* masterPrefix = NoMaster */
	membuf_append(archive, cdir_compress_rec, 10);

	/* Cdir compressed bitstream */
	membuf_append(archive, cdir_compressed.data, cdir_compressed.len);

	membuf_free(&cdir_compressed);
	return 0;
}

static int build_archive(const unsigned char *file_compressed, unsigned file_csize,
                         unsigned file_orig_size, unsigned short file_csum,
                         unsigned master_id, int level, struct membuf *archive)
{
	return build_archive_parts(file_compressed, file_csize, file_orig_size,
		file_csum, master_id, level, archive, NULL, 0, NULL, 0);
}

