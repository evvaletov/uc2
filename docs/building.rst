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
   ctest --test-dir build --output-on-failure -E roundtrip_dosbox

The binary is ``build/cli/uc2`` and the library is
``build/lib/libuc2.a``.

Windows (MSVC)
--------------

.. code-block:: sh

   cmake -B build
   cmake --build build --config Release
   ctest --test-dir build -C Release --output-on-failure -E roundtrip_dosbox

DOS (DJGPP Cross-Compilation)
-----------------------------

Cross-compile from a Linux host using the DJGPP toolchain:

.. code-block:: sh

   cmake -B build-dos -DCMAKE_TOOLCHAIN_FILE=cmake/djgpp.cmake
   cmake --build build-dos

This produces a DOS executable suitable for DOSBox or real hardware.

libarchive reader and bsdtar
----------------------------

The optional reader uses libarchive's private API and must be compiled against
its source tree. It cannot be loaded into an arbitrary system libarchive.
To build a patched bsdtar, first build the UC2 CLI above, then run:

.. code-block:: sh

   curl -fsSLO https://github.com/libarchive/libarchive/releases/download/v3.8.11/libarchive-3.8.11.tar.xz
   echo 'a3dc0872b1b39da34dc18ef1b944f0f11ffd37425d70ae04be679279f1cf43d2  libarchive-3.8.11.tar.xz' | sha256sum -c -
   tar xf libarchive-3.8.11.tar.xz
   python3 contrib/libarchive/build_bsdtar.py --source libarchive-3.8.11 --build build-bsdtar
   python3 tests/scripts/check_bsdtar.py build/cli/uc2 build-bsdtar/build/bin/bsdtar

The build directory must be new. The recipe copies the supplied source before
registering the UC2 reader. It handles directories, long names, masters,
Huffman/rANS, and pipes. Payloads spool to temporary files; metadata and cached
masters still consume memory proportional to archive contents.

The separate ``uc2_libarchive`` target and its public-API test are documented in
``contrib/libarchive/README.md``. The adapter has a BSD license option, but the
patched distribution still includes GPL/LGPL components. Upstream acceptance
requires a compatible dependency arrangement.

File-manager readers
--------------------

``contrib/mc/README.md`` describes the read-only Midnight Commander helper and
its per-user installation. It requires the UC2 CLI on PATH or in ``UC2``.

``contrib/totalcommander/README.md`` describes the Windows WCX plugin. Enable
``UC2_BUILD_WCX`` and provide ``UC2_WCX_SDK_DIR`` containing the official SDK's
``wcxhead.h``. Release packages include 32- and 64-bit DLLs tested in native
Total Commander 11.58.1. Both architectures use normal ``.uc2`` registration.
Users of the old workaround should remove their ``*.uc2`` / ``**u2r`` internal
association as described in the plugin README.


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
   * - ``UC2_BUILD_LIBARCHIVE_PLUGIN``
     - ``OFF``
     - Build the libarchive read handler (needs ``LIBARCHIVE_SOURCE_DIR``)
   * - ``UC2_BUILD_WCX``
     - ``OFF``
     - Build the Windows WCX reader (needs ``UC2_WCX_SDK_DIR``)
   * - ``CMAKE_BUILD_TYPE``
     - (none)
     - ``Release``, ``Debug``, ``RelWithDebInfo``

Running Tests
-------------

.. code-block:: sh

   ctest --test-dir build --output-on-failure -E roundtrip_dosbox

The regular suite covers format detection, fixture extraction, all eight
compression levels, master dictionaries, directories, checksums, ingestion,
integrations, and saved fuzz regressions. The separate ``roundtrip_dosbox``
test needs DOSBox-X and checks both directions against the original UC2 Pro.
See ``tests/scripts/roundtrip_dosbox.sh`` for its environment requirements.
