/* SPDX-License-Identifier: LGPL-3.0-only */

#ifndef LIBUC2_H
#define LIBUC2_H

#ifndef UC2_API
#  if defined(UC2_SHARED) && defined(_WIN32)
#    ifdef UC2_BUILD
#      define UC2_API __declspec(dllexport)
#    else
#      define UC2_API __declspec(dllimport)
#    endif
#  elif defined(UC2_SHARED) && defined(__GNUC__)
#    define UC2_API __attribute__((visibility("default")))
#  else
#    define UC2_API
#  endif
#endif

/* API
 uc2_identify - check UC2 magic
 uc2_open - initialize
 uc2_read_cdir - read dir entry
 uc2_get_tag - read tag
 uc2_finish_cdir - get archive label
 uc2_extract - decompress file
 uc2_message - get error message
 uc2_close - free resources

 uc2_open
 repeat {
  uc2_read_cdir
  if UC2_End
   optionally uc2_finish_cdir
  else while UC2_TaggedEntry
    uc2_get_tag
 }
 uc2_close
*/

struct uc2_io;     // User callbacks
struct uc2_xinfo;  // Extraction info
struct uc2_entry;  // CDir entry
struct uc2_context;
typedef struct uc2_context *uc2_handle;

UC2_API int uc2_identify(void *magic, unsigned magic_size /* 4..21 */);
UC2_API uc2_handle uc2_open(struct uc2_io *io, void *io_ctx);
UC2_API uc2_handle uc2_close(uc2_handle);

/* Get cdir entry. Pass NULL to skip the rest. Returns:
     UC2_End: The end, entry not filled,
     UC2_BareEntry: New entry filled, no tags,
     UC2_TaggedEntry: New entry filled, has tags (must call uc2_get_tag),
     Negative value on error.
   Directories come before content. Duplicates: older first. */
UC2_API int uc2_read_cdir(
	uc2_handle,
	struct uc2_entry * // Entry to fill. Pass NULL to finish early.
);

/* Returns UC2_End, if final tag, UC2_TaggedEntry, if more, or negative on error */
UC2_API int uc2_get_tag(
	uc2_handle,
	struct uc2_entry *, // to fill name
	char **tag, // char[16], pass NULL to skip tags
	void **data,
	unsigned *data_len
);

UC2_API int uc2_finish_cdir(
	uc2_handle,
	char label[12]
);

/* Allowed only after whole cdir has been read.
   write() should return <0 on error. Returns a nonnegative value on
   success (not necessarily a byte count), or a negative error code. */
UC2_API int uc2_extract(
	uc2_handle,
	struct uc2_xinfo *,
	unsigned size,
	int (*write)(void *context, const void *ptr, unsigned len),
	void *context
);

/* Decode a standalone Huffman bitstream with its 512-byte zero history.
   Returns decoded byte count or a negative error. max_output must be 1..INT_MAX.
   compressed_size must be even; rejects consumed padding and trailing words.
   Does not authenticate data; callers must validate framing/checksums. */
UC2_API int uc2_decompress_raw(struct uc2_io *, void *io_context, unsigned offset,
                              unsigned compressed_size, unsigned max_output,
                              int (*write)(void *, const void *, unsigned), void *output);

UC2_API const char *uc2_message(uc2_handle, int ret);

#ifndef UC2_HUFFMAN_ONLY
/* Compress raw data into a UC2 bitstream (no archive framing).
   level: 2=Fast, 3=Normal, 4=Tight, 5=Ultra (Huffman);
   6-9 are the corresponding rANS levels in the full build.
   read() should return bytes read (0 at EOF, <0 on error).
   write() should return <0 on error.
   Returns 0 on success, negative UC2_* error code on failure. */
UC2_API int uc2_compress(
	int level,
	int (*read)(void *context, void *buf, unsigned len),
	void *read_ctx,
	int (*write)(void *context, const void *ptr, unsigned len),
	void *write_ctx,
	unsigned size,
	unsigned short *checksum_out,
	unsigned *compressed_size_out
);

/* Compress with a master-block dictionary prefix.
   The master data pre-fills the LZ77 sliding window, allowing
   back-references into the master for cross-file deduplication.
   Set master=NULL, master_size=0 for no master (same as uc2_compress). */
UC2_API int uc2_compress_ex(
	int level,
	const void *master, unsigned master_size,
	int (*read)(void *context, void *buf, unsigned len),
	void *read_ctx,
	int (*write)(void *context, const void *ptr, unsigned len),
	void *write_ctx,
	unsigned size,
	unsigned short *checksum_out,
	unsigned *compressed_size_out
);
#endif

/* Decompress the built-in SuperMaster (49152 bytes).
   buf must be at least 49152 bytes.
   Returns 49152 on success, negative UC2_* error code on failure. */
UC2_API int uc2_get_supermaster(void *buf, unsigned buf_size);

struct uc2_io {
	/* Read len bytes from the archive at offset pos into buf.
	   Return number of bytes read, or less if eof.
	   Negative value indicates an error. */
	int (*read)(void *io_ctx, unsigned pos, void *buf, unsigned len);

	/* Allocate memory. Return NULL on error */
	void *(*alloc)(void *io_ctx, unsigned size);
	void (*free)(void *io_ctx, void *ptr);

	/* Optional */
	void (*warn)(void *io_ctx, char *fmt, ...);
};

enum {
	UC2_UserFault = -2, // User callback refused to cooperate.
	UC2_BadState = -3, // uc2_scan() should return 0 first.
	UC2_Damaged = -4,
	UC2_Truncated = -5,
	UC2_Unimplemented = -6,
	UC2_InternalError = -7
};

enum {
	UC2_End = 0,
	UC2_BareEntry = 1,
	UC2_TaggedEntry = 3
};

struct uc2_xinfo {
	unsigned offset, master;
	unsigned short csum, method;
};

struct uc2_entry {
	unsigned dirid; // Directory it belongs to. Root is 0.
	unsigned id; // dir only
	struct uc2_xinfo xi;
	unsigned size;  // file only
	unsigned csize; // file only
	unsigned dos_time;
	unsigned char is_dir:1;
	unsigned char has_tags:1;
	unsigned char attr;
	unsigned char dos_name[11]; // not terminated
	unsigned short name_len;
	char name[300]; // ready after tags have been read
};

enum {
	UC2_Attr_R = 1,
	UC2_Attr_H = 2,
	UC2_Attr_S = 4,
	UC2_Attr_D = 16,
	UC2_Attr_A = 32
};

#endif // LIBUC2_H
