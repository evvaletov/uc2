Building
========

Requirements
------------

- CMake >= 3.16
- C99 compiler: GCC, Clang, or MSVC
- Optional: DJGPP cross-compiler for DOS builds

Linux / macOS
-------------

.. code-block:: sh

   cmake -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build
   ctest --test-dir build

The binary is ``build/cli/uc2`` and the library is
``build/lib/libuc2.a``.

Windows (MSVC)
--------------

.. code-block:: sh

   cmake -B build
   cmake --build build --config Release
   ctest --test-dir build -C Release

DOS (DJGPP Cross-Compilation)
-----------------------------

Cross-compile from a Linux host using the DJGPP toolchain:

.. code-block:: sh

   cmake -B build-dos -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp.cmake
   cmake --build build-dos

This produces a DOS executable suitable for DOSBox or real hardware.

Build Options
-------------

.. list-table::
   :header-rows: 1
   :widths: 30 15 55

   * - Option
     - Default
     - Description
   * - ``UC2_BUILD_TESTS``
     - ``ON``
     - Build test programs
   * - ``CMAKE_BUILD_TYPE``
     - (none)
     - ``Release``, ``Debug``, ``RelWithDebInfo``

Running Tests
-------------

.. code-block:: sh

   ctest --test-dir build --output-on-failure

Tests include:

- **identify**: UC2 magic detection
- **extract**: decompression against reference archives
- **roundtrip**: compress → archive → decompress → verify (8 patterns
  × 4 compression levels = 32 tests)
