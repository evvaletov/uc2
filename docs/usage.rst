Command-Line Usage
==================

Synopsis
--------

.. code-block:: none

   uc2 [options] archive.uc2 [patterns...]

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
