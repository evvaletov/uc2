/* Dictionary management for zstd-inspired dictionary compression. */

#include "uc2/uc2_dict.h"
#include "uc2/uc2_merkle.h"
#include "uc2/uc2_cdc.h"
#include <stdlib.h>
#include <string.h>

/* Serialization helpers (little-endian, alignment-safe) */
static void put32(uint8_t *p, uint32_t v) {
	p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24;
}
static void put64(uint8_t *p, uint64_t v) {
	for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (i*8));
}
static uint32_t get32(const uint8_t *p) {
	return p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static uint64_t get64(const uint8_t *p) {
	uint64_t v = 0;
	for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
	return v;
}

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
	return uc2_fnv1a(dict->data, dict->size) == dict->checksum;
}

/* Serialized format: magic(4) + id(8) + checksum(4) + size(4) + reserved(4) = 24 bytes */
#define HDR_SIZE 24

size_t uc2_dict_serialize(const struct uc2_dict *dict, uint8_t **out)
{
	size_t total = HDR_SIZE + dict->size;
	uint8_t *buf = malloc(total);
	if (!buf) { *out = NULL; return 0; }
	put32(buf, UC2_DICT_MAGIC);
	put64(buf + 4, dict->id);
	put32(buf + 12, dict->checksum);
	put32(buf + 16, dict->size);
	put32(buf + 20, 0);
	memcpy(buf + HDR_SIZE, dict->data, dict->size);
	*out = buf;
	return total;
}

int uc2_dict_deserialize(struct uc2_dict *dict, const uint8_t *buf, size_t len)
{
	memset(dict, 0, sizeof *dict);
	if (len < HDR_SIZE) return -1;
	if (get32(buf) != UC2_DICT_MAGIC) return -1;
	uint32_t size = get32(buf + 16);
	if (HDR_SIZE + size > len) return -1;
	dict->id = get64(buf + 4);
	dict->checksum = get32(buf + 12);
	dict->size = size;
	dict->data = malloc(size);
	if (!dict->data) return -1;
	memcpy(dict->data, buf + HDR_SIZE, size);
	return 0;
}

void uc2_dict_free(struct uc2_dict *dict)
{
	free(dict->data);
	memset(dict, 0, sizeof *dict);
}
