/* Dictionary management for zstd-inspired dictionary compression.
 *
 * Formalizes UC2's master blocks as proper dictionaries with content
 * hashes (IDs), integrity checksums, and cross-archive sharing.
 * Combined with the block store (uc2_blockstore.h), this enables
 * distributed dedup: archives in different locations can reference
 * shared dictionaries by content hash.
 *
 * Usage:
 *   struct uc2_dict dict;
 *   uc2_dict_create(&dict, master_data, master_size);
 *   uint64_t id = uc2_dict_id(&dict);
 *   // Store/share/reference by id...
 *   uc2_dict_verify(&dict);  // check integrity
 *   uc2_dict_free(&dict);
 */

#ifndef UC2_DICT_H
#define UC2_DICT_H

#include <stdint.h>
#include <stddef.h>

/* Dictionary header (serialized in archive or block store). */
#define UC2_DICT_MAGIC 0x44324355  /* "UC2D" */

struct uc2_dict {
	uint64_t id;          /* content hash (FNV-1a 64-bit of data) */
	uint32_t checksum;    /* FNV-1a 32-bit integrity check */
	uint32_t size;        /* dictionary data size */
	uint8_t *data;        /* dictionary content (owned) */
};

/* Serialized dictionary header (24 bytes, stored in archive/block store). */
struct uc2_dict_header {
	uint32_t magic;       /* UC2_DICT_MAGIC */
	uint64_t id;          /* content hash */
	uint32_t checksum;    /* integrity */
	uint32_t size;        /* data size following header */
	uint32_t reserved;    /* future use */
};

/* Create a dictionary from raw master data.
 * Computes id (content hash) and checksum.  Copies data (caller
 * can free the original after this call). */
int uc2_dict_create(struct uc2_dict *dict, const uint8_t *data, size_t size);

/* Get dictionary ID (content hash for cross-archive sharing). */
static inline uint64_t uc2_dict_id(const struct uc2_dict *dict)
{
	return dict->id;
}

/* Verify dictionary integrity (returns 1 if valid, 0 if corrupted). */
int uc2_dict_verify(const struct uc2_dict *dict);

/* Serialize dictionary to a buffer (header + data).
 * Allocates *out (caller must free).  Returns total size. */
size_t uc2_dict_serialize(const struct uc2_dict *dict, uint8_t **out);

/* Deserialize dictionary from a buffer.
 * Returns 0 on success, -1 on error. */
int uc2_dict_deserialize(struct uc2_dict *dict, const uint8_t *buf, size_t len);

/* Check if two dictionaries have the same content (by ID). */
static inline int uc2_dict_match(const struct uc2_dict *a, const struct uc2_dict *b)
{
	return a->id == b->id;
}

/* Free dictionary data. */
void uc2_dict_free(struct uc2_dict *dict);

#endif
