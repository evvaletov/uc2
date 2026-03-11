/* Unit test for uc2_identify() magic detection. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uc2/libuc2.h>

static int failures;

static void check(const char *name, int got, int expected)
{
	if (got == expected)
		return;
	fprintf(stderr, "FAIL: %s: got %d, expected %d\n", name, got, expected);
	failures++;
}

int main(void)
{
	/* uc2_identify returns:
	   -1  if buffer too small (< 4 bytes)
	    0  if magic/validation fails
	    1  if valid (possibly partial — passes all checks so far) */

	/* Minimal 4-byte valid magic: "UC2\x1a" */
	unsigned char valid4[] = {'U', 'C', '2', 0x1a};
	check("valid_4bytes", uc2_identify(valid4, 4), 1);

	/* Wrong magic bytes */
	unsigned char bad[] = {'Z', 'I', 'P', 0x00};
	check("bad_magic", uc2_identify(bad, 4), 0);

	/* Correct prefix but wrong 4th byte */
	unsigned char almost[] = {'U', 'C', '2', 0x00};
	check("wrong_marker", uc2_identify(almost, 4), 0);

	/* Too short — returns -1 */
	unsigned char tiny[] = {'U', 'C'};
	check("too_short_2", uc2_identify(tiny, 2), -1);

	unsigned char three[] = {'U', 'C', '2'};
	check("too_short_3", uc2_identify(three, 3), -1);

	/* Load a real archive header and test with that */
	/* (covered implicitly by test_extract opening archives successfully) */

	if (failures) {
		fprintf(stderr, "%d test(s) FAILED\n", failures);
		return EXIT_FAILURE;
	}
	printf("test_identify: all tests passed\n");
	return EXIT_SUCCESS;
}
