/* Delta compression for file versioning.
 *
 * Computes a compact binary delta between a source (old) and target
 * (new) file.  The delta encodes copy-from-source and insert-new-data
 * instructions, similar to xdelta/bsdiff.
 *
 * The delta can be applied to reconstruct the target from the source.
 * Combined with master blocks, this enables version-level dedup:
 * store the first version as a master, subsequent versions as deltas.
 *
 * Usage:
 *   uint8_t *delta; size_t delta_len;
 *   uc2_delta_encode(src, src_len, tgt, tgt_len, &delta, &delta_len);
 *   uint8_t *reconstructed; size_t recon_len;
 *   uc2_delta_apply(src, src_len, delta, delta_len, &reconstructed, &recon_len);
 *   // reconstructed == tgt
 *   free(delta); free(reconstructed);
 */

#ifndef UC2_DELTA_H
#define UC2_DELTA_H

#include <stdint.h>
#include <stddef.h>

/* Encode a delta from source to target.
 * Allocates *out_delta (caller must free).
 * Returns 0 on success, -1 on error. */
int uc2_delta_encode(const uint8_t *src, size_t src_len,
                     const uint8_t *tgt, size_t tgt_len,
                     uint8_t **out_delta, size_t *out_delta_len);

/* Apply a delta to source to reconstruct target.
 * Allocates *out_tgt (caller must free).
 * Returns 0 on success, -1 on error. */
int uc2_delta_apply(const uint8_t *src, size_t src_len,
                    const uint8_t *delta, size_t delta_len,
                    uint8_t **out_tgt, size_t *out_tgt_len);

/* Delta format:
 *   Header: "UC2D" (4 bytes) + target_len (4 bytes LE)
 *   Instructions:
 *     COPY:   0x01 + offset(4 LE) + length(4 LE)  — copy from source
 *     INSERT: 0x02 + length(4 LE) + data[length]  — insert new bytes
 *     END:    0x00                                 — end of delta
 */

#endif
