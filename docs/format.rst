UC2 Archive Format
==================

This documents the binary format as implemented by the original UC2
v2.x and supported by UC2 v3.

Archive Layout
--------------

.. code-block:: none

   FHEAD (13 bytes)
   XHEAD (16 bytes)
   File data blocks (compressed bitstreams)
   COMPRESS + compressed central directory

All multi-byte integers are little-endian.

FHEAD — File Header
~~~~~~~~~~~~~~~~~~~

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

XHEAD — Extended Header
~~~~~~~~~~~~~~~~~~~~~~~

.. list-table::
   :widths: 15 15 70

   * - Offset
     - Size
     - Field
   * - 13
     - 4
     - Cdir volume (always 1)
   * - 17
     - 4
     - Cdir offset
   * - 21
     - 2
     - Fletcher checksum of raw cdir
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
     - **SuperMaster** — built-in 49 152-byte dictionary, decompressed
       from a static blob embedded in the library.
   * - 1
     - **NoMaster** — 512 zero bytes (minimal dictionary).
   * - ≥ 2
     - **Custom master** — archive-specific, described by a MASMETA
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
     - Content key (FNV-1a hash)
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
     - Fletcher checksum of master data

A master entry in the cdir is: type byte (3) + MASMETA (20) +
COMPRESS (10) + LOCATION (8) = 39 bytes.  The compressed master data
is stored at the location pointed to by LOCATION; it is itself
compressed using another master (typically SuperMaster).

Compression Format
------------------

UC2 uses LZ77 with Huffman entropy coding.  The bitstream consists of
blocks, each containing:

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

Fletcher Checksum
-----------------

UC2 uses an XOR-based Fletcher checksum (initial value 0xA55A) for
both file data integrity and central directory validation.  Bytes are
processed in little-endian 16-bit words with a carry flag for
odd-length data.
