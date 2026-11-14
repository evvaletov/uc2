/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Deterministic failures in the actual helper implementations, without
 * adding failure switches to the public library. The static library supplies
 * the callers and CDC implementation; these definitions replace its two
 * corresponding object files for this test executable only. */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define rmdir _rmdir
#else
#include <unistd.h>
#endif
#include <uc2/uc2_ingest.h>
#include <uc2/uc2_merkle.h>
#include <uc2/uc2_blockstore.h>

static unsigned allocation_count, fail_allocation;
static void *failing_realloc(void *ptr, size_t size)
{
	if (++allocation_count == fail_allocation) return NULL;
	return realloc(ptr, size);
}
#define realloc failing_realloc
#include "../../lib/src/uc2_merkle.c"
#undef realloc

enum io_failure { NONE, OPEN, WRITE, CLOSE, READ };
static enum io_failure fail_io;
static unsigned io_count, fail_io_at;
static FILE *failed_read;
static FILE *failing_fopen(const char *path, const char *mode)
{
	if (fail_io == OPEN && ++io_count == fail_io_at) {
		errno = EIO;
		return NULL;
	}
	return fopen(path, mode);
}
static size_t failing_fwrite(const void *data, size_t size, size_t count, FILE *file)
{
	if (fail_io == WRITE && ++io_count == fail_io_at) {
		errno = EIO;
		return fwrite(data, size, count / 2, file);
	}
	return fwrite(data, size, count, file);
}
static int failing_fclose(FILE *file)
{
	if (failed_read == file) failed_read = NULL;
	int result = fclose(file);
	if (fail_io == CLOSE && ++io_count == fail_io_at) {
		errno = EIO;
		return EOF;
	}
	return result;
}
static size_t failing_fread(void *data, size_t size, size_t count, FILE *file)
{
	if (fail_io == READ && ++io_count == fail_io_at) {
		failed_read = file;
		errno = EIO;
		return 0;
	}
	return fread(data, size, count, file);
}
static int failing_ferror(FILE *file)
{
	return failed_read == file || ferror(file);
}
#define fopen failing_fopen
#define fwrite failing_fwrite
#define fclose failing_fclose
#define fread failing_fread
#define ferror failing_ferror
#include "../../lib/src/uc2_blockstore.c"
#undef fopen
#undef fwrite
#undef fclose
#undef fread
#undef ferror

static void allocation_failures(const char *archive_path)
{
	uint8_t data[512 * 1024];
	uint32_t seed = 1234;
	for (size_t i = 0; i < sizeof data; i++) {
		seed = seed * 1103515245 + 12345;
		data[i] = (uint8_t)(seed >> 16);
	}
	struct uc2_merkle tree;
	int result = uc2_merkle_build(&tree, data, sizeof data, 8);
	assert(result == 0 && tree.nchunks > 16);
	unsigned allocations = allocation_count;
	assert(allocations > 1);
	uc2_merkle_free(&tree);
	for (unsigned fail = 1; fail <= allocations; fail++) {
		fail_allocation = fail;
		allocation_count = 0;
		result = uc2_merkle_build(&tree, data, sizeof data, 8);
		assert(result == -1);
		assert(!tree.chunks && !tree.nchunks && !tree.capacity && !tree.root);
		uc2_merkle_free(&tree);

		/* The library caller must detect failure before truncating output. */
		FILE *file = fopen(archive_path, "wb");
		assert(file);
		size_t written = fwrite("keep", 1, 4, file);
		assert(written == 4);
		result = fclose(file);
		assert(result == 0);
		allocation_count = 0;
		struct uc2_ingest_stats stats;
		memset(&stats, 0, sizeof stats);
		result = uc2_ingest_write(archive_path, data, sizeof data, 8, &stats);
		assert(result == -1 && stats.bytes_in == 0);
		file = fopen(archive_path, "rb");
		assert(file);
		char saved[5];
		size_t n = fread(saved, 1, sizeof saved, file);
		assert(n == 4 && !memcmp(saved, "keep", 4));
		result = fclose(file);
		assert(result == 0);
	}
	fail_allocation = 0;
	result = remove(archive_path);
	assert(result == 0);
}

static void clean_store(struct uc2_blockstore *bs, const struct uc2_merkle *tree)
{
	for (int i = 0; i < tree->nchunks; i++) {
		char path[4096];
		int result = hash_path(bs, tree->chunks[i].hash, path, sizeof path);
		assert(result == 0);
		remove(path);
		char *slash = strrchr(path, '/');
		assert(slash);
		*slash = 0;
		rmdir(path);
	}
	int result = rmdir(bs->path);
	assert(result == 0);
	uc2_blockstore_close(bs);
}

static void storage_failures(const char *store_path)
{
	uint8_t data[32];
	for (unsigned i = 0; i < sizeof data; i++) data[i] = (uint8_t)i;
	struct uc2_chunk chunks[2] = {
		{uc2_hash64(data, 16), 0, 16},
		{uc2_hash64(data + 16, 16), 16, 16}
	};
	struct uc2_merkle tree = {chunks, 2, 2, 0};
	for (enum io_failure failure = OPEN; failure <= CLOSE; failure++) {
		for (unsigned at = 1; at <= 2; at++) {
			struct uc2_blockstore bs;
			int result = uc2_blockstore_open(&bs, store_path);
			assert(result == 0);
			fail_io = failure;
			fail_io_at = at;
			io_count = 0;
			result = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
			assert(result == -1);
			assert(bs.nblocks == (int)at - 1);
			assert(bs.total_bytes == ((int)at - 1) * 16 && bs.saved_bytes == 0);
			assert(!uc2_blockstore_has(&bs, chunks[at - 1].hash));
			fail_io = NONE;
			/* A retry must store the missing bytes, not deduplicate a
			   short object left behind by failed write/close. */
			result = uc2_blockstore_ingest(&bs, &tree, data, sizeof data);
			assert(result == 3 - (int)at && bs.nblocks == 2);
			assert(bs.total_bytes == sizeof data);
			for (int i = 0; i < tree.nchunks; i++) {
				uint8_t bytes[16];
				result = uc2_blockstore_read(&bs, chunks[i].hash, bytes, sizeof bytes);
				assert(result == 16 && !memcmp(bytes, data + i * 16, 16));
			}
			for (enum io_failure read_failure = CLOSE; read_failure <= READ; read_failure++) {
				uint8_t bytes[16];
				fail_io = read_failure;
				fail_io_at = 1;
				io_count = 0;
				result = uc2_blockstore_read(&bs, chunks[0].hash, bytes, sizeof bytes);
				assert(result == -1);
			}
			fail_io = NONE;
			clean_store(&bs, &tree);
		}
	}
}

int main(int argc, char **argv)
{
	assert(argc == 2);
	char archive[4096], store[4096];
	int n = snprintf(archive, sizeof archive, "%s-ingest.bin", argv[1]);
	assert(n > 0 && (size_t)n < sizeof archive);
	n = snprintf(store, sizeof store, "%s-store", argv[1]);
	assert(n > 0 && (size_t)n < sizeof store);
	allocation_failures(archive);
	storage_failures(store);
	puts("storage failures: allocation, caller preservation, open/write/close and retry passed");
	return 0;
}
