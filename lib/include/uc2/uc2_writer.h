/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef UC2_WRITER_H
#define UC2_WRITER_H
#include <stdint.h>
#include <uc2/libuc2.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Hosted C99 archive writer. Inputs are copied to an anonymous temporary file
 * as they are added. The completed archive is spooled before output begins.
 * Metadata grows with entries and CDC chunks. Each object is used serially;
 * callbacks must not reenter it. Any failed add/update/finish poisons the object.
 * Free it to discard all temporary data. No callback is retained after return.
 */
typedef struct uc2_writer uc2_writer;
#define UC2_WRITER_UNKNOWN_SIZE UINT64_MAX
#define UC2_WRITER_LIMIT (-8)
#define UC2_WRITER_INVALID (-9)

struct uc2_writer_options {
    int level;                 /* 2-5 Huffman, 6-9 rANS */
    unsigned max_entries;      /* files plus directories; 1..1000000 */
    uint64_t max_input_size;    /* total spooled bytes; 1..UINT32_MAX */
    int chunked;               /* 0: classic layout; 1: shared chunk recipes */
};
#define UC2_WRITER_OPTIONS_INIT {4, 100000, UINT32_MAX, 0}
struct uc2_writer_stats {
    unsigned files, directories, masters, archive_size;
    unsigned unique_chunks, chunk_references;
    uint64_t shared_bytes;
};

/* chunked=1 requires the full revival reader 3.0.0a11 or newer. It stores
 * independent Gear chunks once across files/versions and a recipe per file.
 * Maximum 1048576 logical chunk references across the input collection.
 * Classic output remains the default; historical DOS readers cannot read
 * chunked archives. No cross-archive references are created. */

/* NULL options selects defaults. NULL result: invalid options or allocation/
 * temporary-file failure. Uses malloc/free and the platform's tmpfile(). */
uc2_writer *uc2_writer_create(const struct uc2_writer_options *options);
void uc2_writer_free(uc2_writer *writer);

/* Names are single UTF-8 components representable in CP850, at most 255 encoded
 * bytes and 299 UTF-8 bytes. Empty, dot, dot-dot, slash, backslash, colon and
 * control bytes are rejected. No truncation or Unicode replacement occurs.
 * Parent 0 is the root; otherwise use a previously returned directory ID.
 * Maximum nesting is 64 directories. Duplicate file names store versions in
 * insertion order (last is newest); directory conflicts are rejected.
 * dos_time uses the format's packed local date/time (0 means unspecified).
 * Attributes accept the DOS R/H/S/A bits; the directory bit is added here.
 */
int uc2_writer_add_directory(uc2_writer *, unsigned parent, const char *name,
                             unsigned dos_time, unsigned attributes, unsigned *id);

/* Replace a directory's metadata before finish, including after adding children.
 * id must be a nonzero ID returned by add_directory on this writer; files and
 * the implicit root are invalid. Time/attributes follow add_directory's rules.
 * The name, parent, ID, entry order and limits are unchanged. Returns 0 on
 * success, UC2_WRITER_INVALID for an invalid ID/attributes (poisoning the writer
 * without applying either field), UC2_BadState for a NULL/finalized writer, or
 * the previous error for an already failed writer.
 */
int uc2_writer_set_directory_metadata(uc2_writer *, unsigned id,
                                      unsigned dos_time, unsigned attributes);

/* read returns 0 at EOF, 1..len bytes, or <0 on error. Known size consumes
 * exactly size bytes then checks EOF; it rejects short or excess input.
 * UNKNOWN_SIZE reads through EOF. Both obey max_input_size and UINT32_MAX.
 * Reading may consume one extra byte to detect a declared-size/limit violation.
 */
int uc2_writer_add_file(uc2_writer *, unsigned parent, const char *name,
                        unsigned dos_time, unsigned attributes, uint64_t size,
                        int (*read)(void *, void *, unsigned), void *read_ctx);

/* One-shot finalization. write returns >=0 only after accepting ALL len bytes,
 * or <0 on failure. Before the first call, archive assembly must succeed.
 * A callback error may leave partial output. For atomic path replacement,
 * direct output to an exclusively created sibling temporary file, check flush
 * and close, then rename it. The CLI and Python UC2Writer do this.
 * The archive is limited to UINT32_MAX bytes, the raw central directory to
 * 64 MiB, and the CDC index to 1048576 chunks. Empty archives are supported.
 */
int uc2_writer_finish(uc2_writer *, int (*write)(void *, const void *, unsigned), void *);
const char *uc2_writer_error(const uc2_writer *);
void uc2_writer_get_stats(const uc2_writer *, struct uc2_writer_stats *);

#ifdef __cplusplus
}
#endif
#endif
