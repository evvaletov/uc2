/* Portable replay driver for the libFuzzer harnesses.
 *
 * Runs LLVMFuzzerTestOneInput() over every file given on the command
 * line.  Unlike a real fuzz run this needs no Clang/libFuzzer/ASan, so
 * it compiles with any C compiler and runs in the normal CI matrix and
 * via ctest -- it turns the committed crash/timeout artifacts into a
 * deterministic regression test.  A hang shows up as a ctest TIMEOUT;
 * an out-of-bounds access shows up when the harness is built with a
 * sanitizer.
 */

#include <stdio.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const unsigned char *data, size_t size);

static int run_file(const char *path)
{
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "replay: cannot open %s\n", path);
		return -1;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (n < 0) { fclose(f); return -1; }
	unsigned char *buf = malloc((size_t)n ? (size_t)n : 1);
	if (!buf) { fclose(f); return -1; }
	if (n && fread(buf, 1, (size_t)n, f) != (size_t)n) {
		fclose(f);
		free(buf);
		return -1;
	}
	fclose(f);
	LLVMFuzzerTestOneInput(buf, (size_t)n);
	free(buf);
	printf("replay ok: %s (%ld bytes)\n", path, n);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <file>...\n", argv[0]);
		return 2;
	}
	int rc = 0;
	for (int i = 1; i < argc; i++)
		if (run_file(argv[i]) < 0)
			rc = 1;
	return rc;
}
