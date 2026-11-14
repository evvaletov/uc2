Quick Start
===========

Building
--------

Requires CMake >= 3.16 and a C99 compiler (GCC, Clang, or MSVC).

.. code-block:: sh

   cmake -B build
   cmake --build build

The Unix binary is at ``build/cli/uc2``. The examples below run that build
directly; an installed CLI can be invoked as ``uc2``. For Windows build and
test commands, see :doc:`building`.

Basic Usage
-----------

.. code-block:: sh

   ./build/cli/uc2 -w archive.uc2 file1 file2  # Create archive
   ./build/cli/uc2 archive.uc2                # Extract all files
   ./build/cli/uc2 -l archive.uc2             # List contents
   ./build/cli/uc2 -t archive.uc2             # Test archive integrity
   mkdir -p /tmp/out
   ./build/cli/uc2 -d /tmp/out archive.uc2     # Extract to directory
   ./build/cli/uc2 -w -L 5 big.uc2 data/*      # Create with Ultra compression
