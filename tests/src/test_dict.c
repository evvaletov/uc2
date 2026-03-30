/* Tests for dictionary management. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <uc2/uc2_dict.h>

static int tests_run = 0, tests_passed = 0;
#define TEST(name) do { tests_run++; printf("  %s: ", #name); name(); tests_passed++; printf("OK\n"); } while (0)

static void test_create_verify(void)
{
	uint8_t data[] = "Dictionary content for testing purposes.";
	struct uc2_dict dict;
	{ int _r = uc2_dict_create(&dict, data, sizeof data); (void)_r; assert(_r == 0); }
	assert(dict.size == sizeof data);
	assert(dict.id != 0);
	assert(dict.checksum != 0);
	assert(uc2_dict_verify(&dict));
	uc2_dict_free(&dict);
}

static void test_id_deterministic(void)
{
	uint8_t data[] = "Same content";
	struct uc2_dict d1, d2;
	uc2_dict_create(&d1, data, sizeof data);
	uc2_dict_create(&d2, data, sizeof data);
	{ int _r = uc2_dict_id(&d1); (void)_r; assert(_r == uc2_dict_id(&d2)); }
	assert(uc2_dict_match(&d1, &d2));
	uc2_dict_free(&d1);
	uc2_dict_free(&d2);
}

static void test_id_differs(void)
{
	uint8_t a[] = "Content A";
	uint8_t b[] = "Content B";
	struct uc2_dict da, db;
	uc2_dict_create(&da, a, sizeof a);
	uc2_dict_create(&db, b, sizeof b);
	assert(uc2_dict_id(&da) != uc2_dict_id(&db));
	assert(!uc2_dict_match(&da, &db));
	uc2_dict_free(&da);
	uc2_dict_free(&db);
}

static void test_serialize_roundtrip(void)
{
	uint8_t data[1024];
	for (int i = 0; i < 1024; i++) data[i] = (uint8_t)(i * 37);

	struct uc2_dict orig;
	uc2_dict_create(&orig, data, sizeof data);

	uint8_t *buf;
	size_t len = uc2_dict_serialize(&orig, &buf);
	assert(len > 0);
	assert(buf != NULL);

	struct uc2_dict restored;
	{ int _r = uc2_dict_deserialize(&restored, buf, len); (void)_r; assert(_r == 0); }
	assert(restored.id == orig.id);
	assert(restored.checksum == orig.checksum);
	assert(restored.size == orig.size);
	{ int _r = memcmp(restored.data, orig.data, orig.size); (void)_r; assert(_r == 0); }
	assert(uc2_dict_verify(&restored));

	free(buf);
	uc2_dict_free(&orig);
	uc2_dict_free(&restored);
}

static void test_corrupted(void)
{
	uint8_t data[] = "Integrity test data.";
	struct uc2_dict dict;
	uc2_dict_create(&dict, data, sizeof data);
	assert(uc2_dict_verify(&dict));

	/* Corrupt one byte */
	dict.data[5] ^= 0xFF;
	assert(!uc2_dict_verify(&dict));

	uc2_dict_free(&dict);
}

static void test_deserialize_bad_magic(void)
{
	uint8_t bad[] = "NOT_UC2D_HEADER_WITH_ENOUGH_BYTES";
	struct uc2_dict dict;
	{ int _r = uc2_dict_deserialize(&dict, bad, sizeof bad); (void)_r; assert(_r == -1); }
}

int main(void)
{
	printf("Dictionary tests:\n");
	TEST(test_create_verify);
	TEST(test_id_deterministic);
	TEST(test_id_differs);
	TEST(test_serialize_roundtrip);
	TEST(test_corrupted);
	TEST(test_deserialize_bad_magic);
	printf("%d/%d tests passed\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}
