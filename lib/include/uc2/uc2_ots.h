/* SPDX-License-Identifier: GPL-3.0-or-later */

/* OpenTimestamps integration.
 *
 * UC2 stores an OpenTimestamps proof in a magic-bracketed sidecar
 * trailer appended after the regular UC2 archive bytes.  The trailer
 * does not affect compatibility with the original UC2 Pro reader,
 * which uses the front header's recorded length.
 *
 * The proof itself is the standard `.ots` binary: a 31-byte header
 * magic + version + file-hash op + leaf digest + serialized timestamp.
 * Callers can extract the proof verbatim and run the standard
 * `ots verify` tool on it.
 *
 * Local verification covers structural validity and the calendar-path
 * subset of opcodes (APPEND, PREPEND, SHA256).  Proofs that use other
 * crypto ops (SHA1, RIPEMD160, KECCAK256) are accepted as structurally
 * valid but reported as not locally cryptographically verified;
 * the standard `ots verify` should be used for full validation. */

#ifndef UC2_OTS_H
#define UC2_OTS_H

#include <stdint.h>
#include <stddef.h>

/* OTS opcodes. */
enum {
	UC2_OTS_OP_APPEND     = 0xf0, /* binary: append varbytes operand */
	UC2_OTS_OP_PREPEND    = 0xf1, /* binary: prepend varbytes operand */
	UC2_OTS_OP_REVERSE    = 0xf2, /* unary, deprecated */
	UC2_OTS_OP_HEXLIFY    = 0xf3, /* unary */
	UC2_OTS_OP_SHA1       = 0x02, /* unary */
	UC2_OTS_OP_RIPEMD160  = 0x03, /* unary */
	UC2_OTS_OP_SHA256     = 0x08, /* unary, file-hash op */
	UC2_OTS_OP_KECCAK256  = 0x67, /* unary */
	UC2_OTS_BRANCH        = 0xff,
	UC2_OTS_ATTESTATION   = 0x00
};

#define UC2_OTS_HEADER_MAGIC \
	"\x00OpenTimestamps\x00\x00Proof\x00\xbf\x89\xe2\xe8\x84\xe8\x92\x94"
#define UC2_OTS_HEADER_MAGIC_LEN 31
#define UC2_OTS_VERSION 0x01

/* Attestation tags (8 bytes each). */
#define UC2_OTS_TAG_PENDING  "\x83\xdf\xe3\x0d\x2e\xf9\x0c\x8e"
#define UC2_OTS_TAG_BITCOIN  "\x05\x88\x96\x0d\x73\xd7\x19\x01"
#define UC2_OTS_TAG_LITECOIN "\x06\x86\x9a\x0d\x73\xd7\x1b\x45"
#define UC2_OTS_TAG_LEN 8

/* Hard limits to bound parser cost on hostile input. */
#define UC2_OTS_MAX_DIGEST_LEN  64
#define UC2_OTS_MAX_VARBYTES    8192
#define UC2_OTS_MAX_DEPTH       32
#define UC2_OTS_MAX_VARINT      0xffffffffu

/* Error codes. */
enum {
	UC2_OTS_OK              =  0,
	UC2_OTS_ERR_TRUNCATED   = -1,
	UC2_OTS_ERR_NONCANONICAL= -2,
	UC2_OTS_ERR_OVERFLOW    = -3,
	UC2_OTS_ERR_BAD_MAGIC   = -4,
	UC2_OTS_ERR_BAD_VERSION = -5,
	UC2_OTS_ERR_BAD_HASH_OP = -6,
	UC2_OTS_ERR_DEPTH       = -7,
	UC2_OTS_ERR_TOO_LARGE   = -8,
	UC2_OTS_ERR_BAD_OP      = -9
};

/* Verification result reported by uc2_ots_walk. */
enum {
	UC2_OTS_RESULT_VERIFIED       = 1, /* leaf reaches all attestations via supported ops only */
	UC2_OTS_RESULT_STRUCTURAL     = 2, /* parses cleanly but contains unsupported ops */
	UC2_OTS_RESULT_LEAF_MISMATCH  = 3  /* shape OK but leaf digest doesn't match input */
};

/* Attestation summary callback.  Called once per attestation reached.
 * `digest` is the digest at the leaf where the attestation was emitted.
 * Return non-zero to abort the walk.
 *
 * Note: the digest is only meaningful when uc2_ots_walk returns
 * UC2_OTS_RESULT_VERIFIED.  When the walker returns
 * UC2_OTS_RESULT_STRUCTURAL the proof contains unsupported unary ops
 * (SHA1, RIPEMD160, KECCAK256, REVERSE, HEXLIFY) which leave the digest
 * unchanged for structural traversal; the digest passed to the callback
 * does not represent the cryptographic state at that leaf. */
