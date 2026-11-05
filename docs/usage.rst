Command-Line Usage
==================

Synopsis
--------

.. code-block:: none

   uc2 [options] archive.uc2 [patterns...]
   uc2 -w [-L level] archive.uc2 files...

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
   Create a new archive from the given files.  Levels 2-5 use LZ77+Huffman; levels 6-9 use rANS.  Compression level defaults to 4 (Tight); use
   ``-L`` to change it.

   The archiver groups files that share CDC chunk fingerprints and builds
   custom master dictionaries from their content. A fingerprint collision
   can affect grouping and compression ratio; each file is still compressed
   in full against its assigned dictionary. Masters contain at most 64,000
   bytes. Ungrouped files receive a fallback custom master, because direct
   use of SuperMaster in file records hangs the original UC2 Pro reader.

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
   * - ``-a``
     - Include all file versions (not just latest)
   * - ``-d path``
     - Extract to specified directory
   * - ``-f``
     - Overwrite existing files
   * - ``-p``
     - Extract to stdout
   * - ``-D``
     - Skip directory metadata; ``-DD`` also skips file metadata
   * - ``-T``
     - Tab-separated output (for scripting)

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
