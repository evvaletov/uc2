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
   Create a new archive from the given files.  The original LZ77+Huffman
   algorithm is used.  Compression level defaults to 4 (Tight); use
   ``-L`` to change it.

   The archiver automatically groups similar files using content
   fingerprinting: files sharing identical first 4096 bytes are assigned a
   custom master block built from the largest file in the group.  This
   pre-fills the LZ77 sliding window with shared content, improving
   compression for collections of structurally similar files (e.g. log
   rotations, versioned configs, same-format data files).  Files that
   don't group (or are smaller than 1 KB) use the built-in 49 KB
   SuperMaster dictionary.

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
       5 = Ultra
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
