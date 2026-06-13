/* libFuzzer harness for the UC2 read path.
 *
 * Feeds the fuzzer-provided bytes as a .uc2 archive through the full
 * open -> read_cdir -> finish_cdir -> extract flow with an in-memory
 * reader and a discard writer.  The decoder must never read or write
 * out of bounds on any input.
 *
 * Build (clang):
 *   clang -fsanitize=fuzzer,address -O1 -g -Ilib/include -Ilib/src \
 *       -I<builddir>/lib tests/fuzz/fuzz_extract.c lib/src/*.c \
 *       <builddir>/lib/super_data.S -o fuzz_extract
 * Run: ./fuzz_extract -max_len=65536 corpus/
 */

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
	if (size > (1u << 20))   /* bound work; the format is small anyway */
		return 0;

	struct uc2_io io = { .read = mem_read, .alloc = mem_alloc, .free = mem_free };
	struct mem m = { .data = data, .avail = (unsigned)size };

	uc2_handle h = uc2_open(&io, &m);
	if (!h)
		return 0;

	struct uc2_entry entries[64];
	int n = 0;
	for (int guard = 0; guard < 100000; guard++) {
		struct uc2_entry e;
		int ret = uc2_read_cdir(h, &e);
		if (ret == UC2_End || ret < 0)
			break;
		while (ret == UC2_TaggedEntry) {
			char *tag; void *d; unsigned sz;
			ret = uc2_get_tag(h, &e, &tag, &d, &sz);
			if (ret < 0)
				break;
		}
		if (ret < 0)
			break;
		if (!e.is_dir && n < (int)(sizeof entries / sizeof *entries))
			entries[n++] = e;
	}

	char label[12];
	uc2_finish_cdir(h, label);

	for (int i = 0; i < n; i++)
		uc2_extract(h, &entries[i].xi, entries[i].size, discard, 0);

	uc2_close(h);
	return 0;
}
