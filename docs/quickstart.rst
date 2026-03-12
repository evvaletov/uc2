Quick Start
===========

Building
--------

Requires CMake >= 3.16 and a C99 compiler (GCC, Clang, or MSVC).

.. code-block:: sh

   cmake -B build
   cmake --build build

The binary is at ``build/cli/uc2``.

Basic Usage
-----------

.. code-block:: sh

   uc2 archive.uc2                  # Extract all files
   uc2 -l archive.uc2               # List contents
   uc2 -t archive.uc2               # Test archive integrity
   uc2 -d /tmp/out archive.uc2      # Extract to directory
