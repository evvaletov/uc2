Library API (libuc2)
====================

``libuc2`` provides C99 functions for reading, extracting, and
compressing UC2 archives.  The library is callback-based: callers supply
I/O and memory callbacks, making it suitable for embedded, DOS, and
freestanding environments.

Header: ``<uc2/libuc2.h>``

Archive Reading
---------------

.. c:function:: int uc2_identify(void *magic, unsigned magic_size)

   Check whether a buffer contains a UC2 archive header.

   :param magic: Pointer to the first 4--21 bytes of the file.
   :param magic_size: Number of bytes available.
   :returns: ``1`` if UC2, ``0`` if not, ``-1`` if more bytes needed.

.. c:function:: uc2_handle uc2_open(struct uc2_io *io, void *io_ctx)

   Open a UC2 archive.  The caller provides I/O callbacks via
   :c:type:`uc2_io`.  Returns ``NULL`` on allocation failure.

.. c:function:: uc2_handle uc2_close(uc2_handle h)

   Close the archive and free all resources.  Always returns ``NULL``.

Directory Enumeration
---------------------

.. c:function:: int uc2_read_cdir(uc2_handle h, struct uc2_entry *entry)

   Read the next central directory entry.

   :returns:
      - ``UC2_End`` (0): end of directory, *entry* not filled.
      - ``UC2_BareEntry`` (1): entry filled, no tags.
      - ``UC2_TaggedEntry`` (3): entry filled, call :c:func:`uc2_get_tag`
        to read tags (long filename, etc.).
      - Negative value on error.

   Directories appear before their contents.  Duplicate filenames are
   listed oldest-first.

.. c:function:: int uc2_get_tag(uc2_handle h, struct uc2_entry *entry, char **tag, void **data, unsigned *data_len)

   Read a tag from a tagged entry.  Call repeatedly until it returns
   ``UC2_End``.

.. c:function:: int uc2_finish_cdir(uc2_handle h, char label[12])

   Read the archive tail and retrieve the volume label.

Extraction
----------

.. c:function:: int uc2_extract(uc2_handle h, struct uc2_xinfo *xi, unsigned size, int (*write)(void *ctx, const void *ptr, unsigned len), void *ctx)

   Decompress a file entry.  Call only after the entire central
   directory has been read.  The *write* callback receives decompressed
   data in chunks.

Compression
-----------

.. c:function:: int uc2_compress(int level, int (*read)(void *ctx, void *buf, unsigned len), void *read_ctx, int (*write)(void *ctx, const void *ptr, unsigned len), void *write_ctx, unsigned size, unsigned short *checksum_out, unsigned *compressed_size_out)

   Compress raw data into a UC2 bitstream (no archive framing).

   :param level: Compression level: 2 = Fast, 3 = Normal, 4 = Tight
                 (default), 5 = Ultra.
   :param read: Callback returning bytes read (0 at EOF, <0 on error).
   :param write: Callback returning <0 on error.
   :param size: Total input size in bytes.
   :param checksum_out: Receives the Fletcher checksum of the input.
   :param compressed_size_out: Receives the compressed size.
   :returns: 0 on success, negative ``UC2_*`` error code on failure.

.. c:function:: int uc2_compress_ex(int level, const void *master, unsigned master_size, int (*read)(void *ctx, void *buf, unsigned len), void *read_ctx, int (*write)(void *ctx, const void *ptr, unsigned len), void *write_ctx, unsigned size, unsigned short *checksum_out, unsigned *compressed_size_out)

   Compress with a master-block dictionary prefix.  The master data
   pre-fills the LZ77 sliding window, allowing back-references into
   the master for cross-file deduplication.  Pass ``NULL`` / ``0`` for
   no master (equivalent to :c:func:`uc2_compress`).

   The CLI uses the built-in SuperMaster (49 KB) by default.

.. c:function:: int uc2_get_supermaster(void *buf, unsigned buf_size)

   Decompress the built-in SuperMaster into *buf* (must be at least
   49152 bytes).  Returns ``49152`` on success, negative error code on
   failure.

I/O Callbacks
-------------

.. c:struct:: uc2_io

   .. c:member:: int (*read)(void *io_ctx, unsigned pos, void *buf, unsigned len)

      Read *len* bytes from the archive at offset *pos* into *buf*.
      Return number of bytes read (less if EOF), or negative on error.

   .. c:member:: void *(*alloc)(void *io_ctx, unsigned size)

      Allocate memory.  Return ``NULL`` on failure.

   .. c:member:: void (*free)(void *io_ctx, void *ptr)

      Free memory.

   .. c:member:: void (*warn)(void *io_ctx, char *fmt, ...)

      Optional warning callback.

Data Structures
---------------

.. c:struct:: uc2_entry

   A directory entry.

   .. c:member:: unsigned dirid

      Parent directory (0 = root).

   .. c:member:: unsigned id

      Directory index (directories only).

   .. c:member:: unsigned size

      Uncompressed file size.

   .. c:member:: unsigned csize

      Compressed file size.

   .. c:member:: unsigned dos_time

      DOS-format timestamp.

   .. c:member:: unsigned char attr

      DOS file attributes.

   .. c:member:: char name[300]

      Filename (UTF-8, NUL-terminated).  Populated after tags are read.

Error Codes
-----------

.. list-table::
   :header-rows: 1
   :widths: 30 70

   * - Constant
     - Meaning
   * - ``UC2_UserFault`` (-2)
     - User callback refused to cooperate
   * - ``UC2_BadState`` (-3)
     - API called in wrong order
   * - ``UC2_Damaged`` (-4)
     - Archive data is corrupt
   * - ``UC2_Truncated`` (-5)
     - Unexpected end of data
   * - ``UC2_Unimplemented`` (-6)
     - Feature not yet implemented
   * - ``UC2_InternalError`` (-7)
     - Internal logic error
