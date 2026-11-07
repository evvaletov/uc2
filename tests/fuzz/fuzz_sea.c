/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Bounded standalone SEA Huffman decoder; no executable input runs. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uc2/libuc2.h>

struct mem { const uint8_t *data; unsigned avail; };

static int mem_read(void *ctx, unsigned pos, void *buf, unsigned len)
{
	struct mem *m = ctx;
	if (pos >= m->avail)
		return 0;
	unsigned n = m->avail - pos;
	if (n > len)
		n = len;
	memcpy(buf, m->data + pos, n);
	return (int)n;
}

static void *mem_alloc(void *ctx, unsigned size) { (void)ctx; return malloc(size); }
static void mem_free(void *ctx, void *ptr) { (void)ctx; free(ptr); }
static int discard(void *ctx, const void *p, unsigned len)
{ (void)ctx; (void)p; (void)len; return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (size > 65536) return 0;
    struct uc2_io io = {mem_read, mem_alloc, mem_free, NULL};
    struct mem m = {data, (unsigned)size};
    uc2_decompress_raw(&io, &m, 0, (unsigned)size, 1024*1024, discard, NULL);
    return 0;
}