typedef int (*uc2_ots_attest_cb)(void *ctx,
                                 const uint8_t *tag /* 8 bytes */,
                                 const uint8_t *payload, size_t payload_len,
                                 const uint8_t *digest, size_t digest_len);

/* OTS varint codec.  *out_value is set on success; *consumed is the
 * number of input bytes read. */
int uc2_ots_varint_decode(const uint8_t *in, size_t in_len,
                          uint64_t *out_value, size_t *consumed);
size_t uc2_ots_varint_encode(uint64_t value, uint8_t out[10]);

/* Parse the .ots file envelope (header magic + version + file-hash op +
 * leaf digest + timestamp body).  Sets out_* pointers into the input
 * buffer; no allocation. */
int uc2_ots_parse_file(const uint8_t *file, size_t file_len,
                       uint8_t *out_hash_op,
                       const uint8_t **out_leaf_digest,
                       size_t *out_leaf_digest_len,
                       const uint8_t **out_body,
                       size_t *out_body_len);

/* Build a .ots file from a leaf digest and a serialized timestamp body.
 * Returns total bytes written, or a negative error code. */
int uc2_ots_serialize_file(uint8_t hash_op,
                           const uint8_t *leaf_digest, size_t leaf_digest_len,
                           const uint8_t *body, size_t body_len,
                           uint8_t *out, size_t out_cap);

/* Walk a serialized timestamp body from `leaf_digest`, applying ops and
 * invoking `cb` for each attestation reached.  Returns one of
 * UC2_OTS_RESULT_* on structural success, or a negative error code. */
int uc2_ots_walk(const uint8_t *body, size_t body_len,
                 const uint8_t *leaf_digest, size_t leaf_digest_len,
                 uc2_ots_attest_cb cb, void *ctx);

/* UC2 OTS trailer.
 *
 * Layout (all integers little-endian, 32-bit unsigned):
 *
 *   [archive bytes ...]
 *   "UC2-OTS\0"     (8 bytes, front magic)
 *   u32  version     (= 1)
 *   u32  archive_len (length of preceding archive bytes)
 *   u32  proof_len
 *   bytes  proof    (proof_len bytes, raw .ots file)
 *   u32  proof_len   (duplicate, for reverse-scan)
 *   "UC2-OTS\0"     (8 bytes, back magic)
 */

#define UC2_OTS_TRAILER_MAGIC      "UC2-OTS\0"
#define UC2_OTS_TRAILER_MAGIC_LEN  8
#define UC2_OTS_TRAILER_VERSION    1u
#define UC2_OTS_TRAILER_HEAD_LEN   (UC2_OTS_TRAILER_MAGIC_LEN + 4 + 4 + 4)
#define UC2_OTS_TRAILER_TAIL_LEN   (4 + UC2_OTS_TRAILER_MAGIC_LEN)
#define UC2_OTS_TRAILER_OVERHEAD   (UC2_OTS_TRAILER_HEAD_LEN + UC2_OTS_TRAILER_TAIL_LEN)
#define UC2_OTS_TRAILER_MAX_PROOF  (1u << 20)

/* Build a trailer for an existing archive of length archive_len.
 * Writes [front magic | version | archive_len | proof_len | proof | proof_len | back magic]
 * to out.  Returns total bytes written, or negative on error. */
int uc2_ots_trailer_build(uint32_t archive_len,
                          const uint8_t *proof, size_t proof_len,
                          uint8_t *out, size_t out_cap);

/* Read a trailer from the end of a file image.  On success sets
 *   *out_archive_len = length of preceding archive (the SHA-256 region)
 *   *out_proof, *out_proof_len = pointer/length of proof inside `file`
 * Returns:
 *   UC2_OTS_OK if a well-formed trailer is present,
 *   1 if no trailer (back magic absent),
 *   negative error code if the back magic is present but the trailer is malformed. */
int uc2_ots_trailer_parse(const uint8_t *file, size_t file_len,
                          uint32_t *out_archive_len,
                          const uint8_t **out_proof, size_t *out_proof_len);

/* Convenience: get a human-readable name for a known attestation tag,
 * or NULL if unknown. */
const char *uc2_ots_attest_name(const uint8_t tag[UC2_OTS_TAG_LEN]);

#endif
