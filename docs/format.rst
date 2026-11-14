UC2 Archive Format
==================

This is a partial guide to the ordinary, single-container UC2 format read by
this project. It describes the main headers, central-directory record types,
master dictionaries, and Huffman bitstream. It is not a complete specification
of the original UC2 format.

The original record declarations are in
`SUPERMAN.H <https://github.com/evvaletov/uc2/blob/main/original/UC2_source/src/SUPERMAN.H>`_;
the current reader is in
`decompress.c <https://github.com/evvaletov/uc2/blob/main/lib/src/decompress.c>`_.
Encryption, PCP preprocessing, recovery data, and the Turbo bitstream are
outside this guide. SAS and UC2SEA framing is described in the
`volume guide <https://github.com/evvaletov/uc2/blob/main/docs/volumes.md>`_.
The revival's rANS extension is also outside this guide; method 11 is described
in the `shared-chunk format guide
<https://github.com/evvaletov/uc2/blob/main/docs/chunk-sharing.md>`_.

Archive Layout
--------------

.. code-block:: none

   FHEAD (13 bytes)
   XHEAD (16 bytes)
   File data blocks (compressed bitstreams)
   COMPRESS + compressed central directory

All multi-byte integers are little-endian.

FHEAD: File Header
~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 15 15 70

   * - Offset
     - Size
     - Field
   * - 0
     - 4
     - Magic: ``UC2\x1A`` (0x1A324355)
   * - 4
     - 4
     - Component length
   * - 8
     - 4
     - Component length + 0x01B2C3D4 (validation)
   * - 12
     - 1
     - Damage protection flag

XHEAD: Extended Header
~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 15 15 70

   * - Offset
     - Size
     - Field
   * - 13
     - 4
     - Cdir volume (1 for ordinary containers accepted by this reader)
   * - 17
     - 4
     - Cdir offset
   * - 21
     - 2
     - Historical XOR checksum of raw cdir
   * - 23
     - 1
     - Busy flag
   * - 24
     - 2
     - Version made by (e.g. 200 = v2.00)
   * - 26
     - 2
     - Version needed to extract
   * - 28
     - 1
     - Reserved

Central Directory
-----------------

The central directory is itself compressed using the UC2 compression
engine.  It is located at the offset specified in XHEAD and preceded by
a COMPRESS record.

Each directory entry begins with a 1-byte type tag:

.. list-table::
   :widths: 15 85

   * - 1
     - Directory entry (OSMETA + DIRMETA)
   * - 2
     - File entry (OSMETA + FILEMETA + COMPRESS + LOCATION)
   * - 3
     - Master entry (MASMETA + COMPRESS + LOCATION)
   * - 4
     - End of central directory

The directory ends with XTAIL (17 bytes) + archive serial (4 bytes).

Master Blocks
~~~~~~~~~~~~~

Masters are LZ77 dictionary prefixes that pre-fill the sliding window
before decompression, allowing back-references into shared content
across files.  Three kinds exist:

.. list-table::
   :widths: 15 85

   * - 0
     - **SuperMaster**: built-in 49 152-byte dictionary, decompressed
       from a static blob embedded in the library.
   * - 1
     - **NoMaster**: 512 zero bytes (minimal dictionary).
   * - ≥ 2
     - **Custom master**: archive-specific, described by a MASMETA
       record in the central directory.

MASMETA (20 bytes):

.. list-table::
   :widths: 15 15 70

   * - Offset
     - Size
     - Field
   * - 0
     - 4
     - Master index (≥ 2)
   * - 4
     - 4
     - Master grouping key (writer-dependent)
   * - 8
     - 4
     - Total uncompressed size of referring files
   * - 12
     - 4
     - Number of referring files
   * - 16
     - 2
     - Master data length (uncompressed, ≤ 65 535)
   * - 18
     - 2
     - Historical checksum field (not validated by this reader)

A master entry in the cdir is: type byte (3) + MASMETA (20) +
COMPRESS (10) + LOCATION (8) = 39 bytes.  The compressed master data
is stored at the location pointed to by LOCATION; it is itself
compressed using another master (typically SuperMaster).

The grouping key is not needed for decoding. The original writer's ``ToKey``
routine in
`NEUROMAN.CPP <https://github.com/evvaletov/uc2/blob/main/original/UC2_source/src/NEUROMAN.CPP>`_
derives it from filenames or file types. The revival writer uses an FNV-1a hash
of the master bytes. The original source calls the final MASMETA field a
Fletcher checksum, but historical archives may contain a placeholder there.

Compression Format
------------------

The legacy Huffman methods use LZ77 with Huffman entropy coding. The bitstream
consists of blocks, each containing:

1. **Block-present flag** (1 bit): 1 = block follows, 0 = end of stream.

2. **Huffman tree** encoded as:

   - Tree-changed flag (1 bit): 0 = use default tree, 1 = new tree.
   - Type flags (2 bits): ``has_lo | has_hi << 1``, controlling which
     symbol ranges are encoded.
   - Tree-encoding tree (15 × 3-bit lengths).
   - Delta-coded symbol lengths with RLE (344 symbols total =
     256 literals + 60 distance + 28 length).

3. **Compressed data**: Huffman-coded literals and distance/length pairs.

4. **End-of-block marker**: distance = 64001 with length = 3.

Distance Encoding
~~~~~~~~~~~~~~~~~

60 distance symbols in 4 tiers:

- Tier 0: distances 1--15 (0 extra bits)
- Tier 1: distances 16--255 (4 extra bits)
- Tier 2: distances 256--4095 (8 extra bits)
- Tier 3: distances 4096--64000 (12 extra bits)

Length Encoding
~~~~~~~~~~~~~~~

28 length symbols with varying extra bits, covering lengths 3--35482.

Delta-Coded Trees
~~~~~~~~~~~~~~~~~

Symbol code lengths are delta-coded against the previous block's
lengths using the ``vval`` lookup table.  The first block's default
lengths are hard-coded.  The delta stream uses 14 delta codes (0--13)
plus a repeat code for RLE compression.

Historical XOR Checksum
-----------------------

For file data and the uncompressed central directory, start with 0xA55A and
XOR each little-endian 16-bit word into the result. An odd final byte is XORed
into the low byte of the result. When processing data in pieces, preserve the
byte position across calls so the next byte completes any pending halfword.

The original code calls this routine ``Fletcher`` in
`FLETCH.CPP <https://github.com/evvaletov/uc2/blob/main/original/UC2_source/src/FLETCH.CPP>`_,
but it does not implement the standard Fletcher checksum.
