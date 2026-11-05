# Proposed UC2 reader for libarchive

Prepared for review against libarchive 3.8.11. No contribution has been sent
upstream. `0x110000` is a proposed format code, not an assigned identifier.
The local full-feature plugin continues using its private ID until acceptance.

The patch registers a BSD-2-Clause adapter and links an optional external
`libuc2read` shared library. That library compiles the existing LGPL-3.0-only
reader with rANS disabled. It does not contain the GPL compressor or GPL rANS
module. No existing UC2 or third-party source license changes. Enable it
explicitly with CMake `-DENABLE_UC2READ=ON` or Autotools `--with-uc2read`.
The default build does not acquire the dependency and reports UC2 unavailable.

The adapter reads historical supported Huffman/delta entries and custom
dictionaries. rANS, Turbo, encrypted and multi-volume archives are outside
this proposed dependency's scope. The regular UC2 release retains rANS.
The patch includes registration by code, public declarations, CMake/Autotools
wiring, and generated fixtures for a successful legacy read and rANS rejection.

Input and decoded payloads spool to temporary files. This permits piped input
and avoids keeping whole payloads in RAM. Input is limited by the format's
32-bit address space; metadata and cached dictionaries still grow with archive
contents. This design needs upstream review for resource policy and private API
conventions. Archive versions retain repeated paths in stored order, like the
full adapter. DOS timestamps have no timezone; the adapter treats them as UTC.

To reproduce from the UC2 source checkout:

```sh
cmake -S contrib/libuc2read -B /tmp/uc2-reader \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/tmp/uc2-reader-prefix
cmake --build /tmp/uc2-reader
cmake --install /tmp/uc2-reader
python3 contrib/libarchive/prepare_upstream.py \
  --source /path/to/libarchive-3.8.11 --uc2 /path/to/uc2 \
  --output /tmp/uc2-review
cmake -S /tmp/uc2-review/libarchive -B /tmp/uc2-review/build \
  -DCMAKE_PREFIX_PATH=/tmp/uc2-reader-prefix -DENABLE_UC2READ=ON \
  -DCMAKE_BUILD_TYPE=Release -DENABLE_TEST=ON -DENABLE_WERROR=OFF
cmake --build /tmp/uc2-review/build --target bsdtar libarchive_test
/tmp/uc2-review/build/bin/libarchive_test \
  -r /tmp/uc2-review/libarchive/libarchive/test test_read_format_uc2
python3 tests/scripts/check_bsdtar.py /path/to/uc2 \
  /tmp/uc2-review/build/bin/bsdtar --legacy-only
```

The maintainers still need to decide whether to accept this optional LGPL
dependency and the proposed format number. Full permissive-source rANS support
would require separate licensing work; this patch does not grant it.

The Autotools path was also built and exercised on Linux. After applying the
patch, run `autoreconf -fi` in the libarchive tree, then configure in a separate
build directory with `--with-uc2read`. For a private installation prefix, set
`PKG_CONFIG_PATH` to its `lib/pkgconfig` or `lib64/pkgconfig` directory and
`LD_LIBRARY_PATH` to the matching library directory during both configure and
execution. Configure runs linked test programs, so setting the runtime path
only when running bsdtar is insufficient. This validation built bsdtar with
`--disable-shared --without-xml2 --without-expat` and restored the legacy fixture
byte for byte; it does not establish support for every optional dependency.
