Command-Line Usage
==================

Synopsis
--------

.. code-block:: none

   uc2 [options] archive.uc2 [patterns...]
   uc2 [--chunked] -w [-L level] archive.uc2 files...
   uc2 [--chunked] --ingest archive.uc2
   uc2 [--chunked] --ingest-tar archive.uc2
   uc2 --ingest-restore archive.uc2

Modes
-----

``uc2 archive.uc2``
   Extract all files to the current directory.

``uc2 -l archive.uc2``
   List archive contents.

``uc2 -t archive.uc2``
   Test archive integrity (decompress and verify checksums without
   writing files).

``uc2 -p archive.uc2 filename``
   Extract a file to stdout.

``uc2 -w archive.uc2 files...``
   Create a new archive from the given files. Levels 2-5 use LZ77+Huffman;
   levels 6-9 use rANS. Compression level defaults to 4 (Tight); use ``-L``
   to change it.

   By default, the archiver groups files that share CDC chunk fingerprints and
   builds custom master dictionaries from their content. A fingerprint collision
   can affect grouping and compression ratio. Each distinct file is compressed
   in full against its assigned dictionary; exact duplicates may reuse a stored
   payload. Masters contain at most 64,000 bytes. Ungrouped files receive a
   fallback custom master, because direct
   use of SuperMaster in file records hangs the original UC2 Pro reader.

   The destination is replaced after successful archive assembly and output
   close. Inputs spool to temporary files first; keep them unchanged during
   creation. Symlinks and special files are rejected. Names must be UTF-8 and
   representable in CP850, within 255 CP850 / 299 UTF-8 bytes per component.
   Total input and output are each limited to UINT32_MAX bytes. The writer
   accepts empty directories and uses the same C API as Python UC2Writer.

   Without ``--chunked``, levels 2-5 produce archives readable by the original
   DOS UC2. Levels 6-9 require the revival reader. Adding ``--chunked`` shares
   payload chunks across partly changed files and requires a11 or newer at
   every compression level. See the `chunk-sharing guide
   <https://github.com/evvaletov/uc2/blob/main/docs/chunk-sharing.md>`_.

``uc2 --ingest archive.uc2``
   Read an opaque stream from standard input. The default mode stores chunk
   occurrences as ``part-N`` entries; with ``--chunked``, one ``data`` entry
   represents the full stream.

``uc2 --ingest-tar archive.uc2``
   Read tar from standard input and preserve named files and directories.
   See the `tar-ingestion guide
   <https://github.com/evvaletov/uc2/blob/main/docs/tar-ingestion.md>`_ for supported
   tar records, metadata and path restrictions.

``uc2 --ingest-restore archive.uc2``
   Reassemble an opaque ingested stream and write it to standard output.

The ingestion modes use the current directory and default compression level;
short options such as ``-C``, ``-L``
and ``-q`` have no effect. ``--chunked`` is accepted by the two creation modes.

Options
-------

.. list-table::
   :header-rows: 1
   :widths: 15 85

   * - Flag
     - Description
   * - ``-l``
     - List archive contents
   * - ``-t``
     - Test archive integrity
   * - ``-w``
     - Create archive
   * - ``-L n``
     - Compression level: 2 = Fast, 3 = Normal, 4 = Tight (default),
       5 = Ultra; 6-9 = rANS Fast, Normal, Tight, Ultra
   * - ``--chunked``
     - Share payload chunks during creation; requires the a11 or newer reader
   * - ``-a``
     - Include all file versions (not just latest)
   * - ``-d path``
     - Extract to specified directory
   * - ``-C path``
     - Change working directory (except in ingestion modes)
   * - ``-f``
     - Overwrite existing files
   * - ``-p``
     - Extract to stdout
   * - ``-D``
     - Skip directory metadata; ``-DD`` also skips file metadata
   * - ``-T``
     - Tab-separated output (for scripting)
   * - ``-q``
     - Suppress status messages (except in ingestion modes)
   * - ``-B``
     - Benchmark compression methods without writing an archive

Pattern Matching
----------------

File patterns use glob syntax.  Only files matching the pattern are
listed or extracted:

.. code-block:: sh

   uc2 -l archive.uc2 '*.txt'      # List only .txt files
   uc2 archive.uc2 'src/*'          # Extract src/ subtree

Exit Codes
----------

.. list-table::
   :widths: 15 85

   * - ``0``
     - Success
   * - ``1``
     - Error (damaged archive, I/O failure, etc.)
