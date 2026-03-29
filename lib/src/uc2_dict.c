/* Dictionary management for zstd-inspired dictionary compression.
 *
 * Each dictionary is identified by a 64-bit content hash (FNV-1a)
 * and protected by a 32-bit integrity checksum.  Dictionaries can
 * be serialized to a portable format for storage in block stores
 * or as standalone files. */

#include "uc2/uc2_dict.h"
#include "uc2/uc2_merkle.h"  /* uc2_hash64 */
#include "uc2/uc2_cdc.h"     /* uc2_fnv1a */
#include <stdlib.h>
#include <string.h>

int uc2_dict_create(struct uc2_dict *dict, const uint8_t *data, size_t size)
{
	memset(dict, 0, sizeof *dict);
	if (!data || size == 0) return -1;

	dict->data = malloc(size);
	if (!dict->data) return -1;
	memcpy(dict->data, data, size);
	dict->size = (uint32_t)size;
	dict->id = uc2_hash64(data, size);
	dict->checksum = uc2_fnv1a(data, size);
	return 0;
}

int uc2_dict_verify(const struct uc2_dict *dict)
{
	if (!dict->data || dict->size == 0) return 0;
	uint32_t check = uc2_fnv1a(dict->data, dict->size);
	return check == dict->checksum;
}

size_t uc2_dict_serialize(const struct uc2_dict *dict, uint8_t **out)
{
	size_t total = sizeof(struct uc2_dict_header) + dict->size;
	uint8_t *buf = malloc(total);
	if (!buf) { *out = NULL; return 0; }

	struct uc2_dict_header *hdr = (struct uc2_dict_header *)buf;
	hdr->magic = UC2_DICT_MAGIC;
	hdr->id = dict->id;
	hdr->checksum = dict->checksum;
	hdr->size = dict->size;
	hdr->reserved = 0;
	memcpy(buf + sizeof *hdr, dict->data, dict->size);

	*out = buf;
	return total;
}

int uc2_dict_deserialize(struct uc2_dict *dict, const uint8_t *buf, size_t len)
{
	memset(dict, 0, sizeof *dict);
	if (len < sizeof(struct uc2_dict_header)) return -1;

	const struct uc2_dict_header *hdr = (const struct uc2_dict_header *)buf;
	if (hdr->magic != UC2_DICT_MAGIC) return -1;
	if (sizeof(struct uc2_dict_header) + hdr->size > len) return -1;

	dict->id = hdr->id;
	dict->checksum = hdr->checksum;
	dict->size = hdr->size;
	dict->data = malloc(hdr->size);
	if (!dict->data) return -1;
	memcpy(dict->data, buf + sizeof *hdr, hdr->size);
	return 0;
}

void uc2_dict_free(struct uc2_dict *dict)
{
	free(dict->data);
	memset(dict, 0, sizeof *dict);
}
