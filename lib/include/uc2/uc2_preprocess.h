/* Content-aware preprocessing filters for improved compression.
 *
 * These transforms are applied BEFORE compression to expose redundancy
 * that LZ77+entropy coding can exploit more efficiently.  Each filter
 * is reversible (apply/revert) and content-type specific.
 *
 * Filters:
 *   BCJ  — x86 branch/call/jump address normalization (E8/E9 transform)
 *   BWT  — Burrows-Wheeler transform for text (groups similar contexts)
 *   Delta — byte-wise delta encoding for structured/tabular data
 */

#ifndef UC2_PREPROCESS_H
#define UC2_PREPROCESS_H

#include <stdint.h>
#include <stddef.h>

/* --- BCJ (Branch/Call/Jump) filter for x86 executables --- */

/* Convert relative x86 CALL/JMP addresses to absolute.
 * This makes the same function called from different locations produce
 * identical byte sequences, improving LZ77 matching.
 * Operates in-place.  Returns 0 on success. */
int uc2_bcj_apply(uint8_t *data, size_t len);

/* Revert BCJ transform (absolute → relative). */
int uc2_bcj_revert(uint8_t *data, size_t len);

/* --- BWT (Burrows-Wheeler Transform) for text --- */

/* Apply BWT to data.  Allocates *out (caller must free).
 * Sets *primary_index to the BWT primary index (needed for revert).
 * Returns 0 on success. */
int uc2_bwt_apply(const uint8_t *data, size_t len,
                  uint8_t **out, uint32_t *primary_index);

/* Revert BWT.  Allocates *out (caller must free).
 * Returns 0 on success. */
int uc2_bwt_revert(const uint8_t *data, size_t len,
                   uint32_t primary_index, uint8_t **out);

/* --- Delta filter for structured data --- */

/* Apply byte-wise delta encoding (each byte = current - previous).
 * Operates in-place.  Stride controls the delta distance (1 = adjacent
 * bytes, 2 = every other byte, etc.).  Stride 1 is best for sequential
 * data; stride 2+ for interleaved multi-channel data. */
void uc2_delta_filter_apply(uint8_t *data, size_t len, int stride);

/* Revert byte-wise delta encoding.  Operates in-place. */
void uc2_delta_filter_revert(uint8_t *data, size_t len, int stride);

/* --- Content detection --- */

/* Detect likely content type for automatic filter selection.
 * Returns one of the UC2_CONTENT_* constants. */
#define UC2_CONTENT_BINARY  0  /* generic binary / unknown */
#define UC2_CONTENT_TEXT    1  /* text (high ASCII printable ratio) */
#define UC2_CONTENT_X86     2  /* x86 executable (MZ/PE/ELF header) */
#define UC2_CONTENT_STRUCT  3  /* structured/tabular (regular patterns) */

int uc2_detect_content(const uint8_t *data, size_t len);

#endif
